#ifndef __AM_JOYIN_H_
#define __AM_JOYIN_H_


#include <linux/kernel.h>
#include <linux/mutex.h>

#include "devices/indev_type.h"

typedef struct tag_am_joyin_data {
    int used;
    struct mutex mutex;
    struct timer_list timer;

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
} am_joyin_data_t;


#endif
