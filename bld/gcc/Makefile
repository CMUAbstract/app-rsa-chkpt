EXEC = rsa.out

OBJECTS = \
	main.o \

include $(MAKER_ROOT)/Makefile.gcc

CFLAGS += \
	-I$(WISP_BASE_ROOT)

LFLAGS += \
	-L$(WISP_BASE_ROOT)/gcc \
	-L$(LIBMSPCONSOLE_ROOT)/bld/gcc \

LIBS = \
    -lwisp-base \
	-lmspconsole \

VPATH = ../../src
