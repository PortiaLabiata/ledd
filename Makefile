SHELL := /bin/bash
TOOLCHAIN := /home/portia_labiata/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin
CC := $(TOOLCHAIN)/arm-rockchip830-linux-uclibcgnueabihf-gcc
LD := $(TOOLCHAIN)/arm-rockchip830-linux-uclibcgnueabihf-gcc
LIB := /home/portia_labiata/luckfox-pico/sysdrv/source/buildroot/buildroot-2023.02.6/output/build/libgpiod2-2.3.1/

CFLAGS := -Wall -Wextra -Wpedantic -Werror -O3 -I$(LIB)/include
LDFLAGS := -L$(LIB)/build/lib -lgpiod

SRCS_DAEMON := daemon.c led.c
SRCS_APP := app.c
OBJS_DAEMON := $(SRCS_DAEMON:.c=.o)
OBJS_APP := $(SRCS_APP:.c=.o)
TARGET_DAEMON := ledd
TARGET_APP := led

.PHONY: all clean flash

all: $(TARGET_DAEMON) $(TARGET_APP)

$(TARGET_APP): $(OBJS_APP)
	$(LD) $(OBJS_APP) $(LDFLAGS) -o $(TARGET_APP)

$(TARGET_DAEMON): $(OBJS_DAEMON)
	$(LD) $(OBJS_DAEMON) $(LDFLAGS) -o $(TARGET_DAEMON)

%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@

flash: $(TARGET)
	adb push $(TARGET_DAEMON) /opt
	adb push $(TARGET_APP) /usr/bin

clean:
	rm *.o $(TARGET_APP) $(TARGET_DAEMON)
