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
  * @brief Implements the Context Management Ip and Name Assignment protocol machine (CMINA)
  *
  * Handles assignment of station name and IP address.
  * Manages sending DCP hello requests.
  *
  * Resets the communication stack and user application.
  */


#ifdef UNIT_TEST

#endif

#include <string.h>
#include <ctype.h>
#include "osal.h"
#include "pf_includes.h"

static const char *hello_sync_name = "hello";


/**
 * @internal
 * Send a DCP HELLO message.
 *
 * This is a callback for the scheduler. Arguments should fulfill pf_scheduler_timeout_ftn_t
 *
 * If this was not the last requested HELLO message then re-schedule another call after 1s.
 *
 * @param net              InOut: The p-net stack instance
 * @param arg              In:   Not used.
 * @param current_time     In:   Not used.
 */
static void pf_cmina_send_hello(
  pnet_t *net,
  void *arg,
  uint32_t                current_time)
{
  if ((net->cmina_state == PF_CMINA_STATE_W_CONNECT) &&
      (net->cmina_hello_count > 0))
  {
    (void)pf_dcp_hello_req(net);

    /* Reschedule */
    net->cmina_hello_count--;
    if (pf_scheduler_add(net, PF_CMINA_FS_HELLO_INTERVAL * 1000,
                         hello_sync_name, pf_cmina_send_hello, NULL, &net->cmina_hello_timeout) != 0)
    {
      net->cmina_hello_timeout = UINT32_MAX;
    }
  }
  else
  {
    net->cmina_hello_timeout = UINT32_MAX;
    net->cmina_hello_count = 0;
  }
}

/**
 * @internal
 * Reset the configuration to default values.
 *
 * Triggers the application callback \a pnet_reset_ind() for some \a reset_mode
 * values.
 *
 * Reset modes:
 *
 * 0:  Power-on reset
 * 1:  Reset application parameters
 * 2:  Reset communication parameters
 * 99: Reset application and communication parameters.
 *
 * @param net              InOut: The p-net stack instance
 * @param reset_mode       In:   Reset mode.
 * @return  0  if the operation succeeded.
 *          -1 if an error occurred.
 */
int pf_cmina_set_default_cfg(
  pnet_t *net,
  uint16_t                reset_mode)
{
  int                     ret = -1;
  const pnet_cfg_t *p_cfg = NULL;
  uint16_t                ix;
  bool                    should_reset_user_application = false;

  LOG_DEBUG(PF_DCP_LOG, "CMINA(%d): Setting default configuration. Reset mode: %u\n", __LINE__, reset_mode);

  pf_fspm_get_default_cfg(net, &p_cfg);
  if (p_cfg != NULL)
  {

    net->cmina_perm_dcp_ase.device_initiative = p_cfg->send_hello?1:0;
    net->cmina_perm_dcp_ase.device_role = 1;            /* Means: PNIO Device */

    memcpy(net->cmina_perm_dcp_ase.mac_address.addr, p_cfg->eth_addr.addr, sizeof(pnet_ethaddr_t));

    strcpy(net->cmina_perm_dcp_ase.port_name, "");      /* Terminated */
    strncpy(net->cmina_perm_dcp_ase.manufacturer_specific_string, p_cfg->manufacturer_specific_string, sizeof(net->cmina_perm_dcp_ase.manufacturer_specific_string));
    net->cmina_perm_dcp_ase.manufacturer_specific_string[sizeof(net->cmina_perm_dcp_ase.manufacturer_specific_string) - 1] = '\0';

    net->cmina_perm_dcp_ase.device_id = p_cfg->device_id;
    net->cmina_perm_dcp_ase.oem_device_id = p_cfg->oem_device_id;

    net->cmina_perm_dcp_ase.dhcp_enable = p_cfg->dhcp_enable;
    // PN-AL-services_2712_V24_May19.pdf
    // page 139 - 141
    if (reset_mode == 0)                /* Power-on reset */
    {
      /* ToDo: Get from permanent pool */
      /* osal_get_perm_pool((uint8_t *)&net->cmina_perm_dcp_ase, sizeof(net->cmina_perm_dcp_ase)); */

      OS_IP4_ADDR_TO_U32(net->cmina_perm_dcp_ase.full_ip_suite.ip_suite.ip_addr,
                         p_cfg->ip_addr.a, p_cfg->ip_addr.b, p_cfg->ip_addr.c, p_cfg->ip_addr.d);
      OS_IP4_ADDR_TO_U32(net->cmina_perm_dcp_ase.full_ip_suite.ip_suite.ip_mask,
                         p_cfg->ip_mask.a, p_cfg->ip_mask.b, p_cfg->ip_mask.c, p_cfg->ip_mask.d);
      OS_IP4_ADDR_TO_U32(net->cmina_perm_dcp_ase.full_ip_suite.ip_suite.ip_gateway,
                         p_cfg->ip_gateway.a, p_cfg->ip_gateway.b, p_cfg->ip_gateway.c, p_cfg->ip_gateway.d);
      memcpy(net->cmina_perm_dcp_ase.name_of_station, p_cfg->station_name, sizeof(net->cmina_perm_dcp_ase.name_of_station));
      net->cmina_perm_dcp_ase.name_of_station[sizeof(net->cmina_perm_dcp_ase.name_of_station) - 1] = '\0';
    }
    if (reset_mode == 1 || reset_mode == 99)  /* Reset application parameters */
    {
      should_reset_user_application = true;

      /* Reset I&M data */
      ret = pf_fspm_clear_im_data(net);
      // save the I&M data to disk
      os_save_im_data(net);
    }
    if (reset_mode == 2 || reset_mode == 99)  /* Reset communication parameters */
    {
      /* Reset IP suite */
      net->cmina_perm_dcp_ase.full_ip_suite.ip_suite.ip_addr = 0;
      net->cmina_perm_dcp_ase.full_ip_suite.ip_suite.ip_mask = 0;
      net->cmina_perm_dcp_ase.full_ip_suite.ip_suite.ip_gateway = 0;
      /* Clear name of station */
      memset(net->cmina_perm_dcp_ase.name_of_station, 0, sizeof(net->cmina_perm_dcp_ase.name_of_station));
      // clear PDCheckPortData -> check peers
      memset(&(net->check_peers_data), 0, sizeof(net->check_peers_data));
      os_save_im_data(net);
    }
    if (reset_mode > 0)
    {
      /* User callback */
      (void)pf_fspm_reset_ind(net, should_reset_user_application, reset_mode);
    }

    net->cmina_perm_dcp_ase.standard_gw_value = 0;         /* Means: OwnIP is treated as no gateway */

    net->cmina_perm_dcp_ase.instance_id.high = 0;
    net->cmina_perm_dcp_ase.instance_id.low = 0;

    strncpy(net->cmina_perm_dcp_ase.device_vendor, p_cfg->device_vendor, sizeof(net->cmina_perm_dcp_ase.device_vendor));
    net->cmina_perm_dcp_ase.device_vendor[sizeof(net->cmina_perm_dcp_ase.device_vendor) - 1] = '\0';
    /* Remove trailing spaces */
    ix = (uint16_t)strlen(net->cmina_perm_dcp_ase.device_vendor);
    while ((ix > 1) && (net->cmina_perm_dcp_ase.device_vendor[ix] == ' '))
    {
      ix--;
    }
    net->cmina_perm_dcp_ase.device_vendor[ix] = '\0';

    /* Init the temp values */
    net->cmina_temp_dcp_ase = net->cmina_perm_dcp_ase;


    ret = 0;
  }

  return ret;
}

void pf_cmina_dcp_set_commit(
  pnet_t *net)
{
  if (net->cmina_commit_ip_suite == true)
  {
    net->cmina_commit_ip_suite = false;
// IP suite and station name is done directly in pf_cmina_dcp_set_ind()
//     os_set_ip_suite_and_name(net->fspm_cfg.cb_arg,
//                              net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_addr,
//                              net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_mask,
//                              net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_gateway,
//                              net->cmina_temp_dcp_ase.name_of_station);
  }
}

int pf_cmina_init(
  pnet_t *net)
{
  int                     ret = 0;

  net->cmina_hello_count = 0;
  net->cmina_hello_timeout = UINT32_MAX;
  net->cmina_error_decode = 0;
  net->cmina_error_code_1 = 0;
  net->cmina_commit_ip_suite = false;
  net->cmina_state = PF_CMINA_STATE_SETUP;

  /* Collect the DCP ASE database */
  memset(&net->cmina_perm_dcp_ase, 0, sizeof(net->cmina_perm_dcp_ase));
  (void)pf_cmina_set_default_cfg(net, 0);

  if (strlen(net->cmina_temp_dcp_ase.name_of_station) == 0)
  {
    if (net->cmina_temp_dcp_ase.dhcp_enable == true)
    {
      /* 1 */
      /* ToDo: Send DHCP discover request */
    }
    else if ((net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_addr == 0) &&
             (net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_mask == 0) &&
             (net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_gateway == 0))
    {
      /* 2 */
      /* Ignore - Expect IP to be set by DCP later */
    }
    else
    {
      /* 3 */
      /* Start IP */
      net->cmina_commit_ip_suite = true;
    }

    net->cmina_state = PF_CMINA_STATE_SET_NAME;
  }
  else
  {
    if ((net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_addr == 0) &&
        (net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_mask == 0) &&
        (net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_gateway == 0))
    {
      if (net->cmina_temp_dcp_ase.dhcp_enable == true)
      {
        /* 4 */
        /* ToDo: Send DHCP discover request */
      }
      else
      {
        /* 5 */
        /* Ignore - Expect station name and IP to be set by DCP later */
      }

      net->cmina_state = PF_CMINA_STATE_SET_IP;
    }
    else
    {
      /* 6, 7 */
      /* Start IP */
      net->cmina_commit_ip_suite = true;

      if (net->cmina_perm_dcp_ase.device_initiative == 1)
      {
        /* 6 */
        net->cmina_hello_count = PF_CMINA_FS_HELLO_RETRY;
        /* Send first HELLO now! */
        ret = pf_scheduler_add(net, 0, hello_sync_name, pf_cmina_send_hello, NULL, &net->cmina_hello_timeout);
        if (ret != 0)
        {
          net->cmina_hello_timeout = UINT32_MAX;
        }
      }

      net->cmina_state = PF_CMINA_STATE_W_CONNECT;
    }
  }

  /* Change IP address if necessary */
  pf_cmina_dcp_set_commit(net);

  return ret;
}

//**
// * @internal
// * Check the validity of IP address and subnet mask
// * In case of invalid values log to Error.
// * @param p_ip_suite       In: pointer to IP suite structure to be checked
// * @return  false  if the values are incorrect
// *          true if IP address and subnet mask are valid
// */
static bool pf_cmina_ip_suite_is_valid(
  const pf_ip_suite_t *p_ip_suite)
{
  bool ret                     = true;
  const uint32_t ip_addr       = p_ip_suite->ip_addr;
  const uint32_t ip_mask       = p_ip_suite->ip_mask;
  const uint32_t ip_gateway    = p_ip_suite->ip_gateway;
  const uint32_t network_class = (ip_addr >> 24);
  const uint32_t masked_addr   = ip_addr & ~(ip_mask);

  if (network_class < 128) // network class A
  {
    if (masked_addr >= (256 * 256 * 256))
    {
      ret = false;
    }
  }
  else if (network_class < 192) // network class B
  {
    if (masked_addr >= (256 * 256))
    {
      ret = false;
    }
  }
  else if (network_class < 224) // network class C
  {
    if (masked_addr >= (256))
    {
      ret = false;
    }
  }
  else // network class D or E
  {
    ret = false;
  }

  // check the gateway - must be in the same subnet
  if (ip_gateway != 0)
  {
    const uint32_t inv_masked_addr = ip_addr & ip_mask;
    const uint32_t inv_masked_gw   = ip_gateway & ip_mask;
    if (inv_masked_addr != inv_masked_gw)
    {
      ret = false;
    }
  }
  
  if (ret == false) // display error
  {
    LOG_WARNING(PF_DCP_LOG, "CMINA(%d): invalid IP suite:\n\taddr %d.%d.%d.%d mask %d.%d.%d.%d gw %d.%d.%d.%d\n",
                __LINE__,
                (ip_addr >> 24) & 0xFF,
                (ip_addr >> 16) & 0xFF,
                (ip_addr >> 8) & 0xFF,
                (ip_addr) & 0xFF,
                (ip_mask >> 24) & 0xFF,
                (ip_mask >> 16) & 0xFF,
                (ip_mask >> 8) & 0xFF,
                (ip_mask) & 0xFF,
                (ip_gateway >> 24) & 0xFF,
                (ip_gateway >> 16) & 0xFF,
                (ip_gateway >> 8) & 0xFF,
                (ip_gateway) & 0xFF);

  }
  return ret;
}

//**
// * @internal
// * Check the validity of station name.
// * Only several checks are done now.
// * Valid name is formed by:
// 
// - 1 or more labels, separated by [.]
// - Total length is 1 to 240
// - Label length is 1 to 63
// - Labels consist of [a-z0-9-]
// - Labels do not start with [-]
// - Labels do not end with [-]
// - Labels do not use multiple concatenated [-] except for IETF RFC 5890
// - The first label does not have the form "port-xyz" or "port-xyz-abcde" with a, b, c, d, e,
//   x, y, z = 0...9, to avoid wrong similarity with the field AliasNameValue
// - Station-names do not have the form a.b.c.d with a, b, c, d = 0...999
// 
// * @param net         In: The p-net stack instance
// * @param p_name      In: Station name to be checked
// * @param name_length In: name length
// * @return  false  if the name is bad formed
// *          true if the name is valid
// */
static bool pf_cmina_name_of_station_is_valid(
  const pnet_t *net, 
  const char *p_name, 
  uint16_t name_length)
{
  if (name_length >= sizeof(net->cmina_temp_dcp_ase.name_of_station))
  {
    return false;
  }
  if (p_name[0] == '-')
  {
    return false;
  }

  // check the port-xyz label
  if(name_length >= 6)
  {
    if (strncmp(p_name, "port-", 5) == 0)
    {
      if (isdigit(p_name[5]))
      {
        return false;
      }
    }
  }

  // go through labels
  uint32_t n_numeric_labels = 0;
  uint32_t n_consecutive_numeric_labels = 0;
  uint16_t src_pos = 0;
  char last_c = 0;
  while (src_pos < name_length)
  {
    bool b_is_digit_only = true;
    char prev_c = 0;
    while (src_pos < name_length)
    {
      const char c = p_name[src_pos];
      last_c = c;
      src_pos++;
      if (isdigit(c))
      {
        ; // digit is valid character, do nothing
      }
      else if (islower(c) || c == '-')
      {
        b_is_digit_only = false;
      }
      else if (c == '.') // end of label
      {
        if (prev_c == 0) // only dot in label?
        {
          b_is_digit_only = false;
        }
        break;
      }
      else // invalid character
      {
        return false;
      }
      prev_c = c;
    }
    if (b_is_digit_only)
    {
      n_consecutive_numeric_labels++;
      n_numeric_labels++;
    }
    else
    {
      n_consecutive_numeric_labels = 0;
    }
  }

  // check the last character here to better utilize the cache
  if (last_c == '-')
  {
    return false;
  }

  // check the invalid label a.b.c.d where abcd are digits only
  if (n_consecutive_numeric_labels == 4 &&
      n_numeric_labels == 4 &&
      isdigit(last_c)) // the last character must be a digit
  {
    return false;
  }

  return true;
}

//////////////////////////////////////////////////////////////////////////
// go through active AR's and stop them
//
// 

static void pf_cmina_check_and_stop_ar(
  pnet_t *net,
  size_t err2_code)  
{
  uint16_t ix;
  for (ix = 0U; ix < PNET_MAX_AR; ix++)
  {
    pf_ar_t *p_ar = pf_ar_find_by_index(net, ix);
    if (p_ar != NULL)
    {
      if(p_ar->in_use == true)
      {
        p_ar->err_code = err2_code;
        (void)pf_cmdev_state_ind(net, p_ar, PNET_EVENT_ABORT);
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////

int pf_cmina_dcp_set_ind(
  pnet_t                 *net,
  uint8_t                 opt,
  uint8_t                 sub,
  uint16_t                block_qualifier,
  uint16_t                value_length,
  uint8_t                *p_value,
  uint8_t                *p_block_error)
{
  int                     ret = -1;
  bool                    have_ip = false;
  bool                    have_name = false;
  bool                    have_dhcp = false;
  bool                    change_ip = false;
  bool                    change_name = false;
  bool                    change_dhcp = false;
  bool                    reset_to_factory = false;
  pf_ar_t                *p_ar = NULL;
  uint16_t                ix;
  bool                    found = false;
  bool                    temp = ((block_qualifier & 1) == 0);
  uint16_t                reset_mode = block_qualifier >> 1;

  /* stop sending Hello packets */
  if (net->cmina_hello_timeout != UINT32_MAX)
  {
    pf_scheduler_remove(net, hello_sync_name, net->cmina_hello_timeout);
    net->cmina_hello_timeout = UINT32_MAX;
  }
  switch (opt)
  {
  case PF_DCP_OPT_IP:
    switch (sub)
    {
    case PF_DCP_SUB_IP_PAR:
      if (value_length == sizeof(net->cmina_temp_dcp_ase.full_ip_suite.ip_suite))
      {
	    // check the validity if received data to pass Automated RT tests
        pf_ip_suite_t *p_src_ip_suite = (pf_ip_suite_t *)p_value;
        if (pf_cmina_ip_suite_is_valid(p_src_ip_suite) == true)
        {
          change_ip = (memcmp(&net->cmina_temp_dcp_ase.full_ip_suite.ip_suite, p_value, value_length) != 0);
          memcpy(&net->cmina_temp_dcp_ase.full_ip_suite.ip_suite, p_value, sizeof(net->cmina_temp_dcp_ase.full_ip_suite.ip_suite));
          LOG_DEBUG(PF_DCP_LOG, "CMINA(%d): %s change IP: addr 0x%X mask 0x%X\n",
                    __LINE__,
                    temp == false ? "PERMANENT" : "Temporal",
                    p_src_ip_suite->ip_addr,
                    p_src_ip_suite->ip_mask);
          if (temp == false)
          {
            net->cmina_perm_dcp_ase.full_ip_suite.ip_suite = net->cmina_temp_dcp_ase.full_ip_suite.ip_suite;
          }
		  // directly change the IP suite now, we must choose between temporary and permanent change
		  // Note that temporary means use the address, but save 0.0.0.0 to the flash
		  // for the next device start
          pf_ip_suite_t *p_tmp_ip_suite = &net->cmina_temp_dcp_ase.full_ip_suite.ip_suite;
          os_set_ip_suite(net->fspm_cfg.cb_arg,
                          p_tmp_ip_suite->ip_addr,
                          p_tmp_ip_suite->ip_mask,
                          p_tmp_ip_suite->ip_gateway,
                          temp);
          ret = 0;
        }
        else
        {
          *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SET;
        }
      }
      else
      {
        *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SET;
      }
      break;
    case PF_DCP_SUB_IP_SUITE:
      if (value_length < sizeof(net->cmina_temp_dcp_ase.full_ip_suite))
      {
	    // the same handling as above
        pf_ip_suite_t *p_src_ip_suite = (pf_ip_suite_t *)p_value;
        if (pf_cmina_ip_suite_is_valid(p_src_ip_suite) == true)
        {
          change_ip = (memcmp(&net->cmina_temp_dcp_ase.full_ip_suite, p_value, value_length) != 0);
          memcpy(&net->cmina_temp_dcp_ase.full_ip_suite, p_value, value_length);
          LOG_DEBUG(PF_DCP_LOG, "CMINA(%d): %s change IP: addr 0x%X mask 0x%X\n",
                    __LINE__,
                    temp == false ? "PERMANENT" : "Temporal",
                    p_src_ip_suite->ip_addr,
                    p_src_ip_suite->ip_mask);
          if (temp == false)
          {
            net->cmina_perm_dcp_ase.full_ip_suite = net->cmina_temp_dcp_ase.full_ip_suite;
          }
          pf_ip_suite_t *p_tmp_ip_suite = &net->cmina_temp_dcp_ase.full_ip_suite.ip_suite;
          os_set_ip_suite(net->fspm_cfg.cb_arg,
                          p_tmp_ip_suite->ip_addr,
                          p_tmp_ip_suite->ip_mask,
                          p_tmp_ip_suite->ip_gateway,
                          temp);
          ret = 0;
        }
        else
        {
          *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SET;
        }
      }
      else
      {
        *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SET;
      }
      break;
    default:
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      break;
    }
    break;
  case PF_DCP_OPT_DEVICE_PROPERTIES:
    switch (sub)
    {
    case PF_DCP_SUB_DEV_PROP_NAME:
      if (pf_cmina_name_of_station_is_valid(net, (char *)p_value, value_length) == true)
      {
        change_name = ((strncmp(net->cmina_temp_dcp_ase.name_of_station, (char *)p_value, value_length) != 0) &&
                        (strlen(net->cmina_temp_dcp_ase.name_of_station) != value_length));

        strncpy(net->cmina_temp_dcp_ase.name_of_station, (char *)p_value, value_length);
        net->cmina_temp_dcp_ase.name_of_station[value_length] = '\0';
        LOG_DEBUG(PF_DCP_LOG, "CMINA(%d): %s change (%i) station name request. New name: %s\n",
                  __LINE__,
                  temp == false ? "PERMANENT" : "Temporal",
                  (int)change_name,
                  net->cmina_temp_dcp_ase.name_of_station);
        if (temp == false)
        {
          strcpy(net->cmina_perm_dcp_ase.name_of_station, net->cmina_temp_dcp_ase.name_of_station);   /* It always fits */
        }
		    // temporary station name means store empty string to the flash
        os_set_station_name(net->fspm_cfg.cb_arg,
                            net->cmina_temp_dcp_ase.name_of_station,
                            temp);
        // if the station name is empty string and we have active AR, end it and send an alarm
        if (value_length == 0 || net->cmina_temp_dcp_ase.name_of_station[0] == '\0')
        {
          pf_cmina_check_and_stop_ar(net, PNET_ERROR_CODE_2_ABORT_DCP_STATION_NAME_CHANGED);
        }

        ret = 0;
      }
      else
      {
        // for the error display: must copy the name and properly zero terminate */
        char name[sizeof(net->cmina_temp_dcp_ase.name_of_station)];
        const size_t size_to_copy = MIN(value_length, sizeof(net->cmina_temp_dcp_ase.name_of_station) - 1);
        strncpy(name, (char *)p_value, size_to_copy);
        name[size_to_copy] = '\0';
        LOG_WARNING(PF_DCP_LOG, "CMINA(%d): Invalid station name: %s rejected.\n", __LINE__, name);

        *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SET;
      }
      break;
    default:
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      break;
    }
    break;
  case PF_DCP_OPT_DHCP:
    LOG_INFO(PF_DCP_LOG, "CMINA(%d): Trying to set DHCP\n", __LINE__);
    *p_block_error = PF_DCP_BLOCK_ERROR_OPTION_NOT_SUPPORTED;
    change_dhcp = true;
    break;
  case PF_DCP_OPT_CONTROL:
    if (sub == PF_DCP_SUB_CONTROL_FACTORY_RESET)
    {
      reset_to_factory = true;
      reset_mode = 99;        /* Reset all */
      pf_cmina_check_and_stop_ar(net, PNET_ERROR_CODE_2_ABORT_DCP_RESET_TO_FACTORY);
      ret = 0;
    }
    else if (sub == PF_DCP_SUB_CONTROL_RESET_TO_FACTORY)
    {
      if(reset_mode <= 2)
      {
        reset_to_factory = true;
        pf_cmina_check_and_stop_ar(net, PNET_ERROR_CODE_2_ABORT_DCP_RESET_TO_FACTORY);        

        ret = 0;
      }
      else
      {
        *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      }
    }
    else
    {
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
    }
    break;
  default:
    *p_block_error = PF_DCP_BLOCK_ERROR_OPTION_NOT_SUPPORTED;
    break;
  }

  if (ret == 0)
  {
    /* Evaluate what we have and where to go */
    have_name = (strlen(net->cmina_temp_dcp_ase.name_of_station) > 0);
    have_ip = ((net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_addr != 0) ||
               (net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_mask != 0) ||
               (net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_gateway != 0));
    have_dhcp = false;   /* ToDo: Not supported here */

    switch (net->cmina_state)
    {
    case PF_CMINA_STATE_SETUP:
      /* Should not occur */
      net->cmina_error_decode = PNET_ERROR_DECODE_PNIORW;
      net->cmina_error_code_1 = PNET_ERROR_CODE_1_ACC_STATE_CONFLICT;
      ret = -1;
      break;
    case PF_CMINA_STATE_SET_NAME:
    case PF_CMINA_STATE_SET_IP:
      if (reset_to_factory == true)
      {
        /* Handle reset to factory here */
        ret = pf_cmina_set_default_cfg(net, reset_mode);
      }

      if ((change_name == true) || (change_ip == true))
      {
        if ((have_name == true) && (have_ip == true))
        {
          /* Stop DHCP timer */
          net->cmina_commit_ip_suite = true;

          net->cmina_state = PF_CMINA_STATE_W_CONNECT;

          ret = 0;
        }
        else
        {
          /* Stop IP */
          if (have_name == false)
          {
            net->cmina_state = PF_CMINA_STATE_SET_NAME;
          }
          else if (have_ip == false)
          {
            net->cmina_state = PF_CMINA_STATE_SET_IP;
          }
        }
      }
      else if (change_dhcp == true)
      {
        if (have_dhcp == true)
        {
          memset(&net->cmina_temp_dcp_ase.full_ip_suite.ip_suite, 0, sizeof(net->cmina_temp_dcp_ase.full_ip_suite.ip_suite));
          if (temp == false)
          {
            net->cmina_perm_dcp_ase.full_ip_suite.ip_suite = net->cmina_temp_dcp_ase.full_ip_suite.ip_suite;
          }
          /* dhcp_discover_req */
          net->cmina_state = PF_CMINA_STATE_SET_IP;
        }
        else
        {
          /* Stop DHCP */
        }
      }
      break;
    case PF_CMINA_STATE_W_CONNECT:
      if (reset_to_factory == true)
      {
        /* Handle reset to factory here */
        ret = pf_cmina_set_default_cfg(net, reset_mode);
      }

      if ((change_name == false) && (change_ip == false))
      {
        /* all OK */
      }
      else if ((have_name == false) || (reset_to_factory == true))
      {
        /* Any connection active ? */
        found = false;
        for (ix = 0; ix < PNET_MAX_AR; ix++)
        {
          p_ar = pf_ar_find_by_index(net, ix);
          if ((p_ar != NULL) && (p_ar->in_use == true))
          {
            found = true;
            /* 38 */
            pf_cmdev_state_ind(net, p_ar, PF_CMDEV_STATE_ABORT);

            LOG_ERROR(PF_DCP_LOG, "CMINA(%d): Stopping AR\n", __LINE__);
          }
        }

        if (found == false)
        {
          /* 37 */
          /* Stop IP */
        }
      }
      else if (((have_name == true) && (change_name == true)) ||
               (have_ip == false))
      {
        /* Any connection active ? */
        found = false;
        for (ix = 0; ix < PNET_MAX_AR; ix++)
        {
          p_ar = pf_ar_find_by_index(net, ix);
          if ((p_ar != NULL) && (p_ar->in_use == true))
          {
            found = true;
            /* 40 */
            pf_cmdev_state_ind(net, p_ar, PF_CMDEV_STATE_ABORT);

            LOG_ERROR(PF_DCP_LOG, "CMINA(%d): Stopping AR\n", __LINE__);
          }
        }

        if (found == false)
        {
          /* 39 */
          net->cmina_commit_ip_suite = true;
        }

        net->cmina_state = PF_CMINA_STATE_SET_IP;

        ret = 0;
      }
      else if ((have_ip == true) && (change_ip == true))
      {
        /* Any connection active ?? */
        found = false;
        for (ix = 0; ix < PNET_MAX_AR; ix++)
        {
          p_ar = pf_ar_find_by_index(net, ix);
          if ((p_ar != NULL) && (p_ar->in_use == true))
          {
            found = true;
          }
        }

        if (found == false)
        {
          /* 41 */
          net->cmina_commit_ip_suite = true;

          ret = 0;
        }
        else
        {
          /* 42 */
          /* Not allowed to change IP if AR active */
          net->cmina_error_decode = PNET_ERROR_DECODE_PNIORW;
          net->cmina_error_code_1 = PNET_ERROR_CODE_1_ACC_INVALID_PARAMETER;
          ret = -1;
        }
      }
      else
      {
        /* 43 */
        net->cmina_error_decode = PNET_ERROR_DECODE_PNIORW;
        net->cmina_error_code_1 = PNET_ERROR_CODE_1_ACC_INVALID_PARAMETER;
        ret = -1;
      }
      break;
    }
  }

  return ret;
}

int pf_cmina_dcp_get_req(
  pnet_t *net,
  uint8_t                 opt,
  uint8_t                 sub,
  uint16_t *p_value_length,
  uint8_t **pp_value,
  uint8_t *p_block_error)
{
  int                     ret = 0;    /* Assume all OK */

  switch (opt)
  {
  case PF_DCP_OPT_ALL:
    break;
  case PF_DCP_OPT_IP:
    switch (sub)
    {
    case PF_DCP_SUB_IP_MAC:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.mac_address);
      *pp_value = (uint8_t *)&net->cmina_temp_dcp_ase.mac_address;
      break;
    case PF_DCP_SUB_IP_PAR:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.full_ip_suite.ip_suite);
      *pp_value = (uint8_t *)&net->cmina_temp_dcp_ase.full_ip_suite.ip_suite;
      break;
    case PF_DCP_SUB_IP_SUITE:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.full_ip_suite);
      *pp_value = (uint8_t *)&net->cmina_temp_dcp_ase.full_ip_suite;
      break;
    default:
      LOG_ERROR(PF_DCP_LOG, "CMINA(%d): pf_cmina_dcp_get_req: PF_DCP_OPT_IP unknown suboption %u\n",
                __LINE__,
                sub);
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      ret = -1;
      break;
    }
    break;
  case PF_DCP_OPT_DEVICE_PROPERTIES:
    switch (sub)
    {
    case PF_DCP_SUB_DEV_PROP_VENDOR:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.device_vendor) - 1;   /* Skip terminator */
      *pp_value = (uint8_t *)&net->cmina_temp_dcp_ase.device_vendor;
      break;
    case PF_DCP_SUB_DEV_PROP_NAME:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.name_of_station);
      *pp_value = (uint8_t *)net->cmina_temp_dcp_ase.name_of_station;
      break;
    case PF_DCP_SUB_DEV_PROP_ID:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.device_id);
      *pp_value = (uint8_t *)&net->cmina_temp_dcp_ase.device_id;
      break;
    case PF_DCP_SUB_DEV_PROP_ROLE:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.device_role);
      *pp_value = (uint8_t *)&net->cmina_temp_dcp_ase.device_role;
      break;
    case PF_DCP_SUB_DEV_PROP_OPTIONS:
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      ret = -1;
      break;
    case PF_DCP_SUB_DEV_PROP_ALIAS:
#if 0
      * p_value_length = sizeof(net->cmina_temp_dcp_ase.);
      *pp_value = &net->cmina_temp_dcp_ase.;
#else
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      ret = -1;
#endif
      break;
    case PF_DCP_SUB_DEV_PROP_INSTANCE:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.instance_id);
      *pp_value = (uint8_t *)&net->cmina_temp_dcp_ase.instance_id;
      break;
    case PF_DCP_SUB_DEV_PROP_OEM_ID:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.oem_device_id);
      *pp_value = (uint8_t *)&net->cmina_temp_dcp_ase.oem_device_id;
      break;
    case PF_DCP_SUB_DEV_PROP_GATEWAY:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.standard_gw_value);
      *pp_value = (uint8_t *)&net->cmina_temp_dcp_ase.standard_gw_value;
      break;
    default:
      LOG_ERROR(PF_DCP_LOG, "CMINA(%d): pf_cmina_dcp_get_req: PF_DCP_OPT_DEVICE_PROPERTIES unknown suboption %u\n",
                __LINE__,
                sub);
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      ret = -1;
      break;
    }
    break;
  case PF_DCP_OPT_DHCP:
    /* ToDo: No support for DHCP yet, because it needs a way to get/set in lpiw. */
    switch (sub)
    {
    case PF_DCP_SUB_DHCP_HOSTNAME:
    case PF_DCP_SUB_DHCP_VENDOR_SPEC:
    case PF_DCP_SUB_DHCP_SERVER_ID:
    case PF_DCP_SUB_DHCP_PAR_REQ_LIST:
    case PF_DCP_SUB_DHCP_CLASS_ID:
    case PF_DCP_SUB_DHCP_CLIENT_ID:
      /* 61, 1+strlen(), 0, string */
    case PF_DCP_SUB_DHCP_FQDN:
    case PF_DCP_SUB_DHCP_UUID_CLIENT_ID:
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      ret = -1;
      break;
    case PF_DCP_SUB_DHCP_CONTROL:
      /* Cant get this */
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      ret = -1;
      break;
    default:
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      ret = -1;
      break;
    }
    break;
  case PF_DCP_OPT_CONTROL:
    /* Cant get control */
    *p_block_error = PF_DCP_BLOCK_ERROR_OPTION_NOT_SUPPORTED;
    ret = -1;
    break;
  case PF_DCP_OPT_DEVICE_INITIATIVE:
    switch (sub)
    {
    case PF_DCP_SUB_DEV_INITIATIVE_SUPPORT:
      *p_value_length = sizeof(net->cmina_temp_dcp_ase.device_initiative);
      *pp_value = (uint8_t *)&net->cmina_temp_dcp_ase.device_initiative;
      break;
    default:
      *p_block_error = PF_DCP_BLOCK_ERROR_SUBOPTION_NOT_SUPPORTED;
      ret = -1;
      break;
    }
    break;
  default:
    LOG_WARNING(PF_DCP_LOG, "CMINA(%d): pf_cmina_dcp_get_req unknown option %u (sub %u)\n",
                __LINE__,
                opt,
                sub);
    *p_block_error = PF_DCP_BLOCK_ERROR_OPTION_NOT_SUPPORTED;
    ret = -1;
    break;
  }

  if (ret == 0)
  {
    *p_block_error = PF_DCP_BLOCK_ERROR_NO_ERROR;
  }

  return ret;
}

int pf_cmina_get_station_name(
  pnet_t *net,
  const char **pp_station_name)
{
  *pp_station_name = net->cmina_temp_dcp_ase.name_of_station;
  return 0;
}

int pf_cmina_get_ipaddr(
  pnet_t *net,
  os_ipaddr_t *p_ipaddr)
{
  *p_ipaddr = net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_addr;
  return 0;
}

/**
 * @internal
 * Return a string representation of the CMINA state.
 * @param net              InOut: The p-net stack instance
 * @return  a string representation of the CMINA state.
 */
static const char *pf_cmina_state_to_string(
  pnet_t *net)
{
  const char *s = "<unknown>";
  switch (net->cmina_state)
  {
  case PF_CMINA_STATE_SETUP: s = "PF_CMINA_STATE_SETUP"; break;
  case PF_CMINA_STATE_SET_NAME: s = "PF_CMINA_STATE_SET_NAME"; break;
  case PF_CMINA_STATE_SET_IP: s = "PF_CMINA_STATE_SET_IP"; break;
  case PF_CMINA_STATE_W_CONNECT: s = "PF_CMINA_STATE_W_CONNECT"; break;
  }

  return s;
}

void pf_ip_address_show(uint32_t ip){
   printf("%u.%u.%u.%u",
      (unsigned)((ip >> 24) & 0xFF),
      (unsigned)((ip >> 16) & 0xFF),
      (unsigned)((ip >> 8) & 0xFF),
      (unsigned)(ip & 0xFF));
}
void pf_cmina_show(
  pnet_t *net)
{
  const pnet_cfg_t *p_cfg = NULL;

  pf_fspm_get_default_cfg(net, &p_cfg);

  printf("CMINA state : %s\n\n", pf_cmina_state_to_string(net));

  printf("Default name_of_station        : <%s>\n", p_cfg->station_name);
  printf("Perm name_of_station           : <%s>\n", net->cmina_perm_dcp_ase.name_of_station);
  printf("Temp name_of_station           : <%s>\n", net->cmina_temp_dcp_ase.name_of_station);
  printf("\n");
  printf("Default device_vendor          : <%s>\n", p_cfg->device_vendor);
  printf("Perm device_vendor             : <%s>\n", net->cmina_perm_dcp_ase.device_vendor);
  printf("Temp device_vendor             : <%s>\n", net->cmina_temp_dcp_ase.device_vendor);
  printf("\n");
  printf("Default IP  Netmask  Gateway   : %u.%u.%u.%u  ",
      (unsigned)p_cfg->ip_addr.a, (unsigned)p_cfg->ip_addr.b, (unsigned)p_cfg->ip_addr.c, (unsigned)p_cfg->ip_addr.d);
  printf("%u.%u.%u.%u  ",
      (unsigned)p_cfg->ip_mask.a, (unsigned)p_cfg->ip_mask.b, (unsigned)p_cfg->ip_mask.c, (unsigned)p_cfg->ip_mask.d);
  printf("%u.%u.%u.%u\n",
         (unsigned)p_cfg->ip_gateway.a, (unsigned)p_cfg->ip_gateway.b, (unsigned)p_cfg->ip_gateway.c, (unsigned)p_cfg->ip_gateway.d);
  printf("Perm    IP  Netmask  Gateway   : ");
  pf_ip_address_show(net->cmina_perm_dcp_ase.full_ip_suite.ip_suite.ip_addr);
  printf("  ");
  pf_ip_address_show(net->cmina_perm_dcp_ase.full_ip_suite.ip_suite.ip_mask);
  printf("  ");
  pf_ip_address_show(net->cmina_perm_dcp_ase.full_ip_suite.ip_suite.ip_gateway);
  printf("\nTemp    IP  Netmask  Gateway   : ");
  pf_ip_address_show(net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_addr);
  printf("  ");
  pf_ip_address_show(net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_mask);
  printf("  ");
  pf_ip_address_show(net->cmina_temp_dcp_ase.full_ip_suite.ip_suite.ip_gateway);
  printf("\n");
  printf("MAC                            : %02x:%02x:%02x:%02x:%02x:%02x\n",
         p_cfg->eth_addr.addr[0],
         p_cfg->eth_addr.addr[1],
         p_cfg->eth_addr.addr[2],
         p_cfg->eth_addr.addr[3],
         p_cfg->eth_addr.addr[4],
         p_cfg->eth_addr.addr[5]);
}
