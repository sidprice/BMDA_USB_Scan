ifneq ($(V), 1)
MAKEFLAGS += --no-print-dir
Q := @
endif


LDFLAGS += $(shell pkg-config --libs libusb-1.0) $(shell pkg-config --libs libftdi1)

TARGET ?= BMP_Scan

CFLAGS += -Wall -Wextra -Werror -Wno-char-subscripts \
	-std=c11 -g3 -MD

ifeq ($(ENABLE_DEBUG), 1)
CFLAGS += -DENABLE_DEBUG
endif

SRC =			\
	main.c

OBJ = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SRC)))

$(TARGET): $(OBJ)
	@echo "  LD      $@"
	$(Q)$(CC) -o $@ $(OBJ) $(LDFLAGS)


*.o:	$(SRC)
	@echo "  CC      $<"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

.PHONY:	clean

clean:
	$(Q)echo "  CLEAN"
	-$(Q)$(RM) *.o *.d *.elf *.exe 

