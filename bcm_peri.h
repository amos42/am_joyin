/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __BCM_PERI_H_
#define __BCM_PERI_H_


#include <linux/types.h>


u32 bcm_peri_base_probe(void);

uint32_t bcm_peri_read(volatile uint32_t* paddr);
uint32_t bcm_peri_read_nb(volatile uint32_t* paddr);
void bcm_peri_write(volatile uint32_t* paddr, uint32_t value);
void bcm_peri_write_nb(volatile uint32_t* paddr, uint32_t value);
void bcm_peri_set_bits(volatile uint32_t* paddr, uint32_t value, uint32_t mask);


#endif
