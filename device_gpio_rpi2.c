/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include "gpio_util.h"
#include "parse_util.h"

//#define GPIO_GET(i)   GPIO_READ(i)
//#define GPIO_GET_VALUE(i)   gpio_get_value(i)

#define INPUT_GPIO_CONFIG_TYPE_INDEX    (0)
#define INPUT_GPIO_CONFIG_TYPE_KEYCODE  (1)

typedef struct tag_device_gpio_config {
    int pull_updown;
} device_gpio_config_t;

typedef struct tag_device_gpio_index_item {
    int gpio;
    int button;
    int value;
} device_gpio_index_item_t;

typedef struct tag_device_gpio_index_table {
    device_gpio_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int pin_count;
    int button_start_index;
} device_gpio_index_table_t;

typedef struct tag_device_gpio_data {
    device_gpio_config_t      device_cfg;
    device_gpio_index_table_t button_cfgs[1];
} device_gpio_data_t;


#define INPUT_GPIO_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (13)

static const device_gpio_index_table_t default_input1_gpio_config = {
    {
        {4,  ABS_Y,     -DEFAULT_INPUT_ABS_MAX_VALUE},
        {17, ABS_Y,      DEFAULT_INPUT_ABS_MAX_VALUE},
        {27, ABS_X,     -DEFAULT_INPUT_ABS_MAX_VALUE},
        {22, ABS_X,      DEFAULT_INPUT_ABS_MAX_VALUE},
        {10, BTN_START,  1},
        {9,  BTN_SELECT, 1},
        {25, BTN_A,      1},
        {24, BTN_B,      1},
        {15, BTN_X,      1},
        {18, BTN_Y,      1},
        {14, BTN_TL,     1},
        {23, BTN_TR,     1},
        {2,  BTN_MODE,   1},
    },
    INPUT_GPIO_DEFAULT_KEYCODE_TABLE_ITEM_COUNT,
    0
};

static const device_gpio_index_table_t default_input2_gpio_config = {
    {
        {11, ABS_Y,     -DEFAULT_INPUT_ABS_MAX_VALUE},
        {5,  ABS_Y,      DEFAULT_INPUT_ABS_MAX_VALUE},
        {6,  ABS_X,     -DEFAULT_INPUT_ABS_MAX_VALUE},
        {13, ABS_X,      DEFAULT_INPUT_ABS_MAX_VALUE},
        {19, BTN_START,  1},
        {26, BTN_SELECT, 1},
        {21, BTN_A,      1},
        {20, BTN_B,      1},
        {16, BTN_X,      1},
        {12, BTN_Y,      1},
        {7,  BTN_TL,     1},
        {8,  BTN_TR,     1},
        {3,  BTN_MODE,   1},
    }, 
    INPUT_GPIO_DEFAULT_KEYCODE_TABLE_ITEM_COUNT,
    0
};


// device 파라미터 파싱
static int __parse_device_param_for_gpio(device_gpio_data_t* user_data, char* device_config_str)
{
    char szText[512];
    char* pText;
    char* temp_p;

    if (device_config_str != NULL) {
        strcpy(szText, device_config_str); 
        pText = szText;

        temp_p = strsep(&pText, ",");
        if (temp_p == NULL || strcmp(temp_p, "") == 0 || strcmp(temp_p, "default") == 0) {
            user_data->device_cfg.pull_updown = 0;
        } else {
            user_data->device_cfg.pull_updown = simple_strtol(temp_p, NULL, 10);
        }
    } else {
        user_data->device_cfg.pull_updown = 0;
    }

    return 0;
}


// 각 endpoint 파라미터 파싱
static int __parse_endpoint_param_for_gpio(device_gpio_data_t* user_data, char* endpoint_config_str[], input_endpoint_data_t *endpoint_list[], int endpoint_count)
{
    int i, j;
    device_gpio_index_table_t *src, *des;
    int code_mode;
    char szText[512];
    char* pText;
    char* temp_p;

    des = user_data->button_cfgs;

    for (i = 0; i < endpoint_count; i++ ) {
        input_endpoint_data_t *ep = endpoint_list[i];
        char* cfgtype_p;
        int button_start_index, pin_count;

        if (ep == NULL) continue;

        if (endpoint_config_str[i] != NULL) {
            strcpy(szText, endpoint_config_str[i]); 
            pText = szText;

            cfgtype_p = strsep(&pText, ",");
        } else {
            pText = NULL;
            cfgtype_p = NULL;
        }

        if (cfgtype_p == NULL || strcmp(cfgtype_p, "default1") == 0 || strcmp(cfgtype_p, "default") == 0 || strcmp(cfgtype_p, "") == 0){
            src = (device_gpio_index_table_t *)&default_input1_gpio_config;
            code_mode = INPUT_CODE_TYPE_KEYCODE;

            temp_p = strsep(&pText, ",");
            pin_count = parse_number(temp_p, 10, src->pin_count);

            temp_p = strsep(&pText, ",");
            button_start_index = parse_number(temp_p, 10, 0);
        } else if (strcmp(cfgtype_p, "default2") == 0){
            src = (device_gpio_index_table_t *)&default_input2_gpio_config;
            code_mode = INPUT_CODE_TYPE_KEYCODE;

            temp_p = strsep(&pText, ",");
            pin_count = parse_number(temp_p, 10, src->pin_count);

            temp_p = strsep(&pText, ",");
            button_start_index = parse_number(temp_p, 10, 0);
        } else if (strcmp(cfgtype_p, "custom") == 0){
            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "keycode") == 0 || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
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
                char *block_p, *gpio_p, *button_p, *value_p;
                int gpio, button, value;

                strsep(&pText, "{");
                block_p = strsep(&pText, "}");
                gpio_p = strsep(&block_p, ",");
                button_p = strsep(&block_p, ",");
                value_p = strsep(&block_p, ",");
                strsep(&pText, ",");

                gpio = (button_p != NULL) ? simple_strtol(gpio_p, NULL, 0) : -1;
                button = (button_p != NULL) ? simple_strtol(button_p, NULL, 0) : -1;
                value = (value_p != NULL) ? simple_strtol(value_p, NULL, 0) : -1;

                // 키 설정 추가
                if (button >= 0 && gpio >= 0) {
                    src->buttondef[pin_count].gpio = gpio;
                    src->buttondef[pin_count].button = button;
                    src->buttondef[pin_count].value = value;
                    pin_count++;
                }
            }
        } else {
            continue;
        }

        if (code_mode == INPUT_CODE_TYPE_KEYCODE) {
            for (j = 0; j < pin_count; j++) {
                int idx = find_input_button_data(ep, src->buttondef[j].button, NULL);
                des->buttondef[j].button = idx;
                if (idx != -1) {
                    des->buttondef[j].gpio = src->buttondef[j].gpio;
                    des->buttondef[j].value = src->buttondef[j].value;
                }
            }
            des->pin_count = pin_count;
            des->button_start_index = button_start_index;
        } else if (code_mode == INPUT_CODE_TYPE_INDEX) {
            for (j = 0; j < pin_count; j++) {
                des->buttondef[j].button = src->buttondef[j].button;
                des->buttondef[j].gpio = src->buttondef[j].gpio;
                des->buttondef[j].value = src->buttondef[j].value;
            }
            des->pin_count = pin_count;
            des->button_start_index = button_start_index;
        }

        des++;
    }

    return 0;
}


// device_config_str : 
// endpoint_config_str : endpoint, config_type (default1 | default2 | custom), ...
//        default1: pin_count, start_index
//        default2: pin_count, start_index
//        custom = code_type (0|1), n * {gpio, button, value}
//            code_type = 0 : key code
//            code_type = 1 : index
//
// ex) device1=gpio;;0,default1
//     device2=gpio;;1,custom,0,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
static int init_input_device_for_gpio(input_device_data_t *device_data, char* device_config_str, char* endpoint_config_strs[])
{
    device_gpio_data_t* user_data;
    int result;

    user_data = (device_gpio_data_t *)kzalloc(sizeof(device_gpio_data_t) + sizeof(device_gpio_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);

    result = __parse_device_param_for_gpio(user_data, device_config_str);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    result = __parse_endpoint_param_for_gpio(user_data, endpoint_config_strs, device_data->target_endpoints, device_data->target_endpoint_count);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    device_data->data = (void *)user_data;

    return 0;
}


static void start_input_device_for_gpio(input_device_data_t *device_data)
{
    int i, j;
    int pull_ups[32], pull_idx;
    device_gpio_index_table_t* table;
    device_gpio_data_t* user_data = (device_gpio_data_t *)device_data->data;

    if (user_data == NULL) return;
    table = user_data->button_cfgs;

    pull_idx = 0;
    for (i = 0; i < device_data->target_endpoint_count; i++) {
        device_gpio_index_item_t* item = (device_gpio_index_item_t*)table->buttondef;

        for (j = 0; j < table->pin_count; j++) {
            if (item->gpio != -1) {
                gpio_as_input(item->gpio);
                pull_ups[pull_idx++] = item->gpio;
            }
            item++;
        }
        table++;
    }

    gpio_pullups(pull_ups, pull_idx);

    device_data->is_opend = TRUE;
}


static void check_input_device_for_gpio(input_device_data_t* device_data)
{
    int i, j;
    device_gpio_index_table_t* table;
    device_gpio_data_t* user_data = (device_gpio_data_t *)device_data->data;
    
    if (user_data == NULL) return;
    table = user_data->button_cfgs;
    
    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_gpio_index_item_t* item = &table->buttondef[table->button_start_index];

        for (j = 0; j < table->pin_count; j++) {
            if (item->gpio != -1) {    // to avoid unused buttons
                if (gpio_get_value(item->gpio) == 0) {
                    endpoint->report_button_state[item->button] = item->value;
                }
            }
            item++;
        }
        table++;
    }
}


static void stop_input_device_for_gpio(input_device_data_t *device_data)
{
    device_data->is_opend = FALSE;
}


int register_input_device_for_gpio(input_device_type_desc_t *device_desc)
{
    strcpy(device_desc->device_type, "gpio");
    device_desc->init_input_dev = init_input_device_for_gpio;
    device_desc->start_input_dev = start_input_device_for_gpio;
    device_desc->check_input_dev = check_input_device_for_gpio;
    device_desc->stop_input_dev = stop_input_device_for_gpio;
    return 0;    
}
