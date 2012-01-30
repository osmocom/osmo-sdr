# binary file name
TARGET=rumba

# CROSS_COMPILE=
QUIET=@

# N = build release version
# y = optimize but include debugger info
# Y = build debug version
DEBUG=N

C_SOURCES=\
	src/main.c \
	src/osmosdr.c \
	src/sam3u.c \
	src/serial.c \
	src/utils.c \
	src/lattice/hardware.c \
	src/lattice/slim_pro.c \
	src/lattice/slim_vme.c

# general compiler flags
CFLAGS=-Wall
LDFLAGS=-Wl,-Map=$(FULLTARGET).map
LIBS=-lrt

##############################################################################

SUBDIRS=$(sort $(dir $(C_SOURCES)))

DEPDIR=deps
OBJDIR=objs
BINDIR=bin
DEPDIRS=$(addprefix $(DEPDIR)/,$(SUBDIRS))
OBJDIRS=$(addprefix $(OBJDIR)/,$(SUBDIRS))

CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)gcc

COBJS=$(C_SOURCES:%.c=%.o)
DEPS=$(C_SOURCES:%.c=%.dep)
FULLDEPS=$(addprefix $(DEPDIR)/,$(DEPS))
FULLCOBJS=$(addprefix $(OBJDIR)/,$(COBJS))
FULLTARGET=$(addprefix $(BINDIR)/,$(TARGET))

ifeq ($(DEBUG),Y)
	# debug version
	CFLAGS+=-O0 -g3
	LDFLAGS+=-g3
else ifeq ($(DEBUG),y)
	# optimized version with debugger info
	CFLAGS+=-O2 -g3 -Werror -ffunction-sections -fdata-sections
	LDFLAGS+=-g3 -Wl,--gc-sections
else
	# release version
	CFLAGS+=-O2 -s -Werror -ffunction-sections -fdata-sections
	LDFLAGS+=-s -Wl,--gc-sections
endif

.PHONY: all build clean distclean

all: build

build: $(FULLTARGET)

-include $(FULLDEPS)

$(FULLTARGET): $(DEPDIRS) $(OBJDIRS) $(BINDIR) $(FULLCOBJS)
	@echo LD \ $(TARGET)
	$(QUIET)$(LD) $(LDFLAGS) -o $(FULLTARGET) -Wl,--start-group $(FULLCOBJS) $(LIBS) -Wl,--end-group
	$(QUIET)ln -sf $(FULLTARGET) $(TARGET)

$(FULLCOBJS):
	@echo C\ \ \ $(patsubst $(OBJDIR)/%,%,$(patsubst %.o,%.c, $@))
	$(QUIET)$(CC) $(CFLAGS) $(CFLAGS_$(subst /,_,$(patsubst %.o,%,$@))) -MD -MP -MF $(patsubst %.o,$(DEPDIR)/%.dep,$(patsubst $(OBJDIR)/%,%,$@)) -c $(patsubst $(OBJDIR)/%,%,$(patsubst %.o,%.c, $@)) -o $@

$(DEPDIRS):
	$(QUIET)mkdir -p $@
	
$(OBJDIRS):
	$(QUIET)mkdir -p $@
	
$(BINDIR):
	$(QUIET)mkdir -p $@

clean:
	$(QUIET)echo CLEAN
	$(QUIET)rm -Rf $(DEPDIR) $(OBJDIR) $(BINDIR) $(TARGET) $(TARGET).map *~ *.s *.ss

distclean: clean
