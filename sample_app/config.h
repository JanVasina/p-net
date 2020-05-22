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
// config.h
// all pn-device constants
// 

#ifndef config_h_included
#define config_h_included

#include <stdint.h>
#include "osal.h"
#include "pf_types.h"
#include "log.h"

/********************** Settings **********************************************/

#define IP_INVALID                     0
#define EXIT_CODE_ERROR                1
#define TICK_INTERVAL_US               1000        /* in microseconds */
#define DATA_INTERVAL_US               1000
#define APP_DEFAULT_ETHERNET_INTERFACE "eth0"
#define APP_STACKSIZE                  4096        /* bytes */
#define APP_ALARM_USI                  1
#define APP_MAIN_SLEEPTIME_US          5000*1000

#define IP_SETTINGS_DATA_FILE_NAME     "/TGMotion/system/tgm-pnet-ip.dat"
#define NAME_OF_STATION_DATA_FILE_NAME "/TGMotion/system/tgm-pnet-name.dat"
#define IM_DATA_FILE_NAME              "/TGMotion/system/tgm-pnet-im.dat"


/**************** From the GSDML file ****************************************/

#define APP_DEFAULT_STATION_NAME "tgmmini"

#define I32_PLC_OFFSET_PARAM_IDX_101         101
#define O32_PLC_OFFSET_PARAM_IDX_105         105
#define I32_PLC_OFFSET_DEFAULT_VALUE         1024
#define O32_PLC_OFFSET_DEFAULT_VALUE         5120

#define I64_PLC_OFFSET_PARAM_IDX_102         102
#define O64_PLC_OFFSET_PARAM_IDX_106         106
#define I64_PLC_OFFSET_DEFAULT_VALUE         2048
#define O64_PLC_OFFSET_DEFAULT_VALUE         6144

#define I128_PLC_OFFSET_PARAM_IDX_103        103
#define O128_PLC_OFFSET_PARAM_IDX_107        107
#define I128_PLC_OFFSET_DEFAULT_VALUE        3072
#define O128_PLC_OFFSET_DEFAULT_VALUE        7168

#define I256_PLC_OFFSET_PARAM_IDX_104        104
#define O256_PLC_OFFSET_PARAM_IDX_108        108
#define I256_PLC_OFFSET_DEFAULT_VALUE        4096
#define O256_PLC_OFFSET_DEFAULT_VALUE        8192

#define APP_API                  0

/*
 * Module and submodule ident number for the DAP module.
 * The DAP module and submodules must be plugged by the application after the call to pnet_init.
 */
#define PNET_SLOT_DAP_IDENT                        0x00000000
#define PNET_MOD_DAP_IDENT                         0x00000001     /* For use in slot 0 */
#define PNET_SUBMOD_DAP_IDENT                      0x00000001     /* For use in subslot 1 */
#define PNET_SUBMOD_DAP_INTERFACE_1_IDENT          0x00008000     /* For use in subslot 0x8000 */
#define PNET_SUBMOD_DAP_INTERFACE_1_PORT_0_IDENT   0x00008001     /* For use in subslot 0x8001 */

 /*
  * I/O Modules. These modules and their sub-modules must be plugged by the
  * application after the call to pnet_init.
  *
  * Assume that all modules only have a single submodule, with same number.
  */

#define APP_ALARM_PAYLOAD_SIZE            1     /* bytes */

#define PNET_SUBMOD_CUSTOM_IDENT      0x00000001

#define PNET_MOD_32_I_IDENT            0x00000031 /* 32 bytes I */
#define PNET_MOD_32_I_DATASIZE_INPUT   32                                       
#define PNET_MOD_64_I_IDENT            0x00000032 /* 64 bytes I */
#define PNET_MOD_64_I_DATASIZE_INPUT   64
#define PNET_MOD_128_I_IDENT           0x00000033 /* 128 bytes I/O */
#define PNET_MOD_128_I_DATASIZE_INPUT  128
#define PNET_MOD_256_I_IDENT           0x00000034 /* 256 bytes I/O */
#define PNET_MOD_256_I_DATASIZE_INPUT  256

#define PNET_MOD_32_O_IDENT            0x00000035 /* 32 bytes O */
#define PNET_MOD_32_O_DATASIZE_OUTPUT  32
#define PNET_MOD_64_O_IDENT            0x00000036 /* 64 bytes O */
#define PNET_MOD_64_O_DATASIZE_OUTPUT  64
#define PNET_MOD_128_O_IDENT           0x00000037 /* 128 bytes I/O */
#define PNET_MOD_128_O_DATASIZE_OUTPUT 128
#define PNET_MOD_256_O_IDENT           0x00000038 /* 256 bytes I/O */
#define PNET_MOD_256_O_DATASIZE_OUTPUT 256

  // 1x DAP, 2x 32 bytes (I + O), 2x 64bytes I+O, 2x 128bytes I+O, 2x 256bytes I+O
#define NUMBER_OF_DEFINED_MODULES      9 
  // 3x DAP, 2x 32 bytes (I + O), 2x 64bytes I+O, 2x 128bytes I+O, 2x 256bytes I+O
#define NUMBER_OF_DEFINED_SUBMODULES   (NUMBER_OF_DEFINED_MODULES+2) 

#define PNET_MAX_MODULE_DATA_SIZE   512 // maximal allowed data size for I/O data

typedef struct SLOT_TYPE
{
  uint32_t module_id;
  uint16_t size;     // bytes
  uint16_t reserved; // padding
  uint32_t param_id;    // parameter ID
  uint32_t data_offset; // offset to PLC memory
} slot_t;

typedef struct MODULE_TYPE
{
  uint32_t module_id;
  uint16_t insize;                 // bytes
  uint16_t outsize;                // bytes
  pnet_submodule_dir_t data_dir;   // data direction to fill SLOT_TYPES
  uint32_t in_offset_param_id;     // parameter ID of the input offset
  uint32_t in_def_data_offset;     // input data: default offset to PLC memory
  uint32_t out_offset_param_id;    // parameter ID of the output offset
  uint32_t out_def_data_offset;    // output data: default offset to PLC memory
} module_t;

typedef struct SUBMODULE_TYPE
{
  uint32_t                api;
  uint32_t                module_ident_nbr;
  uint32_t                submodule_ident_nbr;
  pnet_submodule_dir_t    data_dir;
  uint16_t                insize;     // bytes
  uint16_t                outsize;    // bytes
} submodule_t;

#define IP_SETTINGS_FILE  0
#define IP_SETTINGS_EMPTY 1
#define IP_SETTINGS_SYS   2

typedef struct cmd_args
{
  char station_name[STATTION_NAME_SIZE];
  char eth_interface[64];
  int  verbosity;
  int  use_ip_settings;
  int  use_led;
} cmd_args_t;

typedef struct app_data_obj
{
  os_timer_t *main_timer;
  os_thread_t *timer_thread;
  os_thread_t *main_thread;
  os_event_t *main_events;
  uint32_t    main_arep;
  uint32_t    parameterValueInBigEndian; // temporary value in Profinet big endian format
  bool        alarm_allowed;
  bool        init_done;
  bool        b_disp_wrong_offset_warning;
  cmd_args_t  arguments;
  uint32_t    def_ip;
  slot_t      custom_input_slots[PNET_MAX_MODULES];
  slot_t      custom_output_slots[PNET_MAX_MODULES];
  int         i2c_file;
  volatile bool running;
} app_data_t;

typedef struct app_data_and_stack_obj
{
  app_data_t *appdata;
  pnet_t     *net;
} app_data_and_stack_t;

#endif // config_h_included


