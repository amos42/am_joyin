/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>

#include "parse_util.h"


int parse_number(char* token, int radix, int default_value)
{
    int value;

    if (token == NULL || strcmp(token, "default") == 0 || strcmp(token, "") == 0) {
        value = default_value;
    } else {
        value = simple_strtol(token, NULL, radix);
    }

    return value;
}
