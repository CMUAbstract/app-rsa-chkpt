EXEC = rsa.out

OBJECTS = \
	main.o \

BOARD ?= wisp
CONFIG_EDB ?= 1
CONFIG_PRINTF_LIB ?= libedb
CONFIG_LIBEDB_PRINTF ?= eif

MEMENTOS_MODE ?= latch
MEMENTOS_IGNORE_GLOBALS ?= 1

include $(MAKER_ROOT)/Makefile.mementos

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
LLVM_LIBS += $(LIBMSPCONSOLE_ROOT)/bld/clang/libmspconsole.a.bc
endif

CFLAGS += \
	-I$(WISP_BASE_ROOT) \
	-I$(LIBMSP_ROOT)/src/include \
	-I$(LIBMSPMATH_ROOT)/src/include \

LLVM_NOOPT_LIBS += \
	$(LIBMSPMATH_ROOT)/bld/clang/libmspmath.a.bc \

LFLAGS+= \
	-L$(LIBMSPBUILTINS_ROOT)/bld/gcc \
	-L$(WISP_BASE_ROOT)/gcc \

LIBS += \
	-lmspbuiltins \
	-lwisp-base \


VPATH = ../../src
