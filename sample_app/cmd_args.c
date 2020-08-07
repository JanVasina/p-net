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
// cmd_args.c
// command line arguments handling
// 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cmd_args.h"
#include "config.h"

void show_usage()
{
  printf("\nTGMmini Profinet I/O device based on p-net stack.\n");
  printf("\n");
  printf("Wait for connection from IO-controller.\n");
  printf("\n");
  printf("Assumes the default gateway is found on .1 on same subnet as the IP address.\n");
  printf("\n");
  printf("Optional arguments:\n");
  printf("   --help       Show this help text and exit\n");
  printf("   -h           Show this help text and exit\n");
  printf("   -v           Increase verbosity\n");
  printf("   -f           Log the output to file\n");
  printf("   -i INTERF    Set Ethernet interface name. Defaults to %s\n", APP_DEFAULT_ETHERNET_INTERFACE);
  printf("   -s NAME      Set station name. Defaults to %s\n", APP_DEFAULT_STATION_NAME);
  printf("   -e           Use 0.0.0.0 as default IP address\n");
  printf("   -d           Use system IP address as Profinet I/O address\n");
  printf("                (ignore tgm-pnet-ip.dat)\n");
  printf("   -l           Disable LED control\n");

  printf("\n");
}

/**
 * Parse command line arguments
 *
 * @param argc      In: Number of arguments
 * @param argv      In: Arguments
 * @return Parsed arguments
*/
struct cmd_args parse_commandline_arguments(int argc, char *argv[])
{
  // Special handling of long argument
  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0)
    {
      show_usage();
      exit(EXIT_CODE_ERROR);
    }
  }

  // Default values
  struct cmd_args output_arguments;
  output_arguments.station_name[0] = '\0';
  strcpy(output_arguments.eth_interface, APP_DEFAULT_ETHERNET_INTERFACE);
  output_arguments.verbosity = 0;
  output_arguments.use_ip_settings = IP_SETTINGS_FILE;
  output_arguments.use_led = 1;
  log_to_file = 0;

  int option;
  while ((option = getopt(argc, argv, "hedvfli:s:")) != -1)
  {
    switch (option)
    {
    case 'v':
      output_arguments.verbosity++;
      break;
    case 'f':
      log_to_file = 1;
      break;
    case 'e':
      output_arguments.use_ip_settings = IP_SETTINGS_EMPTY;
      break;
    case 'd':
      output_arguments.use_ip_settings = IP_SETTINGS_SYS;
      break;
    case 'l':
      output_arguments.use_led = 0;
      break;
    case 'i':
      strcpy(output_arguments.eth_interface, optarg);
      break;
    case 's':
      strcpy(output_arguments.station_name, optarg);
      break;
    case 'h':
    case '?':
    default:
      show_usage();
      exit(EXIT_CODE_ERROR);
    }
  }

  return output_arguments;
}



