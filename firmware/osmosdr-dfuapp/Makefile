# binary file name
TARGET=dfuapp

CROSS_COMPILE=arm-none-eabi-
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)gcc
OBJCOPY=$(CROSS_COMPILE)objcopy
OBJDUMP=$(CROSS_COMPILE)objdump
BUILDDIR=build
OBJDIR=$(BUILDDIR)/obj
DEPDIR=$(BUILDDIR)/dep

RUMBA=rumba
RUMBA_TTY=/dev/ttyACM0

# prevent make from displaying full command lines
QUIET=@

C_SOURCES=\
	main.c \
	startup.c \
	vectors.c \
	\
	at91sam3u4/core_cm3.c \
	\
	crt/printf.c \
	crt/sprintf.c \
	crt/stdio.c \
	crt/vsprintf.c \
	crt/vprintf.c \
	\
	driver/dbgio.c \
	driver/flash.c \
	driver/led.c \
	driver/irq.c \
	driver/sys.c \
	driver/usbdhs.c \
	\
	usb/descriptors.c \
	usb/hardware.c \
	usb/usb.c \
	usb/usbdevice.c \
	usb/slimpro.c

all: build

# general compiler flags
CFLAGS=\
	-mcpu=cortex-m3 -mthumb -mlong-calls -ffunction-sections -fdata-sections -std=gnu99 \
	-Os -g3 -Wall -Dat91sam3u4 \
	-ffunction-sections \
	-fdata-sections \
	-fno-builtin-vsprintf \
	-fno-builtin-vprintf \
	-fno-builtin-puts \
	-fno-builtin-printf \
	-Wimplicit
LDFLAGS=-g3 -Wl,-Map=$(BUILDDIR)/$(TARGET).map -nostdlib -nostartfiles -mcpu=cortex-m3 -mthumb -Wl,--gc-sections -T"src/at91sam3u4/flash.lds"
LIBS=

##############################################################################

SUBDIRS=$(sort $(dir $(C_SOURCES)))

DEPDIRS=$(addprefix $(DEPDIR)/,$(SUBDIRS))
OBJDIRS=$(addprefix $(OBJDIR)/,$(SUBDIRS))

COBJS=$(addprefix $(OBJDIR)/,$(C_SOURCES:%.c=%.o))
DEPS=$(C_SOURCES:%.c=%.dep)
FULLDEPS=$(addprefix $(DEPDIR)/,$(DEPS))

BUILD_DATE=$(shell date "+%Y-%m-%d")
BUILD_TIME=$(shell date "+%H:%M:%S")
BUILD_ZONE=$(shell date "+%Z")
BUILD_HOST=$(shell uname -n)
BUILD_GCC=$(shell $(CC) --version | head -1)
BUILD_VCSID=$(shell ../../git-version-gen ../../.tarball_version)

.PHONY: all build buildid clean distclean flash dfuflash

build: $(OBJDIRS) $(DEPDIRS) buildid $(BUILDDIR)/$(TARGET).bin $(BUILDDIR)/$(TARGET).asm

buildid: $(OBJDIRS)
	@echo [GEN]\ $(BUILDDIR)/buildid.h
	$(QUIET)echo -en \
"#ifndef INCLUDE_BUILDID_H\n"\
"#define INCLUDE_BUILDID_H\n"\
"\n"\
"#define BUILDID_DATE \"$(BUILD_DATE)\"\n"\
"#define BUILDID_TIME \"$(BUILD_TIME)\"\n"\
"#define BUILDID_ZONE \"$(BUILD_ZONE)\"\n"\
"#define BUILDID_HOST \"$(BUILD_HOST)\"\n"\
"#define BUILDID_GCC \"$(BUILD_GCC)\"\n"\
"#define BUILDID_VCSID \"$(BUILD_VCSID)\"\n"\
"\n"\
"#endif // INCLUDE_BUILDID_H\n"\
	> $(BUILDDIR)/buildid.h 

-include $(FULLDEPS)

$(BUILDDIR)/$(TARGET).bin: $(BUILDDIR)/$(TARGET).elf
	@echo [BIN] $@
	$(QUIET)$(OBJCOPY) \
		-O binary $(BUILDDIR)/$(TARGET).elf $(BUILDDIR)/$(TARGET).bin
	$(QUIET)ls -la $@ | awk '{ print "      image has", $$5, "bytes"; }'

$(BUILDDIR)/$(TARGET).asm: $(BUILDDIR)/$(TARGET).elf
	@echo [ASM] $@
	$(QUIET)$(OBJDUMP) \
		-deS --disassembler-options=force-thumb $(BUILDDIR)/$(TARGET).elf > $(BUILDDIR)/$(TARGET).asm

$(BUILDDIR)/$(TARGET).elf: $(COBJS)
	@echo [LD\ ]\ $@
	$(QUIET)$(LD) \
		$(LDFLAGS) \
		-o $(BUILDDIR)/$(TARGET).elf \
		-Wl,--start-group $(COBJS) $(LIBS) -Wl,--end-group

$(COBJS):
	@echo [C\ \ ]\ $(patsubst %.o,%.c,$(patsubst $(OBJDIR)/%,%,$@))
	$(QUIET)$(CC) \
		$(CFLAGS) \
		$(CFLAGS_$(subst /,_,$(patsubst %.o,%,$(patsubst $(OBJDIR)/%,%,$@)))) \
		-MD -MP -MF $(patsubst %.o,$(DEPDIR)/%.dep,$(patsubst $(OBJDIR)/%,%,$@)) \
		-c src/$(patsubst %.o,%.c,$(patsubst $(OBJDIR)/%,%,$@)) \
		-o $@

$(OBJDIRS) $(DEPDIRS) $(GENERATED):
	$(QUIET)mkdir -p $@

clean:
	@echo [CLEAN]
	$(QUIET)rm -Rf $(OBJDIR) $(DEPDIR) $(BUILDDIR)/buildid.h

distclean:
	@echo [DISTCLEAN]
	$(QUIET)rm -Rf $(BUILDDIR)

flash: build
	$(QUIET)$(RUMBA) $(RUMBA_TTY) flashmcu $(BUILDDIR)/$(TARGET).bin

dfuflash: build
	$(QUIET)sudo dfu-util -d 16c0:0763 -a 1 -D $(BUILDDIR)/$(TARGET).bin -R
