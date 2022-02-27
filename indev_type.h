/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __INDEV_TYPE_H_
#define __INDEV_TYPE_H_

#ifdef __cplusplus
extern "C" {
#endif


#include "build_cfg.h"

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef TRUE
#define TRUE (1)
#endif
#ifndef FALSE
#define FALSE (0)
#endif

#ifndef NULL
    #ifdef __cplusplus
        #define NULL 0
    #else
        #define NULL ((void *)0)
    #endif
#endif


#define MAX_INPUT_BUTTON_COUNT                (32)

#define MAX_INPUT_DEFAULT_BUTTONSET_COUNT     (3)
#define MAX_INPUT_USERDEF_BUTTONSET_COUNT     (4)
#define MAX_INPUT_BUTTONSET_COUNT             (MAX_INPUT_DEFAULT_BUTTONSET_COUNT + MAX_INPUT_USERDEF_BUTTONSET_COUNT)

#define MAX_INPUT_ENDPOINT_COUNT              (8)

#define MAX_INPUT_DEVICE_TYPE_DESC_COUNT      (12)
#define MAX_INPUT_DEVICE_COUNT                (8)

#define ENDPOINT_TYPE_JOYSTICK                (0)
#define ENDPOINT_TYPE_MOUSE                   (1)
#define ENDPOINT_TYPE_KEYBOARD                (2)

#define INPUT_CODE_TYPE_NONE                  (-1)
#define INPUT_CODE_TYPE_KEYCODE               (0)
#define INPUT_CODE_TYPE_INDEX                 (1)

#define DEFAULT_INPUT_ABS_MAX_VALUE           (100)
#define DEFAULT_INPUT_BUTTON_COUNT            (14)
#define DEFAULT_INPUT_BUTTONS { \
                                {ABS_Y,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {ABS_X,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {BTN_START,     0, 1}, \
                                {BTN_SELECT,    0, 1}, \
                                {BTN_A,         0, 1}, \
                                {BTN_B,         0, 1}, \
                                {BTN_X,         0, 1}, \
                                {BTN_Y,         0, 1}, \
                                {BTN_TL,        0, 1}, \
                                {BTN_TR,        0, 1}, \
                                {BTN_MODE,      0, 1}, \
                                {BTN_TL2,       0, 1}, \
                                {BTN_TR2,       0, 1}, \
                                {BTN_TRIGGER,   0, 1} \
                            }
#define DEFAULT_INPUT_ABS_STICKS_COUNT        (8)
#define DEFAULT_INPUT_ABS_STICKS { \
                                {ABS_X,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {ABS_Y,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {ABS_RX,       -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {ABS_RY,       -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {ABS_THROTTLE, -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {ABS_WHEEL,    -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {ABS_HAT0X,    -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {ABS_HAT0Y,    -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE} \
                            }

#define DEFAULT_INPUT_MOUSE_COUNT             (5)
#define DEFAULT_INPUT_MOUSE { \
                                {REL_X,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {REL_Y,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE}, \
                                {BTN_LEFT,     0, 1}, \
                                {BTN_RIGHT,    0, 1}, \
                                {BTN_MIDDLE,   0, 1} \
                            }

// 개별 버튼 설정
typedef struct tag_input_button_data {
    int button_code;     // 버튼 코드. linux/input.h 참조
    int min_value;      // 버튼의 최소값. 일반 버튼은 0
    int max_value;      // 버튼의 최대값. 일반 버튼은 1
} input_button_data_t;

// 버튼셋 설정
typedef struct tag_input_buttonset_data {
    char buttonset_name[32];
    input_button_data_t button_data[MAX_INPUT_BUTTON_COUNT];
    int button_count;
} input_buttonset_data_t;

// endpoint 설정. 기본적으로 /dev/input/js# 파일에 대응
typedef struct tag_input_endpoint_data {
    int endpoint_id;
    char endpoint_name[32];
    int endpoint_type;

    input_buttonset_data_t* target_buttonset;
    int button_count;

    BOOL is_opened;
    struct input_dev* indev;

    int current_button_state[MAX_INPUT_BUTTON_COUNT];
    int report_button_state[MAX_INPUT_BUTTON_COUNT];
} input_endpoint_data_t;

typedef struct tag_input_device_type_desc input_device_type_desc_t;

// 각 장치 설정
typedef struct tag_input_device_data {
    BOOL is_opend;

    // 장치 타입
    input_device_type_desc_t *device_type_desc;

    // 대상 endpoint들
    input_endpoint_data_t* target_endpoints[MAX_INPUT_ENDPOINT_COUNT];
    int target_endpoint_count;

    // 장치 설정 스트링
    char device_config_str[256];

    // 장치 데이터
    void* data;
} input_device_data_t;

// 장치 타입 정보 설정
typedef struct tag_input_device_type_desc {
    char device_type[16];
    char device_desc_data[64];

    int (*init_input_dev)(void* device_desc_data, input_device_data_t* device_data, char* device_config_str, char* endpoint_config_str[]);
    void (*start_input_dev)(input_device_data_t* device_data);
    void (*check_input_dev)(input_device_data_t* device_data);
    void (*stop_input_dev)(input_device_data_t* device_data);
} input_device_type_desc_t;


extern const input_button_data_t default_buttonset[DEFAULT_INPUT_BUTTON_COUNT];
extern const input_button_data_t default_abs_buttonset[DEFAULT_INPUT_ABS_STICKS_COUNT];
extern const input_button_data_t default_mouse_buttonset[DEFAULT_INPUT_MOUSE_COUNT];


int find_input_button_data(input_endpoint_data_t* endpoint, int button_code, input_button_data_t* button_data);


#ifdef __cplusplus
}
#endif

#endif
