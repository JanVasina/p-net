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

 /**
  * @file
  * @brief Map incoming cyclic Profinet data Ethernet frames to frame handlers
  *
  * The frame id map is used to quickly find the function responsible for
  * handling a frame with a specific frame id.
  * Clients may add or remove entries on the fly, but there is no locking of the table.
  * ToDo: This may be a problem in the future.
  * Note that frames may arrive at any time.
  */

#ifdef UNIT_TEST
#define os_eth_init mock_os_eth_init
#endif

#include <string.h>
#include "pf_includes.h"

static pnet_ethaddr_t broadcast_mac = { { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }  };

int pf_eth_init(
  pnet_t                  *net)
{
  int ret = 0;

  memset(net->eth_id_map, 0, sizeof(net->eth_id_map));

  return ret;
}

int pf_eth_recv(
  void                    *arg,
  os_buf_t                *p_buf)
{
  int         ret = 0;       /* Means: "Not handled" */
  uint16_t    type_pos = 2 * sizeof(pnet_ethaddr_t);
  uint16_t    type;
  uint16_t    frame_id;
  uint16_t    *p_data;
  size_t      ix = 0;
  pnet_t      *net = (pnet_t*)arg;

  // check destination mac address
  const pnet_cfg_t *p_cfg = NULL;
  pf_fspm_get_default_cfg(net, &p_cfg);

  pnet_ethaddr_t *p_dst_mac_addr = (pnet_ethaddr_t *)(p_buf->payload);
  if((p_dst_mac_addr->addr[0] & 0x1) == 0x0) // not multicast?
  {
    if (memcmp(p_dst_mac_addr->addr, p_cfg->eth_addr.addr, sizeof(pnet_ethaddr_t)) != 0)
    {
      if (memcmp(p_dst_mac_addr->addr, broadcast_mac.addr, sizeof(pnet_ethaddr_t)) != 0)
      {
        return 0; // not handled
      }
    }
  }

  /* Skip ALL VLAN tags */
  p_data = (uint16_t *)(&((uint8_t *)p_buf->payload)[type_pos]);
  type = ntohs(p_data[0]);
  while (type == OS_ETHTYPE_VLAN)
  {
    type_pos += 4;    /* Sizeof VLAN tag */

    p_data = (uint16_t *)(&((uint8_t *)p_buf->payload)[type_pos]);
    type = ntohs(p_data[0]);
  }
  frame_id = ntohs(p_data[1]);

  switch (type)
  {
  case OS_ETHTYPE_PROFINET:
    /* Find the associated frame handler */
    ix = 0;
    while ((ix < NELEMENTS(net->eth_id_map)) &&
           ((net->eth_id_map[ix].in_use == false) ||
            (net->eth_id_map[ix].frame_id != frame_id)))
    {
      ix++;
    }
    if (ix < NELEMENTS(net->eth_id_map))
    {
      /* Call the frame handler */
      ret = net->eth_id_map[ix].frame_handler(net, frame_id, p_buf,
                                              type_pos + sizeof(uint16_t), net->eth_id_map[ix].p_arg);
    }
#ifdef _DEBUG
    else if(frame_id == PF_DCP_ID_REQ_FRAME_ID)
    {
      LOG_WARNING(PF_DCP_LOG, "PF_ETHP(%d): 0x%X frame not found in table\n", __LINE__, PF_DCP_ID_REQ_FRAME_ID);
      pf_eth_show(net);
    }
#endif // _DEBUG
    break;
  case OS_ETHTYPE_LLDP:
    // LLDP packet is processed, but the buffer can be reused, so return 0 as not handled
    pf_lldp_recv(net, p_buf);
    ret = 0;
    break;
  default:
    /* Not a profinet packet. */
    ret = 0;
    break;
  }

  return ret;
}

void pf_eth_frame_id_map_add(
  pnet_t                  *net,
  uint16_t                frame_id,
  pf_eth_frame_handler_t  frame_handler,
  void                    *p_arg,
  bool                     b_replace_old)
{
  size_t  ix = 0;

  // find entry with the same frame_id and lowest time
  uint64_t min_time = UINT64_MAX;
  size_t   min_time_idx = NELEMENTS(net->eth_id_map);


  while ((ix < NELEMENTS(net->eth_id_map)) &&
         (net->eth_id_map[ix].in_use == true))
  {
    if (net->eth_id_map[ix].frame_id == frame_id)
    {
      if (min_time < net->eth_id_map[ix].time_created)
      {
        min_time = net->eth_id_map[ix].time_created;
        min_time_idx = ix;
      }
    }
    ix++;
  }

  if (   (b_replace_old == true)
      && (ix >= NELEMENTS(net->eth_id_map)) 
      && (min_time_idx < NELEMENTS(net->eth_id_map)))
  {
    ix = min_time_idx;
    LOG_WARNING(PF_ETH_LOG, "ETH(%d): Replace older FrameId 0x%X in idx %u\n",
                __LINE__,
                frame_id,
                ix);
  }

  if (ix < NELEMENTS(net->eth_id_map))
  {
    LOG_INFO(PF_ETH_LOG, "ETH(%d): Add FrameIds 0x%x at index %u\n",
             __LINE__,
             frame_id,
             ix);
    net->eth_id_map[ix].in_use        = true;
    net->eth_id_map[ix].frame_id      = frame_id;
    net->eth_id_map[ix].frame_handler = frame_handler;
    net->eth_id_map[ix].p_arg         = p_arg;
    net->eth_id_map[ix].time_created  = os_get_current_time_us();
  }
  else
  {
    LOG_ERROR(PF_ETH_LOG, "ETH(%d): No more room for FrameId 0x%X\n", 
              __LINE__,
              frame_id);
  }
}

void pf_eth_frame_id_map_remove(
  pnet_t                  *net,
  uint16_t                frame_id)
{
  size_t ix = 0;

  while ((ix < NELEMENTS(net->eth_id_map)) &&
         ((net->eth_id_map[ix].in_use == false) ||
          (net->eth_id_map[ix].frame_id != frame_id)))
  {
    ix++;
  }

  if (ix < NELEMENTS(net->eth_id_map))
  {
    net->eth_id_map[ix].in_use = false;
    LOG_INFO(PF_ETH_LOG, "ETH(%d): Free room for FrameId 0x%x at index %u\n",
             __LINE__,
             frame_id,
             ix);
  }
}

void pf_eth_show(pnet_t *net)
{
  size_t ix;

  printf("pf_eth_show:\n");
  for (ix = 0; ix < NELEMENTS(net->eth_id_map); ix++)
  {
    printf("%u: id 0x%X in use %i\n", 
           ix, 
           net->eth_id_map[ix].frame_id, 
           (int)net->eth_id_map[ix].in_use);
  }
}
