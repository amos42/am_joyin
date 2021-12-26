/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __I2C_UTIL_H_
#define __I2C_UTIL_H_


#include <linux/types.h>


int  i2c_init(u32 peri_base_addr, int size);
void i2c_close(void);
void i2c_raw_write(char dev_addr, char *buf, int len);
void i2c_raw_read(char dev_addr, char *buf, int len);
void i2c_write(char dev_addr, char reg_addr, char *buf, int len);
void i2c_read(char dev_addr, char reg_addr, char *buf, int len);
void wait_i2c_done(void);
void i2c_write_1byte(char dev_addr, char reg_addr, char value);
void i2c_write_1word(char dev_addr, char reg_addr, short value);
void i2c_raw_write_1byte(char dev_addr, char value);
void i2c_raw_write_1word(char dev_addr, short value);
char i2c_read_1byte(char dev_addr, char reg_addr);
short i2c_read_1word(char dev_addr, char reg_addr);
char i2c_raw_read_1byte(char dev_addr);
short i2c_raw_read_1word(char dev_addr);


#endif
