EXEC = rsa.out

OBJECTS = \
	main.o \

LIBCHAIN_ENABLE_DIAGNOSTICS ?= 0
CONFIG_PRINTF_LIB ?= libedb
BOARD ?= wisp

include $(MAKER_ROOT)/Makefile.gcc

ifeq ($(LIBCHAIN_ENABLE_DIAGNOSTICS),1)
CFLAGS += -DLIBCHAIN_ENABLE_DIAGNOSTICS
endif

ifeq ($(BOARD),wisp)
CFLAGS += -DBOARD_WISP
endif

ifeq ($(BOARD),mspts430)
CFLAGS += -DBOARD_MSP_TS430
endif

ifeq ($(CONFIG_PRINTF_LIB),libedb)
CFLAGS += -DCONFIG_LIBEDB_PRINTF
LIBS += -ledb
LFLAGS += -L$(LIBEDB_ROOT)/gcc
CFLAGS += -I$(LIBEDB_ROOT)/src/include
endif

ifeq ($(CONFIG_PRINTF_LIB),libmspconsole)
CFLAGS += -DCONFIG_LIBMSPCONSOLE_PRINTF
LIBS += -lmspconsole
LFLAGS += -L$(LIBMSPCONSOLE_ROOT)/bld/gcc
endif

CFLAGS += \
	-I$(WISP_BASE_ROOT)

LFLAGS += \
	-L$(WISP_BASE_ROOT)/gcc \

LIBS += \
    -lwisp-base \

VPATH = ../../src
