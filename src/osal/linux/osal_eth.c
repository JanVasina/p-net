/*********************************************************************
 *        _       _         _
 *  _ __ | |_  _ | |  __ _ | |__   ___
 * | '__|| __|(_)| | / _` || '_ \ / __|
 * | |   | |_  _ | || (_| || |_) |\__ \
 * |_|    \__|(_)|_| \__,_||_.__/ |___/
 *
 * www.rt-labs.com
 * Copyright 2018 rt-labs AB, Sweden.
 *
 * This software is dual-licensed under GPLv3 and a commercial
 * license. See the file LICENSE.md distributed with this software for
 * full license information.
 ********************************************************************/

#include "options.h"
#include "osal.h"
#include "osal_sys.h"
#include "pf_includes.h"
#include "log.h"
#include "cc.h"
#include <stdlib.h>
#include <errno.h>
#include <net/if.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <linux/if_ether.h>
#include "config.h"

 /**
  * @internal
  * Run a thread that listens to incoming raw Ethernet sockets.
  * Delegate the actual work to thread_arg->callback
  *
  * This is a function to be passed into os_thread_create()
  * Do not change the argument types.
  *
  * @param thread_arg     InOut: Will be converted to os_eth_handle_t
  */
static void *os_eth_pf_task(void *thread_arg)
{
  os_eth_handle_t *eth_handle = thread_arg;
  pnet_t          *net = (pnet_t *)(eth_handle->arg);
  app_data_t      *p_appdata = (app_data_t *)(net->p_fspm_default_cfg->cb_arg);
  ssize_t          readlen;
  int              handled = 0;

  rpmalloc_thread_initialize();
  eth_handle->n_bytes_recv = 0;
  eth_handle->n_bytes_sent = 0;
  os_buf_t *p = os_buf_alloc(OS_BUF_MAX_SIZE);
  assert(p != NULL);

  while (p_appdata->running != false)
  {
//     unsigned long nBytes = 0;
//     ioctl(eth_handle->socket, FIONREAD, &nBytes);
//     if (nBytes == 0)
//     {
//       os_usleep(200);
//       continue;
//     }
    readlen = recv(eth_handle->pf_socket, p->payload, OS_BUF_MAX_SIZE, 0);
    if (readlen == -1)
    {
      os_usleep(200);
      continue;
    }
    p->len = readlen;

    if (eth_handle->callback != NULL)
    {
      eth_handle->n_bytes_recv += readlen;
      handled = eth_handle->callback(eth_handle->arg, p);
    }
    else
    {
      handled = 0;
    }

    if (handled == 1)
    {
      p = os_buf_alloc(OS_BUF_MAX_SIZE);
      assert(p != NULL);
    }
  }
  if(p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "os_eth_task terminated\n");
  }
  rpmalloc_thread_finalize();
  os_mutex_destroy(eth_handle->mutex);
  os_free(eth_handle);
  return NULL;
}

/**
* @internal
* Run a thread that listens to incoming raw Ethernet sockets.
* Delegate the actual work to thread_arg->callback
*
* This is a function to be passed into os_thread_create()
* Do not change the argument types.
*
* @param thread_arg     InOut: Will be converted to os_eth_handle_t
*/
static void *os_eth_lldp_task(void *thread_arg)
{
  os_eth_handle_t *eth_handle = thread_arg;
  pnet_t          *net = (pnet_t *)(eth_handle->arg);
  app_data_t      *p_appdata = (app_data_t *)(net->p_fspm_default_cfg->cb_arg);
  ssize_t          readlen;
  int              handled = 0;

  rpmalloc_thread_initialize();
  os_buf_t *p = os_buf_alloc(OS_BUF_MAX_SIZE);
  assert(p != NULL);

  while (p_appdata->running != false)
  {
    readlen = recv(eth_handle->lldp_socket, p->payload, OS_BUF_MAX_SIZE, 0);
    if (readlen == -1)
    {
      os_usleep(200);
      continue;
    }
    p->len = readlen;

    if (eth_handle->callback != NULL)
    {
      handled = eth_handle->callback(eth_handle->arg, p);
    }
    else
    {
      handled = 0;
    }

    if (handled == 1)
    {
      p = os_buf_alloc(OS_BUF_MAX_SIZE);
      assert(p != NULL);
    }
  }
  if(p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "os_lldp_task terminated\n");
  }
  rpmalloc_thread_finalize();
  os_mutex_destroy(eth_handle->mutex);
  os_free(eth_handle);
  return NULL;
}

static int open_socket(
  const char        *if_name,
  int                protocol)
{
  struct timeval          timeout;
  struct ifreq            ifr;
  struct sockaddr_ll      sll;

  int s = socket(PF_PACKET, SOCK_RAW, htons(protocol));

  timeout.tv_sec = 0;
  timeout.tv_usec = 1;
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  int i = 1;
  setsockopt(s, SOL_SOCKET, SO_DONTROUTE, &i, sizeof(i));

  strcpy(ifr.ifr_name, if_name);
  ioctl(s, SIOCGIFINDEX, &ifr);

  int ifindex = ifr.ifr_ifindex;
  strcpy(ifr.ifr_name, if_name);
  ifr.ifr_flags = 0;
  /* reset flags of NIC interface */
  ioctl(s, SIOCGIFFLAGS, &ifr);

  /* set flags of NIC interface, here promiscuous and broadcast */
  ifr.ifr_flags = ifr.ifr_flags | IFF_PROMISC | IFF_BROADCAST;
  ioctl(s, SIOCSIFFLAGS, &ifr);

  /* bind socket to protocol */
  sll.sll_family = AF_PACKET;
  sll.sll_ifindex = ifindex;
  sll.sll_protocol = htons(protocol);
  bind(s, (struct sockaddr *)&sll, sizeof(sll));

  return s;
}

os_eth_handle_t *os_eth_init(
  const char        *if_name,
  os_eth_callback_t *callback,
  void              *arg)
{
  os_eth_handle_t *handle;

  handle = os_malloc(sizeof(os_eth_handle_t));
  if (handle == NULL)
  {
    return NULL;
  }

  handle->arg = arg;
  handle->callback = callback;

  handle->pf_socket = open_socket(if_name, OS_ETHTYPE_PROFINET);
  handle->lldp_socket = open_socket(if_name, OS_ETHTYPE_LLDP);
  
  if (handle->pf_socket > 0 && handle->lldp_socket > 0)
  {
    handle->mutex       = os_mutex_create();
    handle->pf_thread   = os_thread_create("os_eth_pf_task",   ETH_PRIO,  4096, os_eth_pf_task, handle);
    handle->lldp_thread = os_thread_create("os_eth_lldp_task", LLDP_PRIO, 4096, os_eth_lldp_task, handle);
    return handle;
  }
  else
  {
    os_free(handle);
    return NULL;
  }
}

int os_eth_send(
  os_eth_handle_t *handle,
  os_buf_t *buf)
{
  int ret = 0;
  if(handle != NULL)
  {
    os_mutex_lock(handle->mutex);
    handle->n_bytes_sent += buf->len;
    ret = send(handle->pf_socket, buf->payload, buf->len, 0);
    os_mutex_unlock(handle->mutex);    
  }
  return ret;
}
