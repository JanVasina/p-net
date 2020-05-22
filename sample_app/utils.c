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
// utils.c
// utility functions
// 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>

#include "utils.h"


/**
 * Check if network interface exists
 *
 * @param name      In: Name of network interface
 * @return true if interface exists
*/
bool does_network_interface_exist(const char *name)
{
  uint32_t index = if_nametoindex(name);

  return index != 0;
}

/**
 * Print contents of a buffer
 *
 * @param bytes      In: input buffer
 * @param len        In: Number of bytes to print
*/
void print_bytes(uint8_t *bytes, uint32_t len)
{
  bool bPrintCRLF = true;
  printf("  Bytes:\n");
  for (uint32_t i = 0; i < len; i++)
  {
    if ((i & 0xF) == 0)
    {
      printf("%X02:", i);
    }
    printf(" %02X", bytes[i]);
    if (((i + 1) & 0xF) == 0)
    {
      printf("\n");
      bPrintCRLF = false;
    }
    else
    {
      bPrintCRLF = true;
    }
  }
  if(bPrintCRLF)
  {
    printf("\n");
  }
}

/**
 * Copy an IP address (as an integer) to a struct
 *
 * @param destination_struct  Out: destination
 * @param ip                  In: IP address
*/
void copy_ip_to_struct(pnet_cfg_ip_addr_t *destination_struct, uint32_t ip)
{
  destination_struct->a = (ip & 0xFF);
  destination_struct->b = ((ip >> 8) & 0xFF);
  destination_struct->c = ((ip >> 16) & 0xFF);
  destination_struct->d = ((ip >> 24) & 0xFF);
}

/**
 * Print an IPv4 address (without newline)
 *
 * @param ip      In: IP address
*/
void print_ip_address(const char *prefix, uint32_t ip)
{
  printf("%s%d.%d.%d.%d\n",
         prefix,
         (ip & 0xFF),
         ((ip >> 8) & 0xFF),
         ((ip >> 16) & 0xFF),
         ((ip >> 24) & 0xFF)
  );
}

/**
 * Print an IPv4 address to buffer
 *
 * @param ip      In: IP address
*/
void sprint_ip_address(char *buffer, size_t buffer_size, uint32_t ip)
{
  snprintf(buffer,
           buffer_size,
           "%d.%d.%d.%d",
           (ip & 0xFF),
           ((ip >> 8) & 0xFF),
           ((ip >> 16) & 0xFF),
           ((ip >> 24) & 0xFF)
  );
}

/**
 * Read the IP address as an integer. For IPv4.
 *
 * @param interface_name      In: Name of network interface
 * @return IP address on success and
 *         0 if an error occurred
*/
uint32_t read_ip_address(char *interface_name)
{
  int fd;
  struct ifreq ifr;
  uint32_t ip;

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
  ioctl(fd, SIOCGIFADDR, &ifr);
  ip = ((struct sockaddr_in *) & ifr.ifr_addr)->sin_addr.s_addr;
  close(fd);
  return ip;
}

/**
 * Read the MAC address.
 *
 * @param interface_name      In: Name of network interface
 * @param mac_addr            Out: MAC address
 *
 * @return 0 on success and
 *         -1 if an error occurred
*/
int read_mac_address(char *interface_name, pnet_ethaddr_t *mac_addr)
{
  int fd;
  int ret = 0;
  struct ifreq ifr;

  fd = socket(AF_INET, SOCK_DGRAM, 0);

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);

  ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
  if (ret == 0)
  {
    memcpy(mac_addr->addr, ifr.ifr_hwaddr.sa_data, 6);
  }
  close(fd);
  return ret;
}

/**
 * Read the netmask as an integer. For IPv4.
 *
 * @param interface_name      In: Name of network interface
 * @return netmask
*/
uint32_t read_netmask(char *interface_name)
{

  int fd;
  struct ifreq ifr;
  uint32_t netmask;

  fd = socket(AF_INET, SOCK_DGRAM, 0);

  ifr.ifr_addr.sa_family = AF_INET;
  strncpy(ifr.ifr_name, interface_name, IFNAMSIZ - 1);
  ioctl(fd, SIOCGIFNETMASK, &ifr);
  netmask = ((struct sockaddr_in *) & ifr.ifr_addr)->sin_addr.s_addr;
  close(fd);

  return netmask;
}

/**
 * Read the default gateway address as an integer. For IPv4.
 *
 * Assumes the default gateway is found on .1 on same subnet as the IP address.
 *
 * @param interface_name      In: Name of network interface
 * @return netmask
*/
uint32_t read_default_gateway(char *interface_name)
{
  // TODO Read the actual default gateway (somewhat complicated)

  uint32_t ip;
  uint32_t gateway;

  ip = read_ip_address(interface_name);
  gateway = (ip & 0x00FFFFFF) | 0x01000000;

  return gateway;
}

uint64_t timeGetTime64_ns()
{
  struct timespec Time1;
  clock_gettime(CLOCK_MONOTONIC, &Time1);
  return ((uint64_t)(Time1.tv_sec) * 1000000000ULL) + ((uint64_t)(Time1.tv_nsec));
}
