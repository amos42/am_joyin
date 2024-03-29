﻿/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __AM_JOYIN_H_
#define __AM_JOYIN_H_


#include "build_cfg.h"

#include <linux/kernel.h>
#include <linux/mutex.h>

#include "indev_type.h"

#if !defined(KBUILD_MODNAME)
#define KBUILD_MODNAME  DEVICE_NAME
#endif


typedef struct tag_am_joyin_data {
    // 키체크 주기
    unsigned long report_period;
    // 디버그 모드
    BOOL is_debug;

    // 버튼셋 리스트
    input_buttonset_data_t buttonset_list[MAX_INPUT_BUTTONSET_COUNT];
    int input_buttonset_count;

    // endpoint 리스트
    input_endpoint_data_t endpoint_list[MAX_INPUT_ENDPOINT_COUNT];
    int input_endpoint_count;

    // 징치 타입 기술 리스트
    input_device_type_desc_t device_type_desc_list[MAX_INPUT_DEVICE_TYPE_DESC_COUNT];
    int input_device_type_desc_count;

    // 장치 리스트
    input_device_data_t device_list[MAX_INPUT_DEVICE_COUNT];
    int input_device_count;

    // 런타임 참조 정보
    int used;
#if defined(USE_REPORT_TIMER)
    unsigned long timer_period;
    struct timer_list report_timer;
#else
    struct task_struct* report_task;
#endif
    struct mutex mutex;
    int missing_timer_count;
} am_joyin_data_t;


#endif
