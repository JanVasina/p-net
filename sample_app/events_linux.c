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
// events_linux.c
// supplement functions for main_linux.c

#include <stdio.h>
#include <string.h>

#include "events_linux.h"
#include "pnet_api.h"
#include "config.h"
#include "plc_memory.h"
#include "utils.h"

// global functions
void handleReadyForDataEvent(pnet_t *net, app_data_t *p_appdata)
{
  // Send appl ready to profinet stack.
  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO,
           "Application will signal that it is ready for data, arep 0x%X.\n", 
           p_appdata->main_arep);
  }

  pnet_pnio_status_t status = { 0, 0, 0, 0 };

  const int ret = pnet_application_ready(net, p_appdata->main_arep);
  if(ret != 0)
  {
    if (p_appdata->arguments.verbosity > 0)
    {
      os_log(LOG_LEVEL_ERROR,
             "Error returned when application telling that it is ready for data. Have you set IOCS or IOPS for all subslots?\n");
    }
    status.error_code   = 0x20; // application specific
    status.error_code_1 = ret;
    status.error_code_1 = 1;    // to be changed
  }

  // create entry in logbook
  pnet_create_log_book_entry(net, p_appdata->main_arep, &status, 0U);

  /*
   * cm_ccontrol_cnf(+/-) is indicated later (app_state_ind(DATA)), when the
   * confirmation arrives from the controller.
   */
}

void handleAlarmEvent(pnet_t *net, app_data_t *p_appdata)
{
  pnet_pnio_status_t pnio_status = { 0,0,0,0 };
  const int ret = pnet_alarm_send_ack(net, p_appdata->main_arep, &pnio_status);
  if (ret != 0)
  {
    printf("Error when sending alarm ACK. Error: %d\n", ret);
  }
  else if (p_appdata->arguments.verbosity > 0)
  {
    printf("Alarm ACK sent\n");
  }
}

/* Set initial data and IOPS for custom input modules, and IOCS for custom output modules */
void setupPluggedModules(pnet_t *net, app_data_t *p_appdata)
{
  const int verbosity = p_appdata->arguments.verbosity;
  for (uint16_t slot = 0; slot < PNET_MAX_MODULES; slot++)
  {
    slot_t *pInputSlot = &(p_appdata->custom_input_slots[slot]);
    const uint16_t input_size = pInputSlot->size;
    if ((pInputSlot->module_id != 0U) && (input_size > 0U))
    {
      if (verbosity > 0)
      {
        printf("  Setting input data and IOPS for slot %u subslot %u module ID 0x%X\n",
               slot,
               PNET_SUBMOD_CUSTOM_IDENT,
               pInputSlot->module_id);
      }
      uint8_t *ptr_to_plc_memory = get_plc_memory_ptr(pInputSlot->data_offset, input_size);
      if (ptr_to_plc_memory != NULL)
      {
        (void)pnet_input_set_data_and_iops(net,
                                           APP_API,
                                           slot,
                                           PNET_SUBMOD_CUSTOM_IDENT,
                                           ptr_to_plc_memory,
                                           input_size,
                                           PNET_IOXS_GOOD);
      }
      else
      {   
        if(p_appdata->b_disp_wrong_offset_warning != false)
        {
          printf("setupPluggedModules: Wrong data offset (%u) or size (%u) for PLC memory of module ID 0x%X\n",
                 pInputSlot->data_offset,
                 (uint32_t)input_size,
                 pInputSlot->module_id);
        }
        p_appdata->b_disp_wrong_offset_warning = false;
      }
    }

    slot_t *pOutputSlot = &(p_appdata->custom_output_slots[slot]);
    if (pOutputSlot->module_id != 0)
    {
      if (verbosity > 0)
      {
        printf("  Setting output IOCS for slot %u subslot %u module ID 0x%X\n",
               slot,
               PNET_SUBMOD_CUSTOM_IDENT,
               pOutputSlot->module_id);
      }
      (void)pnet_output_set_iocs(net,
                                 APP_API,
                                 slot,
                                 PNET_SUBMOD_CUSTOM_IDENT,
                                 PNET_IOXS_GOOD);
    }
  }
}

//////////////////////////////////////////////////////////////////////////
// copy data from input arrays to profinet slots

void setInputDataToController(pnet_t *net, app_data_t *p_appdata)
{
  for (uint16_t slot = 0; slot < PNET_MAX_MODULES; slot++)
  {
    slot_t *pInputSlot = &(p_appdata->custom_input_slots[slot]);
    const uint32_t module_id = pInputSlot->module_id;
    if (module_id != 0)
    {
      const uint16_t size = pInputSlot->size;
      uint8_t *ptr_to_plc_memory = get_plc_memory_ptr(pInputSlot->data_offset, size);
      if(ptr_to_plc_memory != NULL)
      {
        (void)pnet_input_set_data_and_iops(net,
                                           APP_API,
                                           slot,
                                           PNET_SUBMOD_CUSTOM_IDENT,
                                           ptr_to_plc_memory,
                                           size,
                                           PNET_IOXS_GOOD);
      }
      else
      {
        if (p_appdata->b_disp_wrong_offset_warning != false)
        {
          printf("setInputDataToController: Wrong data offset (%u) or size (%u) for PLC memory of module ID 0x%X\n",
                 pInputSlot->data_offset,
                 (uint32_t)size,
                 module_id);
        }
        p_appdata->b_disp_wrong_offset_warning = false;
      }
    }
  }
}

#if 1
// zero copy variant
void getOutputDataFromController(pnet_t *net, app_data_t *p_appdata)
{
  for (size_t slot = 0; slot < PNET_MAX_MODULES; slot++)
  {
    slot_t *pOutputSlot = &(p_appdata->custom_output_slots[slot]);
    const uint32_t module_id = pOutputSlot->module_id;
    if (module_id != 0)
    {
      const uint16_t dataSize = pOutputSlot->size;
      uint8_t        outputdata_iops = 0;
      uint16_t       outputdata_length = dataSize;
      bool           is_updated = false;
      uint8_t       *ptr_to_plc_memory = get_plc_memory_ptr(pOutputSlot->data_offset, dataSize);
      (void)pnet_output_get_data_and_iops(net,
                                          APP_API,
                                          slot,
                                          PNET_SUBMOD_CUSTOM_IDENT,
                                          &is_updated,
                                          ptr_to_plc_memory,
                                          &outputdata_length,
                                          &outputdata_iops);
    }
  }
}

#else
// temporary buffer variant
void getOutputDataFromController(pnet_t *net, app_data_t *p_appdata)
{
  bool    outputdata_is_updated = false;
  uint8_t tempData[PNET_MAX_MODULE_DATA_SIZE];
  for (size_t slot = 0; slot < PNET_MAX_MODULES; slot++)
  {
    slot_t *pOutputSlot = &(p_appdata->custom_output_slots[slot]);
    const uint32_t module_id = pOutputSlot->module_id;
    if (module_id != 0)
    {
      const uint16_t dataSize = pOutputSlot->size;
      uint8_t        outputdata_iops = 0;
      uint16_t       outputdata_length = sizeof(tempData);
      bool           is_updated = false;
      (void)pnet_output_get_data_and_iops(net,
                                          APP_API,
                                          slot,
                                          PNET_SUBMOD_CUSTOM_IDENT,
                                          &is_updated,
                                          tempData,
                                          &outputdata_length,
                                          &outputdata_iops);
      if (is_updated)
      {
        outputdata_is_updated = true;
      }
      if ((outputdata_is_updated == true)
          && (outputdata_iops == PNET_IOXS_GOOD)
          && (outputdata_length <= dataSize))
      {
        // data ok: copy output data to shared memory
        // offset and size validation is done in copy_to_plc_memory()
        copy_to_plc_memory(tempData, pOutputSlot->data_offset, outputdata_length);
      }
    }
  }
}
#endif

//////////////////////////////////////////////////////////////////////////
// user parameters support
int writeUserParameter(uint16_t       slot,
                       uint16_t       subslot,
                       uint16_t       write_length,
                       uint8_t       *p_write_data,
                       app_data_t    *p_appdata,
                       uint16_t       idx,
                       pnet_result_t *p_result)
{
  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "Write call-back. Slot: %u Subslot: %u Index: %u Length: %u\n",
           slot,
           subslot,
           (unsigned)idx,
           write_length);
  }

  bool bParameterIsOK = false;
  if ((slot < PNET_MAX_MODULES) && (write_length == sizeof(uint32_t)))
  {
    const uint32_t data = *(uint32_t *)(p_write_data);
    const uint32_t swapped_data = __builtin_bswap32(data);
    slot_t *pInputSlot = &(p_appdata->custom_input_slots[slot]);
    slot_t *pOutputSlot = &(p_appdata->custom_output_slots[slot]);
    if (pInputSlot->param_id == idx)
    {
      pInputSlot->data_offset = swapped_data;
      bParameterIsOK = true;
    }
    else if (pOutputSlot->param_id == idx)
    {
      pOutputSlot->data_offset = swapped_data;
      bParameterIsOK = true;
    }
    if (bParameterIsOK == false)
    {
      os_log(LOG_LEVEL_WARNING, "Wrong index in write call-back: %u\n", (unsigned)idx);
    }
    else if (p_appdata->arguments.verbosity > 0)
    {
      os_log(LOG_LEVEL_INFO, "Value: %u (0x%X)\n", swapped_data, swapped_data);
    }
    else
    {
      ; // NOP
    }
  }
  else
  {
    os_log(LOG_LEVEL_WARNING, "Wrong length in write call-back. Index: %u Length: %u Expected length: %u\n",
           (unsigned)idx,
           (unsigned)write_length,
           (unsigned)sizeof(uint32_t));
  }

  if (bParameterIsOK)
  {
    p_result->pnio_status.error_code   = 0;
    p_result->pnio_status.error_decode = 0;
    p_result->pnio_status.error_code_1 = 0;
    p_result->pnio_status.error_code_2 = 0;
    return 0;
  }

  p_result->pnio_status.error_code = PNET_ERROR_CODE_PNIO;
  p_result->pnio_status.error_decode = PNET_ERROR_DECODE_PNIORW;
  p_result->pnio_status.error_code_1 = PNET_ERROR_CODE_1_ACC_INVALID_PARAMETER;
  p_result->pnio_status.error_code_2 = 0;
  return -1;
}

int readUserParameter(uint16_t       slot, 
                      uint16_t       subslot,
                      uint16_t       idx, 
                      uint16_t      *p_read_length, 
                      uint8_t      **pp_read_data, 
                      app_data_t    *p_appdata, 
                      pnet_result_t *p_result)
{
  if (p_appdata->arguments.verbosity > 0)
  {
    os_log(LOG_LEVEL_INFO, "Read call-back. Slot: %u Subslot: %u Index: %u Max length: %u\n",
           slot,
           subslot,
           (unsigned)idx,
           (unsigned)*p_read_length);
  }

  bool bParameterIsOK = false;
  if (slot < PNET_MAX_MODULES && *p_read_length >= sizeof(uint32_t))
  {
    const slot_t *pInputSlot = &(p_appdata->custom_input_slots[slot]);
    const slot_t *pOutputSlot = &(p_appdata->custom_output_slots[slot]);
    if (pInputSlot->param_id == idx)
    {
      p_appdata->parameterValueInBigEndian = __builtin_bswap32(pInputSlot->data_offset);
      bParameterIsOK = true;
    }
    else if (pOutputSlot->param_id == idx)
    {
      p_appdata->parameterValueInBigEndian = __builtin_bswap32(pOutputSlot->data_offset);
      bParameterIsOK = true;
    }
    else
    {
      ; // NOP
    }
    *pp_read_data = (uint8_t *)&(p_appdata->parameterValueInBigEndian);
    *p_read_length = sizeof(uint32_t);
    if ((p_appdata->arguments.verbosity > 0) && (bParameterIsOK != false))
    {
      print_bytes(*pp_read_data, *p_read_length);
    }
  }

  if (bParameterIsOK)
  {
    p_result->pnio_status.error_code   = 0;
    p_result->pnio_status.error_decode = 0;
    p_result->pnio_status.error_code_1 = 0;
    p_result->pnio_status.error_code_2 = 0;
    return 0;
  }

  p_result->pnio_status.error_code   = PNET_ERROR_CODE_PNIO;
  p_result->pnio_status.error_decode = PNET_ERROR_DECODE_PNIORW;
  p_result->pnio_status.error_code_1 = PNET_ERROR_CODE_1_ACC_INVALID_PARAMETER;
  p_result->pnio_status.error_code_2 = 0;
  return -1;
}
