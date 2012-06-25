# ----------------------------------------------------------------------------
#         ATMEL Microcontroller Software Support 
# ----------------------------------------------------------------------------
# Copyright (c) 2008, Atmel Corporation
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# - Redistributions of source code must retain the above copyright notice,
# this list of conditions and the disclaimer below.
#
# Atmel's name may not be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
# DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# ----------------------------------------------------------------------------

# 	Makefile for compiling the usb-device-core-project project

#-------------------------------------------------------------------------------
#		User-modifiable options
#-------------------------------------------------------------------------------

# Chip & board used for compilation
# (can be overriden by adding CHIP=chip and BOARD=board to the command-line)
CHIP  = at91sam3u4
BOARD = at91sam3u-ek

# Trace level used for compilation
# (can be overriden by adding TRACE_LEVEL=#number to the command-line)
# TRACE_LEVEL_DEBUG      5
# TRACE_LEVEL_INFO       4
# TRACE_LEVEL_WARNING    3
# TRACE_LEVEL_ERROR      2
# TRACE_LEVEL_FATAL      1
# TRACE_LEVEL_NO_TRACE   0
TRACE_LEVEL = 4

# Optimization level, put in comment for debugging
OPTIMIZATION = -Os

# AT91 library directory
AT91LIB = ../../../at91lib

# External library
EXT_LIBS= ../external_libs

# Output file basename
OUTPUT = usb-dfu-experiment-$(BOARD)-$(CHIP)

# Compile with chip specific features
include $(AT91LIB)/boards/$(BOARD)/$(CHIP)/chip.mak

# Compile for all memories available on the board (this sets $(MEMORIES))
include $(AT91LIB)/boards/$(BOARD)/board.mak

# Output directories
BIN = bin
OBJ = obj

GIT_REVISION := $(shell ../../git-version-gen ../../.tarball_version)
ASFLAGS += -DGIT_REVISION=\"$(GIT_REVISION)\"
CFLAGS += -DGIT_REVISION=\"$(GIT_REVISION)\"

#-------------------------------------------------------------------------------
#		Tools
#-------------------------------------------------------------------------------

# Tool suffix when cross-compiling
CROSS_COMPILE = arm-cm3-eabi-

# Compilation tools
CC = $(CROSS_COMPILE)gcc
SIZE = $(CROSS_COMPILE)size
STRIP = $(CROSS_COMPILE)strip
OBJCOPY = $(CROSS_COMPILE)objcopy

# Flags
INCLUDES += -I$(AT91LIB)/boards/$(BOARD)
INCLUDES += -I$(AT91LIB)/peripherals
INCLUDES += -I$(AT91LIB)/components
INCLUDES += -I$(AT91LIB)/drivers
INCLUDES += -I$(AT91LIB)
INCLUDES += -I$(EXT_LIBS)
INCLUDES += -I$(AT91LIB)/cmsis
INCLUDES += -I.

ifeq ($(CHIP_CORE), cortexm3)
TARGET_OPTS = -mcpu=cortex-m3 -mthumb
else
TARGET_OPTS =
endif

CFLAGS += $(TARGET_OPTS)
CFLAGS += -Wall -mlong-calls -ffunction-sections
CFLAGS += -g $(OPTIMIZATION) $(INCLUDES) -D$(CHIP) -DTRACE_LEVEL=$(TRACE_LEVEL)
ASFLAGS = $(TARGET_OPTS) -Wall -g $(OPTIMIZATION) $(INCLUDES) -D$(CHIP) -D__ASSEMBLY__
LDFLAGS = -g $(OPTIMIZATION) -nostdlib -nostartfiles $(TARGET_OPTS) -Wl,--gc-sections

#-------------------------------------------------------------------------------
#		Files
#-------------------------------------------------------------------------------

# Directories where source files can be found
PERIPH = $(AT91LIB)/peripherals
BOARDS = $(AT91LIB)/boards
UTILITY = $(AT91LIB)/utility
COMP = $(AT91LIB)/components
DRIVER = $(AT91LIB)/drivers
USB  = $(AT91LIB)/usb

VPATH += $(UTILITY)
VPATH += $(PERIPH)/dbgu
VPATH += $(PERIPH)/pio
VPATH += $(PERIPH)/irq
VPATH += $(PERIPH)/pmc
VPATH += $(PERIPH)/rstc
VPATH += $(PERIPH)/cp15
VPATH += $(BOARDS)/$(BOARD)
VPATH += $(BOARDS)/$(BOARD)/$(CHIP)
VPATH += $(PERIPH)/eefc
VPATH += $(PERIPH)/dma
VPATH += $(DRIVER)/dmad
VPATH += $(AT91LIB)/cmsis
VPATH += $(AT91LIB)/memories/flash

VPATH += $(USB)/device/core
VPATH += $(USB)/device/dfu
VPATH += $(USB)/common/core
VPATH += $(USB)/common/audio

# Objects built from C source files
C_OBJECTS += main.o
C_OBJECTS += hardware.o
C_OBJECTS += slimpro.o
C_OBJECTS += dfu_desc.o
C_OBJECTS += sam3u_chipid_usbserial.o
C_OBJECTS += USBD_UDPHS.o
C_OBJECTS += USBDDriver.o
C_OBJECTS += USBDCallbacks_Initialized.o
C_OBJECTS += USBDCallbacks_Reset.o
#C_OBJECTS += USBDCallbacks_Resumed.o
#C_OBJECTS += USBDCallbacks_Suspended.o
C_OBJECTS += USBDDriverCb_CfgChanged.o
#C_OBJECTS += USBDDriverCb_IfSettingChanged.o
C_OBJECTS += USBInterfaceRequest.o
C_OBJECTS += USBFeatureRequest.o
C_OBJECTS += USBGenericRequest.o
C_OBJECTS += USBGetDescriptorRequest.o
C_OBJECTS += USBSetAddressRequest.o
C_OBJECTS += USBSetConfigurationRequest.o
C_OBJECTS += USBGenericDescriptor.o
C_OBJECTS += USBConfigurationDescriptor.o
C_OBJECTS += USBEndpointDescriptor.o
C_OBJECTS += dfu_driver.o
C_OBJECTS += dbgu.o
C_OBJECTS += pio.o
C_OBJECTS += pio_it.o
C_OBJECTS += pmc.o
C_OBJECTS += led.o
C_OBJECTS += string.o
C_OBJECTS += stdio.o
C_OBJECTS += math.o
C_OBJECTS += trace.o
C_OBJECTS += board_memories.o
C_OBJECTS += board_lowlevel.o
C_OBJECTS += flashd_eefc.o eefc.o
C_OBJECTS += rstc.o


# Objects for different chips
ifeq ($(CHIP_CORE), cortexm3)
C_OBJECTS += nvic.o
C_OBJECTS += exceptions.o
C_OBJECTS += board_cstartup_gnu.o
C_OBJECTS += core_cm3.o
else
C_OBJECTS += aic.o
C_OBJECTS += cp15.o
endif

# Objects built from Assembly source files
ifneq ($(CHIP_CORE), cortexm3)
ASM_OBJECTS += board_cstartup.o
ASM_OBJECTS += cp15_asm.o
endif

# Append OBJ and BIN directories to output filename
OUTPUT := $(BIN)/$(OUTPUT)

#-------------------------------------------------------------------------------
#		Rules
#-------------------------------------------------------------------------------

all: $(BIN) $(OBJ) $(MEMORIES)

$(BIN) $(OBJ):
	mkdir $@

define RULES
C_OBJECTS_$(1) = $(addprefix $(OBJ)/$(1)_, $(C_OBJECTS))
ASM_OBJECTS_$(1) = $(addprefix $(OBJ)/$(1)_, $(ASM_OBJECTS))

$(1): $$(ASM_OBJECTS_$(1)) $$(C_OBJECTS_$(1))
	$(CC) $(LDFLAGS) -T"$(AT91LIB)/boards/$(BOARD)/$(CHIP)/$$@.lds" -o $(OUTPUT)-$$@.elf $$^
	$(OBJCOPY) -O binary $(OUTPUT)-$$@.elf $(OUTPUT)-$$@.bin
	$(SIZE) $$^ $(OUTPUT)-$$@.elf

$$(C_OBJECTS_$(1)): $(OBJ)/$(1)_%.o: %.c Makefile $(OBJ) $(BIN)
	$(CC) $(CFLAGS) -D$(1) -c -o $$@ $$<

$$(ASM_OBJECTS_$(1)): $(OBJ)/$(1)_%.o: %.S Makefile $(OBJ) $(BIN)
	$(CC) $(ASFLAGS) -D$(1) -c -o $$@ $$<

debug_$(1): $(1)
	perl ../resources/gdb/debug.pl $(OUTPUT)-$(1).elf

endef

$(foreach MEMORY, $(MEMORIES), $(eval $(call RULES,$(MEMORY))))

clean:
	-rm -f $(OBJ)/*.o $(BIN)/*.bin $(BIN)/*.elf

