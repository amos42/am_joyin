/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>
#include <linux/input.h>

#include "indev_type.h"


/** input.h에 정의 된 키 일부
#define ABS_X			0x00
#define ABS_Y			0x01
#define ABS_MAX			0x3f

#define BTN_TRIGGER		0x120

#define BTN_GAMEPAD		0x130
#define BTN_SOUTH		0x130
#define BTN_A			BTN_SOUTH
#define BTN_EAST		0x131
#define BTN_B			BTN_EAST
#define BTN_C			0x132
#define BTN_NORTH		0x133
#define BTN_X			BTN_NORTH
#define BTN_WEST		0x134
#define BTN_Y			BTN_WEST
#define BTN_Z			0x135
#define BTN_TL			0x136
#define BTN_TR			0x137
#define BTN_TL2			0x138
#define BTN_TR2			0x139
#define BTN_SELECT		0x13a
#define BTN_START		0x13b
#define BTN_MODE		0x13c
#define BTN_THUMBL		0x13d
#define BTN_THUMBR		0x13e
*/


// const input_button_data_t default_buttonset[DEFAULT_INPUT_BUTTON_COUNT] = {
//     {ABS_Y,      -100, 100},
//     {ABS_X,      -100, 100},
//     {BTN_START,   0, 1},
//     {BTN_SELECT,  0, 1},
//     {BTN_A,       0, 1},
//     {BTN_B,       0, 1},
//     {BTN_X,       0, 1},
//     {BTN_Y,       0, 1},
//     {BTN_TL,      0, 1},
//     {BTN_TR,      0, 1},
//     {BTN_MODE,    0, 1},
//     {BTN_TL2,     0, 1},
//     {BTN_TR2,     0, 1},
//     {BTN_TRIGGER, 0, 1}
// };

const input_button_data_t default_buttonset[DEFAULT_INPUT_BUTTON_COUNT] = DEFAULT_INPUT_BUTTONS;
const input_button_data_t default_abs_buttonset[DEFAULT_INPUT_ABS_STICKS_COUNT] = DEFAULT_INPUT_ABS_STICKS;
const input_button_data_t default_mouse_buttonset[DEFAULT_INPUT_MOUSE_COUNT] = DEFAULT_INPUT_MOUSE;


/**
 * 버튼 코드를 이용해 endpoint에 할당 된 버튼 정보를 얻는다.
*/
int find_input_button_data(input_endpoint_data_t *endpoint, int button_code, input_button_data_t *button_data)
{
    int i, cnt;
    input_buttonset_data_t *buttonset;

    buttonset = endpoint->target_buttonset;
    if (buttonset == NULL) {
        return -1;
    }

    cnt = endpoint->button_count;
    for (i = 0; i < cnt; i++) {
        if (button_code == buttonset->button_data[i].button_code) {
            if (button_data != NULL) *button_data = buttonset->button_data[i];
            return i;
        }
    }

    return -1;
}
