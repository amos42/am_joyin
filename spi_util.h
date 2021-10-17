/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __SPI_UTIL_H_
#define __SPI_UTIL_H_


#include <linux/types.h>


int  spi_init(void);
void spi_write(char dev_addr, char reg_addr, char *buf, int len);
void spi_read(char dev_addr, char reg_addr, char *buf, int len);
void wait_spi_done(void);


#endif
