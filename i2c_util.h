/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __I2C_UTIL_H_
#define __I2C_UTIL_H_


#include <linux/types.h>


int i2c_init(u32 peri_base_addr, int size);
void i2c_write(char dev_addr, char reg_addr, char *buf, int len);
void i2c_read(char dev_addr, char reg_addr, char *buf, int len);
void wait_i2c_done(void);


#endif
