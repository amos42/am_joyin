/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __PARSE_UTIL_H_
#define __PARSE_UTIL_H_


#include <linux/types.h>


int parse_number(char** token, char* seps, int radix, int default_value);
char* parse_string(char *str, int len, char** token, char* seps, char* default_value);


#endif
