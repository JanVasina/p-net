/****************************************************************************
**
** Copyright (C) 2020 TGDrives, s.r.o.
** https://www.tgdrives.cz
**
** This file is part of the TGMmini Profinet I/O device.
**
**
**  TGMmini Profinet I/O device free software: 
**  you can redistribute it and/or modify it under the terms of the 
**  GNU General Public License as published by the Free Software Foundation, 
**  either version 3 of the License, or (at your option) any later version.
**
**  TGMmini Profinet I/O device is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of 
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License along with 
**  TGMmini Profinet I/O device. If not, see <https://www.gnu.org/licenses/>.
**
****************************************************************************/
// main_linux.c
// main program file
// 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>

#include "pnet_api.h"
#include "osal.h"
#include "log.h"
#include "events_linux.h"
#include "cmd_args.h"
#include "config.h"
#include "utils.h"
#include "plc_memory.h"

#ifndef __STD
#define __STD(a)  a
#endif

/******************************************************************************/
// global variables
// 
int log_to_file;

/********************* Call-back function declarations ************************/

static int app_exp_module_ind(pnet_t *net, void *arg, uint32_t api, uint16_t slot, uint32_t module_ident_number);
static int app_exp_submodule_ind(pnet_t *net, void *arg, uint32_t api, uint16_t slot, uint16_t subslot, uint32_t module_ident_number, uint32_t submodule_ident_number);
static int app_new_data_status_ind(pnet_t *net, void *arg, uint32_t arep, uint32_t crep, uint8_t changes, uint8_t data_status);
static int app_connect_ind(pnet_t *net, void *arg, uint32_t arep, pnet_result_t *p_result);
static int app_state_ind(pnet_t *net, void *arg, uint32_t arep, pnet_event_values_t state);
static int app_release_ind(pnet_t *net, void *arg, uint32_t arep, pnet_result_t *p_result);
static int app_dcontrol_ind(pnet_t *net, void *arg, uint32_t arep, pnet_control_command_t control_command, pnet_result_t *p_result);
static int app_ccontrol_cnf(pnet_t *net, void *arg, uint32_t arep, pnet_result_t *p_result);
static int app_write_ind(pnet_t *net, void *arg, uint32_t arep, uint32_t api, uint16_t slot, uint16_t subslot, uint16_t idx, uint16_t sequence_number, uint16_t write_length, uint8_t *p_write_data, pnet_result_t *p_result);
static int app_read_ind(pnet_t *net, void *arg, uint32_t arep, uint32_t api, uint16_t slot, uint16_t subslot, uint16_t idx, uint16_t sequence_number, uint8_t **pp_read_data, uint16_t *p_read_length, pnet_result_t *p_result);
static int app_alarm_cnf(pnet_t *net, void *arg, uint32_t arep, pnet_pnio_status_t *p_pnio_status);
static int app_alarm_ind(pnet_t *net, void *arg, uint32_t arep, uint32_t api, uint16_t slot, uint16_t subslot, uint16_t data_len, uint16_t data_usi, uint8_t *p_data);
static int app_alarm_ack_cnf(pnet_t *net, void *arg, uint32_t arep, int res);
static int app_reset_ind(pnet_t *net, void *arg, bool should_reset_application, uint16_t reset_mode);
static void app_plug_dap(pnet_t *net, void *arg);

/******* list of supported modules and submodules                      ********/

// for internal use
const module_t cfg_available_module_types[NUMBER_OF_DEFINED_MODULES] =
{
  {
    .module_id           = PNET_MOD_DAP_IDENT,
    .insize              = 0,
    .outsize             = 0,
    .data_dir            = PNET_DIR_NO_IO,
    .in_offset_param_id  = 0,
    .in_def_data_offset  = 0,
    .out_offset_param_id = 0,
    .out_def_data_offset = 0,
  },

  {
    .module_id           = PNET_MOD_32_I_IDENT,
    .insize              = PNET_MOD_32_I_DATASIZE_INPUT,
    .outsize             = 0,
    .data_dir            = PNET_DIR_INPUT,
    .in_offset_param_id  = I32_PLC_OFFSET_PARAM_IDX_101,
    .in_def_data_offset  = I32_PLC_OFFSET_DEFAULT_VALUE,
    .out_offset_param_id = 0,
    .out_def_data_offset = 0,
  },

  {
    .module_id           = PNET_MOD_64_I_IDENT,
    .insize              = PNET_MOD_64_I_DATASIZE_INPUT,
    .outsize             = 0,
    .data_dir            = PNET_DIR_INPUT,
    .in_offset_param_id  = I64_PLC_OFFSET_PARAM_IDX_102,
    .in_def_data_offset  = I64_PLC_OFFSET_DEFAULT_VALUE,
    .out_offset_param_id = 0,
    .out_def_data_offset = 0,
  },

  {
    .module_id           = PNET_MOD_128_I_IDENT,
    .insize              = PNET_MOD_128_I_DATASIZE_INPUT,
    .outsize             = 0,
    .data_dir            = PNET_DIR_INPUT,
    .in_offset_param_id  = I128_PLC_OFFSET_PARAM_IDX_103,
    .in_def_data_offset  = I128_PLC_OFFSET_DEFAULT_VALUE,
    .out_offset_param_id = 0,
    .out_def_data_offset = 0,
  },

  {
    .module_id           = PNET_MOD_256_I_IDENT,
    .insize              = PNET_MOD_256_I_DATASIZE_INPUT,
    .outsize             = 0,
    .data_dir            = PNET_DIR_INPUT,
    .in_offset_param_id  = I256_PLC_OFFSET_PARAM_IDX_104,
    .in_def_data_offset  = I256_PLC_OFFSET_DEFAULT_VALUE,
    .out_offset_param_id = 0,
    .out_def_data_offset = 0,
  },

  {
    .module_id           = PNET_MOD_32_O_IDENT,
    .insize              = 0,
    .outsize             = PNET_MOD_32_O_DATASIZE_OUTPUT,
    .data_dir            = PNET_DIR_OUTPUT,
    .in_offset_param_id  = 0,
    .in_def_data_offset  = 0,
    .out_offset_param_id = O32_PLC_OFFSET_PARAM_IDX_105,
    .out_def_data_offset = O32_PLC_OFFSET_DEFAULT_VALUE,
  },

  {
    .module_id           = PNET_MOD_64_O_IDENT,
    .insize              = 0,
    .outsize             = PNET_MOD_64_O_DATASIZE_OUTPUT,
    .data_dir            = PNET_DIR_OUTPUT,
    .in_offset_param_id  = 0,
    .in_def_data_offset  = 0,
    .out_offset_param_id = O64_PLC_OFFSET_PARAM_IDX_106,
    .out_def_data_offset = O64_PLC_OFFSET_DEFAULT_VALUE,
  },

  {
    .module_id           = PNET_MOD_128_O_IDENT,
    .insize              = 0,
    .outsize             = PNET_MOD_128_O_DATASIZE_OUTPUT,
    .data_dir            = PNET_DIR_OUTPUT,
    .in_offset_param_id  = 0,
    .in_def_data_offset  = 0,
    .out_offset_param_id = O128_PLC_OFFSET_PARAM_IDX_107,
    .out_def_data_offset = O128_PLC_OFFSET_DEFAULT_VALUE,
  },

  {
    .module_id           = PNET_MOD_256_O_IDENT,
    .insize              = 0,
    .outsize             = PNET_MOD_256_O_DATASIZE_OUTPUT,
    .data_dir            = PNET_DIR_OUTPUT,
    .in_offset_param_id  = 0,
    .in_def_data_offset  = 0,
    .out_offset_param_id = O256_PLC_OFFSET_PARAM_IDX_108,
    .out_def_data_offset = O256_PLC_OFFSET_DEFAULT_VALUE,
  },


};

const submodule_t cfg_available_submodule_types[NUMBER_OF_DEFINED_SUBMODULES] =
{
   {APP_API, PNET_MOD_DAP_IDENT, PNET_SUBMOD_DAP_IDENT,                    PNET_DIR_NO_IO,  0, 0 },
   {APP_API, PNET_MOD_DAP_IDENT, PNET_SUBMOD_DAP_INTERFACE_1_IDENT,        PNET_DIR_NO_IO,  0, 0 },
   {APP_API, PNET_MOD_DAP_IDENT, PNET_SUBMOD_DAP_INTERFACE_1_PORT_0_IDENT, PNET_DIR_NO_IO,  0, 0 },
   {APP_API, PNET_MOD_32_I_IDENT,  PNET_SUBMOD_CUSTOM_IDENT,               PNET_DIR_INPUT,  PNET_MOD_32_I_DATASIZE_INPUT,  0  },
   {APP_API, PNET_MOD_64_I_IDENT,  PNET_SUBMOD_CUSTOM_IDENT,               PNET_DIR_INPUT,  PNET_MOD_64_I_DATASIZE_INPUT,  0  },
   {APP_API, PNET_MOD_128_I_IDENT, PNET_SUBMOD_CUSTOM_IDENT,               PNET_DIR_INPUT,  PNET_MOD_128_I_DATASIZE_INPUT, 0  },
   {APP_API, PNET_MOD_256_I_IDENT, PNET_SUBMOD_CUSTOM_IDENT,               PNET_DIR_INPUT,  PNET_MOD_256_I_DATASIZE_INPUT, 0  },
   {APP_API, PNET_MOD_32_O_IDENT,  PNET_SUBMOD_CUSTOM_IDENT,               PNET_DIR_OUTPUT, 0, PNET_MOD_32_O_DATASIZE_OUTPUT  },
   {APP_API, PNET_MOD_64_O_IDENT,  PNET_SUBMOD_CUSTOM_IDENT,               PNET_DIR_OUTPUT, 0, PNET_MOD_64_O_DATASIZE_OUTPUT  },
   {APP_API, PNET_MOD_128_O_IDENT, PNET_SUBMOD_CUSTOM_IDENT,               PNET_DIR_OUTPUT, 0, PNET_MOD_128_O_DATASIZE_OUTPUT },
   {APP_API, PNET_MOD_256_O_IDENT, PNET_SUBMOD_CUSTOM_IDENT,               PNET_DIR_OUTPUT, 0, PNET_MOD_256_O_DATASIZE_OUTPUT },
};


static pf_device_t thisDevice;

/************ Configuration of product ID, software version etc **************/
static pnet_cfg_t  pnet_default_cfg =
{
  /* Call-backs */
  .state_cb = app_state_ind,
  .connect_cb = app_connect_ind,
  .release_cb = app_release_ind,
  .dcontrol_cb = app_dcontrol_ind,
  .ccontrol_cb = app_ccontrol_cnf,
  .read_cb = app_read_ind,
  .write_cb = app_write_ind,
  .exp_module_cb = app_exp_module_ind,
  .exp_submodule_cb = app_exp_submodule_ind,
  .new_data_status_cb = app_new_data_status_ind,
  .alarm_ind_cb = app_alarm_ind,
  .alarm_cnf_cb = app_alarm_cnf,
  .alarm_ack_cnf_cb = app_alarm_ack_cnf,
  .reset_cb = app_reset_ind,
  .cb_arg = NULL,

  .im_0_data =
  {
    .vendor_id_hi = 0x05,
    .vendor_id_lo = 0x44,
    .order_id = "<orderid>           ",
    .im_serial_number = "<serial nbr>    ",
    .im_hardware_revision = 2,
    .sw_revision_prefix = 'V', /* 'V', 'R', 'P', 'U', or 'T' */
    .im_sw_revision_functional_enhancement = 2,
    .im_sw_revision_bug_fix = 0,
    .im_sw_revision_internal_change = 0,
    .im_revision_counter = 0,
    .im_profile_id = 0x1,
    .im_profile_specific_type = 0x1,
    .im_version_major = 1,
    .im_version_minor = 1,
    .im_supported = 0x001e,         /* Only I&M0..I&M4 supported */
    //    This parameter indicates the
    //    availability of the I&M records :
    // Byte 0 :
    //   2^7 = 0 : I&M15(not supported)
    //   2^6 = 0 : I&M14(not supported)
    //   2^5 = 0 : I&M13(not supported)
    //   2^4 = 0 : I&M12(not supported)
    //   2^3 = 0 : I&M11(not supported)
    //   2^2 = 0 : I&M10(not supported)
    //   2^1 = 0 : I&M9(not supported)
    //   2^0 = 0 : I&M8(not supported)
    //   Byte 1 :
    //   2^7 = 0 : I&M7(not supported)
    //   2^6 = 0 : I&M6(not supported)
    //   2^5 = 0 : I&M5(not supported)
    //   2^4 = 0 : I&M4(optional)
    //   2^3 = 1 : I&M3(mandatory)
    //   2^2 = 1 : I&M2(mandatory)
    //   2^1 = 1 : I&M1(mandatory)
    //   2^0 = 0 : profile specific I & M
    //   (not supported)
  },
  .im_1_data =
  {
   .im_tag_function = "",
   .im_tag_location = ""
  },
  .im_2_data =
  {
   .im_date = ""
  },
  .im_3_data =
  {
   .im_descriptor = ""
  },
  .im_4_data =
  {
   .im_signature = ""
  },

  /* Device configuration */
  .device_id =
  {  /* device id: vendor_id_hi, vendor_id_lo, device_id_hi, device_id_lo */
     0x05, 0x44, 0x00, 0x01,
  },
  .oem_device_id =
  {  /* OEM device id: vendor_id_hi, vendor_id_lo, device_id_hi, device_id_lo */
     0x05, 0x44, 0x00, 0x01,
  },
  .station_name = "",              /* Set by command line argument */
  .device_vendor = "TGDrives",
  .manufacturer_specific_string = "tgmmini",

  .lldp_cfg =
  {
// set below in main()
//    .chassis_id = "a",   /* Is this a valid name? '-' allowed?*/
//    .port_id = "port-001",
   .ttl = 20,          /* seconds */
   .rtclass_2_status = 0,
   .rtclass_3_status = 0,
   .cap_aneg = PNET_LLDP_AUTONEG_SUPPORTED | PNET_LLDP_AUTONEG_ENABLED,
   .cap_phy = PNET_LLDP_AUTONEG_CAP_100BaseTX_HALF_DUPLEX | PNET_LLDP_AUTONEG_CAP_100BaseTX_FULL_DUPLEX,
   .mau_type = PNET_MAU_COPPER_100BaseTX_FULL_DUPLEX,
  },

  /* Network configuration */
  .send_hello = 1,                    /* Send HELLO */
  .dhcp_enable = 0,
  .ip_addr = { 0 },                   /* Read from Linux kernel */
  .ip_mask = { 0 },                   /* Read from Linux kernel */
  .ip_gateway = { 0 },                /* Read from Linux kernel */
  .eth_addr = { { 0 } },                  /* Read from Linux kernel */
  .p_default_device = &thisDevice,
  .temp_check_peers_data = { 0, 0, { 0 }, 0, { 0 } },
  .adjust_peer_to_peer_boundary = 0,
};


/*********************************** Callbacks ********************************/

static int app_connect_ind(
  pnet_t        *net,
  void          *arg,
  uint32_t       arep,
  pnet_result_t *p_result)
{
//  app_data_t *p_appdata = (app_data_t *)arg;

//  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "Connect call-back. AREP: %u  Status codes: %d %d %d %d\n",
           arep,
           p_result->pnio_status.error_code,
           p_result->pnio_status.error_decode,
           p_result->pnio_status.error_code_1,
           p_result->pnio_status.error_code_2);
  }
  /*
   *  Handle the request on an application level.
   *  This is a very simple application which does not need to handle anything.
   *  All the needed information is in the AR data structure.
   */

  return 0;
}

static int app_release_ind(
  pnet_t       *net,
  void         *arg,
  uint32_t      arep,
  pnet_result_t *p_result)
{
//  app_data_t *p_appdata = (app_data_t *)arg;

//  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "Release (disconnect) call-back. AREP: %u  Status codes: %d %d %d %d\n",
           arep,
           p_result->pnio_status.error_code,
           p_result->pnio_status.error_decode,
           p_result->pnio_status.error_code_1,
           p_result->pnio_status.error_code_2);
  }

  return 0;
}

static int app_dcontrol_ind(
  pnet_t                *net,
  void                  *arg,
  uint32_t               arep,
  pnet_control_command_t control_command,
  pnet_result_t         *p_result)
{
  app_data_t *p_appdata = (app_data_t *)arg;

  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_DEBUG, "DControl call-back. AREP: %u  Command: %d  Status codes: %d %d %d %d\n",
           arep,
           control_command,
           p_result->pnio_status.error_code,
           p_result->pnio_status.error_decode,
           p_result->pnio_status.error_code_1,
           p_result->pnio_status.error_code_2);
  }

  return 0;
}

static int app_ccontrol_cnf(
  pnet_t        *net,
  void          *arg,
  uint32_t       arep,
  pnet_result_t *p_result)
{
  app_data_t *p_appdata = (app_data_t *)arg;

  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_DEBUG, "CControl confirmation call-back. AREP: %u  Status codes: %d %d %d %d\n",
           arep,
           p_result->pnio_status.error_code,
           p_result->pnio_status.error_decode,
           p_result->pnio_status.error_code_1,
           p_result->pnio_status.error_code_2);
  }

  return 0;
}

// write parameter (from controller to device)
// parameter is offset to PLC memory
static int app_write_ind(
  pnet_t        *net,
  void          *arg,
  uint32_t       arep,
  uint32_t       api,
  uint16_t       slot,
  uint16_t       subslot,
  uint16_t       idx,
  uint16_t       sequence_number,
  uint16_t       write_length,
  uint8_t       *p_write_data,
  pnet_result_t *p_result)
{
  app_data_t *p_appdata = (app_data_t *)arg;
  
  if (idx <= PF_IDX_USER_MAX)
  {
    return writeUserParameter(slot, subslot, write_length, p_write_data, p_appdata, idx, p_result);
  }

  // do not set p_result here!

  return -1;
}

// read parameter which is a data offset to PLC memory
// convert the value to big endian and send to controller
static int app_read_ind(
  pnet_t        *net,
  void          *arg,
  uint32_t       arep,
  uint32_t       api,
  uint16_t       slot,
  uint16_t       subslot,
  uint16_t       idx,
  uint16_t       sequence_number,
  uint8_t      **pp_read_data,
  uint16_t      *p_read_length,
  pnet_result_t *p_result)
{
  app_data_t *p_appdata = (app_data_t *)arg;

  if (idx <= PF_IDX_USER_MAX)
  {
    return readUserParameter(slot, subslot, idx, p_read_length, pp_read_data, p_appdata, p_result);
  }

  // do not set p_result here!

  return -1;
}


static int app_state_ind(
  pnet_t             *net,
  void               *arg,
  uint32_t            arep,
  pnet_event_values_t state)
{
  uint16_t    err_cls = 0;
  uint16_t    err_code = 0;
  app_data_t *p_appdata = (app_data_t *)arg;
  const char              *error_description = "";

  if (state == PNET_EVENT_ABORT)
  {
    if (pnet_get_ar_error_codes(net, arep, &err_cls, &err_code) == 0)
    {
      if (p_appdata->arguments.verbosity > 0)
      {
        /* A few of the most common error codes */
        switch (err_cls)
        {
        case 0:
          error_description = "Unknown error class";
          break;
        case PNET_ERROR_CODE_1_RTA_ERR_CLS_PROTOCOL:
          switch (err_code)
          {
          case PNET_ERROR_CODE_2_ABORT_AR_CONSUMER_DHT_EXPIRED:
            error_description = "AR_CONSUMER_DHT_EXPIRED";
            break;
          case PNET_ERROR_CODE_2_ABORT_AR_CMI_TIMEOUT:
            error_description = "ABORT_AR_CMI_TIMEOUT";
            break;
          case PNET_ERROR_CODE_2_ABORT_AR_RELEASE_IND_RECEIVED:
            error_description = "Controller sent release request.";
            break;
          }
          break;
        }
        os_log(LOG_LEVEL_DEBUG, 
               "Callback on event PNET_EVENT_ABORT. Error class: %u Error code: %u  %s\n",
               (unsigned)err_cls, (unsigned)err_code, error_description);
      }
    }
    else
    {
      if (p_appdata->arguments.verbosity > 0)
      {
        printf("Callback on event PNET_EVENT_ABORT. No error status available\n");
      }
    }
    /* Only abort AR with correct session key */
    os_event_set(p_appdata->main_events, EVENT_ABORT);
  }
  else if (state == PNET_EVENT_PRMEND)
  {
    if (p_appdata->arguments.verbosity > 0)
    {
      os_log(LOG_LEVEL_DEBUG, "Callback on event PNET_EVENT_PRMEND. AREP: %u\n", arep);
    }

    /* Set IOPS for DAP slot (has same numbering as the module identifiers) */
    (void)pnet_input_set_data_and_iops(net, APP_API, PNET_SLOT_DAP_IDENT, PNET_SUBMOD_DAP_IDENT, NULL, 0, PNET_IOXS_GOOD);
    (void)pnet_input_set_data_and_iops(net, APP_API, PNET_SLOT_DAP_IDENT, PNET_SUBMOD_DAP_INTERFACE_1_IDENT, NULL, 0, PNET_IOXS_GOOD);
    (void)pnet_input_set_data_and_iops(net, APP_API, PNET_SLOT_DAP_IDENT, PNET_SUBMOD_DAP_INTERFACE_1_PORT_0_IDENT, NULL, 0, PNET_IOXS_GOOD);

    setupPluggedModules(net, p_appdata);

    (void)pnet_set_provider_state(net, true);

    /* Save the arep for later use */
    p_appdata->main_arep = arep;
    os_event_set(p_appdata->main_events, EVENT_READY_FOR_DATA);
  }
  else if (state == PNET_EVENT_DATA)
  {
    if (p_appdata->arguments.verbosity > 0)
    {
      os_log(LOG_LEVEL_DEBUG, "Callback on event PNET_EVENT_DATA\n");
    }
  }
  else if (state == PNET_EVENT_STARTUP)
  {
    if (p_appdata->arguments.verbosity > 0)
    {
      os_log(LOG_LEVEL_DEBUG, "Callback on event PNET_EVENT_STARTUP\n");
    }
  }
  else if (state == PNET_EVENT_APPLRDY)
  {
    if (p_appdata->arguments.verbosity > 0)
    {
      os_log(LOG_LEVEL_DEBUG, "Callback on event PNET_EVENT_APPLRDY\n");
    }
  }

  return 0;
}

static int app_reset_ind(
  pnet_t   *net,
  void     *arg,
  bool      should_reset_application,
  uint16_t  reset_mode)
{
  app_data_t *p_appdata = (app_data_t *)arg;
  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "*** Reset call-back. Application reset mandatory: %u  Reset mode: %d ***\n",
           should_reset_application,
           reset_mode);
  }
  if(reset_mode == 1) // Reset application parameters
  {
  }
  else if (reset_mode == 2) // reset communication parameters
  {
    os_ipaddr_t zero = 0;
    os_set_ip_suite(p_appdata, zero, zero, zero, false);
    os_set_station_name(p_appdata, "", false);
  }
  return 0;
}

static int app_exp_module_ind(
  pnet_t   *net,
  void     *arg,
  uint32_t  api,
  uint16_t  slot,
  uint32_t  module_ident)
{
  int         ret = -1;   /* Not supported in specified slot */
  app_data_t *p_appdata = (app_data_t *)arg;
  const int   verbosity = p_appdata->arguments.verbosity;

  if (verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "Module plug call-back\n");
  }

  /* Find it in the list of supported modules */
  uint16_t  ix = 0;
  while ((ix < NELEMENTS(cfg_available_module_types)) &&
         (cfg_available_module_types[ix].module_id != module_ident))
  {
    ix++;
  }

  if (ix < NELEMENTS(cfg_available_module_types))
  {
    if(verbosity > 0)
    {
      os_log(LOG_LEVEL_INFO, "  Pull old module.    API: %u Slot: 0x%x\n", api, slot);
    }

    if (pnet_pull_module(net, api, slot) != 0)
    {
      if (verbosity > 0)
      {
        os_log(LOG_LEVEL_INFO, "    Slot was empty.\n");
      }
    }

    /* For now support any of the known modules in any slot */
    if (verbosity > 0)
    {
      os_log(LOG_LEVEL_INFO, "  Plug module.        API: %u Slot: 0x%x Module ID: 0x%x Index in supported modules: %u\n", api, slot, (unsigned)module_ident, ix);
    }
    ret = pnet_plug_module(net, api, slot, module_ident);
    if (ret != 0)
    {
      os_log(LOG_LEVEL_WARNING, 
             "Plug module failed. Ret: %u API: %u Slot: %u Module ID: 0x%x Index in list of supported modules: %u\n", ret, api, slot, (unsigned)module_ident, ix);
    }
    else
    {
      /* Remember what is plugged in each slot */
      if (slot < PNET_MAX_MODULES)
      {
        const pnet_submodule_dir_t data_dir = cfg_available_module_types[ix].data_dir;
        if ((data_dir == PNET_DIR_INPUT) || (data_dir == PNET_DIR_IO))
        {
          slot_t *pInputSlot      = &(p_appdata->custom_input_slots[slot]);
          pInputSlot->module_id   = module_ident;
          pInputSlot->size        = cfg_available_module_types[ix].insize;
          pInputSlot->param_id    = cfg_available_module_types[ix].in_offset_param_id;
          pInputSlot->data_offset = cfg_available_module_types[ix].in_def_data_offset;
        }
        if ((data_dir == PNET_DIR_OUTPUT) || (data_dir == PNET_DIR_IO))
        {
          slot_t *pOutputSlot      = &(p_appdata->custom_output_slots[slot]);
          pOutputSlot->module_id   = module_ident;
          pOutputSlot->size        = cfg_available_module_types[ix].outsize;
          pOutputSlot->param_id    = cfg_available_module_types[ix].out_offset_param_id;
          pOutputSlot->data_offset = cfg_available_module_types[ix].out_def_data_offset;
        }
      }
      else
      {
        os_log(LOG_LEVEL_WARNING, "Wrong slot number received: %u  It should be less than %u\n", slot, PNET_MAX_MODULES);
      }
    }

  }
  else
  {
    os_log(LOG_LEVEL_WARNING, 
           "Module ID %08x not found. API: %u Slot: %u\n",
           (unsigned)module_ident,
           api,
           slot);
  }

  return ret;
}

static int app_exp_submodule_ind(
  pnet_t  *net,
  void    *arg,
  uint32_t api,
  uint16_t slot,
  uint16_t subslot,
  uint32_t module_ident,
  uint32_t submodule_ident)
{
  int         ret = -1;
  app_data_t *p_appdata = (app_data_t *)arg;
  const int   verbosity = p_appdata->arguments.verbosity;

  if (verbosity > 0)
  {
    printf("Submodule plug call-back.\n");
  }

  /* Find it in the list of supported submodules */
  uint16_t ix = 0;
  while ((ix < NELEMENTS(cfg_available_submodule_types)) &&
         ((cfg_available_submodule_types[ix].module_ident_nbr != module_ident) ||
          (cfg_available_submodule_types[ix].submodule_ident_nbr != submodule_ident)))
  {
    ix++;
  }

  if (ix < NELEMENTS(cfg_available_submodule_types))
  {
    if (verbosity > 0)
    {
      os_log(LOG_LEVEL_INFO, "  Pull old submodule. API: %u Slot: 0x%x Subslot: 0x%x\n",
             api,
             slot,
             subslot);
    }

    if (pnet_pull_submodule(net, api, slot, subslot) != 0)
    {
      if (verbosity > 0)
      {
        os_log(LOG_LEVEL_INFO, "     Subslot was empty.\n");
      }
    }

    if (verbosity > 0)
    {
      os_log(LOG_LEVEL_INFO, "  Plug submodule.     API: %u Slot: 0x%x Module ID: 0x%-4x Subslot: 0x%x Submodule ID: 0x%x Index in supported submodules: %u Dir: %u In: %u Out: %u bytes\n",
             api,
             slot,
             (unsigned)module_ident,
             subslot,
             (unsigned)submodule_ident,
             ix,
             cfg_available_submodule_types[ix].data_dir,
             cfg_available_submodule_types[ix].insize,
             cfg_available_submodule_types[ix].outsize);
    }
    ret = pnet_plug_submodule(net, api, slot, subslot,
                              module_ident,
                              submodule_ident,
                              cfg_available_submodule_types[ix].data_dir,
                              cfg_available_submodule_types[ix].insize,
                              cfg_available_submodule_types[ix].outsize);
    if (ret != 0)
    {
      os_log(LOG_LEVEL_WARNING, "Plug submodule failed. Ret: %u API: %u Slot: %u Subslot 0x%x Module ID: 0x%x Submodule ID: 0x%x Index in list of supported modules: %u\n",
             ret,
             api,
             slot,
             subslot,
             (unsigned)module_ident,
             (unsigned)submodule_ident,
             ix);
    }
  }
  else
  {
    os_log(LOG_LEVEL_ERROR, " Submodule ID 0x%x in module ID 0x%x not found. API: %u Slot: %u Subslot %u \n",
           (unsigned)submodule_ident,
           (unsigned)module_ident,
           api,
           slot,
           subslot);
  }

  return ret;
}

static int app_new_data_status_ind(
  pnet_t   *net,
  void     *arg,
  uint32_t  arep,
  uint32_t  crep,
  uint8_t   changes,
  uint8_t   data_status)
{
  app_data_t *p_appdata = (app_data_t *)arg;

  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, 
           "New data status callback. AREP: %u  Status changes: 0x%02x  Status: 0x%02x\n", 
           arep, 
           changes, 
           data_status);
  }

  return 0;
}

static int app_alarm_ind(
  pnet_t   *net,
  void     *arg,
  uint32_t  arep,
  uint32_t  api,
  uint16_t  slot,
  uint16_t  subslot,
  uint16_t  data_len,
  uint16_t  data_usi,
  uint8_t  *p_data)
{
  app_data_t *p_appdata = (app_data_t *)arg;

  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, 
           "Alarm indicated callback. AREP: %u  API: %d  Slot: %d  Subslot: %d  Length: %d  USI: %d\n",
           arep,
           api,
           slot,
           subslot,
           data_len,
           data_usi);
  }

//   if(slot == 0 && subslot == 0x8000)
//   {
//     p_appdata->alarm_pnio_status.error_code = PNET_ERROR_CODE_RTA_ERROR;
//     p_appdata->alarm_pnio_status.error_decode = PNET_ERROR_DECODE_PNIO;
//     p_appdata->alarm_pnio_status.error_code_1 = PNET_ERROR_CODE_1_RTA_ERR_CLS_PROTOCOL;
//     p_appdata->alarm_pnio_status.error_code_2 = PNET_ERROR_CODE_2_ABORT_AR_ALARM_DATA_TOO_LONG;
//     os_event_set(p_appdata->main_events, EVENT_ALARM);
//   }

//  os_event_set(p_appdata->main_events, EVENT_ALARM);

  return 0;
}

static int app_alarm_cnf(
  pnet_t             *net,
  void               *arg,
  uint32_t            arep,
  pnet_pnio_status_t *p_pnio_status)
{
  app_data_t *p_appdata = (app_data_t *)arg;

  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, 
           "Alarm confirmed (by controller) callback. AREP: %u  Status code %u, %u, %u, %u\n",
           arep,
           p_pnio_status->error_code,
           p_pnio_status->error_decode,
           p_pnio_status->error_code_1,
           p_pnio_status->error_code_2);
  }
  p_appdata->alarm_allowed = true;

  return 0;
}

static int app_alarm_ack_cnf(
  pnet_t  *net,
  void    *arg,
  uint32_t arep,
  int      res)
{
  app_data_t *p_appdata = (app_data_t *)arg;

  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, 
           "Alarm ACK confirmation (from controller) callback. AREP: %u  Result: %d\n", arep, res);
  }

  return 0;
}

// init the default I&MFilterData device for 0xF840 data record
static void fillDefaultIM0FilterData(pf_device_t *dev)
{
  pf_api_t *p_api = dev->apis;
  p_api->in_use = true;
  p_api->api_id = 0;
  // use only the slot 0
  pf_slot_t *p_slot0               = &(p_api->slots[0]);
  p_slot0->in_use                  = true;
  p_slot0->exp_module_ident_number = PNET_MOD_DAP_IDENT;
  p_slot0->module_ident_number     = PNET_MOD_DAP_IDENT;
  p_slot0->plug_state              = PF_MOD_PLUG_PROPER_MODULE;
  p_slot0->slot_nbr                = 0;

  // add subslots (actually only slot 0 is used in response to 0xF840 request)
  pf_subslot_t *p_subslot0_0               = &(p_slot0->subslots[0]);
  p_subslot0_0->in_use                     = true;
  p_subslot0_0->subslot_nbr                = PNET_SUBMOD_DAP_IDENT;
  p_subslot0_0->exp_submodule_ident_number = PNET_SUBMOD_DAP_IDENT;
  p_subslot0_0->submodule_ident_number     = PNET_SUBMOD_DAP_IDENT;
  p_subslot0_0->direction                  = PNET_DIR_NO_IO;
  p_subslot0_0->submodule_state.ident_info = PF_SUBMOD_PLUG_OK;
  
  pf_subslot_t *p_subslot0_1               = &(p_slot0->subslots[1]);
  p_subslot0_1->in_use                     = true;
  p_subslot0_1->subslot_nbr                = PNET_SUBMOD_DAP_INTERFACE_1_IDENT;
  p_subslot0_1->exp_submodule_ident_number = PNET_SUBMOD_DAP_IDENT; // here is the change!
  p_subslot0_1->submodule_ident_number     = PNET_SUBMOD_DAP_INTERFACE_1_IDENT;
  p_subslot0_1->direction                  = PNET_DIR_NO_IO;
  p_subslot0_1->submodule_state.ident_info = PF_SUBMOD_PLUG_OK;

  pf_subslot_t *p_subslot0_2               = &(p_slot0->subslots[2]);
  p_subslot0_2->in_use                     = true;
  p_subslot0_2->subslot_nbr                = PNET_SUBMOD_DAP_INTERFACE_1_PORT_0_IDENT;
  p_subslot0_2->exp_submodule_ident_number = PNET_SUBMOD_DAP_INTERFACE_1_PORT_0_IDENT;
  p_subslot0_2->submodule_ident_number     = PNET_SUBMOD_DAP_INTERFACE_1_PORT_0_IDENT;
  p_subslot0_2->direction                  = PNET_DIR_NO_IO;
  p_subslot0_2->submodule_state.ident_info = PF_SUBMOD_PLUG_OK;

  const uint32_t idents[PNET_MAX_MODULES] = {
    PNET_SUBMOD_DAP_IDENT,
    PNET_MOD_32_I_IDENT,
    PNET_MOD_64_I_IDENT,
    PNET_MOD_128_I_IDENT,
    PNET_MOD_256_I_IDENT,
    PNET_MOD_32_O_IDENT,
    PNET_MOD_64_O_IDENT,
    PNET_MOD_128_O_IDENT,
    PNET_MOD_256_O_IDENT,
  };

  const pnet_submodule_dir_t dirs[PNET_MAX_MODULES] = {
    PNET_DIR_NO_IO,
    PNET_DIR_INPUT,
    PNET_DIR_INPUT,
    PNET_DIR_INPUT,
    PNET_DIR_INPUT,
    PNET_DIR_OUTPUT,
    PNET_DIR_OUTPUT,
    PNET_DIR_OUTPUT,
    PNET_DIR_OUTPUT,
  };

  for (size_t i = 1U; i < PNET_MAX_MODULES; i++)
  {
    pf_slot_t *p_slot               = &(p_api->slots[i]);
    p_slot->in_use                  = true;
    p_slot->exp_module_ident_number = idents[i];
    p_slot->module_ident_number     = idents[i];
    p_slot->plug_state              = PF_MOD_PLUG_PROPER_MODULE;
    p_slot->slot_nbr                = i;

    pf_subslot_t *p_subslot               = &(p_slot->subslots[0]);
    p_subslot->in_use                     = true;
    p_subslot->subslot_nbr                = 1;
    p_subslot->exp_submodule_ident_number = PNET_SUBMOD_CUSTOM_IDENT;
    p_subslot->submodule_ident_number     = PNET_SUBMOD_CUSTOM_IDENT;
    p_subslot->direction                  = dirs[i];
    p_subslot->submodule_state.ident_info = PF_SUBMOD_PLUG_OK;
  }
}

/************************* Utilities ******************************************/
void main_timer_tick(
  os_timer_t *timer,
  void       *arg)
{
  app_data_and_stack_t *appdata_and_stack = (app_data_and_stack_t *)arg;
  app_data_t           *p_appdata = appdata_and_stack->appdata;

  os_event_set(p_appdata->main_events, EVENT_TIMER);
}

void *timer_thread_func(void *arg)
{
  app_data_and_stack_t *appdata_and_stack = (app_data_and_stack_t *)arg;
  app_data_t           *p_appdata = appdata_and_stack->appdata;
  pnet_t               *net = appdata_and_stack->net;

  rpmalloc_thread_initialize();

  struct timespec timeAbs;
  clock_gettime(CLOCK_REALTIME, &timeAbs);
  int64_t tCur = (int64_t)(timeAbs.tv_sec) * 1000000000ll + (int64_t)(timeAbs.tv_nsec);

  int64_t cycleTimeIn_ns = (int64_t)(TICK_INTERVAL_US * 1000ll);
  tCur += cycleTimeIn_ns;
  timeAbs.tv_sec = (time_t)(tCur / 1000000000ll);
  timeAbs.tv_nsec = tCur - (time_t)(timeAbs.tv_sec) * 1000000000ll;

  while(p_appdata->running != false)
  {
    clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &timeAbs, NULL);

    /* Set data for custom input modules, if any */
    setInputDataToController(net, p_appdata);

    /* Read data from first of the custom output modules, if any */
    getOutputDataFromController(net, p_appdata);

    pnet_handle_periodic(net);

    tCur += cycleTimeIn_ns;
    timeAbs.tv_sec = (time_t)(tCur / 1000000000ll);
    timeAbs.tv_nsec = tCur - (time_t)(timeAbs.tv_sec) * 1000000000ll;
  }

  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "[INFO ] timer_thread_func terminated\n");
  }

  rpmalloc_thread_finalize();

  return NULL;
}

void *pn_main(void *arg)
{
  uint32_t mask = EVENT_READY_FOR_DATA | EVENT_TIMER | EVENT_ALARM | EVENT_ABORT | EVENT_TERMINATE;
  uint32_t flags = 0;
  uint32_t tick_ctr_update_data = 0;

  app_data_and_stack_t *appdata_and_stack = (app_data_and_stack_t *)arg;
  app_data_t           *p_appdata         = appdata_and_stack->appdata;
  pnet_t               *net               = appdata_and_stack->net;

  rpmalloc_thread_initialize();

  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "Waiting for connect request from IO-controller\n");
  }

  /* Main loop */
  while(p_appdata->running != false)
  {
    os_event_wait(p_appdata->main_events, mask, &flags, OS_WAIT_FOREVER);
    if (flags & EVENT_READY_FOR_DATA)
    {
      os_event_clr(p_appdata->main_events, EVENT_READY_FOR_DATA); /* Re-arm */
      handleReadyForDataEvent(net, p_appdata);
    }
    else if (flags & EVENT_ALARM)
    {
      os_event_clr(p_appdata->main_events, EVENT_ALARM); /* Re-arm */
      handleAlarmEvent(net, p_appdata);
    }
    else if (flags & EVENT_TIMER)
    {
      os_event_clr(p_appdata->main_events, EVENT_TIMER); /* Re-arm */
      const uint64_t timeStart = timeGetTime64_ns();
      tick_ctr_update_data += TICK_INTERVAL_US;

      /* Set input and output data every 8 ms */
      if ((p_appdata->main_arep != UINT32_MAX) && tick_ctr_update_data >= DATA_INTERVAL_US)
      {
        tick_ctr_update_data = 0;

        /* Set data for custom input modules, if any */
        setInputDataToController(net, p_appdata);

        /* Read data from first of the custom output modules, if any */
        getOutputDataFromController(net, p_appdata);
      }

      pnet_handle_periodic(net);
      const uint64_t timeEnd = timeGetTime64_ns();
      if (timeEnd - timeStart < (TICK_INTERVAL_US * 400))
      {
        os_usleep(TICK_INTERVAL_US / 2);
      }
    }
    else if (flags & EVENT_ABORT)
    {
      p_appdata->main_arep = UINT32_MAX;
      p_appdata->alarm_allowed = true;
      p_appdata->b_disp_wrong_offset_warning = true;
      os_event_clr(p_appdata->main_events, EVENT_ABORT); /* Re-arm */
      if (p_appdata->arguments.verbosity > 0)
      {
        os_log(LOG_LEVEL_INFO, "Aborting the application\n");
      }
    }
    else if (flags & EVENT_TERMINATE)
    {
      break;
    }
  }
  rpmalloc_thread_finalize();
  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "[INFO ] pn_main terminated\n");
  }
  return NULL;
}

/******************************************************************************/
void app_plug_dap(pnet_t *net, void *arg)
{
  // Use existing callback functions to plug the (sub-)modules
  app_exp_module_ind(net, arg, APP_API, PNET_SLOT_DAP_IDENT, PNET_MOD_DAP_IDENT);

  app_exp_submodule_ind(net, arg, APP_API, PNET_SLOT_DAP_IDENT, PNET_SUBMOD_DAP_IDENT,
                        PNET_MOD_DAP_IDENT, PNET_SUBMOD_DAP_IDENT);
  app_exp_submodule_ind(net, arg, APP_API, PNET_SLOT_DAP_IDENT, PNET_SUBMOD_DAP_INTERFACE_1_IDENT,
                        PNET_MOD_DAP_IDENT, PNET_SUBMOD_DAP_INTERFACE_1_IDENT);
  app_exp_submodule_ind(net, arg, APP_API, PNET_SLOT_DAP_IDENT, PNET_SUBMOD_DAP_INTERFACE_1_PORT_0_IDENT,
                        PNET_MOD_DAP_IDENT, PNET_SUBMOD_DAP_INTERFACE_1_PORT_0_IDENT);
}

//////////////////////////////////////////////////////////////////////////

static void faulthand(int32_t sig)
{
  signal(sig, SIG_DFL);
  kill(getpid(), sig);
}


/****************************** Main ******************************************/

int main(int argc, char *argv[])
{
  static app_data_and_stack_t appdata_and_stack;
  static app_data_t appdata;

  memset(&appdata, 0, sizeof(appdata));
  appdata.alarm_allowed = true;
  appdata.main_arep = UINT32_MAX;
  appdata.b_disp_wrong_offset_warning = true;
  /* Parse and display command line arguments */
  appdata.arguments = parse_commandline_arguments(argc, argv);
  appdata.running = true;
  os_init(&appdata);

  printf("\n");
  os_log(LOG_LEVEL_INFO, "******* Starting TGMmini Profinet I/O application *******\n");
  printf(   "***     Built " __DATE__ " " __TIME__ "                    ***\n");
  printf(   "***     Using Profinet p-net stack by rt-labs.com     ***\n");
  printf(   "***          (https://rt-labs.com/docs/pnet)          ***\n");
  printf(   "***      and memory allocator by Mattias Jansson      ***\n");
  printf(   "*******  (https://github.com/mjansson/rpmalloc)   *******\n\n");

  /* Read IP, netmask, gateway and MAC address from operating system */
  if (!does_network_interface_exist(appdata.arguments.eth_interface))
  {
    os_log(LOG_LEVEL_ERROR, "Error: The given Ethernet interface does not exist: %s\n", appdata.arguments.eth_interface);
    exit(EXIT_CODE_ERROR);
  }

  uint32_t ip_int = read_ip_address(appdata.arguments.eth_interface);
  if (ip_int == IP_INVALID)
  {
    os_log(LOG_LEVEL_ERROR, "Error: Invalid IP address.\n");
    exit(EXIT_CODE_ERROR);
  }
  appdata.def_ip = ip_int;
  pnet_default_cfg.ip_sys_addr = ip_int;

  uint32_t netmask_int = read_netmask(appdata.arguments.eth_interface);

  uint32_t gateway_ip_int = read_default_gateway(appdata.arguments.eth_interface);
  if (gateway_ip_int == IP_INVALID)
  {
    os_log(LOG_LEVEL_ERROR, "Error: Invalid gateway IP address.\n");
    exit(EXIT_CODE_ERROR);
  }

  pnet_ethaddr_t macbuffer;
  int ret = read_mac_address(appdata.arguments.eth_interface, &macbuffer);
  if (ret != 0)
  {
    os_log(LOG_LEVEL_ERROR, "Error: Can not read MAC address for Ethernet interface %s\n", appdata.arguments.eth_interface);
    exit(EXIT_CODE_ERROR);
  }

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGHUP);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);

  signal(SIGSEGV, faulthand);
  signal(SIGBUS, faulthand);

  bool b_station_name_set_by_cmd_line = false;
  bool b_set_ip_suite = false;
  if (appdata.arguments.station_name[0] != '\0')
  {
    b_station_name_set_by_cmd_line = true;
  }

  // create default check peers data before read from disk (os_get_ip_suite())
   pnet_default_cfg.temp_check_peers_data.number_of_peers = 1;
   pnet_default_cfg.temp_check_peers_data.length_peer_port_id = 8;
   strcpy(pnet_default_cfg.temp_check_peers_data.peer_port_id, "port-001");
   pnet_default_cfg.temp_check_peers_data.length_peer_chassis_id = 1;
   strcpy(pnet_default_cfg.temp_check_peers_data.peer_chassis_id, "a");

  if (appdata.arguments.use_ip_settings == IP_SETTINGS_EMPTY)
  {
    ip_int = 0;
    netmask_int = 0;
    gateway_ip_int = 0;
  }
  else
  {
    char name[STATTION_NAME_SIZE];
    os_ipaddr_t ip   = 0;
    os_ipaddr_t mask = 0;
    os_ipaddr_t gw   = 0;
    os_get_ip_suite(&ip, &mask, &gw, name, &pnet_default_cfg);
    if (b_station_name_set_by_cmd_line == false)
    {
      strcpy(appdata.arguments.station_name, name);
    }
    if (appdata.arguments.use_ip_settings == IP_SETTINGS_FILE)
    {
      ip_int         = ip;
      netmask_int    = mask;
      gateway_ip_int = gw;
      b_set_ip_suite = true;
    }
  }

  strcpy(pnet_default_cfg.lldp_cfg.port_id, "port-001");
  strcpy(pnet_default_cfg.lldp_cfg.chassis_id, appdata.arguments.station_name);

  if (open_plc_memory(&appdata) == false)
  {
    os_log(LOG_LEVEL_ERROR, "Error: Shared PLC memory cannot be opened.\n");
    exit(EXIT_CODE_ERROR);
  }

  os_log(LOG_LEVEL_INFO, "Number of slots:    %u (incl slot for DAP module)\n", PNET_MAX_MODULES);
  os_log(LOG_LEVEL_INFO, "Log level:          %u (DEBUG=0, ERROR=3)\n", LOG_LEVEL);
  os_log(LOG_LEVEL_INFO, "Verbosity level:    %u\n", appdata.arguments.verbosity);
  os_log(LOG_LEVEL_INFO, "Ethernet interface: %s\n", appdata.arguments.eth_interface);
  print_ip_address("Linux IP address:   ", appdata.def_ip);
  os_log(LOG_LEVEL_INFO, "Station name:       %s\n", appdata.arguments.station_name);
  os_log(LOG_LEVEL_INFO, "MAC address:        %02x:%02x:%02x:%02x:%02x:%02x\n",
         macbuffer.addr[0],
         macbuffer.addr[1],
         macbuffer.addr[2],
         macbuffer.addr[3],
         macbuffer.addr[4],
         macbuffer.addr[5]);
  print_ip_address("IP address:         ", ip_int);
  print_ip_address("Netmask:            ", netmask_int);
  print_ip_address("Gateway:            ", gateway_ip_int);
  os_log(LOG_LEVEL_INFO, "Adjust peer2peer:   0x%X\n\n", pnet_default_cfg.adjust_peer_to_peer_boundary);
  os_log(LOG_LEVEL_INFO, "Temporary check peers data:\n\tNumberOfPeers %u\n\tLengthPeerPortID %u\n\tPeerPortID %s\n\tLengthPeerChassisID %u\n\tPeerChassisID %s\n\n\n",
         pnet_default_cfg.temp_check_peers_data.number_of_peers,
         pnet_default_cfg.temp_check_peers_data.length_peer_port_id,
         pnet_default_cfg.temp_check_peers_data.peer_port_id,
         pnet_default_cfg.temp_check_peers_data.length_peer_chassis_id,
         pnet_default_cfg.temp_check_peers_data.peer_chassis_id);

  /* Set IP and gateway */
  strcpy(pnet_default_cfg.im_0_data.order_id, "2707");
  strcpy(pnet_default_cfg.im_0_data.im_serial_number, "00001");
  copy_ip_to_struct(&pnet_default_cfg.ip_addr, ip_int);
  copy_ip_to_struct(&pnet_default_cfg.ip_gateway, gateway_ip_int);
  copy_ip_to_struct(&pnet_default_cfg.ip_mask, netmask_int);
  strcpy(pnet_default_cfg.station_name, appdata.arguments.station_name);
  memcpy(pnet_default_cfg.eth_addr.addr, macbuffer.addr, sizeof(pnet_ethaddr_t));
  pnet_default_cfg.cb_arg = (void *)&appdata;

  memset(&thisDevice, 0, sizeof(thisDevice));
  fillDefaultIM0FilterData(&thisDevice);

  os_set_led(&appdata, 0, false);

  if ((ret = mlockall(MCL_CURRENT | MCL_FUTURE)) == -1)
  {
    os_log(LOG_LEVEL_ERROR, "Error: mlockall failed with error %i\n", ret);
    exit(EXIT_CODE_ERROR);
  }

  /* Initialize Profinet stack */
  pnet_t *net = pnet_init(appdata.arguments.eth_interface, TICK_INTERVAL_US, &pnet_default_cfg);

  if (net == NULL)
  {
    os_log(LOG_LEVEL_ERROR, "Failed to initialize p-net. Do you have enough Ethernet interface permission?\n");
    close_plc_memory();
    rpmalloc_finalize();
    exit(EXIT_CODE_ERROR);
  }

  if (b_set_ip_suite)
  {
    os_set_ip_suite(&appdata, __builtin_bswap32(ip_int), netmask_int, gateway_ip_int, false);
  }

  appdata_and_stack.appdata = &appdata;
  appdata_and_stack.net = net;

  app_plug_dap(net, &appdata);

  /* Initialize timer */
  appdata.main_events = os_event_create();
  appdata.main_thread = os_thread_create("pn_main", APP_PRIO, APP_STACKSIZE, pn_main, (void *)&appdata_and_stack);

#if 1
  appdata.main_timer = os_timer_create(TICK_INTERVAL_US, main_timer_tick, (void *)&appdata_and_stack, false);
  os_timer_start(appdata.main_timer);
#else
  appdata.timer_thread = os_thread_create("os_timer", TIMER_PRIO, APP_STACKSIZE, timer_thread_func, (void *)&appdata_and_stack);
#endif // _DEBUG

  // wait for Ctrl-C (SIGTERM) to finish
  int sig = 0;
  __STD(sigwait(&mask, &sig));

  os_event_set(appdata.main_events, EVENT_TERMINATE);  
  appdata.running = false;
  if (appdata.arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "\n[INFO ] finishing...\n");
  }

  os_timer_destroy(appdata.main_timer); 
  close_plc_memory();
  os_set_led(&appdata, 0, true);
  os_exit(&appdata);

  os_log(LOG_LEVEL_INFO, "\n** TGMmini Profinet I/O application finished **\n");

  return 0;
}

