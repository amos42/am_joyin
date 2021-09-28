obj-m := src/am_joyin.o
#am_joyin-objs := src/gpio_util.o
#am_joyin-objs += src/gpio_rpi2.o src/74hc165.o
KVERSION := `uname -r`

ifneq (,$(findstring -v7, $(KVERSION)))
CFLAGS_am_joyin.o := -DRPI2
endif

src-ccflags-y := -I src -I src/util -I src/devices

all:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

