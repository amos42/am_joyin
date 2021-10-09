/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "gpio_util.h"
#include "parse_util.h"

//#define GPIO_GET(i)   GPIO_READ(i)
//#define GPIO_GET_VALUE(i)   getGpio(i)
//#define GPIO_SET_VALUE(i,v)   setGpio((i), (v))

static const int default_74hc165_gpio_maps[3] = {16, 20, 21};

//#define DEFAULT_74HC165_BUTTON_COUNT  (16)

// static const device_74hc165_index_item_t default_74hc165_button_config[DEFAULT_74HC165_BUTTON_COUNT] = {
//     {ABS_Y,      -1},
//     {ABS_Y,       1},
//     {ABS_X,      -1},
//     {ABS_X,       1}, 
//     {BTN_START,   1},
//     {BTN_SELECT,  1},
//     {BTN_A,       1},
//     {BTN_B,       1},
//     {BTN_X,       1},
//     {BTN_Y,       1},
//     {BTN_TL,      1},
//     {BTN_TR,      1},
//     {BTN_MODE,    1},
//     {BTN_TL2,     1},
//     {BTN_TR2,     1},
//     {BTN_TRIGGER, 1}
// };

typedef struct tag_device_74hc165_config {
    int gpio_ck;
    int gpio_ld;
    int gpio_dt;
    int io_count;
    int bit_order;
    int pull_updown;
} device_74hc165_config_t;

typedef struct tag_device_74hc165_index_item {
    int button;
    int value;
} device_74hc165_index_item_t;

typedef struct tag_device_74hc165_index_table {
    device_74hc165_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int pin_count;
    int button_start_index;
    int io_skip_count;
} device_74hc165_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_74hc165_data {
    device_74hc165_config_t         device_cfg;
    device_74hc165_index_table_t    button_cfgs[1];
} device_74hc165_data_t;


#define INPUT_74HC165_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (16)

static const device_74hc165_index_table_t default_input_74hc165_config = {
    {
        {ABS_Y,      -DEFAULT_INPUT_ABS_MAX_VALUE},
        {ABS_Y,       DEFAULT_INPUT_ABS_MAX_VALUE},
        {ABS_X,      -DEFAULT_INPUT_ABS_MAX_VALUE},
        {ABS_X,       DEFAULT_INPUT_ABS_MAX_VALUE}, 
        {BTN_START,   1},
        {BTN_SELECT,  1},
        {BTN_A,       1},
        {BTN_B,       1},
        {BTN_X,       1},
        {BTN_Y,       1},
        {BTN_TL,      1},
        {BTN_TR,      1},
        {BTN_MODE,    1},
        {BTN_TL2,     1},
        {BTN_TR2,     1},
        {BTN_TRIGGER, 1}
    }, 
    INPUT_74HC165_DEFAULT_KEYCODE_TABLE_ITEM_COUNT,
    0, 0
};


// device 파라미터 파싱
static int __parse_device_param_for_74hc165(device_74hc165_data_t* user_data, char* device_config_str)
{
    char szText[512];
    char* pText;

    strcpy(szText, device_config_str); 
    pText = szText;

    user_data->device_cfg.gpio_ld = parse_number(&pText, ",", 0, -1);
    if (user_data->device_cfg.gpio_ld < 0) return -EINVAL;

    user_data->device_cfg.gpio_ck = parse_number(&pText, ",", 0, -1);
    if (user_data->device_cfg.gpio_ck < 0) return -EINVAL;

    user_data->device_cfg.gpio_dt = parse_number(&pText, ",", 0, -1);
    if (user_data->device_cfg.gpio_dt < 0) return -EINVAL;

    user_data->device_cfg.io_count = parse_number(&pText, ",", 10, INPUT_74HC165_DEFAULT_KEYCODE_TABLE_ITEM_COUNT);
    user_data->device_cfg.bit_order = parse_number(&pText, ",", 10, 0);
    user_data->device_cfg.pull_updown = parse_number(&pText, ",", 10, 0);

    return 0;
}


// 각 endpoint 파라미터 파싱
static int __parse_endpoint_param_for_74hc165(device_74hc165_data_t* user_data, char* endpoint_config_str[], input_endpoint_data_t *endpoint_list[], int endpoint_count)
{
    int i, j;
    int code_mode;
    device_74hc165_index_table_t *src, *des;
    char szText[512];
    char* pText;
    char* temp_p;
    
    des = user_data->button_cfgs;

    for (i = 0; i < endpoint_count; i++ ) {
        input_endpoint_data_t *ep = endpoint_list[i];
        char* cfgtype_p;
        int pin_count, button_start_index, io_skip_count;

        if (ep == NULL) continue;

        if (endpoint_config_str[i] != NULL) {
            strcpy(szText, endpoint_config_str[i]); 
            pText = szText;

            cfgtype_p = strsep(&pText, ",");
        } else {
            pText = NULL;
            cfgtype_p = NULL;
        }

        if (cfgtype_p == NULL || strcmp(cfgtype_p, "default") == 0 || strcmp(cfgtype_p, "") == 0){
            src = (device_74hc165_index_table_t *)&default_input_74hc165_config;
            code_mode = INPUT_CODE_TYPE_KEYCODE;

            pin_count = parse_number(&pText, ",", 10, src->pin_count);
            button_start_index = parse_number(&pText, ",", 10, 0);
            io_skip_count = parse_number(&pText, ",", 10, 0);

            // 버튼 갯수를 보정
            if (button_start_index + pin_count > src->pin_count) {
                pin_count = src->pin_count - button_start_index;
            }
        } else if (strcmp(cfgtype_p, "custom") == 0){
            io_skip_count = parse_number(&pText, ",", 10, 0);

            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "keycode") == 0 || strcmp(temp_p, "0") == 0 || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                code_mode = INPUT_CODE_TYPE_KEYCODE;
            } else if (strcmp(temp_p, "index") == 0 || strcmp(temp_p, "1") == 0) {
                code_mode = INPUT_CODE_TYPE_INDEX;
            } else {
                code_mode = INPUT_CODE_TYPE_NONE;
            }

            button_start_index = 0;
            pin_count = 0;
            src = &user_data->button_cfgs[i];
            while (pText != NULL && pin_count < MAX_INPUT_BUTTON_COUNT) {
                char *block_p, *button_p, *value_p;
                int button, value;

                strsep(&pText, "{");
                block_p = strsep(&pText, "}");
                button_p = strsep(&block_p, ",");
                value_p = strsep(&block_p, ",");
                strsep(&pText, ",");

                button = (button_p != NULL) ? simple_strtol(button_p, NULL, 0) : -1;
                value = (value_p != NULL) ? simple_strtol(value_p, NULL, 0) : 0;

                // 키 설정 추가
                src->buttondef[pin_count].button = button;
                src->buttondef[pin_count].value = value;
                pin_count++;
            }
        } else {
            continue;
        }

        if (code_mode == INPUT_CODE_TYPE_KEYCODE) {
            for (j = 0; j < pin_count; j++) {
                int idx = find_input_button_data(ep, src->buttondef[j].button, NULL);
                des->buttondef[j].button = idx;
                if (idx != -1) {
                    des->buttondef[j].value = src->buttondef[j].value;
                }
            }
            des->pin_count = pin_count;
            des->button_start_index = button_start_index;
            des->io_skip_count = io_skip_count;
        } else if (code_mode == INPUT_CODE_TYPE_INDEX) {
            for (j = 0; j < pin_count; j++) {
                des->buttondef[j].button = src->buttondef[j].button;
                des->buttondef[j].value = src->buttondef[j].value;
            }
            des->pin_count = pin_count;
            des->button_start_index = button_start_index;
            des->io_skip_count = io_skip_count;
        }

        des++;
    }

    return 0;
}


// device_config_str : ck, ld, rd, io_count, bit_order (0 | 1)
//        bit_order: 0 = assending, 1 = descending
//        default: pin_count, start_index, io_skip_count
//        custom: io_skip_count, code_type (0|1), n * {button, value}
//            code_type = 0 : key code
//            code_type = 1 : index
//
// ex) device1=74hc165;16,20,21,13,1;0,default,12
//     device2=74hc165;16,20,21,24,1;0,default,12;1,custom,,0,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
static int init_input_device_for_74hc165(input_device_data_t *device_data, char* device_config_str, char* endpoint_config_strs[])
{
    device_74hc165_data_t* user_data;
    int result;
    
    if (device_config_str == NULL || strcmp(device_config_str, "") == 0) return -EINVAL;

    user_data = (device_74hc165_data_t *)kzalloc(sizeof(device_74hc165_data_t) + sizeof(device_74hc165_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);

    result = __parse_device_param_for_74hc165(user_data, device_config_str);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    result = __parse_endpoint_param_for_74hc165(user_data, endpoint_config_strs, device_data->target_endpoints, device_data->target_endpoint_count);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    // if (user_data->device_cfg.io_count > all_io_count) {
    //     user_data->device_cfg.io_count = all_io_count;
    // }

    device_data->data = (void *)user_data;

    return 0;
}


static void start_input_device_for_74hc165(input_device_data_t *device_data)
{
    device_74hc165_data_t *user_data = (device_74hc165_data_t *)device_data->data;

    gpio_as_output(user_data->device_cfg.gpio_ck);
    gpio_as_output(user_data->device_cfg.gpio_ld);
    gpio_as_input(user_data->device_cfg.gpio_dt);

    device_data->is_opend = TRUE;
}


static void check_input_device_for_74hc165(input_device_data_t *device_data)
{
    int i, j, cnt;
    int ld, ck, dt, io_count;
    device_74hc165_data_t *user_data = (device_74hc165_data_t *)device_data->data;

    if (user_data == NULL) return;

    ck = user_data->device_cfg.gpio_ck;
    ld = user_data->device_cfg.gpio_ld;
    dt = user_data->device_cfg.gpio_dt;
    io_count = user_data->device_cfg.io_count;

    if (io_count <= 0) return;

    gpio_set_value(ld, 0);
    udelay(5);
    gpio_set_value(ld, 1);

    if (user_data->device_cfg.bit_order == 0) {
        for (i = 0; i < device_data->target_endpoint_count; i++) {
            input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
            device_74hc165_index_table_t* table = &user_data->button_cfgs[i];
            device_74hc165_index_item_t* btndef = &table->buttondef[table->button_start_index];

            cnt = table->io_skip_count;
            if (cnt > 0) {
                if (cnt > io_count) cnt = io_count;
                io_count -= cnt;

                for (j = 0; j < cnt; j++ ){
                    gpio_set_value(ck, 1);
                    udelay(5);
                    gpio_set_value(ck, 0);
                }

                if (io_count <= 0) break;
            }

            cnt = table->pin_count;
            if (cnt > io_count) cnt = io_count;
            io_count -= cnt;

            for (j = 0; j < cnt; j++ ){
                if (btndef->button >= 0 && gpio_get_value(dt) == 0) {
                    endpoint->report_button_state[btndef->button] = btndef->value;
                }

                gpio_set_value(ck, 1);
                udelay(5);
                gpio_set_value(ck, 0);

                btndef++;
            }

            if (io_count <= 0) break;
        }
    } else if (user_data->device_cfg.bit_order == 1) {
        int endpoint_idx = 0;
        input_endpoint_data_t* endpoint = device_data->target_endpoints[endpoint_idx];
        device_74hc165_index_table_t* table = &user_data->button_cfgs[endpoint_idx];
        device_74hc165_index_item_t* btndef = &table->buttondef[table->button_start_index];

        int chips = io_count / 8 + ((io_count % 8)? 1 : 0);

        int io_skip_count = table->io_skip_count;

        int idx = 0;
        for (i = 0; i < chips; i++ ){
            int data[8];

            for (j = 0; j < 8; j++ ){
                data[7 - j] = gpio_get_value(dt);

                gpio_set_value(ck, 1);
                udelay(5);
                gpio_set_value(ck, 0);
            }

            cnt = (io_count > 8)? 8 : io_count;
            io_count -= cnt;

            if (io_skip_count < cnt) {
                j = io_skip_count;
                while (cnt--){
                    if (btndef->button >= 0 && data[j] == 0) {
                        endpoint->report_button_state[btndef->button] = btndef->value;
                    }
                    btndef++;
                    if (++idx >= table->pin_count) {
                        if (++endpoint_idx >= device_data->target_endpoint_count) break;
                        endpoint = device_data->target_endpoints[endpoint_idx];
                        table = &user_data->button_cfgs[endpoint_idx];
                        btndef = &table->buttondef[table->button_start_index];
                        io_skip_count = table->io_skip_count;
                        idx = 0;

                        if (j + io_skip_count < cnt) {
                            cnt -= io_skip_count;    
                            io_skip_count = 0;
                        } else {
                            io_skip_count -= cnt - 1 - j;
                            break;    
                        }
                    }

                    j++;
                }
                io_skip_count = 0;
            } else {
                io_skip_count -= cnt;
            }
            
            if (io_count <= 0) break;
        }
    }
}


static void stop_input_device_for_74hc165(input_device_data_t *device_data)
{
    device_data->is_opend = FALSE;
}


int register_input_device_for_74hc165(input_device_type_desc_t *device_desc)
{
    strcpy(device_desc->device_type, "74hc165");
    device_desc->init_input_dev = init_input_device_for_74hc165;
    device_desc->start_input_dev = start_input_device_for_74hc165;
    device_desc->check_input_dev = check_input_device_for_74hc165;
    device_desc->stop_input_dev = stop_input_device_for_74hc165;
    return 0;
}
