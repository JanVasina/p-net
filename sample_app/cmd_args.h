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
// cmd_args.h
// command arguments handling header file
// 

#ifndef cmd_args_h_included
#define cmd_args_h_included

#include "config.h"

struct cmd_args parse_commandline_arguments(int argc, char *argv[]);

#endif //  cmd_args_h_included


