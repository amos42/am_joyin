/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __SPI_UTIL_H_
#define __SPI_UTIL_H_


#include <linux/types.h>


int  bcm2835_spi_init(u32 peri_base_addr, int size);
void bcm2835_spi_close(void);
int bcm2835_spi_begin(void);
void bcm2835_spi_end(void);
void bcm2835_spi_setClockDivider(uint16_t divider);
void bcm2835_spi_setDataMode(uint8_t mode);
uint8_t bcm2835_spi_transfer(uint8_t value);
void bcm2835_spi_transfernb(char* tbuf, char* rbuf, uint32_t len);
void bcm2835_spi_writenb(const char* tbuf, uint32_t len);
void bcm2835_spi_transfern(char* buf, uint32_t len);
void bcm2835_spi_setChipSelectPolarity(uint8_t cs, uint8_t active);
void bcm2835_spi_write(uint16_t data);


#endif
