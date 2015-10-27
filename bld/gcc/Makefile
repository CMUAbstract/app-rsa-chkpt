EXEC = rsa.out

OBJECTS = \
	main.o \

BOARD ?= wisp
CONFIG_EDB ?= 1
CONFIG_PRINTF_LIB ?= libedb
CONFIG_LIBEDB_PRINTF ?= eif

include $(MAKER_ROOT)/Makefile.gcc

ifeq ($(BOARD),wisp)
CFLAGS += -DBOARD_WISP
endif

ifeq ($(BOARD),mspts430)
CFLAGS += -DBOARD_MSP_TS430
endif

ifeq ($(CONFIG_EDB),1)
CFLAGS += -DCONFIG_EDB
CFLAGS += -I$(LIBEDB_ROOT)/src/include
LFLAGS += -L$(LIBEDB_ROOT)/gcc
LIBS += -ledb
endif

ifeq ($(CONFIG_PRINTF_LIB),libedb)

ifeq ($(CONFIG_LIBEDB_PRINTF),eif)
CFLAGS += -DCONFIG_LIBEDB_PRINTF_EIF
else ifeq ($(CONFIG_LIBEDB_PRINTF),bare)
CFLAGS += -DCONFIG_LIBEDB_PRINTF_BARE
endif # CONFIG_LIBEDB_PRINTF

else ifeq ($(CONFIG_PRINTF_LIB),libmspconsole)
CFLAGS += -DCONFIG_LIBMSPCONSOLE_PRINTF
LFLAGS += -L$(LIBMSPCONSOLE_ROOT)/bld/gcc
LIBS += -lmspconsole
endif

CFLAGS += \
	-I$(WISP_BASE_ROOT) \
	-I$(LIBMSP_ROOT)/src/include \
	-I$(LIBMSPMATH_ROOT)/src/include \

LFLAGS += \
	-L$(WISP_BASE_ROOT)/gcc \
	-L$(LIBMSPMATH_ROOT)/bld/gcc \

LIBS += \
    -lwisp-base \
	-lmspmath \

VPATH = ../../src
