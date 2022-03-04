obj-m := am_joyin.o
#am_joyin-objs := src/gpio_util.o
#am_joyin-objs += src/gpio_rpi2.o src/74hc165.o
KVERSION := `uname -r`

MY_CFLAGS += -g -DDEBUG
# ccflags-y += ${MY_CFLAGS}
# CC += ${MY_CFLAGS}

ifneq (,$(findstring -v7, $(KVERSION)))
CFLAGS_am_joyin.o := -DRPI2
endif

#src-ccflags-y := -I src -I src/util -I src/devices

all:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

debug:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules EXTRA_CFLAGS="$(MY_CFLAGS)"

clean:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) clean

