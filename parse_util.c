/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>

#include "parse_util.h"


int parse_number(char** token, char* seps, int radix, int default_value)
{
    int value;
    char* temp_p = strsep(token, seps);

    if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
        value = default_value;
    } else {
        value = simple_strtol(temp_p, NULL, radix);
    }

    return value;
}

char* parse_string(char *str, int len, char** token, char* seps, char* default_value)
{
    char* temp_p = strsep(token, seps);

    if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
        if (default_value != NULL) {
            return strncpy(str, default_value, len);
        }
    } else {
        return strncpy(str, temp_p, len);
    }

    return NULL;
}
