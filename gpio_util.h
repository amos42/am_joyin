/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __GPIO_UTIL_H_
#define __GPIO_UTIL_H_


#include <linux/types.h>


int gpio_init(u32 peri_base_addr);
void gpio_clean(void);

void gpio_as_input(int gpio_no);
void gpio_as_output(int gpio_no);

void gpio_fsel(int pin, uint8_t mode);

void gpio_pullups_mask(unsigned pull_ups);
void gpio_pullups(int gpio_map[], int count);
unsigned gpio_get_pullup_mask(int gpio_map[], int count);

unsigned gpio_get_value(int gpio_no);
void gpio_set_value(int gpio_no, int onoff);


#endif
