/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "log_util.h"
#include "gpio_util.h"
#include "bcm_peri.h"
#include "bcm2835.h"


//#define PERI_BASE	bcm_peri_base
#define GPIO_BASE           (0x200000) /* GPIO controller */


#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define GPIO_READ(g)  *(gpio + 13) &= (1<<(g))

#define GET_GPIO(g) (*(gpio+13) & (1<<(g)))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)
#define GPIO_CLR *(gpio+10)


/*
 * defines for BCM 2711
 *
 * refer to "Chapter 5. General Purpose I/O (GPIO)"
 * in "BCM2711 ARM Peripherals", 2020-02-05
 */
#define PUD_2711_MASK		    (0x3)
#define PUD_2711_REG_OFFSET(p)	((p) / 16)
#define PUD_2711_REG_SHIFT(p)	(((p) % 16) * 2)
#define BCM2711_PULL_UP		    (0x1)

/* BCM 2711 has a different mechanism for pin pull-up/down/enable  */
#define GPIO_PUP_PDN_CNTRL_REG0 (57)      /* Pin pull-up/down for pins 15:0  */
#define GPIO_PUP_PDN_CNTRL_REG1 (58)      /* Pin pull-up/down for pins 31:16 */
#define GPIO_PUP_PDN_CNTRL_REG2 (59)      /* Pin pull-up/down for pins 47:32 */
#define GPIO_PUP_PDN_CNTRL_REG3 (60)      /* Pin pull-up/down for pins 57:48 */


volatile uint32_t* gpio = NULL;

static int is_2711;


static void gpio_pullups_mask_2835(unsigned pull_ups)
{
    // *(gpio + 37) = 0x02;
    // udelay(10);
    // *(gpio + 38) = pull_ups;
    // udelay(10);
    // *(gpio + 37) = 0x00;
    // *(gpio + 38) = 0x00;

    *(gpio + BCM2835_GPPUD/4) = 0x02;
    udelay(10);
    *(gpio + BCM2835_GPPUDCLK0/4) = pull_ups;
    udelay(10);
    *(gpio + BCM2835_GPPUD/4) = 0x00;
    *(gpio + BCM2835_GPPUDCLK0/4) = 0x00;
}


static void gpio_pullups_2835(int gpio_map[], int count)
{
    gpio_pullups_mask_2835(gpio_get_pullup_mask(gpio_map, count));
}


static void gpio_pullups_2711(int gpio_map[], int count)
{
    int i;
    for (i = 0; i < count; i++) {
        if (gpio_map[i] != -1) {
            u32 pud_reg = GPIO_PUP_PDN_CNTRL_REG0 + PUD_2711_REG_OFFSET(gpio_map[i]);
            u32 shift = PUD_2711_REG_SHIFT(gpio_map[i]);
            u32 val = *(gpio + pud_reg);
            val &= ~(PUD_2711_MASK << shift);
            val |= (BCM2711_PULL_UP << shift);
            *(gpio + pud_reg) = val;
        }
    }
}


void gpio_pullups(int gpio_map[], int count)
{
    if (is_2711) {
        gpio_pullups_2711(gpio_map, count);
    } else {
        gpio_pullups_2835(gpio_map, count);
    }
}


void gpio_as_input(int gpio_no)
{
    INP_GPIO(gpio_no);
}


void gpio_as_output(int gpio_no)
{
    OUT_GPIO(gpio_no);
}


void gpio_fsel(int pin, uint8_t mode)
{
    volatile uint32_t* paddr = gpio + BCM2835_GPFSEL0/sizeof(uint32_t) + (pin/10);
    uint8_t   shift = (pin % 10) * 3;
    uint32_t  mask = BCM2835_GPIO_FSEL_MASK << shift;
    uint32_t  value = mode << shift;
    bcm_peri_set_bits(paddr, value, mask);
}


unsigned gpio_get_pullup_mask(int gpio_map[], int count)
{
    int mask = 0x0000000;
    int i;
    for (i = 0; i < count; i++) {
        if (gpio_map[i] != -1) {   // to avoid unused pins
            int pin_mask = 1 << gpio_map[i];
            mask |= pin_mask;
        }
    }
    return mask;
}


unsigned gpio_get_value(int gpio_no)
{
    return GPIO_READ(gpio_no);
}


void gpio_set_value(int gpio_no, int onoff)
{
    if (onoff)
        GPIO_SET = ((unsigned)1 << gpio_no);
    else
        GPIO_CLR = ((unsigned)1 << gpio_no);
}


int gpio_init(u32 peri_base_addr)
{
    //peri_base = bcm_peri_base_probe();
    //peri_base = peri_base_addr;
    if (!peri_base_addr) {
        am_log_err("failed to find peripherals address base via device-tree - not a Raspberry PI board ?");
        return -EINVAL;
    }

    /* Set up gpio pointer for direct register access */
    if ((gpio = (volatile uint32_t *)ioremap(peri_base_addr + GPIO_BASE, 0xB0)) == NULL) {
        am_log_err("io remap failed");
        return -EINVAL;
    }

    is_2711 = *(gpio + GPIO_PUP_PDN_CNTRL_REG3) != 0x6770696f;

    return 0;
}


void gpio_clean(void)
{
    if (gpio != NULL) {
        iounmap((void *)gpio);
        gpio = NULL;
    }
}