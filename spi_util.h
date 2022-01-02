/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __SPI_UTIL_H_
#define __SPI_UTIL_H_


#include <linux/types.h>


#define MAX_SPI_CHANNEL_COUNT   (2)


int  spi_init(u32 peri_base_addr, int size);
void spi_close(void);
int  spi_begin(void);
void spi_end(void);
void spi_setClockDivider(uint16_t divider);
void spi_setDataMode(uint8_t mode);
void spi_chipSelect(uint8_t cs);
void spi_setChipSelectPolarity(uint8_t cs, uint8_t active);
uint8_t spi_transfer(uint8_t value);
void spi_transfernb(char* tbuf, char* rbuf, uint32_t len);
void spi_writenb(const char* tbuf, uint32_t len);
void spi_transfern(char* buf, uint32_t len);
void spi_write(uint16_t data);


#endif
