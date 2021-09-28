#include <linux/kernel.h>
#include <linux/input.h>

#include "indev_type.h"


/** input.h에 정의 된 키 일부
#define ABS_X			0x00
#define ABS_Y			0x01
#define ABS_MAX			0x3f

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
//     {ABS_Y,     -1, 1},
//     {ABS_X,     -1, 1},
//     {BTN_START,  0, 1},
//     {BTN_SELECT, 0, 1},
//     {BTN_A,      0, 1},
//     {BTN_B,      0, 1},
//     {BTN_X,      0, 1},
//     {BTN_Y,      0, 1},
//     {BTN_TL,     0, 1},
//     {BTN_TR,     0, 1},
//     {BTN_MODE,   0, 1},
//     {BTN_TL2,    0, 1},
//     {BTN_TR2,    0, 1},
//     {BTN_Z,      0, 1}
// };

const input_button_data_t default_buttonset[DEFAULT_INPUT_BUTTON_COUNT] = DEFAULT_INPUT_BUTTONS;



// // 버튼셋 리스트. 기본적으로 1개 포함
// input_buttonset_data_t buttonset_list[MAX_INPUT_BUTTONSET_COUNT]; // = {
// //     {
// //         {
// //             {ABS_Y,     -1, 1},
// //             {ABS_X,     -1, 1},
// //             {BTN_START,  0, 1},
// //             {BTN_SELECT, 0, 1},
// //             {BTN_A,      0, 1},
// //             {BTN_B,      0, 1},
// //             {BTN_X,      0, 1},
// //             {BTN_Y,      0, 1},
// //             {BTN_TL,     0, 1},
// //             {BTN_TR,     0, 1},
// //             {BTN_MODE,   0, 1},
// //             {BTN_TL2,    0, 1},
// //             {BTN_TR2,    0, 1},
// //             {BTN_Z,      0, 1}
// //         },
// //         DEFAULT_INPUT_BUTTON_COUNT
// //     },    
// // };
// int input_buttonset_count = 0; //MAX_INPUT_DEFAULT_BUTTONSET_COUNT;

// // endpoint 리스트
// input_endpoint_data_t endpoint_list[MAX_INPUT_ENDPOINT_COUNT];
// int input_endpoint_count = 0;

// // 징치 타입 기술 리스트
// input_device_type_desc_t device_type_desc_list[MAX_INPUT_DEVICE_TYPE_DESC_COUNT];
// int input_device_type_desc_count;

// // 장치 리스트
// input_device_data_t device_list[MAX_INPUT_DEVICE_COUNT];
// int input_device_count = 0;


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
