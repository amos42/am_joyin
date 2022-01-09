/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "bcm_peri.h"
#include "gpio_util.h"
#include "i2c_util.h"
#include "parse_util.h"


// AM_SPININ Defines
#define AM_SPININ_READ_VALUE                (0x00 | 0x00)
#define AM_SPININ_WRITE_VALUE  	            (0x40 | 0x01)
#define AM_SPININ_SET_MODE          	    (0x40 | 0x02)
#define AM_SPININ_SET_SAMPLERATE            (0x40 | 0x03)
#define AM_SPININ_SET_MIN_VALUE             (0x40 | 0x04)
#define AM_SPININ_SET_MAX_VALUE             (0x40 | 0x05)

#define AM_SPININ_COMM_I2C                  (0)
#define AM_SPININ_COMM_SPI                  (1)

// default i2c addr
#define AM_SPININ_DEFAULT_I2C_ADDR          (0x34)
#define INPUT_AM_SPININ_DEFAULT_PPR         (360)
#define INPUT_AM_SPININ_DEFAULT_MIN_VALUE   (-5000)
#define INPUT_AM_SPININ_DEFAULT_MAX_VALUE   (5000)
#define INPUT_AM_SPININ_DEFAULT_SAMPLE_RATE (10)
#define INPUT_MOUSE_DEFAULT_DPI             (1000)

typedef struct tag_device_am_spinin_config {
    int comm_type;
    int addr;
    int rotary_ppr;
    int min_value;
    int max_value;
    int sample_rate;
} device_am_spinin_config_t;

typedef struct tag_device_am_spinin_index_item {
    int button;
    int value;
} device_am_spinin_index_item_t;

typedef struct tag_device_am_spinin_index_table {
    device_am_spinin_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int pin_count;
    int button_start_index;
    int mouse_dpi;
} device_am_spinin_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_am_spinin_data {
    device_am_spinin_config_t         device_cfg;
    device_am_spinin_index_table_t    button_cfgs[1];
} device_am_spinin_data_t;


#define INPUT_AM_SPININ_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (1)

static const device_am_spinin_index_table_t default_input_am_spinin_config = {
    {
        {REL_X,       0}
    }, 
    INPUT_AM_SPININ_DEFAULT_KEYCODE_TABLE_ITEM_COUNT,
    0, 0
};

// device 파라미터 파싱
static int __parse_device_param_for_am_spinin(device_am_spinin_data_t* user_data, char* device_config_str)
{
    char szText[512];
    char* pText;

    if (device_config_str != NULL) {
        char str[10];

        strcpy(szText, device_config_str); 
        pText = szText;

        if (parse_string(str, 10, &pText, ",", "i2c") == NULL) {
            return -1;
        }
        if (strcmp(str, "i2c") == 0) {
            user_data->device_cfg.comm_type = AM_SPININ_COMM_I2C;
        } else if (strcmp(str, "spi") == 0) {
            user_data->device_cfg.comm_type = AM_SPININ_COMM_SPI;
        } else {
            return -1;
        }
        user_data->device_cfg.addr = parse_number(&pText, ",", 0, AM_SPININ_DEFAULT_I2C_ADDR);
        user_data->device_cfg.rotary_ppr = parse_number(&pText, ",", 10, INPUT_AM_SPININ_DEFAULT_PPR);
        user_data->device_cfg.min_value = parse_number(&pText, ",", 10, INPUT_AM_SPININ_DEFAULT_MIN_VALUE);
        user_data->device_cfg.max_value = parse_number(&pText, ",", 10, INPUT_AM_SPININ_DEFAULT_MAX_VALUE);
        user_data->device_cfg.sample_rate = parse_number(&pText, ",", 10, INPUT_AM_SPININ_DEFAULT_SAMPLE_RATE);
    } else {
        user_data->device_cfg.comm_type = AM_SPININ_COMM_I2C;
        user_data->device_cfg.addr = AM_SPININ_DEFAULT_I2C_ADDR;
        user_data->device_cfg.rotary_ppr = INPUT_AM_SPININ_DEFAULT_PPR;
        user_data->device_cfg.min_value = INPUT_AM_SPININ_DEFAULT_MIN_VALUE;
        user_data->device_cfg.max_value = INPUT_AM_SPININ_DEFAULT_MAX_VALUE;
        user_data->device_cfg.sample_rate = INPUT_AM_SPININ_DEFAULT_SAMPLE_RATE;
    }

    return 0;
}


// 각 endpoint 파라미터 파싱
static int __parse_endpoint_param_for_am_spinin(device_am_spinin_data_t* user_data, char* endpoint_config_str[], input_endpoint_data_t *endpoint_list[], int endpoint_count)
{
    int i, j;
    int code_mode;
    device_am_spinin_index_table_t *src, *des;
    char szText[512];
    char* pText;
    char* temp_p;

    des = user_data->button_cfgs;

    for (i = 0; i < endpoint_count; i++ ) {
        input_endpoint_data_t *ep = endpoint_list[i];
        char* cfgtype_p;
        int pin_count, button_start_index, mouse_dpi;

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
            src = (device_am_spinin_index_table_t *)&default_input_am_spinin_config;
            code_mode = INPUT_CODE_TYPE_KEYCODE;

            pin_count = parse_number(&pText, ",", 10, src->pin_count);
            button_start_index = parse_number(&pText, ",", 10, 0);
            mouse_dpi = parse_number(&pText, ",", 10, INPUT_MOUSE_DEFAULT_DPI);
        } else if (strcmp(cfgtype_p, "custom") == 0){
            mouse_dpi = parse_number(&pText, ",", 10, INPUT_MOUSE_DEFAULT_DPI);

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
                char *block_p;
                int button;

                strsep(&pText, "{");
                block_p = strsep(&pText, "}");
                button = parse_number(&block_p, ",", 0, -1);
                strsep(&pText, ",");

                // 키 설정 추가
                src->buttondef[pin_count].button = button;
                if (button >= 0) {
                    src->buttondef[pin_count].value = parse_number(&block_p, ",", 0, 0);
                }
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
            des->mouse_dpi = mouse_dpi;
        } else if (code_mode == INPUT_CODE_TYPE_INDEX) {
            for (j = 0; j < pin_count; j++) {
                des->buttondef[j].button = src->buttondef[j].button;
                des->buttondef[j].value = src->buttondef[j].value;
            }
            des->pin_count = pin_count;
            des->button_start_index = button_start_index;
            des->mouse_dpi = mouse_dpi;
        }

        des++;
    }

    return 0;
}


static short __spi_trans(char cmd, short value) 
{
    unsigned char buf[3];
    buf[0] = cmd;
    buf[1] = (value >> 8) & 0xFF;
    buf[2] = value & 0xFF;
    spi_transfern(buf, 3);
    return (short)((unsigned short)buf[1] << 8 | buf[2]);
}

// device_config_str : comm_type, addr, io_count
// endpoint_config_str : endpoint, config_type (default | custom), ...
//        default: pin_count, start_index, io_skip_count
//        custom: io_skip_count, code_type (0|1), n * {button, value}
//            code_type = 0 : key code
//            code_type = 1 : index
//
// ex) device1=am_spinin;0x20,16;0,default,12
//     device2=am_spinin;0x20;1,custom,,0,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
static int init_input_device_for_am_spinin(void* device_desc_data, input_device_data_t *device_data, char* device_config_str, char* endpoint_config_strs[])
{
    device_am_spinin_data_t* user_data;
    int result;
    
    user_data = (device_am_spinin_data_t *)kzalloc(sizeof(device_am_spinin_data_t) + sizeof(device_am_spinin_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);

    result = __parse_device_param_for_am_spinin(user_data, device_config_str);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    result = __parse_endpoint_param_for_am_spinin(user_data, endpoint_config_strs, device_data->target_endpoints, device_data->target_endpoint_count);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    device_data->data = (void *)user_data;

    return 0;
}


static void start_input_device_for_am_spinin(input_device_data_t *device_data)
{
    device_am_spinin_data_t *user_data = (device_am_spinin_data_t *)device_data->data;

    if (user_data->device_cfg.comm_type == AM_SPININ_COMM_I2C) {
        i2c_init(bcm_peri_base_probe(), 0xB0);
        i2c_write_1word(user_data->device_cfg.addr, AM_SPININ_SET_MODE, 0);
        i2c_write_1word(user_data->device_cfg.addr, AM_SPININ_WRITE_VALUE, 0);
        i2c_write_1word(user_data->device_cfg.addr, AM_SPININ_SET_MIN_VALUE, user_data->device_cfg.min_value);
        i2c_write_1word(user_data->device_cfg.addr, AM_SPININ_SET_MAX_VALUE, user_data->device_cfg.max_value);
        i2c_write_1word(user_data->device_cfg.addr, AM_SPININ_SET_SAMPLERATE, user_data->device_cfg.sample_rate);
    } else if (user_data->device_cfg.comm_type == AM_SPININ_COMM_SPI) {
        spi_init(bcm_peri_base_probe(), 0xB0);
        spi_setClockDivider(0x01);
        spi_setDataMode(0);

        spi_begin();
        __spi_trans(AM_SPININ_SET_MODE, 0);
        __spi_trans(AM_SPININ_WRITE_VALUE, 0);
        __spi_trans(AM_SPININ_SET_MIN_VALUE, user_data->device_cfg.min_value);
        __spi_trans(AM_SPININ_SET_MAX_VALUE, user_data->device_cfg.max_value);
        __spi_trans(AM_SPININ_SET_SAMPLERATE, user_data->device_cfg.sample_rate);
        spi_end();
    } else {
        return;
    }

    device_data->is_opend = TRUE;
}


static void check_input_device_for_am_spinin(input_device_data_t *device_data)
{
    int i;
    int addr, rotary_ppr, value;
    device_am_spinin_data_t *user_data = (device_am_spinin_data_t *)device_data->data;

    if (user_data == NULL) return;

    rotary_ppr = user_data->device_cfg.rotary_ppr;
    addr = user_data->device_cfg.addr;

    if (user_data->device_cfg.comm_type == AM_SPININ_COMM_I2C) {
        value = i2c_raw_read_1word(addr);
        if (value != 0) {
            i2c_write_1word(addr, AM_SPININ_WRITE_VALUE, 0);
        }
    } else if (user_data->device_cfg.comm_type == AM_SPININ_COMM_SPI) {
        spi_begin();
        spi_chipSelect(addr);
        value = __spi_trans(AM_SPININ_READ_VALUE, 0);
        if (value != 0) {
            __spi_trans(AM_SPININ_WRITE_VALUE, 0);
        }
        spi_end();
    } else {
        return;
    }

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_am_spinin_index_table_t* table = &user_data->button_cfgs[i];
        device_am_spinin_index_item_t* btndef = &table->buttondef[table->button_start_index];
        int mouse_dpi = table->mouse_dpi;

        endpoint->report_button_state[btndef->button] = value * mouse_dpi / rotary_ppr;
    }
}


static void stop_input_device_for_am_spinin(input_device_data_t *device_data)
{
    device_am_spinin_data_t *user_data = (device_am_spinin_data_t *)device_data->data;

    device_data->is_opend = FALSE;

    if (user_data->device_cfg.comm_type == AM_SPININ_COMM_I2C) {
        i2c_close();
    } else if (user_data->device_cfg.comm_type == AM_SPININ_COMM_SPI) {
        spi_close();
    }    
}


int register_input_device_for_am_spinin(input_device_type_desc_t *device_desc)
{
    strcpy(device_desc->device_type, "am_spinin");
    device_desc->init_input_dev = init_input_device_for_am_spinin;
    device_desc->start_input_dev = start_input_device_for_am_spinin;
    device_desc->check_input_dev = check_input_device_for_am_spinin;
    device_desc->stop_input_dev = stop_input_device_for_am_spinin;
    return 0;
}
