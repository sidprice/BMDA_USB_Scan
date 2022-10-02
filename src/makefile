ifneq ($(V), 1)
MAKEFLAGS += --no-print-dir
Q := @
endif

CFLAGS += -Wall -Wextra -Werror -Wno-char-subscripts \
	-std=c11 -g3 -MD

LDFLAGS += $(shell pkg-config --libs libusb-1.0) $(shell pkg-config --libs libftdi1)

ifeq ($(OS), Windows_NT)
	CFLAGS += -I../3rdparty/ftdi
	ifeq ($(PROCESSOR_ARCHITECTURE), AMD64)
		LDFLAGS += ../3rdparty/ftdi/amd64/ftd2xx.lib
	else
		LDFLAGS += ../3rdparty/ftdi/i386/ftd2xx.lib
	endif
endif

TARGET ?= BMP_Scan

ifeq ($(ENABLE_DEBUG), 1)
CFLAGS += -DENABLE_DEBUG
endif

SRC =			\
	main.c

OBJ = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SRC)))

$(TARGET): $(OBJ)
	@echo $(shell pwd)
	@echo "  LD      $@"
	$(Q)$(CC) -o $@ $(OBJ) $(LDFLAGS)

*.o:	$(SRC)
	@echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

.PHONY:	clean

clean:
	$(Q)echo "  CLEAN"
	-$(Q)$(RM) *.o *.d *.elf *.exe 

