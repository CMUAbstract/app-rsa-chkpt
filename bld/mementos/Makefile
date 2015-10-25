EXEC = rsa.out

OBJECTS = \
	main.o \

BOARD ?= wisp
CONFIG_PRINTF_LIB ?= libedb

include $(MAKER_ROOT)/Makefile.mementos

ifeq ($(BOARD),wisp)
CFLAGS += -DBOARD_WISP
endif

ifeq ($(BOARD),mspts430)
CFLAGS += -DBOARD_MSP_TS430
endif

ifeq ($(CONFIG_PRINTF_LIB),libedb)
CFLAGS += -DCONFIG_LIBEDB_PRINTF
CFLAGS += -I$(LIBEDB_ROOT)/src/include
LLVM_NOOPT_LIBS += $(LIBEDB_ROOT)/clang/libedb.a.bc
endif

ifeq ($(CONFIG_PRINTF_LIB),libmspconsole)
CFLAGS += -DCONFIG_LIBMSPCONSOLE_PRINTF
LLVM_NOOPT_LIBS += $(LIBMSPCONSOLE_ROOT)/bld/clang/libmspconsole.a.bc
endif

CFLAGS += \
	-I$(WISP_BASE_ROOT) \
	-I$(LIBMSPMATH_ROOT)/src/include \

LLVM_NOOPT_LIBS += \
	$(WISP_BASE_ROOT)/clang/libwisp-base.a.bc \
	$(LIBMSPMATH_ROOT)/bld/clang/libmspmath.a.bc \

LFLAGS+= \
	-L$(LIBMSPBUILTINS_ROOT)/bld/gcc \

LIBS += \
	-lmspbuiltins \


VPATH = ../../src
