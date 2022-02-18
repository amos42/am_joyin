/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>

#include "indev_type.h"
#include "am_joyin.h"

#include "parse_util.h"

/*
#include <stdlib.h>
#define simple_strtol(a,b,c)  strtol(a,b,c)

#include <string.h>
char *strsep(char **stringp, const char *delim)
{
    char *ptr = *stringp;
    if (ptr == NULL)
    {
        return NULL;
    }
    while (**stringp)
    {
        if (strchr(delim, **stringp) != NULL)
        {
            **stringp = 0x00;
            (*stringp)++;
            return ptr;
        }
        (*stringp)++;
    }
    *stringp = NULL;
    return ptr;
}
*/


#define AM_PARAM_ARRAY_VARIABLE(N, I) static char *N[I] /*__initdata*/;
#define AM_PARAM_DEFINE(N, V)           \
    module_param_named(N, V, charp, 0); \
    MODULE_PARM_DESC(N, "Joystick device " #N " button config parameters");

#define AM_PARAM_DEFINE_WITH_VARIABLE(N, V) \
    static char *V /*__initdata*/;          \
    module_param_named(N, V, charp, 0);     \
    MODULE_PARM_DESC(N, "Joystick device " #N " button config parameters");

AM_PARAM_DEFINE_WITH_VARIABLE(drivercfg, am_driver_cfg)

AM_PARAM_ARRAY_VARIABLE(am_buttonset_cfg, MAX_INPUT_BUTTONSET_COUNT)
AM_PARAM_DEFINE(buttonset1, am_buttonset_cfg[0])
AM_PARAM_DEFINE(buttonset2, am_buttonset_cfg[1])

AM_PARAM_DEFINE_WITH_VARIABLE(endpoints, am_endpoints_cfg)

AM_PARAM_ARRAY_VARIABLE(am_device_cfg, MAX_INPUT_DEVICE_COUNT)
AM_PARAM_DEFINE(device1, am_device_cfg[0])
AM_PARAM_DEFINE(device2, am_device_cfg[1])
AM_PARAM_DEFINE(device3, am_device_cfg[2])
AM_PARAM_DEFINE(device4, am_device_cfg[3])


void /*__init*/ prepocess_params(void)
{
    int i, cnt;

    // endpoint 생략시, 기본 파라미터 세팅
    if (am_endpoints_cfg == NULL)
    {
        am_endpoints_cfg = "joystick";
    }

    // device 모두 생략시, 기본 파라미터 세팅
    cnt = 0;    
    for (i = 0; i < MAX_INPUT_DEVICE_COUNT; i++)
    {
        if (am_device_cfg[i] != NULL)
            cnt++;
    }
    if (cnt <= 0)
    {
        am_device_cfg[0] = "gpio;;0,default1";
    }
}

static input_buttonset_data_t* __find_buttonset(am_joyin_data_t* a_input, char* buttonset_name, int exclude_count)
{
    int i;
    int target_buttonset_count, buttonset_idx;
    char* endptr;

    if (buttonset_name == NULL || strcmp(buttonset_name, "default") == 0 || strcmp(buttonset_name, "") == 0) {
        return &a_input->buttonset_list[0];
    }

    target_buttonset_count = a_input->input_buttonset_count - exclude_count;
    buttonset_idx = simple_strtol(buttonset_name, &endptr, 10);

    if (endptr != buttonset_name && buttonset_idx < target_buttonset_count) {
        return &a_input->buttonset_list[buttonset_idx];
    } else {
        for (i = 0; i < target_buttonset_count; i++) {
            if (strcmp(buttonset_name, a_input->buttonset_list[i].buttonset_name) == 0) {
                return &a_input->buttonset_list[i];
            }
        }
    }

    return NULL;
}

static int __append_button(input_buttonset_data_t* target_buttonset, int key_code, int min_value, int max_value)
{
    int i;
    int idx = -1;

    if (key_code < 0) return -1;

    for (i = 0; i < target_buttonset->button_count; i++) {
        if (key_code == target_buttonset->button_data[i].button_code) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        if (target_buttonset->button_count >= MAX_INPUT_BUTTON_COUNT) {
            return -1;
        }
        idx = target_buttonset->button_count++;
    }
    target_buttonset->button_data[idx].button_code = key_code;
    target_buttonset->button_data[idx].min_value = min_value;
    target_buttonset->button_data[idx].max_value = max_value;

    return idx;
}

void parsing_device_config_params(am_joyin_data_t* a_input)
{
    int i, j;
    char szText[512];
    char* pText;
    int idx;
    input_buttonset_data_t* target_buttonset;

    // default driver 설정 초기화
    a_input->report_period = 0;
    a_input->is_debug = FALSE;

    // default buttonset 설정 초기화
    target_buttonset = &a_input->buttonset_list[0];
    strcpy(target_buttonset->buttonset_name, "default");
    for (i = 0; i < DEFAULT_INPUT_BUTTON_COUNT; i++) {
        target_buttonset->button_data[i] = default_buttonset[i];
    }
    target_buttonset->button_count = DEFAULT_INPUT_BUTTON_COUNT;
    target_buttonset = &a_input->buttonset_list[1];
    strcpy(target_buttonset->buttonset_name, "default_abs");
    for (i = 0; i < DEFAULT_INPUT_ABS_STICKS_COUNT; i++) {
        target_buttonset->button_data[i] = default_abs_buttonset[i];
    }
    target_buttonset->button_count = DEFAULT_INPUT_ABS_STICKS_COUNT;
    target_buttonset = &a_input->buttonset_list[2];
    strcpy(target_buttonset->buttonset_name, "default_mouse");
    for (i = 0; i < DEFAULT_INPUT_MOUSE_COUNT; i++) {
        target_buttonset->button_data[i] = default_mouse_buttonset[i];
    }
    target_buttonset->button_count = DEFAULT_INPUT_MOUSE_COUNT;
    a_input->input_buttonset_count = MAX_INPUT_DEFAULT_BUTTONSET_COUNT;

    // 드라이버 설정
    if (am_driver_cfg != NULL) {
        char* debug_p;

        strcpy(szText, am_driver_cfg);
        pText = szText;

        a_input->report_period = parse_number(&pText, ",", 10, 0);

        debug_p = strsep(&pText, ",");
        if (debug_p != NULL && strcmp(debug_p, "debug") == 0) {
            a_input->is_debug = TRUE;
        }
    }

    // buttonset 설정
    for (i = 0; i < MAX_INPUT_BUTTONSET_COUNT; i++) {
        if (am_buttonset_cfg[i] == NULL) continue;

        target_buttonset = &a_input->buttonset_list[a_input->input_buttonset_count++];
        snprintf(target_buttonset->buttonset_name, 31, "buttonset%d", i + 1);

        strcpy(szText, am_buttonset_cfg[i]);
        pText = szText;

        while (pText != NULL) {
            char *section = strsep(&pText, ";");
            if(section[0] == '{') {
                while (section != NULL) {
                    char *block_p;
                    int key_code, min_value, max_value;

                    strsep(&section, "{");
                    block_p = strsep(&section, "}");
                    key_code = parse_number(&block_p, ",", 0, -1);
                    min_value = parse_number(&block_p, ",", 10, 0);
                    max_value = parse_number(&block_p, ",", 10, 0);
                    strsep(&section, ",");

                    // 키 설정 추가
                    if (__append_button(target_buttonset, key_code, min_value, max_value) < 0) {
                        break;
                    }
                }
            } else {
                char* parent_name = strsep(&section, ",");
                int start_idx = parse_number(&section, ",", 10, 0);
                int count = parse_number(&section, ",", 10, -1);

                input_buttonset_data_t* src_buttonset = __find_buttonset(a_input, parent_name, 1);
                if (src_buttonset != NULL) {
                    input_button_data_t *btn;
                    if (count < 0 || (start_idx + count > src_buttonset->button_count)) {
                        count = src_buttonset->button_count - start_idx;
                    }
                    btn = &src_buttonset->button_data[start_idx];
                    for (j = 0; j < count; j++ ){
                        if (__append_button(target_buttonset, btn->button_code, btn->min_value, btn->max_value) < 0) {
                            break;
                        }
                        btn++;
                    }
                }
            }
        }
    }

    if (am_endpoints_cfg != NULL) {
        int js_idx, mos_idx, kbd_idx;

        strcpy(szText, am_endpoints_cfg);
        pText = szText;

        // endpoint 설정
        idx = 0;
        js_idx = mos_idx = kbd_idx = 0;
        while (pText != NULL && idx < MAX_INPUT_ENDPOINT_COUNT) {
            input_buttonset_data_t* btncfg_item;
            char *ptr, *buttonset_name;
            char temp_str[16];
            int endpoint_type;

            ptr = strsep(&pText, ";");

            parse_string(temp_str, 12, &ptr, ",", "joystick");

            if (strcmp(temp_str, "joystick") == 0) {
                endpoint_type = ENDPOINT_TYPE_JOYSTICK;
                js_idx++;
            } else if (strcmp(temp_str, "mouse") == 0) {
                endpoint_type = ENDPOINT_TYPE_MOUSE;
                mos_idx++;
            } else if (strcmp(temp_str, "keyboard") == 0) {
                endpoint_type = ENDPOINT_TYPE_KEYBOARD;
                kbd_idx++;
            } else {
                continue;
            }
            a_input->endpoint_list[idx].endpoint_type = endpoint_type;

            if (parse_string(a_input->endpoint_list[idx].endpoint_name, 31, &ptr, ",", NULL) == NULL) {
                if (endpoint_type == ENDPOINT_TYPE_JOYSTICK) {
                    snprintf(a_input->endpoint_list[idx].endpoint_name, 31, "AmosJoystick_%d", js_idx);
                } else if (endpoint_type == ENDPOINT_TYPE_MOUSE) {
                    snprintf(a_input->endpoint_list[idx].endpoint_name, 31, "AmosMouse_%d", mos_idx);
                } else if (endpoint_type == ENDPOINT_TYPE_KEYBOARD) {
                    snprintf(a_input->endpoint_list[idx].endpoint_name, 31, "AmosKeyboard_%d", kbd_idx);
                }
            }

            a_input->endpoint_list[idx].endpoint_id = idx;

            buttonset_name = strsep(&ptr, ",");
            btncfg_item = __find_buttonset(a_input, buttonset_name, 0);
            if (btncfg_item == NULL) {
                printk(KERN_NOTICE"unknown buttonset \"%s\"", buttonset_name);
                continue;
            }

            a_input->endpoint_list[idx].target_buttonset = btncfg_item;
            a_input->endpoint_list[idx].button_count = parse_number(&ptr, ",", 10, btncfg_item->button_count);

            idx++;
        }
        a_input->input_endpoint_count = idx;
    }

    // device들 설정
    for (i = 0; i < MAX_INPUT_DEVICE_COUNT; i++) {
        input_device_type_desc_t* type_desc;
        input_device_data_t* device;
        char *typeid_p, *device_params_p;
        char* endpoint_params[MAX_INPUT_ENDPOINT_COUNT];

        if (am_device_cfg[i] == NULL) continue;

        strcpy(szText, am_device_cfg[i]);
        pText = szText;

        typeid_p = strsep(&pText, ";");
        if (typeid_p == NULL || strcmp(typeid_p, "") == 0) continue;

        type_desc = NULL;
        for (j = 0; j < a_input->input_device_type_desc_count; j++) {
            input_device_type_desc_t* desc = &a_input->device_type_desc_list[j];
            if (strcmp(desc->device_type, typeid_p) == 0) {
                type_desc = desc;
                break;
            }
        }
        if (type_desc == NULL) {
            printk(KERN_NOTICE"unknown device \"%s\"", typeid_p);
            continue;
        }

        device = &a_input->device_list[a_input->input_device_count];

        device->device_type_desc = type_desc;

        device_params_p = strsep(&pText, ";");

        if (pText != NULL) {
            while (pText != NULL){
                char* ptr = strsep(&pText, ";");

                char* endpoint_p = strsep(&ptr, ",");
                char* endpoint_cfg_p = ptr; // strsep(&ptr, ",");

                int endpoint_idx;
                if (strcmp(endpoint_p, "") == 0) {
                    endpoint_idx = 0;
                }
                else {
                    endpoint_idx = simple_strtol(endpoint_p, NULL, 10);
                }

                if (endpoint_idx < a_input->input_endpoint_count) {
                    device->target_endpoints[device->target_endpoint_count] = &a_input->endpoint_list[endpoint_idx];
                    endpoint_params[device->target_endpoint_count] = endpoint_cfg_p;
                    device->target_endpoint_count++;
                }
            }
        } else {
            device->target_endpoints[0] = &a_input->endpoint_list[0];
            endpoint_params[0] = "0,default";
            device->target_endpoint_count = 1;
        }

        if (type_desc->init_input_dev != NULL) {
            int err = type_desc->init_input_dev((void *)type_desc->device_desc_data, device, device_params_p, endpoint_params);
            if (err != 0) {
                printk(KERN_ERR"invalid initialize parameter for %s", typeid_p);
                continue;
            }
        }

        a_input->input_device_count++;
    }
}

/*

buttonset1_cfg={0x102,-1,1},{0x103,0,1},{0x102,-1,1},{0x103,0,1},{0x102,0,1},{0x103,0,1},{0x102,0,1},{0x103,0,1},{0x102,0,1},{0x103,0,1}

// endpoint 생략시 1개에 0번 키설정에 키 default (13) 개
endpoints=default,0;0,12;1,default // endpoint1_buttonset_cfg=default, endpoint1_key_count=default(13), endpoint2_buttonset_cfg=default, endpoint2_key_count=12, endpoint3_key_cfg=buttonset1, endpoint3_key_count=default

// device_type; endpoint_id, ...; config1=default1
device1=gpio;;0,default1;1,default2

// type=gpio, endpoint1_id=1
device2=gpio;;1,custom,code,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1};2,custom,index,{10,0,-1},{10,0,1},{10,1,1},{10,2,1},{10,3,1},{10,4,1}

// type=gpio, endpoint1_id=1, endpoint2_id=2
device3=gpio;1,custom,code,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}

// type=gpio, endpoint1_id=1, endpoint2_id=2
device4=gpio;2,custom,index,{10,0,-1},{10,0,1},{10,2,1},{10,3,1},{10,4,1},{10,5,1}

// type=74hc165, endpoint1_id=1
device2=74hc165;1,2;16,2,13,1;default,default

// type=74hc165, endpoint1_id=1, endpoint2_id=2
device2=74hc165;21,19,18,24,1;0,default,12;1,default,12

*/

