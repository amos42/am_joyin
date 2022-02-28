/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __LOG_UTIL_H_
#define __LOG_UTIL_H_


#include "build_cfg.h"

#include <linux/types.h>
#include <linux/printk.h>


#define LOG_PREFIX		DEVICE_NAME ": "
#define LOG_MSG(msg) 	LOG_PREFIX msg "\n"

#define am_log_err(fmt, ...) 	pr_err(LOG_MSG(fmt), ##__VA_ARGS__)
#define am_log_warn(fmt, ...) 	pr_warn(LOG_MSG(fmt), ##__VA_ARGS__)
#define am_log_info(fmt, ...) 	pr_info(LOG_MSG(fmt), ##__VA_ARGS__)
#define am_log_debug(fmt, ...) 	pr_debug(LOG_MSG(fmt), ##__VA_ARGS__)


#endif
