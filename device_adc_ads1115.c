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


/*
 * ADS1115 Defines
 */
/*=========================================================================
    I2C ADDRESS/BITS
    -----------------------------------------------------------------------*/
#define ADS1X15_ADDRESS (0x48) ///< 1001 000 (ADDR = GND)
/*=========================================================================*/

/*=========================================================================
    POINTER REGISTER
    -----------------------------------------------------------------------*/
#define ADS1X15_REG_POINTER_MASK (0x03)      ///< Point mask
#define ADS1X15_REG_POINTER_CONVERT (0x00)   ///< Conversion
#define ADS1X15_REG_POINTER_CONFIG (0x01)    ///< Configuration
#define ADS1X15_REG_POINTER_LOWTHRESH (0x02) ///< Low threshold
#define ADS1X15_REG_POINTER_HITHRESH (0x03)  ///< High threshold
/*=========================================================================*/

/*=========================================================================
    CONFIG REGISTER
    -----------------------------------------------------------------------*/
#define ADS1X15_REG_CONFIG_OS_MASK (0x8000) ///< OS Mask
#define ADS1X15_REG_CONFIG_OS_SINGLE (0x8000) ///< Write: Set to start a single-conversion
#define ADS1X15_REG_CONFIG_OS_BUSY (0x0000) ///< Read: Bit = 0 when conversion is in progress
#define ADS1X15_REG_CONFIG_OS_NOTBUSY (0x8000) ///< Read: Bit = 1 when device is not performing a conversion

#define ADS1X15_REG_CONFIG_MUX_MASK (0x7000) ///< Mux Mask
#define ADS1X15_REG_CONFIG_MUX_DIFF_0_1 (0x0000) ///< Differential P = AIN0, N = AIN1 (default)
#define ADS1X15_REG_CONFIG_MUX_DIFF_0_3 (0x1000) ///< Differential P = AIN0, N = AIN3
#define ADS1X15_REG_CONFIG_MUX_DIFF_1_3 (0x2000) ///< Differential P = AIN1, N = AIN3
#define ADS1X15_REG_CONFIG_MUX_DIFF_2_3 (0x3000) ///< Differential P = AIN2, N = AIN3
#define ADS1X15_REG_CONFIG_MUX_SINGLE_0 (0x4000) ///< Single-ended AIN0
#define ADS1X15_REG_CONFIG_MUX_SINGLE_1 (0x5000) ///< Single-ended AIN1
#define ADS1X15_REG_CONFIG_MUX_SINGLE_2 (0x6000) ///< Single-ended AIN2
#define ADS1X15_REG_CONFIG_MUX_SINGLE_3 (0x7000) ///< Single-ended AIN3

#define ADS1X15_REG_CONFIG_PGA_MASK (0x0E00)   ///< PGA Mask
#define ADS1X15_REG_CONFIG_PGA_6_144V (0x0000) ///< +/-6.144V range = Gain 2/3
#define ADS1X15_REG_CONFIG_PGA_4_096V (0x0200) ///< +/-4.096V range = Gain 1
#define ADS1X15_REG_CONFIG_PGA_2_048V (0x0400) ///< +/-2.048V range = Gain 2 (default)
#define ADS1X15_REG_CONFIG_PGA_1_024V (0x0600) ///< +/-1.024V range = Gain 4
#define ADS1X15_REG_CONFIG_PGA_0_512V (0x0800) ///< +/-0.512V range = Gain 8
#define ADS1X15_REG_CONFIG_PGA_0_256V (0x0A00) ///< +/-0.256V range = Gain 16

#define ADS1X15_REG_CONFIG_MODE_MASK (0x0100)   ///< Mode Mask
#define ADS1X15_REG_CONFIG_MODE_CONTIN (0x0000) ///< Continuous conversion mode
#define ADS1X15_REG_CONFIG_MODE_SINGLE (0x0100) ///< Power-down single-shot mode (default)

#define ADS1X15_REG_CONFIG_RATE_MASK (0x00E0) ///< Data Rate Mask

#define ADS1X15_REG_CONFIG_CMODE_MASK (0x0010) ///< CMode Mask
#define ADS1X15_REG_CONFIG_CMODE_TRAD (0x0000) ///< Traditional comparator with hysteresis (default)
#define ADS1X15_REG_CONFIG_CMODE_WINDOW (0x0010) ///< Window comparator

#define ADS1X15_REG_CONFIG_CPOL_MASK (0x0008) ///< CPol Mask
#define ADS1X15_REG_CONFIG_CPOL_ACTVLOW (0x0000) ///< ALERT/RDY pin is low when active (default)
#define ADS1X15_REG_CONFIG_CPOL_ACTVHI (0x0008) ///< ALERT/RDY pin is high when active

#define ADS1X15_REG_CONFIG_CLAT_MASK (0x0004) ///< Determines if ALERT/RDY pin latches once asserted
#define ADS1X15_REG_CONFIG_CLAT_NONLAT (0x0000) ///< Non-latching comparator (default)
#define ADS1X15_REG_CONFIG_CLAT_LATCH (0x0004) ///< Latching comparator

#define ADS1X15_REG_CONFIG_CQUE_MASK (0x0003) ///< CQue Mask
#define ADS1X15_REG_CONFIG_CQUE_1CONV (0x0000) ///< Assert ALERT/RDY after one conversions
#define ADS1X15_REG_CONFIG_CQUE_2CONV (0x0001) ///< Assert ALERT/RDY after two conversions
#define ADS1X15_REG_CONFIG_CQUE_4CONV (0x0002) ///< Assert ALERT/RDY after four conversions
#define ADS1X15_REG_CONFIG_CQUE_NONE (0x0003) ///< Disable the comparator and put ALERT/RDY in high state (default)
/*=========================================================================*/

/** Gain settings */
typedef enum {
  GAIN_TWOTHIRDS = ADS1X15_REG_CONFIG_PGA_6_144V,
  GAIN_ONE = ADS1X15_REG_CONFIG_PGA_4_096V,
  GAIN_TWO = ADS1X15_REG_CONFIG_PGA_2_048V,
  GAIN_FOUR = ADS1X15_REG_CONFIG_PGA_1_024V,
  GAIN_EIGHT = ADS1X15_REG_CONFIG_PGA_0_512V,
  GAIN_SIXTEEN = ADS1X15_REG_CONFIG_PGA_0_256V
} adsGain_t;

/** Data rates */
#define RATE_ADS1015_128SPS (0x0000)  ///< 128 samples per second
#define RATE_ADS1015_250SPS (0x0020)  ///< 250 samples per second
#define RATE_ADS1015_490SPS (0x0040)  ///< 490 samples per second
#define RATE_ADS1015_920SPS (0x0060)  ///< 920 samples per second
#define RATE_ADS1015_1600SPS (0x0080) ///< 1600 samples per second (default)
#define RATE_ADS1015_2400SPS (0x00A0) ///< 2400 samples per second
#define RATE_ADS1015_3300SPS (0x00C0) ///< 3300 samples per second

#define RATE_ADS1115_8SPS (0x0000)   ///< 8 samples per second
#define RATE_ADS1115_16SPS (0x0020)  ///< 16 samples per second
#define RATE_ADS1115_32SPS (0x0040)  ///< 32 samples per second
#define RATE_ADS1115_64SPS (0x0060)  ///< 64 samples per second
#define RATE_ADS1115_128SPS (0x0080) ///< 128 samples per second (default)
#define RATE_ADS1115_250SPS (0x00A0) ///< 250 samples per second
#define RATE_ADS1115_475SPS (0x00C0) ///< 475 samples per second
#define RATE_ADS1115_860SPS (0x00E0) ///< 860 samples per second




#define MAX_ADS1115_ADC_CHANNEL_COUNT  (4)
#define MAX_ADS1115_ADC_VALUE          (65535)

// default i2c address
#define ADS1115_DEFAULT_I2C_ADDR    (0x48)


typedef struct tag_device_ads1115_config {
    int i2c_addr;
    int value_weight_percent;
    int sampling_count;
} device_ads1115_config_t;

typedef struct tag_device_ads1115_index_item {
    int adc_channel;
    int abs_input;
    int min_value;
    int max_value;
    int min_adc_value;
    int max_adc_value;
    int mid_adc_value;
} device_ads1115_index_item_t;

typedef struct tag_device_ads1115_index_table {
    device_ads1115_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int pin_count;
    int button_start_index;
} device_ads1115_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_ads1115_data {
    int currentAdcValue[MAX_ADS1115_ADC_CHANNEL_COUNT];

    device_ads1115_config_t         device_cfg;
    device_ads1115_index_table_t    button_cfgs[1];
} device_ads1115_data_t;


#define INPUT_ADS1115_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (4)

static const device_ads1115_index_table_t default_input_ads1115_config = {
    {
        {0, ABS_X,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP3008_ADC_VALUE, MAX_MCP3008_ADC_VALUE/2},
        {1, ABS_Y,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP3008_ADC_VALUE, MAX_MCP3008_ADC_VALUE/2},
        {2, ABS_RX,       -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP3008_ADC_VALUE, MAX_MCP3008_ADC_VALUE/2},
        {3, ABS_RY,       -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_MCP3008_ADC_VALUE, MAX_MCP3008_ADC_VALUE/2}
    }, 
    INPUT_ADS1115_DEFAULT_KEYCODE_TABLE_ITEM_COUNT,
    0
};


// device 파라미터 파싱
static int __parse_device_param_for_ads1115(device_ads1115_data_t* user_data, char* device_config_str)
{
    char szText[512];
    char* pText;

    if (device_config_str != NULL) {
        strcpy(szText, device_config_str); 
        pText = szText;

        user_data->device_cfg.i2c_addr = parse_number(&pText, ",", 0, ADS1115_DEFAULT_I2C_ADDR);
        user_data->device_cfg.value_weight_percent = parse_number(&pText, ",", 10, 100);
        user_data->device_cfg.sampling_count = parse_number(&pText, ",", 10, 1);
    } else {
        user_data->device_cfg.i2c_addr = ADS1115_DEFAULT_I2C_ADDR;
        user_data->device_cfg.value_weight_percent = 100;
        user_data->device_cfg.sampling_count = 1;
    }

    return 0;
}


// 각 endpoint 파라미터 파싱
static int __parse_endpoint_param_for_ads1115(device_ads1115_data_t* user_data, char* endpoint_config_str[], input_endpoint_data_t *endpoint_list[], int endpoint_count)
{
    int i, j;
    int code_mode;
    device_ads1115_index_table_t *src, *des;
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
            src = (device_ads1115_index_table_t *)&default_input_ads1115_config;
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
                if (adc_channel >= 0 && abs_input >= 0 && adc_channel < MAX_ADS1115_ADC_CHANNEL_COUNT) {
                    src->buttondef[pin_count].adc_channel = adc_channel;
                    src->buttondef[pin_count].abs_input = abs_input;
                    src->buttondef[pin_count].min_value = parse_number(&block_p, ",", 10, -DEFAULT_INPUT_ABS_MAX_VALUE);
                    src->buttondef[pin_count].max_value = parse_number(&block_p, ",", 10, DEFAULT_INPUT_ABS_MAX_VALUE);
                    src->buttondef[pin_count].min_adc_value = parse_number(&block_p, ",", 10, 0);
                    src->buttondef[pin_count].max_adc_value = parse_number(&block_p, ",", 10, MAX_ADS1115_ADC_VALUE);
                    src->buttondef[pin_count].mid_adc_value = parse_number(&block_p, ",", 10, MAX_ADS1115_ADC_VALUE / 2);
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


// device_config_str : spi_channel
// endpoint_config_str : endpoint, config_type (default | custom), ...
//        default: pin_count, start_index
//        custom: code_type (0|1), n * {button, value}
//            code_type = 0 : key code
//            code_type = 1 : index
//
// ex) device1=ads1115;0x20,16;0,default,12
//     device2=ads1115;0x20;1,custom,,0,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
static int init_input_device_for_ads1115(input_device_data_t *device_data, char* device_config_str, char* endpoint_config_strs[])
{
    device_ads1115_data_t* user_data;
    int result;
    int i;
    
    user_data = (device_ads1115_data_t *)kzalloc(sizeof(device_ads1115_data_t) + sizeof(device_ads1115_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);

    result = __parse_device_param_for_ads1115(user_data, device_config_str);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    result = __parse_endpoint_param_for_ads1115(user_data, endpoint_config_strs, device_data->target_endpoints, device_data->target_endpoint_count);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    for (i = 0; i < MAX_ADS1115_ADC_CHANNEL_COUNT; i++) {
        user_data->currentAdcValue[i] = MAX_MCP3008_ADC_VALUE / 2;
    }

    device_data->data = (void *)user_data;

    return 0;
}


static void start_input_device_for_ads1115(input_device_data_t *device_data)
{
    // device_ads1115_data_t *user_data = (device_ads1115_data_t *)device_data->data;

    i2c_init(bcm_peri_base_probe(), 0xB0);
    // udelay(1000);
    // // Put all GPIOA inputs on MCP23017 in INPUT mode
    // i2c_write(user_data->device_cfg.i2c_addr, MCP23017_GPIOA_MODE, &FF, 1);
    // udelay(1000);
    // // Put all inputs on MCP23017 in pullup mode
    // i2c_write(user_data->device_cfg.i2c_addr, MCP23017_GPIOA_PULLUPS_MODE, &FF, 1);
    // udelay(1000);
    // // Put all GPIOB inputs on MCP23017 in INPUT mode
    // i2c_write(user_data->device_cfg.i2c_addr, MCP23017_GPIOB_MODE, &FF, 1);
    // udelay(1000);
    // // Put all inputs on MCP23017 in pullup mode
    // i2c_write(user_data->device_cfg.i2c_addr, MCP23017_GPIOB_PULLUPS_MODE, &FF, 1);
    // udelay(1000);
    // // Put all inputs on MCP23017 in pullup mode a second time
    // // Known bug : if you remove this line, you will not have pullups on GPIOB 
    // i2c_write(user_data->device_cfg.i2c_addr, MCP23017_GPIOB_PULLUPS_MODE, &FF, 1);
    // udelay(1000);

    device_data->is_opend = TRUE;
}


static void check_input_device_for_ads1115(input_device_data_t *device_data)
{
    int i, j, k, cnt;
    int i2c_addr;
    char resultA, resultB;
    unsigned io_value;
    device_ads1115_data_t *user_data = (device_ads1115_data_t *)device_data->data;
    int sampling_count, value_weight, prev_weight;

    if (user_data == NULL) return;

    i2c_addr = user_data->device_cfg.i2c_addr;

    io_value = ((unsigned)resultB << 8) | (unsigned)resultA;

    // spi_begin(spi_channel);

    sampling_count = user_data->device_cfg.sampling_count;
    value_weight = user_data->device_cfg.value_weight_percent;
    prev_weight = 100 - value_weight;

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_ads1115_index_table_t* table = &user_data->button_cfgs[i];
        device_ads1115_index_item_t* btndef = &table->buttondef[table->button_start_index];
        // unsigned char wrbuf[3] = { 0x01, 0, 0x00 };
        // unsigned char rdbuf[3];
        int v, v0, md;

        cnt = table->pin_count;

        for (j = 0; j < cnt; j++ ){
            if (btndef->abs_input >= 0 && btndef->adc_channel >= 0) {
                // wrbuf[1] = (0x08 | btndef->adc_channel) << 4;

                if (value_weight >= 100) {
                    // spi_transfernb(wrbuf, rdbuf, 3);
                    // v = (int)(((unsigned short)rdbuf[1] & 0x3) << 8 | rdbuf[2]);
v = 0;                    
                } else {
                    v = user_data->currentAdcValue[btndef->adc_channel];
                    for (k = 0; k < sampling_count; k++) {
                        // spi_transfernb(wrbuf, rdbuf, 3);
                        // v0 = (int)(((unsigned short)rdbuf[1] & 0x3) << 8 | rdbuf[2]);
v0  = 0;
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

    // spi_end(spi_channel);
}


static void stop_input_device_for_ads1115(input_device_data_t *device_data)
{
    device_data->is_opend = FALSE;

    i2c_close();
}


int register_input_device_for_ads1115(input_device_type_desc_t *device_desc)
{
    strcpy(device_desc->device_type, "ads1115");
    device_desc->init_input_dev = init_input_device_for_ads1115;
    device_desc->start_input_dev = start_input_device_for_ads1115;
    device_desc->check_input_dev = check_input_device_for_ads1115;
    device_desc->stop_input_dev = stop_input_device_for_ads1115;
    return 0;
}
