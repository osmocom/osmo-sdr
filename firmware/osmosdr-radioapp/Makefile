# binary file name
TARGET=radioapp

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
	cmd_e4k.c \
	cmd_fpga.c \
	cmd_si570.c \
	cmd_sys.c \
	components.c \
	console.c \
	main.c \
	startup.c \
	vectors.c \
	\
	crt/ctype.c \
	crt/printf.c \
	crt/sprintf.c \
	crt/stdio.c \
	crt/string.c \
	crt/strtox.c \
	crt/vsprintf.c \
	crt/vprintf.c \
	\
	driver/chanfilter.c \
	driver/dbgio.c \
	driver/dma.c \
	driver/dmabuffer.c \
	driver/e4k.c \
	driver/flash.c \
	driver/led.c \
	driver/irq.c \
	driver/mci.c \
	driver/pio.c \
	driver/sdrfpga.c \
	driver/sdrmci.c \
	driver/sdrssc.c \
	driver/si570.c \
	driver/spi.c \
	driver/ssc.c \
	driver/sys.c \
	driver/twi.c \
	driver/usbdhs.c \
	\
	dsp/arm_bitreversal.c \
	dsp/arm_cfft_radix4_init_q31.c \
	dsp/arm_cfft_radix4_q31.c \
	dsp/arm_cmplx_mag_q31.c \
	dsp/arm_common_tables.c \
	dsp/arm_sqrt_q31.c \
	\
	usb/descriptors.c \
	usb/usb.c \
	usb/usbdevice.c

all: build

# general compiler flags
CFLAGS=\
	-mcpu=cortex-m3 -mthumb -mlong-calls -ffunction-sections -fdata-sections -std=gnu99 \
	-O2 -g3 -Wall -Dat91sam3u4 -DARM_MATH_CM3 -D__CHECK_DEVICE_DEFINES \
	\
	-D__CM3_REV=0x0200 \
	-D__MPU_PRESENT=0 \
	-D__NVIC_PRIO_BITS=4 \
	-D__Vendor_SysTickConfig=0 \
	\
	-ffunction-sections \
	-fdata-sections \
	-fno-builtin-vsprintf \
	-fno-builtin-vprintf \
	-fno-builtin-puts \
	-fno-builtin-printf \
	-Wimplicit
LDFLAGS=-g3 -Wl,-Map=$(BUILDDIR)/$(TARGET).map -nostdlib -nostartfiles -mcpu=cortex-m3 -mthumb -Wl,--gc-sections -T"src/at91sam3u4/flash.lds"
LIBS=$(shell $(CC) -print-libgcc-file-name)

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

.PHONY: all build buildid clean distclean dfuflash

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

dfuflash: build
	$(QUIET)sudo dfu-util -d 16c0:0763 -a 0 -D $(BUILDDIR)/$(TARGET).bin -R
