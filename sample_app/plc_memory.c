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
// plc_memory.c
// Shared PLC memory getter and setter
// 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h> /* For mode constants */
#include <sys/mman.h>

#include "plc_memory.h"

#define PLC_MEMORY_NAME "TGM_Data"
#define PLC_MEMORY_SIZE 524288U

// static variables
static int32_t  m_fd =  0;
static uint8_t *m_pBasePtr = NULL;

// implementation

bool open_plc_memory(app_data_t *p_appdata)
{
  const int verbosity = p_appdata->arguments.verbosity;
  bool rv = false;
  m_pBasePtr = NULL;
  m_fd = shm_open(PLC_MEMORY_NAME, O_RDWR, 0666);
  if (m_fd > 0)
  {
    if (ftruncate(m_fd, PLC_MEMORY_SIZE) == 0)
    {
      void *addr;
      /* Map the memory object */
      addr = mmap(0, PLC_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
      if (addr != MAP_FAILED)
      {
        m_pBasePtr = (uint8_t *)(addr);
        if (verbosity > 0)
        {
          printf("Shared memory %s of size %u connected\n", PLC_MEMORY_NAME, PLC_MEMORY_SIZE);
        }
        rv = true;
      }
      else
      {
        printf("[ERROR] mmap(size=%u) of %s failed: %s\n",
               PLC_MEMORY_SIZE,
               PLC_MEMORY_NAME,
               strerror(errno));
      }
    }
    else
    {
      printf("[ERROR] ftruncate(%u) of %s failed: %s\n", PLC_MEMORY_SIZE, PLC_MEMORY_NAME, strerror(errno));
    }
  }
  else
  {
    printf("[ERROR] shm_open(size=%u) of %s failed: %s\n", PLC_MEMORY_SIZE, PLC_MEMORY_NAME, strerror(errno));
  }
  return rv;
}

void close_plc_memory()
{
  if (m_fd > 0)
  {
    close(m_fd);
  }
  m_fd = 0;
  m_pBasePtr = NULL;
}

uint8_t *get_plc_memory_ptr(uint32_t offset, uint32_t size)
{
  if (m_pBasePtr != NULL)
  {
    if (offset + size < PLC_MEMORY_SIZE)
    {
      return m_pBasePtr + offset;
    }
  }
  return NULL;
}

void copy_to_plc_memory(const uint8_t *ptrToTempBuffer, uint32_t offset, uint32_t size)
{
  if (m_pBasePtr != NULL)
  {
    if (offset + size < PLC_MEMORY_SIZE)
    {
      memcpy(m_pBasePtr + offset, ptrToTempBuffer, size);
    }
  }
}
