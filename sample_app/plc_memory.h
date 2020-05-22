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
// plc_memory.h
// PLC memory function prototypes

#ifndef plc_memory_h_included
#define plc_memory_h_included

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

bool     open_plc_memory(app_data_t *p_appdata);
void     close_plc_memory();
uint8_t *get_plc_memory_ptr(uint32_t offset, uint32_t size);
void     copy_to_plc_memory(const uint8_t *ptrToTempBuffer, uint32_t offset, uint32_t size);

#endif


