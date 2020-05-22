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
// utils.h
// utility functions
// 

#ifndef utils_h_included
#define utils_h_included

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <net/if.h>
#include "pnet_api.h"
#include "config.h"

// function prototypes
bool does_network_interface_exist(const char *name);
uint32_t read_default_gateway(char *interface_name);
uint32_t read_netmask(char *interface_name);
int read_mac_address(char *interface_name, pnet_ethaddr_t *mac_addr);
uint32_t read_ip_address(char *interface_name);
void print_ip_address(const char *prefix, uint32_t ip);
void sprint_ip_address(char *buffer, size_t buffer_size, uint32_t ip);
void copy_ip_to_struct(pnet_cfg_ip_addr_t *destination_struct, uint32_t ip);
void print_bytes(uint8_t *bytes, uint32_t len);
uint64_t timeGetTime64_ns();


#endif // utils_h_included

