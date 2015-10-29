EXEC = rsa.out

OBJECTS = \
	main.o \

BOARD ?= mspts430
CONFIG_EDB ?= 0
CONFIG_PRINTF_LIB ?= libmspconsole
#CONFIG_LIBEDB_PRINTF ?= eif

include $(MAKER_ROOT)/Makefile.dino
include $(MAKER_ROOT)/Makefile.board
include $(MAKER_ROOT)/Makefile.console
include $(MAKER_ROOT)/Makefile.edb

CFLAGS += \
	-I$(LIBMSP_ROOT)/src/include \
	-I$(LIBMSPMATH_ROOT)/src/include \

LLVM_LIBS += \
	$(LIBMSPMATH_ROOT)/bld/clang/libmspmath.a.bc \

LFLAGS += \
	-L$(LIBMSPBUILTINS_ROOT)/bld/gcc \

LIBS += \
	-lmspbuiltins \

VPATH = ../../src
