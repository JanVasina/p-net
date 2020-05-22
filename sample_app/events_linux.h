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
// events_linux.h
// events functions protoypes

#ifndef events_linux_h_included
#define events_linux_h_included

#include <stdbool.h>
#include "osal.h"
#include "pnet_api.h"
#include "config.h"

#define EVENT_READY_FOR_DATA     BIT(0)
#define EVENT_TIMER              BIT(1)
#define EVENT_ALARM              BIT(2)
#define EVENT_ABORT              BIT(15)
#define EVENT_TERMINATE          BIT(31)

// function prototypes
void handleReadyForDataEvent(pnet_t *net, app_data_t *p_appdata);
void handleAlarmEvent(pnet_t *net, app_data_t *p_appdata);
void setupPluggedModules(pnet_t *net, app_data_t *p_appdata);
void setInputDataToController(pnet_t *net, app_data_t *p_appdata);
void getOutputDataFromController(pnet_t *net, app_data_t *p_appdata);
int  writeUserParameter(uint16_t slot,
                        uint16_t       subslot,
                        uint16_t write_length,
                        uint8_t * p_write_data,
                        app_data_t * p_appdata,
                        uint16_t idx,
                        pnet_result_t *p_result);
int readUserParameter(uint16_t       slot,
                      uint16_t       subslot,
                      uint16_t       idx,
                      uint16_t      *p_read_length,
                      uint8_t      **pp_read_data,
                      app_data_t    *p_appdata,
                      pnet_result_t *p_result);


#endif //  events_linux_h_included


