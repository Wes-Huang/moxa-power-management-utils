CROSS_COMPILE =
CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip
CFLAGS:=-Wall

all:
	$(CC) -lpthread -o moxa-mcud moxa-mcud.c
	$(CC) -o mx-mcu-ctl mx-mcu-ctl.c
	$(STRIP) -s moxa-mcud mx-mcu-ctl

.PHONY: clean
clean:
	rm -f moxa-mcud mx-mcu-ctl *.o
