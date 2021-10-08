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

//#define GPIO_GET(i)   GPIO_READ(i)
//#define GPIO_GET_VALUE(i)   getGpio(i)
//#define GPIO_SET_VALUE(i,v)   setGpio((i), (v))

//#define MAX_ADDR_IO_COUNT   (6)

/*
 * MCP23017 Defines
 */
#define MPC23017_GPIOA_MODE	            (0x00)
#define MPC23017_GPIOB_MODE	            (0x01)
#define MPC23017_GPIOA_PULLUPS_MODE	    (0x0C)
#define MPC23017_GPIOB_PULLUPS_MODE     (0x0D)
#define MPC23017_GPIOA_READ             (0x12)
#define MPC23017_GPIOB_READ             (0x13)

// default i2c addr
#define MPC23017_DEFAULT_I2C_ADDR       (0x20)


typedef struct tag_device_mcp23017_config {
    int i2c_addr;
    int io_count;
    int pull_updown;
} device_mcp23017_config_t;

typedef struct tag_device_mcp23017_index_item {
    int button;
    int value;
} device_mcp23017_index_item_t;

typedef struct tag_device_mcp23017_index_table {
    device_mcp23017_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int io_skip_count;
    int button_start_index;
    int button_count;
} device_mcp23017_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_mcp23017_data {
    device_mcp23017_config_t         device_cfg;
    device_mcp23017_index_table_t    button_cfgs[1];
} device_mcp23017_data_t;


#define INPUT_MCP23017_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (16)

static const device_mcp23017_index_table_t default_input_mcp23017_config = {
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
    0, 0, INPUT_MCP23017_DEFAULT_KEYCODE_TABLE_ITEM_COUNT
};


// device 파라미터 파싱
static int __parse_device_param_for_mcp23017(device_mcp23017_data_t* user_data, char* device_config_str)
{
    char szText[512];
    char* pText;
    char* temp_p;

    if (device_config_str != NULL) {
        strcpy(szText, device_config_str); 
        pText = szText;

        temp_p = strsep(&pText, ",");
        if (temp_p == NULL || strcmp(temp_p, "") == 0 || strcmp(temp_p, "default") == 0) {
            user_data->device_cfg.i2c_addr = MPC23017_DEFAULT_I2C_ADDR;
        } else {
            user_data->device_cfg.i2c_addr = simple_strtol(temp_p, NULL, 0);
        }

        temp_p = strsep(&pText, ",");
        if (temp_p == NULL || strcmp(temp_p, "") == 0 || strcmp(temp_p, "default") == 0) {
            user_data->device_cfg.io_count = INPUT_MCP23017_DEFAULT_KEYCODE_TABLE_ITEM_COUNT;
        } else {
            user_data->device_cfg.io_count = simple_strtol(temp_p, NULL, 10);
        }

        temp_p = strsep(&pText, ",");
        if (temp_p == NULL || strcmp(temp_p, "") == 0 || strcmp(temp_p, "default") == 0) {
            user_data->device_cfg.pull_updown = 0;
        } else {
            user_data->device_cfg.pull_updown = simple_strtol(temp_p, NULL, 10);
        }
    } else {
        user_data->device_cfg.i2c_addr = MPC23017_DEFAULT_I2C_ADDR;
        user_data->device_cfg.io_count = INPUT_MCP23017_DEFAULT_KEYCODE_TABLE_ITEM_COUNT;
        user_data->device_cfg.pull_updown = 0;
    }

    return 0;
}


// 각 endpoint 파라미터 파싱
static int __parse_endpoint_param_for_mcp23017(device_mcp23017_data_t* user_data, char* endpoint_config_str[], input_endpoint_data_t *endpoint_list[], int endpoint_count)
{
    int i, j;
    int code_mode;
    device_mcp23017_index_table_t *src, *des;
    char szText[512];
    char* pText;
    char* temp_p;

    des = user_data->button_cfgs;

    for (i = 0; i < endpoint_count; i++ ) {
        input_endpoint_data_t *ep = endpoint_list[i];
        char* cfgtype_p;
        int io_skip_count, button_start_index, button_count;

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
            src = (device_mcp23017_index_table_t *)&default_input_mcp23017_config;
            code_mode = 0;            

            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                button_count = src->button_count;
            } else {
                button_count = simple_strtol(temp_p, NULL, 10);
            }

            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                button_start_index = 0;
            } else {
                button_start_index = simple_strtol(temp_p, NULL, 10);
            }

            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                io_skip_count = 0;
            } else {
                io_skip_count = simple_strtol(temp_p, NULL, 10);
            }
        } else if(strcmp(cfgtype_p, "custom") == 0){
            char* keycode_p;

            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                io_skip_count = 0;
            } else {
                io_skip_count = simple_strtol(temp_p, NULL, 10);
            }

            keycode_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                code_mode = 0;
            } else {
                code_mode = simple_strtol(keycode_p, NULL, 10);
            }

            button_start_index = 0;
            button_count = 0;
            src = &user_data->button_cfgs[i];
            while (pText != NULL && button_count < MAX_INPUT_BUTTON_COUNT) {
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
                if (button >= 0) {
                    src->buttondef[button_count].button = button;
                    src->buttondef[button_count].value = value;
                    button_count++;
                }
            }
        } else {
            continue;
        }

        if (code_mode == 0) {
            // if (button_count > ep->button_count) button_count = ep->button_count;
            for (j = 0; j < button_count; j++) {
                int k;
                int code = src->buttondef[j].button;
                for (k = 0; k < ep->target_buttonset->button_count; k++) {
                    if (code == ep->target_buttonset->button_data[k].button_code) {
                        des->buttondef[j].button = k;
                        des->buttondef[j].value = src->buttondef[j].value;
                        break;
                    }
                }
            }
            des->button_count = button_count;
            des->button_start_index = button_start_index;
            des->io_skip_count = io_skip_count;
        } else if (code_mode == 1) {
            // if (button_count > ep->button_count) button_count = ep->button_count;
            for (j = 0; j < button_count; j++) {
                des->buttondef[j].button = src->buttondef[j].button;
                des->buttondef[j].value = src->buttondef[j].value;
            }
            des->button_count = button_count;
            des->button_start_index = button_start_index;
            des->io_skip_count = io_skip_count;
        }

        des++;
    }

    return 0;
}


// device_config_str : i2c_addr, io_count
// endpoint_config_str : endpoint, config_type (default | custom), ...
//        default: button_count, start_index, io_skip_count
//        custom: io_skip_count, code_type (0|1), n * {button, value}
//            code_type = 0 : key code
//            code_type = 1 : index
//
// ex) device1=mcp23017;0x20,16;0,default,12
//     device2=mcp23017;0x20;1,custom,,0,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
static int init_input_device_for_mcp23017(input_device_data_t *device_data, char* device_config_str, char* endpoint_config_strs[])
{
    device_mcp23017_data_t* user_data;
    int result;
    
    user_data = (device_mcp23017_data_t *)kzalloc(sizeof(device_mcp23017_data_t) + sizeof(device_mcp23017_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);

    result = __parse_device_param_for_mcp23017(user_data, device_config_str);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    result = __parse_endpoint_param_for_mcp23017(user_data, endpoint_config_strs, device_data->target_endpoints, device_data->target_endpoint_count);
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


static void start_input_device_for_mcp23017(input_device_data_t *device_data)
{
    device_mcp23017_data_t *user_data = (device_mcp23017_data_t *)device_data->data;
    char FF = 0xFF;

    i2c_init();
    udelay(1000);
    // Put all GPIOA inputs on MCP23017 in INPUT mode
    i2c_write(user_data->device_cfg.i2c_addr, MPC23017_GPIOA_MODE, &FF, 1);
    udelay(1000);
    // Put all inputs on MCP23017 in pullup mode
    i2c_write(user_data->device_cfg.i2c_addr, MPC23017_GPIOA_PULLUPS_MODE, &FF, 1);
    udelay(1000);
    // Put all GPIOB inputs on MCP23017 in INPUT mode
    i2c_write(user_data->device_cfg.i2c_addr, MPC23017_GPIOB_MODE, &FF, 1);
    udelay(1000);
    // Put all inputs on MCP23017 in pullup mode
    i2c_write(user_data->device_cfg.i2c_addr, MPC23017_GPIOB_PULLUPS_MODE, &FF, 1);
    udelay(1000);
    // Put all inputs on MCP23017 in pullup mode a second time
    // Known bug : if you remove this line, you will not have pullups on GPIOB 
    i2c_write(user_data->device_cfg.i2c_addr, MPC23017_GPIOB_PULLUPS_MODE, &FF, 1);
    udelay(1000);

    device_data->is_opend = TRUE;
}


static void check_input_device_for_mcp23017(input_device_data_t *device_data)
{
    int i, j, cnt;
    int i2c_addr, io_count;
    char resultA, resultB;
    unsigned io_value;
    device_mcp23017_data_t *user_data = (device_mcp23017_data_t *)device_data->data;

    if (user_data == NULL) return;

    i2c_addr = user_data->device_cfg.i2c_addr;
    io_count = user_data->device_cfg.io_count;

    if (io_count <= 0) return;

    i2c_read(i2c_addr, MPC23017_GPIOA_READ, &resultA, 1);
    i2c_read(i2c_addr, MPC23017_GPIOB_READ, &resultB, 1);

    io_value = ((unsigned)resultB << 8) | (unsigned)resultA;

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_mcp23017_index_table_t* table = &user_data->button_cfgs[i];
        device_mcp23017_index_item_t* btndef = &table->buttondef[table->button_start_index];

        cnt = table->button_count;
        if (cnt > io_count) cnt = io_count;
        io_count -= cnt;

        io_value >>= table->io_skip_count;
        for (j = table->io_skip_count; j < cnt; j++ ){
            if ((io_value & 0x1) == 0) {
                endpoint->report_button_state[btndef->button] = btndef->value;
            }
            io_value >>= 1;

            btndef++;
        }

        if (io_count <= 0) break;
    }
}


static void stop_input_device_for_mcp23017(input_device_data_t *device_data)
{
    device_data->is_opend = FALSE;
}


int register_input_device_for_mcp23017(input_device_type_desc_t *device_desc)
{
    strcpy(device_desc->device_type, "mcp23017");
    device_desc->init_input_dev = init_input_device_for_mcp23017;
    device_desc->start_input_dev = start_input_device_for_mcp23017;
    device_desc->check_input_dev = check_input_device_for_mcp23017;
    device_desc->stop_input_dev = stop_input_device_for_mcp23017;
    return 0;
}
