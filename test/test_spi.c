/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __KERNEL__
#define __KERNEL__
#endif

#ifndef MODULE
#define MODULE
#endif

#define VENDOR_ID       (0xA042)
#define PRODUCT_ID      (0x00FF)
#define PRODUCT_VERSION (0x0100)
#define DEVICE_NAME     "Amos Test"


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/slab.h>


MODULE_AUTHOR("Amos42");
MODULE_DESCRIPTION("GPIO Test Driver");
MODULE_LICENSE("GPL");


/*============================================================*/

//#include "../bcm_peri.h"
#include "../bcm_peri.c"

//#include "../gpio_util.h"
#include "../gpio_util.c"
//#include "../i2c_util.h"
#include "../i2c_util.c"
//#include "../spi_util.h"
#include "../spi_util.c"



static int am_test_init(void)
{
    int err;
    unsigned short v;
    unsigned char vv[] = { 0x01, (0x08 | 0) << 4, 0x00 }; // mcp3008 ch0
    unsigned char vv1[] = { 0x01, (0x08 | 1) << 4, 0x00 }; // mcp3008 ch1
    unsigned char vv2[3] = { 0x0, };    

    err = gpio_init(bcm_peri_base_probe());
    if (err < 0) {
        goto err_free_dev;
    }

    err = spi_init(bcm_peri_base_probe(), 0xB0);
    if (err < 0) {
        goto err_free_dev;
    }

    printk("test module loaded\n");


    spi_setClockDivider(0);
    spi_setDataMode(0x00);

    spi_begin();

    memset(vv2, 0, 3);
    spi_transfernb(vv, vv2, 3);
    printk(">>>>>> 1 - %02x %02x %02x\n", vv2[0], vv2[1], vv2[2]);
    v = (unsigned short)(vv2[1] & 0x3) << 8 | vv2[2];
    printk(">>>>>>>>>>>>>> %d (%d %%)\n", v, v * 100 / 1023);

    memset(vv2, 0, 3);
    spi_transfernb(vv1, vv2, 3);
    printk(">>>>>> 2 - %02x %02x %02x\n", vv2[0], vv2[1], vv2[2]);
    v = (unsigned short)(vv2[1] & 0x3) << 8 | vv2[2];
    printk(">>>>>>>>>>>>>> %d (%d %%)\n", v, v * 100 / 1023);

    spi_end();

    return 0;

err_free_dev:
    spi_close();
    gpio_clean();
    return err;
}

static void am_test_exit(void)
{
    spi_close();
    gpio_clean();
    printk("test module unloaded\n");
}


module_init(am_test_init);
module_exit(am_test_exit);
