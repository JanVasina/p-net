# /****************************************************************************
# **
# ** Copyright (C) 2020 TGDrives, s.r.o.
# ** https://www.tgdrives.cz
# **
# ** This file is part of the TGMmini Profinet I/O device.
# **
# **
# **  TGMmini Profinet I/O device free software: 
# **  you can redistribute it and/or modify it under the terms of the 
# **  GNU General Public License as published by the Free Software Foundation, 
# **  either version 3 of the License, or (at your option) any later version.
# **
# **  TGMmini Profinet I/O device is distributed in the hope that it will be useful,
# **  but WITHOUT ANY WARRANTY; without even the implied warranty of 
# **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
# **  GNU General Public License for more details.
# **
# **  You should have received a copy of the GNU General Public License along with 
# **  TGMmini Profinet I/O device. If not, see <https://www.gnu.org/licenses/>.
# **
# ****************************************************************************/
# simple makefile
# can be used for native compiling on TGMmini (Linux) or for cross-compiling on Windows
# set the variables depending on host OS

PROJECT_NAME=tgm-pnet

ifeq ($(OS),Windows_NT)
	SRC_PATH=Z:/ProfiNET/OpenSource/work
	SYS_LIBS_PATH=Z:/TGDev/update-sysroot/sysroot
	CC=Z:/TGDev/PLC/projects/gnu/gcc-arm-linux-gnueabi/bin/arm-linux-gnueabihf-gcc.exe
	OUT_PATH=Z:/TGDev/TGMmini_cross_compile/TGMotion/system
	BUILD_PATH=Z:/ProfiNET/OpenSource/tmp
	POST_BUILD_CMD=Z:/SysGCC/WinSCP/pnet.bat
	OBJDUMP=Z:/TGDev/PLC/projects/gnu/gcc-arm-linux-gnueabi/bin/arm-linux-gnueabihf-objdump.exe
	OBJDUMP_CMD_PREFIX=cmd /C
else
	CC=gcc
	OBJDUMP=objdump.exe
	OBJDUMP_CMD_PREFIX=
	CC_PREFIX=-mcpu=cortex-a9
	LD_PREFIX=-mcpu=cortex-a9
	SRC_PATH=/home/shareman/p-net-work
	SYS_LIBS_PATH=
	OUT_PATH=/TGMotion/system
	BUILD_PATH=/home/shareman/tmp
	POST_BUILD_CMD=
endif

OUT_FILE_NAME=$(OUT_PATH)/$(PROJECT_NAME)
EXECUTABLE=$(OUT_FILE_NAME)

DUMP_FILE=$(SRC_PATH)/../$(PROJECT_NAME).dbg
OBJDUMP_CMD=$(OBJDUMP_CMD_PREFIX) $(OBJDUMP) -S -g $(EXECUTABLE) >$(DUMP_FILE)

# this value can be overriden by command line: make DEBUG=1
DEBUG=0
DEBUG_INFO=

ifeq ($(DEBUG), 1)
	OPTIMIZE=-Og -DDEBUG -D_DEBUG
	DEBUG_INFO=-g
else
	OPTIMIZE=-O2
	DEBUG_INFO=
endif

SOURCES=$(SRC_PATH)/sample_app/main_linux.c \
		$(SRC_PATH)/sample_app/events_linux.c \
		$(SRC_PATH)/sample_app/cmd_args.c \
		$(SRC_PATH)/sample_app/utils.c \
		$(SRC_PATH)/sample_app/plc_memory.c \
		$(SRC_PATH)/src/common/pf_alarm.c \
		$(SRC_PATH)/src/common/pf_cpm.c \
		$(SRC_PATH)/src/common/pf_dcp.c \
		$(SRC_PATH)/src/common/pf_eth.c \
		$(SRC_PATH)/src/common/pf_lldp.c \
		$(SRC_PATH)/src/common/pf_ppm.c \
		$(SRC_PATH)/src/common/pf_ptcp.c \
		$(SRC_PATH)/src/common/pf_scheduler.c \
		$(SRC_PATH)/src/device/pf_block_reader.c \
		$(SRC_PATH)/src/device/pf_block_writer.c \
		$(SRC_PATH)/src/device/pf_cmdev.c \
		$(SRC_PATH)/src/device/pf_cmdmc.c \
		$(SRC_PATH)/src/device/pf_cmina.c \
		$(SRC_PATH)/src/device/pf_cmio.c \
		$(SRC_PATH)/src/device/pf_cmpbe.c \
		$(SRC_PATH)/src/device/pf_cmrdr.c \
		$(SRC_PATH)/src/device/pf_cmrpc.c \
		$(SRC_PATH)/src/device/pf_cmrs.c \
		$(SRC_PATH)/src/device/pf_cmsm.c \
		$(SRC_PATH)/src/device/pf_cmsu.c \
		$(SRC_PATH)/src/device/pf_cmwrr.c \
		$(SRC_PATH)/src/device/pf_diag.c \
		$(SRC_PATH)/src/device/pf_fspm.c \
		$(SRC_PATH)/src/device/pnet_api.c \
		$(SRC_PATH)/src/rpmalloc/rpmalloc.c \
		$(SRC_PATH)/src/osal/linux/osal.c \
		$(SRC_PATH)/src/osal/linux/osal_eth.c \
		$(SRC_PATH)/src/osal/linux/osal_udp.c \
		$(SRC_PATH)/src/osal/linux/i2c_led.c \

HEADERS=$(SRC_PATH)/include/pnet_api.h \
		$(SRC_PATH)/sample_app/events_linux.h \
		$(SRC_PATH)/sample_app/config.h \
		$(SRC_PATH)/sample_app/utils.h \
		$(SRC_PATH)/sample_app/cmd_args.h \
		$(SRC_PATH)/sample_app/plc_memory.h \
		$(SRC_PATH)/src/common/pf_alarm.h \
		$(SRC_PATH)/src/common/pf_cpm.h \
		$(SRC_PATH)/src/common/pf_dcp.h \
		$(SRC_PATH)/src/common/pf_eth.h \
		$(SRC_PATH)/src/common/pf_lldp.h \
		$(SRC_PATH)/src/common/pf_ppm.h \
		$(SRC_PATH)/src/common/pf_ptcp.h \
		$(SRC_PATH)/src/common/pf_scheduler.h \
		$(SRC_PATH)/src/device/pf_block_reader.h \
		$(SRC_PATH)/src/device/pf_block_writer.h \
		$(SRC_PATH)/src/device/pf_cmdev.h \
		$(SRC_PATH)/src/device/pf_cmdmc.h \
		$(SRC_PATH)/src/device/pf_cmina.h \
		$(SRC_PATH)/src/device/pf_cmio.h \
		$(SRC_PATH)/src/device/pf_cmpbe.h \
		$(SRC_PATH)/src/device/pf_cmrdr.h \
		$(SRC_PATH)/src/device/pf_cmrpc.h \
		$(SRC_PATH)/src/device/pf_cmrs.h \
		$(SRC_PATH)/src/device/pf_cmsm.h \
		$(SRC_PATH)/src/device/pf_cmsu.h \
		$(SRC_PATH)/src/device/pf_cmwrr.h \
		$(SRC_PATH)/src/device/pf_diag.h \
		$(SRC_PATH)/src/device/pf_fspm.h \
		$(SRC_PATH)/src/log.h \
		$(SRC_PATH)/src/osal.h \
		$(SRC_PATH)/src/pf_includes.h \
		$(SRC_PATH)/src/pf_types.h \
		$(SRC_PATH)/src/osal/linux/cc.h \
		$(SRC_PATH)/src/osal/linux/osal_sys.h \
		$(SRC_PATH)/src/osal/linux/i2c_led.h \
		$(SRC_PATH)/src/rpmalloc/rpmalloc.h \
		$(SRC_PATH)/build/src/options.h \
		$(SRC_PATH)/build/src/version.h \
		$(SRC_PATH)/build/include/pnet_export.h \
		$(SRC_PATH)/makefile \

		
USER_PATH=-I$(SRC_PATH)/include \
		  -I$(SRC_PATH)/src \
		  -I$(SRC_PATH)/src/common \
		  -I$(SRC_PATH)/src/device \
		  -I$(SRC_PATH)/src/rpmalloc \
		  -I$(SRC_PATH)/src/osal/linux \
		  -I$(SRC_PATH)/build/include \
		  -I$(SRC_PATH)/build/src \
		  -I$(SRC_PATH)/sample_app \

DEFINES=-D__linux__ -DUSE_SCHED_FIFO

BSP_PATH=-L$(SYS_LIBS_PATH)/usr/lib \
         -L$(SYS_LIBS_PATH)/lib/arm-linux-gnueabihf \
         -L$(SYS_LIBS_PATH)/usr/lib/arm-linux-gnueabihf \

CFLAGS=$(OPTIMIZE) $(DEBUG_INFO) $(USER_PATH) \
		-mfpu=vfp3 \
		-D_GNU_SOURCE \
		-D_REENTRANT -fPIC $(DEFINES) \
		-pedantic -W -Wall -Wextra -Wno-unused-parameter

LIBS=-lpthread -lrt

LDFLAGS=$(DEBUG_INFO) $(BSP_PATH) \
		-mfpu=vfpv3 -mfloat-abi=hard $(LIBS) \
		-mlittle-endian


OBJECTS=$(BUILD_PATH)/main_linux.o \
		$(BUILD_PATH)/events_linux.o \
		$(BUILD_PATH)/cmd_args.o \
		$(BUILD_PATH)/utils.o \
		$(BUILD_PATH)/plc_memory.o \
		$(BUILD_PATH)/i2c_led.o \
		$(BUILD_PATH)/pf_alarm.o \
		$(BUILD_PATH)/pf_cpm.o \
		$(BUILD_PATH)/pf_dcp.o \
		$(BUILD_PATH)/pf_eth.o \
		$(BUILD_PATH)/pf_lldp.o \
		$(BUILD_PATH)/pf_ppm.o \
		$(BUILD_PATH)/pf_ptcp.o \
		$(BUILD_PATH)/pf_scheduler.o \
		$(BUILD_PATH)/pf_block_reader.o \
		$(BUILD_PATH)/pf_block_writer.o \
		$(BUILD_PATH)/pf_cmdev.o \
		$(BUILD_PATH)/pf_cmdmc.o \
		$(BUILD_PATH)/pf_cmina.o \
		$(BUILD_PATH)/pf_cmio.o \
		$(BUILD_PATH)/pf_cmpbe.o \
		$(BUILD_PATH)/pf_cmrdr.o \
		$(BUILD_PATH)/pf_cmrpc.o \
		$(BUILD_PATH)/pf_cmrs.o \
		$(BUILD_PATH)/pf_cmsm.o \
		$(BUILD_PATH)/pf_cmsu.o \
		$(BUILD_PATH)/pf_cmwrr.o \
		$(BUILD_PATH)/pf_diag.o \
		$(BUILD_PATH)/pf_fspm.o \
		$(BUILD_PATH)/pnet_api.o \
		$(BUILD_PATH)/osal.o \
		$(BUILD_PATH)/osal_eth.o \
		$(BUILD_PATH)/osal_udp.o \
		$(BUILD_PATH)/rpmalloc.o \
			
all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) $(SOURCES) $(HEADERS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	$(POST_BUILD_CMD)
#	$(OBJDUMP_CMD)

$(BUILD_PATH)/main_linux.o: $(SRC_PATH)/sample_app/main_linux.c $(HEADERS)
	$(CC) $(SRC_PATH)/sample_app/main_linux.c -c $(CFLAGS) -o $(BUILD_PATH)/main_linux.o

$(BUILD_PATH)/events_linux.o: $(SRC_PATH)/sample_app/events_linux.c $(HEADERS)
	$(CC) $(SRC_PATH)/sample_app/events_linux.c -c $(CFLAGS) -o $(BUILD_PATH)/events_linux.o

$(BUILD_PATH)/cmd_args.o: $(SRC_PATH)/sample_app/cmd_args.c $(HEADERS)
	$(CC) $(SRC_PATH)/sample_app/cmd_args.c -c $(CFLAGS) -o $(BUILD_PATH)/cmd_args.o

$(BUILD_PATH)/utils.o: $(SRC_PATH)/sample_app/utils.c $(HEADERS)
	$(CC) $(SRC_PATH)/sample_app/utils.c -c $(CFLAGS) -o $(BUILD_PATH)/utils.o

$(BUILD_PATH)/plc_memory.o: $(SRC_PATH)/sample_app/plc_memory.c $(HEADERS)
	$(CC) $(SRC_PATH)/sample_app/plc_memory.c -c $(CFLAGS) -o $(BUILD_PATH)/plc_memory.o

$(BUILD_PATH)/i2c_led.o: $(SRC_PATH)/src/osal/linux/i2c_led.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/osal/linux/i2c_led.c -c $(CFLAGS) -o $(BUILD_PATH)/i2c_led.o

$(BUILD_PATH)/pf_alarm.o: $(SRC_PATH)/src/common/pf_alarm.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/common/pf_alarm.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_alarm.o

$(BUILD_PATH)/pf_cpm.o: $(SRC_PATH)/src/common/pf_cpm.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/common/pf_cpm.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cpm.o

$(BUILD_PATH)/pf_dcp.o: $(SRC_PATH)/src/common/pf_dcp.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/common/pf_dcp.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_dcp.o

$(BUILD_PATH)/pf_eth.o: $(SRC_PATH)/src/common/pf_eth.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/common/pf_eth.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_eth.o

$(BUILD_PATH)/pf_lldp.o: $(SRC_PATH)/src/common/pf_lldp.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/common/pf_lldp.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_lldp.o

$(BUILD_PATH)/pf_ppm.o: $(SRC_PATH)/src/common/pf_ppm.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/common/pf_ppm.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_ppm.o

$(BUILD_PATH)/pf_ptcp.o: $(SRC_PATH)/src/common/pf_ptcp.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/common/pf_ptcp.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_ptcp.o

$(BUILD_PATH)/pf_scheduler.o: $(SRC_PATH)/src/common/pf_scheduler.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/common/pf_scheduler.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_scheduler.o

$(BUILD_PATH)/pf_block_reader.o: $(SRC_PATH)/src/device/pf_block_reader.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_block_reader.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_block_reader.o

$(BUILD_PATH)/pf_block_writer.o: $(SRC_PATH)/src/device/pf_block_writer.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_block_writer.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_block_writer.o

$(BUILD_PATH)/pf_cmdev.o: $(SRC_PATH)/src/device/pf_cmdev.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmdev.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmdev.o

$(BUILD_PATH)/pf_cmdmc.o: $(SRC_PATH)/src/device/pf_cmdmc.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmdmc.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmdmc.o

$(BUILD_PATH)/pf_cmina.o: $(SRC_PATH)/src/device/pf_cmina.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmina.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmina.o

$(BUILD_PATH)/pf_cmio.o: $(SRC_PATH)/src/device/pf_cmio.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmio.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmio.o

$(BUILD_PATH)/pf_cmpbe.o: $(SRC_PATH)/src/device/pf_cmpbe.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmpbe.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmpbe.o

$(BUILD_PATH)/pf_cmrdr.o: $(SRC_PATH)/src/device/pf_cmrdr.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmrdr.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmrdr.o

$(BUILD_PATH)/pf_cmrpc.o: $(SRC_PATH)/src/device/pf_cmrpc.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmrpc.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmrpc.o

$(BUILD_PATH)/pf_cmrs.o: $(SRC_PATH)/src/device/pf_cmrs.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmrs.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmrs.o

$(BUILD_PATH)/pf_cmsm.o: $(SRC_PATH)/src/device/pf_cmsm.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmsm.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmsm.o

$(BUILD_PATH)/pf_cmsu.o: $(SRC_PATH)/src/device/pf_cmsu.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmsu.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmsu.o

$(BUILD_PATH)/pf_cmwrr.o: $(SRC_PATH)/src/device/pf_cmwrr.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_cmwrr.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_cmwrr.o

$(BUILD_PATH)/pf_diag.o: $(SRC_PATH)/src/device/pf_diag.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_diag.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_diag.o

$(BUILD_PATH)/pf_fspm.o: $(SRC_PATH)/src/device/pf_fspm.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pf_fspm.c -c $(CFLAGS) -o $(BUILD_PATH)/pf_fspm.o

$(BUILD_PATH)/pnet_api.o: $(SRC_PATH)/src/device/pnet_api.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/device/pnet_api.c -c $(CFLAGS) -o $(BUILD_PATH)/pnet_api.o

$(BUILD_PATH)/osal.o: $(SRC_PATH)/src/osal/linux/osal.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/osal/linux/osal.c -c $(CFLAGS) -o $(BUILD_PATH)/osal.o

$(BUILD_PATH)/osal_eth.o: $(SRC_PATH)/src/osal/linux/osal_eth.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/osal/linux/osal_eth.c -c $(CFLAGS) -o $(BUILD_PATH)/osal_eth.o

$(BUILD_PATH)/osal_udp.o: $(SRC_PATH)/src/osal/linux/osal_udp.c $(HEADERS)
	$(CC) $(SRC_PATH)/src/osal/linux/osal_udp.c -c $(CFLAGS) -o $(BUILD_PATH)/osal_udp.o

$(BUILD_PATH)/rpmalloc.o: $(SRC_PATH)/src/rpmalloc/rpmalloc.c $(SRC_PATH)/src/rpmalloc/rpmalloc.h
	$(CC) $(SRC_PATH)/src/rpmalloc/rpmalloc.c -c $(CFLAGS) -o $(BUILD_PATH)/rpmalloc.o

clean:
	rm -f $(OBJECTS)

clean_all:
	rm -f $(OBJECTS) $(EXECUTABLE)

