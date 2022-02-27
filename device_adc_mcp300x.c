/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include "build_cfg.h"

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "indev_type.h"
#include "bcm_peri.h"
#include "gpio_util.h"
#if defined(USE_SPI_DIRECT)
#include "spi_util.h"
#else
#include <linux/spi/spi.h>
#endif
#include "parse_util.h"


#define MAX_MCP300X_ADC_CHANNEL_COUNT  (8)
#define MAX_MCP300X_ADC_VALUE          (1023)

// default spi channel
#define MCP300X_DEFAULT_SPI_CHANNEL    (0)

typedef struct tag_device_mcp300x_desc_config {
    int max_channel_count;
} device_mcp300x_desc_config_t;

typedef struct tag_device_mcp300x_config {
    int spi_channel;
    int value_weight_percent;
    int sampling_count;
} device_mcp300x_config_t;

typedef struct tag_device_mcp300x_index_item {
    int adc_channel;
    int abs_input;
    int min_value;
    int max_value;
    int min_adc_value;
    int max_adc_value;
    int mid_adc_value;
} device_mcp300x_index_item_t;

typedef struct tag_device_mcp300x_index_table {
    device_mcp300x_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int pin_count;
    int button_start_index;
} device_mcp300x_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_mcp300x_data {
    int currentAdcValue[MAX_MCP300X_ADC_CHANNEL_COUNT];

    device_mcp300x_desc_config_t    device_desc_data;
    device_mcp300x_config_t         device_cfg;
#if !defined(USE_SPI_DIRECT)
    struct spi_device*              spi;
#endif
    device_mcp300x_index_table_t    button_cfgs[1];
} device_mcp300x_data_t;


#define INPUT_MCP300X_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (8)

static const device_mcp300x_index_table_t default_input_mcp300x_config = {
    {
        {0, ABS_X,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP300X_ADC_VALUE, MAX_MCP300X_ADC_VALUE/2},
        {1, ABS_Y,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP300X_ADC_VALUE, MAX_MCP300X_ADC_VALUE/2},
        {2, ABS_RX,       -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP300X_ADC_VALUE, MAX_MCP300X_ADC_VALUE/2},
        {3, ABS_RY,       -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP300X_ADC_VALUE, MAX_MCP300X_ADC_VALUE/2},
        {4, ABS_THROTTLE, -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP300X_ADC_VALUE, MAX_MCP300X_ADC_VALUE/2},
        {5, ABS_WHEEL,    -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP300X_ADC_VALUE, MAX_MCP300X_ADC_VALUE/2},
        {6, ABS_HAT0X,    -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP300X_ADC_VALUE, MAX_MCP300X_ADC_VALUE/2},
        {7, ABS_HAT0Y,    -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP300X_ADC_VALUE, MAX_MCP300X_ADC_VALUE/2}
    },
    INPUT_MCP300X_DEFAULT_KEYCODE_TABLE_ITEM_COUNT,
    0
};


// device 파라미터 파싱
static int __parse_device_param_for_mcp300x(device_mcp300x_data_t* user_data, char* device_config_str)
{
    char szText[512];
    char* pText;

    if (device_config_str != NULL) {
        strcpy(szText, device_config_str);
        pText = szText;

        user_data->device_cfg.spi_channel = parse_number(&pText, ",", 0, MCP300X_DEFAULT_SPI_CHANNEL);
        user_data->device_cfg.value_weight_percent = parse_number(&pText, ",", 10, 100);
        user_data->device_cfg.sampling_count = parse_number(&pText, ",", 10, 10);
    } else {
        user_data->device_cfg.spi_channel = MCP300X_DEFAULT_SPI_CHANNEL;
        user_data->device_cfg.value_weight_percent = 100;
        user_data->device_cfg.sampling_count = 10;
    }

    return 0;
}


// 각 endpoint 파라미터 파싱
static int __parse_endpoint_param_for_mcp300x(device_mcp300x_data_t* user_data, char* endpoint_config_str[], input_endpoint_data_t *endpoint_list[], int endpoint_count)
{
    int i, j;
    int code_mode;
    device_mcp300x_index_table_t *src, *des;
    char szText[512];
    char* pText;
    char* temp_p;

    des = user_data->button_cfgs;

    for (i = 0; i < endpoint_count; i++ ) {
        input_endpoint_data_t *ep = endpoint_list[i];
        char* cfgtype_p;
        int pin_count, button_start_index;

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
            src = (device_mcp300x_index_table_t *)&default_input_mcp300x_config;
            code_mode = INPUT_CODE_TYPE_KEYCODE;

            pin_count = parse_number(&pText, ",", 10, src->pin_count);
            button_start_index = parse_number(&pText, ",", 10, 0);
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
                char *block_p;
                int adc_channel, abs_input;

                strsep(&pText, "{");
                block_p = strsep(&pText, "}");
                adc_channel = parse_number(&block_p, ",", 10, -1);
                abs_input = parse_number(&block_p, ",", 0, -1);
                strsep(&pText, ",");

                // 키 설정 추가
                if (adc_channel >= 0 && abs_input >= 0 && adc_channel < MAX_MCP300X_ADC_CHANNEL_COUNT) {
                    src->buttondef[pin_count].adc_channel = adc_channel;
                    src->buttondef[pin_count].abs_input = abs_input;
                    src->buttondef[pin_count].min_value = parse_number(&block_p, ",", 10, -DEFAULT_INPUT_ABS_MAX_VALUE);
                    src->buttondef[pin_count].max_value = parse_number(&block_p, ",", 10, DEFAULT_INPUT_ABS_MAX_VALUE);
                    src->buttondef[pin_count].min_adc_value = parse_number(&block_p, ",", 10, 0);
                    src->buttondef[pin_count].max_adc_value = parse_number(&block_p, ",", 10, MAX_MCP300X_ADC_VALUE);
                    src->buttondef[pin_count].mid_adc_value = parse_number(&block_p, ",", 10, MAX_MCP300X_ADC_VALUE / 2);
                    pin_count++;
                }
            }
        } else {
            continue;
        }

        if (code_mode == INPUT_CODE_TYPE_KEYCODE) {
            for (j = 0; j < pin_count; j++) {
                int idx = find_input_button_data(ep, src->buttondef[j].abs_input, NULL);
                des->buttondef[j].abs_input = idx;
                if (idx != -1) {
                    des->buttondef[j].adc_channel = src->buttondef[j].adc_channel;
                    des->buttondef[j].min_value = src->buttondef[j].min_value;
                    des->buttondef[j].max_value = src->buttondef[j].max_value;
                    des->buttondef[j].min_adc_value = src->buttondef[j].min_adc_value;
                    des->buttondef[j].max_adc_value = src->buttondef[j].max_adc_value;
                    des->buttondef[j].mid_adc_value = src->buttondef[j].mid_adc_value;
                }
            }
            des->pin_count = pin_count;
            des->button_start_index = button_start_index;
        } else if (code_mode == INPUT_CODE_TYPE_INDEX) {
            for (j = 0; j < pin_count; j++) {
                des->buttondef[j] = src->buttondef[j];
            }
            des->pin_count = pin_count;
            des->button_start_index = button_start_index;
        }

        des++;
    }

    return 0;
}

#if defined(USE_SPI_DIRECT)
static int __mcp300x_spi_read(uint8_t adc_channel)
{
    unsigned char buf[3] = {0x01, (0x08 | adc_channel) << 4, 0};
    spi_transfern(buf, 3);
    return (int)(((unsigned short)buf[1] & 0x3) << 8 | buf[2]);
}
#else
static int __mcp300x_spi_read(struct spi_device *spi, uint8_t adc_channel)
{
    unsigned char buf[3] = {0x01, (0x08 | adc_channel) << 4, 0};
	struct spi_transfer	t = {
        .tx_buf		= buf,
        .rx_buf		= buf,
        .len		= 3,
    };
	int r = spi_sync_transfer(spi, &t, 1);
    return (r >= 0)? (int)(((unsigned short)buf[1] & 0x3) << 8 | buf[2]) : r;
}
#endif

// device_config_str : spi_channel, value_weight_percent, sampling_count
// endpoint_config_str : endpoint, config_type (default | custom), ...
//        default: pin_count, start_index
//        custom: code_type (0|1), n * {adc_channel, button, min_value, max_value, adc_min_value, adc_max_value, adc_mid_value}
//            code_type = 0 : key code
//            code_type = 1 : index
//
// ex) device1=mcp300x;0;0,default,12
//     device2=mcp300x;;1,custom,,0,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
static int init_input_device_for_mcp300x(void* device_desc_data, input_device_data_t *device_data, char* device_config_str, char* endpoint_config_strs[])
{
    device_mcp300x_data_t* user_data;
    int result;
    int i;

    user_data = (device_mcp300x_data_t *)kzalloc(sizeof(device_mcp300x_data_t) + sizeof(device_mcp300x_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);
    user_data->device_desc_data = *(device_mcp300x_desc_config_t *)device_desc_data;

    result = __parse_device_param_for_mcp300x(user_data, device_config_str);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    result = __parse_endpoint_param_for_mcp300x(user_data, endpoint_config_strs, device_data->target_endpoints, device_data->target_endpoint_count);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    for (i = 0; i < MAX_MCP300X_ADC_CHANNEL_COUNT; i++) {
        user_data->currentAdcValue[i] = MAX_MCP300X_ADC_VALUE / 2;
    }

    device_data->data = (void *)user_data;

    return 0;
}


static void start_input_device_for_mcp300x(input_device_data_t *device_data)
{
#if !defined(USE_SPI_DIRECT)
    device_mcp300x_data_t *user_data = (device_mcp300x_data_t *)device_data->data;
#endif

#if defined(USE_SPI_DIRECT)
    spi_init(bcm_peri_base_probe(), 0xB0);

    spi_setClockDivider(256);
#else
    struct spi_board_info spi_device_info = {
        .modalias     = "mcp300x",
        .max_speed_hz = 1 * 1000 * 1000,     // speed your device (slave) can handle
        .bus_num      = 0,                   // SPI 0
        .chip_select  = user_data->device_cfg.spi_channel,
        .mode         = SPI_MODE_0           // SPI mode 0
    };

    struct spi_master* master = spi_busnum_to_master(spi_device_info.bus_num);
    if (IS_ERR_OR_NULL(master)) {
        pr_err("SPI Master {%d} not found.\n", spi_device_info.bus_num);
        return;
    }

    user_data->spi = spi_new_device(master, &spi_device_info);
    if (IS_ERR_OR_NULL(user_data->spi)) {
        pr_err("spi open device error {%d}.\n", user_data->device_cfg.spi_channel);
        return;
    }
#endif

    device_data->is_opend = TRUE;
}


static void check_input_device_for_mcp300x(input_device_data_t *device_data)
{
    int i, j, k, cnt;
    device_mcp300x_data_t *user_data = (device_mcp300x_data_t *)device_data->data;
    int sampling_count, value_weight, prev_weight;
#if !defined(USE_SPI_DIRECT)
    struct spi_device* spi;
#endif

    if (user_data == NULL) return;

#if defined(USE_SPI_DIRECT)
    spi_begin();
    spi_chipSelect(user_data->device_cfg.spi_channel);
#else
    spi = user_data->spi;
#endif

    sampling_count = user_data->device_cfg.sampling_count;
    value_weight = user_data->device_cfg.value_weight_percent;
    prev_weight = 100 - value_weight;

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_mcp300x_index_table_t* table = &user_data->button_cfgs[i];
        device_mcp300x_index_item_t* btndef = &table->buttondef[table->button_start_index];
        int v, v0, md;

        cnt = table->pin_count;

        for (j = 0; j < cnt; j++ ){
            if (btndef->abs_input >= 0 && btndef->adc_channel >= 0) {
                if (value_weight >= 100) {
#if defined(USE_SPI_DIRECT)
                    v = __mcp300x_spi_read(btndef->adc_channel);
#else
                    v = __mcp300x_spi_read(spi, btndef->adc_channel);
#endif
                } else {
                    v = user_data->currentAdcValue[btndef->adc_channel];
                    for (k = 0; k < sampling_count; k++) {
#if defined(USE_SPI_DIRECT)
                        v0 = __mcp300x_spi_read(btndef->adc_channel);
#else
                        v0 = __mcp300x_spi_read(spi, btndef->adc_channel);
#endif
                        v = (v0 * value_weight + v * prev_weight) / 100;
                    }
                    user_data->currentAdcValue[btndef->adc_channel] = v;
                }

                md = (btndef->max_value - btndef->min_value) / 2;
                if( v <= btndef->mid_adc_value ) {
                    v = btndef->min_value + (v - btndef->min_adc_value) * md / (btndef->mid_adc_value - btndef->min_adc_value);
                } else {
                    v = btndef->min_value + md + (v - btndef->mid_adc_value) * md / (btndef->max_adc_value - btndef->mid_adc_value);
                }

                endpoint->report_button_state[btndef->abs_input] = v;
            }

            btndef++;
        }
    }

#if defined(USE_SPI_DIRECT)
    spi_end();
#endif
}


static void stop_input_device_for_mcp300x(input_device_data_t *device_data)
{
#if !defined(USE_SPI_DIRECT)
    device_mcp300x_data_t *user_data = (device_mcp300x_data_t *)device_data->data;
#endif

    device_data->is_opend = FALSE;

#if defined(USE_SPI_DIRECT)
    spi_close();
#else
    if (!IS_ERR_OR_NULL(user_data->spi)) {
        spi_unregister_device(user_data->spi);
        user_data->spi = NULL;
    }
#endif
}


int register_input_device_for_mcp3008(input_device_type_desc_t *device_desc)
{
    device_mcp300x_desc_config_t* desc = (device_mcp300x_desc_config_t *)device_desc->device_desc_data;

    strcpy(device_desc->device_type, "mcp3008");
    desc->max_channel_count = 8;

    device_desc->init_input_dev = init_input_device_for_mcp300x;
    device_desc->start_input_dev = start_input_device_for_mcp300x;
    device_desc->check_input_dev = check_input_device_for_mcp300x;
    device_desc->stop_input_dev = stop_input_device_for_mcp300x;

    return 0;
}


int register_input_device_for_mcp3004(input_device_type_desc_t *device_desc)
{
    device_mcp300x_desc_config_t* desc = (device_mcp300x_desc_config_t *)device_desc->device_desc_data;

    strcpy(device_desc->device_type, "mcp3004");
    desc->max_channel_count = 4;

    device_desc->init_input_dev = init_input_device_for_mcp300x;
    device_desc->start_input_dev = start_input_device_for_mcp300x;
    device_desc->check_input_dev = check_input_device_for_mcp300x;
    device_desc->stop_input_dev = stop_input_device_for_mcp300x;

    return 0;
}
