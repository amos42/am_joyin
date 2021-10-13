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

//#define DEFAULT_MUX_BUTTON_COUNT  (16)

// static const device_mux_index_item_t default_mux_button_config[DEFAULT_MUX_BUTTON_COUNT] = {
        // {ABS_Y,      -1},
        // {ABS_Y,       1},
        // {ABS_X,      -1},
        // {ABS_X,       1}, 
        // {BTN_START,   1},
        // {BTN_SELECT,  1},
        // {BTN_A,       1},
        // {BTN_B,       1},
        // {BTN_X,       1},
        // {BTN_Y,       1},
        // {BTN_TL,      1},
        // {BTN_TR,      1},
        // {BTN_MODE,    1},
        // {BTN_TL2,     1},
        // {BTN_TR2,     1},
        // {BTN_TRIGGER, 1}
// };

#define MAX_MUX_ADDR_REG_COUNT (6)

typedef struct tag_device_mux_config {
    int rw_gpio;
    int addr_gpio[MAX_MUX_ADDR_REG_COUNT];
    int addr_bit_count;

    int io_count;
    //int start_offset;
    int pull_updown;
} device_mux_config_t;

typedef struct tag_device_mux_index_item {
    int button;
    int value;
} device_mux_index_item_t;

typedef struct tag_device_mux_index_table {
    device_mux_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int pin_count;
    int button_start_index;
    int io_skip_count;
    int cs_gpio;
} device_mux_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_mux_data {
    device_mux_config_t         device_cfg;
    device_mux_index_table_t    button_cfgs[1];
} device_mux_data_t;


#define INPUT_MUX_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (16)

static const device_mux_index_table_t default_input_mux_config = {
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
    INPUT_MUX_DEFAULT_KEYCODE_TABLE_ITEM_COUNT,
    0, 0
};


// device 파라미터 파싱
static int __parse_device_param_for_mux(device_mux_data_t* user_data, char* device_config_str)
{
    int i;
    char szText[512];
    char* pText;
    char* temp_p, *block_p;

    strcpy(szText, device_config_str); 
    pText = szText;

    temp_p = strsep(&pText, ",");
    if (temp_p == NULL || strcmp(temp_p, "") == 0) return -EINVAL;
    user_data->device_cfg.rw_gpio = temp_p != NULL ? simple_strtol(temp_p, NULL, 0) : -1;

    user_data->device_cfg.addr_bit_count = 0;
    strsep(&pText, "{");
    block_p = strsep(&pText, "}");
    for (i = 0; i < MAX_MUX_ADDR_REG_COUNT; i++) {
        int gpio = parse_number(&block_p, ",", 10, -1);
        if (gpio < 0) break;
        user_data->device_cfg.addr_gpio[user_data->device_cfg.addr_bit_count++] = gpio;
    }
    if (user_data->device_cfg.addr_bit_count <= 0) return -EINVAL;
    strsep(&pText, ",");

    user_data->device_cfg.io_count = parse_number(&pText, ",", 10, INPUT_MUX_DEFAULT_KEYCODE_TABLE_ITEM_COUNT);
    user_data->device_cfg.pull_updown = parse_number(&pText, ",", 10, 0);

    return 0;
}


// 각 endpoint 파라미터 파싱
static int __parse_endpoint_param_for_mux(device_mux_data_t* user_data, char* endpoint_config_str[], input_endpoint_data_t *endpoint_list[], int endpoint_count)
{
    int i, j;
    int code_mode;
    device_mux_index_table_t *src, *des;
    char szText[512];
    char* pText;
    char* temp_p;

    des = user_data->button_cfgs;

    for (i = 0; i < endpoint_count; i++ ) {
        input_endpoint_data_t *ep = endpoint_list[i];
        char* cfgtype_p;
        int pin_count, button_start_index, io_skip_count, cs_gpio;

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
            src = (device_mux_index_table_t *)&default_input_mux_config;
            code_mode = INPUT_CODE_TYPE_KEYCODE;

            pin_count = parse_number(&pText, ",", 10, src->pin_count);
            button_start_index = parse_number(&pText, ",", 10, 0);
            io_skip_count = parse_number(&pText, ",", 10, 0);
            cs_gpio = parse_number(&pText, ",", 10, -1);
        } else if (strcmp(cfgtype_p, "custom") == 0){
            io_skip_count = parse_number(&pText, ",", 10, 0);
            cs_gpio = parse_number(&pText, ",", 10, -1);

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
                char *block_p, *button_p, *value_p;
                int button, value;

                strsep(&pText, "{");
                block_p = strsep(&pText, "}");
                button_p = strsep(&block_p, ",");
                value_p = strsep(&block_p, ",");
                strsep(&pText, ",");

                button = (button_p != NULL) ? simple_strtol(button_p, NULL, 0) : -1;
                value = (value_p != NULL) ? simple_strtol(value_p, NULL, 0) : -1;

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
            des->cs_gpio = cs_gpio;
        } else if (code_mode == INPUT_CODE_TYPE_INDEX) {
            for (j = 0; j < pin_count; j++) {
                des->buttondef[j].button = src->buttondef[j].button;
                des->buttondef[j].value = src->buttondef[j].value;
            }
            des->pin_count = pin_count;
            des->button_start_index = button_start_index;
            des->io_skip_count = io_skip_count;
            des->cs_gpio = cs_gpio;
        }

        des++;
    }

    return 0;
}


// device_config_str : read_gpio, {addr0_gpio, addr1_gpio, ...}, io_count
// endpoint_config_str : endpoint, config_type (default | custom), ...
//        default: pin_count, start_index, io_skip_count, cs_gpio
//        custom: io_skip_count, cs_gpio, code_type (0|1), n * {button, value}
//            code_type = 0 : key code
//            code_type = 1 : index
//
// ex) device1=mcp23017;0x20,16;0,default,12
//     device2=mcp23017;0x20;1,custom,,0,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
static int init_input_device_for_mux(input_device_data_t *device_data, char* device_config_str, char* endpoint_config_strs[])
{
    device_mux_data_t* user_data;
    int result;
    //int i, all_io_count;
    
    if (device_config_str == NULL || strcmp(device_config_str, "") == 0) return -EINVAL;

    user_data = (device_mux_data_t *)kzalloc(sizeof(device_mux_data_t) + sizeof(device_mux_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);

    result = __parse_device_param_for_mux(user_data, device_config_str);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    result = __parse_endpoint_param_for_mux(user_data, endpoint_config_strs, device_data->target_endpoints, device_data->target_endpoint_count);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    // all_io_count = 0;
    // for (i = 0; i < device_data->endpoint_count; i++ ) {
    //     ep = &user_data->user_data->button_cfgs[i]
    //     all_io_count += ep->io_skip_count + ep->button_count;
    // }
    // if (user_data->device_cfg.io_count > all_io_count) {
    //     user_data->device_cfg.io_count = all_io_count;
    // }


    device_data->data = (void *)user_data;

    return 0;
}


static void start_input_device_for_mux(input_device_data_t *device_data)
{
    device_mux_data_t *user_data = (device_mux_data_t *)device_data->data;
    int i;

    for (i = 0; i < 6; i++) {
        if (user_data->device_cfg.addr_gpio[i] != -1) {
            gpio_as_output(user_data->device_cfg.addr_gpio[i]);
        }
    }

    gpio_as_input(user_data->device_cfg.rw_gpio);
    gpio_pullups(&user_data->device_cfg.rw_gpio, 1);

    device_data->is_opend = TRUE;
}


static void check_input_device_for_mux(input_device_data_t *device_data)
{
    int i, j, k, cnt;
    int addr, io_count;
    int read_gpio;
    device_mux_data_t *user_data = (device_mux_data_t *)device_data->data;

    if (user_data == NULL) return;

    io_count = user_data->device_cfg.io_count;

    if (io_count <= 0) return;

    read_gpio = user_data->device_cfg.rw_gpio;

    //addr = user_data->device_cfg.start_offset;
    addr = 0;

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_mux_index_table_t* table = &user_data->button_cfgs[i];
        device_mux_index_item_t* btndef = &table->buttondef[table->button_start_index];

        addr += table->io_skip_count;
        cnt = table->pin_count;
        if (cnt > io_count) cnt = io_count;
        io_count -= cnt;

        if (table->cs_gpio != -1) {
            gpio_set_value(table->cs_gpio, 1);
        }        
        for (j = 0; j < cnt; j++ ){
            int addr_reg = addr;
            int *addr_gpio = user_data->device_cfg.addr_gpio;
            int addr_bit_count = user_data->device_cfg.addr_bit_count;
            for (k = 0; k < addr_bit_count; k++ ){
                int gpio = *addr_gpio++;
                gpio_set_value(gpio, addr_reg & 1);
                addr_reg >>= 1;
            }
            addr++;

            udelay(5);

            if (gpio_get_value(read_gpio) == 0) {
                endpoint->report_button_state[btndef->button] = btndef->value;
            }

            btndef++;
        }
        if (table->cs_gpio != -1) {
            gpio_set_value(table->cs_gpio, 0);
        }        

        if (io_count <= 0) break;
    }
}


static void stop_input_device_for_mux(input_device_data_t *device_data)
{
    device_data->is_opend = FALSE;
}


int register_input_device_for_mux(input_device_type_desc_t *device_desc)
{
    strcpy(device_desc->device_type, "mux");
    device_desc->init_input_dev = init_input_device_for_mux;
    device_desc->start_input_dev = start_input_device_for_mux;
    device_desc->check_input_dev = check_input_device_for_mux;
    device_desc->stop_input_dev = stop_input_device_for_mux;
    return 0;
}
