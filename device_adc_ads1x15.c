/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#include "build_cfg.h"

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "bcm_peri.h"
#include "gpio_util.h"
#if defined(USE_I2C_DIRECT)
#include "i2c_util.h"
#else
#include <linux/i2c.h>
#endif
#include "parse_util.h"


/*
 * ADS1X15 Defines
 */

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

/** Data rates */
#define RATE_ADS1015_128SPS (0x0000)  ///< 128 samples per second
#define RATE_ADS1015_250SPS (0x0020)  ///< 250 samples per second
#define RATE_ADS1015_490SPS (0x0040)  ///< 490 samples per second
#define RATE_ADS1015_920SPS (0x0060)  ///< 920 samples per second
#define RATE_ADS1015_1600SPS (0x0080) ///< 1600 samples per second (default)
#define RATE_ADS1015_2400SPS (0x00A0) ///< 2400 samples per second
#define RATE_ADS1015_3300SPS (0x00C0) ///< 3300 samples per second

#define RATE_ADS1X15_8SPS (0x0000)   ///< 8 samples per second
#define RATE_ADS1X15_16SPS (0x0020)  ///< 16 samples per second
#define RATE_ADS1X15_32SPS (0x0040)  ///< 32 samples per second
#define RATE_ADS1X15_64SPS (0x0060)  ///< 64 samples per second
#define RATE_ADS1X15_128SPS (0x0080) ///< 128 samples per second (default)
#define RATE_ADS1X15_250SPS (0x00A0) ///< 250 samples per second
#define RATE_ADS1X15_475SPS (0x00C0) ///< 475 samples per second
#define RATE_ADS1X15_860SPS (0x00E0) ///< 860 samples per second

/** Gain settings */
static int ads1x15_gain_setting[][2] = {
    {ADS1X15_REG_CONFIG_PGA_6_144V, 6144},  // +/-6.144V range = Gain 2/3
    {ADS1X15_REG_CONFIG_PGA_4_096V, 4096},  // +/-4.096V range = Gain 1
    {ADS1X15_REG_CONFIG_PGA_2_048V, 2048},  // +/-2.048V range = Gain 2 (default)
    {ADS1X15_REG_CONFIG_PGA_1_024V, 1024},  // +/-1.024V range = Gain 4
    {ADS1X15_REG_CONFIG_PGA_0_512V, 512},   // +/-0.512V range = Gain 8
    {ADS1X15_REG_CONFIG_PGA_0_256V, 256}    // +/-0.256V range = Gain 16
};


#define MAX_ADS1X15_ADC_CHANNEL_COUNT  (4)
#define MAX_ADS1X15_ADC_VALUE          (32767)

// default i2c address (ADDR = GND)
#define ADS1X15_DEFAULT_I2C_ADDR       (0x48)


typedef struct tag_device_ads1x15_desc_config {
    int shift_size;
    int value_size;
} device_ads1x15_desc_config_t;

typedef struct tag_device_ads1x15_config {
    int i2c_addr;
    int ref_milli_volt;
    int ads_gain;
    int value_weight_percent;
    int sampling_count;
} device_ads1x15_config_t;

typedef struct tag_device_ads1x15_index_item {
    int adc_channel;
    int abs_input;
    int min_value;
    int max_value;
    int min_adc_value;
    int max_adc_value;
    int mid_adc_value;
} device_ads1x15_index_item_t;

typedef struct tag_device_ads1x15_index_table {
    device_ads1x15_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int pin_count;
    int button_start_index;
} device_ads1x15_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_ads1x15_data {
    int currentAdcValue[MAX_ADS1X15_ADC_CHANNEL_COUNT];

    device_ads1x15_desc_config_t    device_desc_data;
    device_ads1x15_config_t         device_cfg;
#if !defined(USE_I2C_DIRECT)    
	struct i2c_client *i2c;
#endif
    device_ads1x15_index_table_t    button_cfgs[1];
} device_ads1x15_data_t;


#define INPUT_ADS1X15_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (4)

static const device_ads1x15_index_table_t default_input_ads1x15_config = {
    {
        {0, ABS_X,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_ADS1X15_ADC_VALUE, MAX_ADS1X15_ADC_VALUE/2},
        {1, ABS_Y,        -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_ADS1X15_ADC_VALUE, MAX_ADS1X15_ADC_VALUE/2},
        {2, ABS_RX,       -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_ADS1X15_ADC_VALUE, MAX_ADS1X15_ADC_VALUE/2},
        {3, ABS_RY,       -DEFAULT_INPUT_ABS_MAX_VALUE, DEFAULT_INPUT_ABS_MAX_VALUE, 0, MAX_ADS1X15_ADC_VALUE, MAX_ADS1X15_ADC_VALUE/2}
    }, 
    INPUT_ADS1X15_DEFAULT_KEYCODE_TABLE_ITEM_COUNT,
    0
};


// device 파라미터 파싱
static int __parse_device_param_for_ads1x15(device_ads1x15_data_t* user_data, char* device_config_str)
{
    char szText[512];
    char* pText;

    if (device_config_str != NULL) {
        strcpy(szText, device_config_str); 
        pText = szText;

        user_data->device_cfg.i2c_addr = parse_number(&pText, ",", 0, ADS1X15_DEFAULT_I2C_ADDR);
        user_data->device_cfg.ref_milli_volt = parse_number(&pText, ",", 0, 3300);
        user_data->device_cfg.ads_gain = parse_number(&pText, ",", 0, 1);
        user_data->device_cfg.value_weight_percent = parse_number(&pText, ",", 10, 100);
        user_data->device_cfg.sampling_count = parse_number(&pText, ",", 10, 1);
    } else {
        user_data->device_cfg.i2c_addr = ADS1X15_DEFAULT_I2C_ADDR;
        user_data->device_cfg.ref_milli_volt = 3300;
        user_data->device_cfg.ads_gain = 1;
        user_data->device_cfg.value_weight_percent = 100;
        user_data->device_cfg.sampling_count = 1;
    }

    return 0;
}


// 각 endpoint 파라미터 파싱
static int __parse_endpoint_param_for_ads1x15(device_ads1x15_data_t* user_data, char* endpoint_config_str[], input_endpoint_data_t *endpoint_list[], int endpoint_count)
{
    int i, j;
    int code_mode;
    device_ads1x15_index_table_t *src, *des;
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
            src = (device_ads1x15_index_table_t *)&default_input_ads1x15_config;
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
                if (adc_channel >= 0 && abs_input >= 0 && adc_channel < MAX_ADS1X15_ADC_CHANNEL_COUNT) {
                    src->buttondef[pin_count].adc_channel = adc_channel;
                    src->buttondef[pin_count].abs_input = abs_input;
                    src->buttondef[pin_count].min_value = parse_number(&block_p, ",", 10, -DEFAULT_INPUT_ABS_MAX_VALUE);
                    src->buttondef[pin_count].max_value = parse_number(&block_p, ",", 10, DEFAULT_INPUT_ABS_MAX_VALUE);
                    src->buttondef[pin_count].min_adc_value = parse_number(&block_p, ",", 10, 0);
                    src->buttondef[pin_count].max_adc_value = parse_number(&block_p, ",", 10, MAX_ADS1X15_ADC_VALUE);
                    src->buttondef[pin_count].mid_adc_value = parse_number(&block_p, ",", 10, MAX_ADS1X15_ADC_VALUE / 2);
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


// device_config_str : i2c_addr, ref_milli_volt, adc_gain, value_weight_percent, sampling_count
// endpoint_config_str : endpoint, config_type (default | custom), ...
//        default: pin_count, start_index
//        custom: code_type (0|1), n * {adc_channel, button, min_value, max_value, adc_min_value, adc_max_value, adc_mid_value}
//            code_type = 0 : key code
//            code_type = 1 : index
//
// ex) device1=ads1x15;0x48,3300,1;0,default,12
//     device2=ads1x15;0x48;1,custom,,0,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
static int init_input_device_for_ads1x15(void* device_desc_data, input_device_data_t *device_data, char* device_config_str, char* endpoint_config_strs[])
{
    device_ads1x15_data_t* user_data;
    int result;
    int i;
    
    user_data = (device_ads1x15_data_t *)kzalloc(sizeof(device_ads1x15_data_t) + sizeof(device_ads1x15_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);
    user_data->device_desc_data = *(device_ads1x15_desc_config_t *)device_desc_data;

    result = __parse_device_param_for_ads1x15(user_data, device_config_str);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    result = __parse_endpoint_param_for_ads1x15(user_data, endpoint_config_strs, device_data->target_endpoints, device_data->target_endpoint_count);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    for (i = 0; i < MAX_ADS1X15_ADC_CHANNEL_COUNT; i++) {
        user_data->currentAdcValue[i] = MAX_ADS1X15_ADC_VALUE / 2;
    }

    device_data->data = (void *)user_data;

    return 0;
}


#if !defined(USE_I2C_DIRECT)
static int __ads1x15_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	return 0;
}

static int __ads1x15_remove(struct i2c_client *i2c)
{
	return 0;
}

static const struct of_device_id __ads1x15_of_ids[] = {
    { .compatible = "brcm,bcm2835" },
	{} /* sentinel */
};
MODULE_DEVICE_TABLE(of, __ads1x15_of_ids);

static const struct i2c_device_id __ads1x15_i2c_ids[] = {
	{ "ads1x15", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, __ads1x15_i2c_ids);

static struct i2c_driver __ads1x15_driver = {
	.driver = {
		.name = "ads1x15",
        .owner = THIS_MODULE,
		.of_match_table = of_match_ptr(__ads1x15_of_ids)
	},
	.probe = __ads1x15_probe,
	.remove = __ads1x15_remove,
    .id_table = __ads1x15_i2c_ids,
};
#endif

static void start_input_device_for_ads1x15(input_device_data_t *device_data)
{
#if !defined(USE_I2C_DIRECT)
    device_ads1x15_data_t *user_data = (device_ads1x15_data_t *)device_data->data;
#endif    

#if defined(USE_I2C_DIRECT)
    i2c_init(bcm_peri_base_probe(), 0xB0);
#else
    // add driver
	int r = i2c_add_driver(&__ads1x15_driver);
    // printk("i2c_add_driver = %d", r);

    if (r >= 0) {
        struct i2c_board_info i2c_board_info = {
            I2C_BOARD_INFO("ads1x15", user_data->device_cfg.i2c_addr)
        };
        struct i2c_adapter* i2c_adap = i2c_get_adapter(1);
        if (i2c_adap == NULL) {
            return;
        }
        user_data->i2c = i2c_new_client_device(i2c_adap, &i2c_board_info);
        i2c_put_adapter(i2c_adap);
    }
#endif    

    device_data->is_opend = TRUE;
}


#if defined(USE_I2C_DIRECT)
static uint16_t __readRegister(int i2c_addr, uint8_t reg) 
{
    uint8_t buffer[2];
    i2c_read(i2c_addr, reg, buffer, 2);
    return ((buffer[0] << 8) | buffer[1]);
}

static void __writeRegister(int i2c_addr, uint8_t reg, uint16_t value) 
{
    uint8_t buffer[2];
    buffer[0] = value >> 8;
    buffer[1] = value & 0xFF;
    i2c_write(i2c_addr, reg, buffer, 2);
}
#else
static uint16_t __readRegister(struct i2c_client* i2c, uint8_t reg) 
{
    if (!IS_ERR_OR_NULL(i2c)) {
        int value = i2c_smbus_read_word_data(i2c, reg);
        return (value < 0) ? 0 : value;
    } else {
        return 0;
    }
}

static void __writeRegister(struct i2c_client* i2c, uint8_t reg, uint16_t value) 
{
    if (!IS_ERR_OR_NULL(i2c)) {
        i2c_smbus_write_word_data(i2c, reg, value);
    }
}
#endif



#if defined(USE_I2C_DIRECT)
static int16_t __readADC_SingleEnded(int i2c_addr, uint8_t channel, unsigned gain) 
#else
static int16_t __readADC_SingleEnded(struct i2c_client* i2c_addr, uint8_t channel, unsigned gain) 
#endif
{
    uint16_t config;

    if (channel > 3) {
        return 0;
    }

    // Start with default values
    config = ADS1X15_REG_CONFIG_CQUE_NONE |    // Disable the comparator (default val)
            ADS1X15_REG_CONFIG_CLAT_NONLAT |  // Non-latching (default val)
            ADS1X15_REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
            ADS1X15_REG_CONFIG_CMODE_TRAD |   // Traditional comparator (default val)
            ADS1X15_REG_CONFIG_MODE_SINGLE;   // Single-shot mode (default)

    // Set PGA/voltage range
    config |= gain;

    // Set data rate
    config |= RATE_ADS1015_2400SPS;

    // Set single-ended input channel
    switch (channel) {
        case 0:
            config |= ADS1X15_REG_CONFIG_MUX_SINGLE_0;
            break;
        case 1:
            config |= ADS1X15_REG_CONFIG_MUX_SINGLE_1;
            break;
        case 2:
            config |= ADS1X15_REG_CONFIG_MUX_SINGLE_2;
            break;
        case 3:
            config |= ADS1X15_REG_CONFIG_MUX_SINGLE_3;
            break;
    }

    // Set 'start single-conversion' bit
    config |= ADS1X15_REG_CONFIG_OS_SINGLE;

    // Write config register to the ADC
    __writeRegister(i2c_addr, ADS1X15_REG_POINTER_CONFIG, config);

    // Wait for the conversion to complete
    while ((__readRegister(i2c_addr, ADS1X15_REG_POINTER_CONFIG) & 0x8000) == 0) {}

    // Read the conversion results
    return __readRegister(i2c_addr, ADS1X15_REG_POINTER_CONVERT);
}


static void check_input_device_for_ads1x15(input_device_data_t *device_data)
{
    int i, j, k, cnt;
#if defined(USE_I2C_DIRECT)    
    int i2c;
#else    
    struct i2c_client* i2c;
#endif    
    unsigned adc_gain;
    int adc_gain_milli_volt, ref_milli_volt;
    device_ads1x15_data_t *user_data = (device_ads1x15_data_t *)device_data->data;
    int sampling_count, value_weight, prev_weight;

    if (user_data == NULL) return;

#if defined(USE_I2C_DIRECT)    
    i2c = user_data->device_cfg.i2c_addr;
#else
    i2c = user_data->i2c;
    if (IS_ERR_OR_NULL(i2c)) {
        return;
    }
#endif    
    adc_gain = ads1x15_gain_setting[user_data->device_cfg.ads_gain][0]; /* +/- 4.096V range (limited to VDD +0.3V max!) */
    adc_gain_milli_volt = ads1x15_gain_setting[user_data->device_cfg.ads_gain][1];
    ref_milli_volt = user_data->device_cfg.ref_milli_volt;

    sampling_count = user_data->device_cfg.sampling_count;
    value_weight = user_data->device_cfg.value_weight_percent;
    prev_weight = 100 - value_weight;

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_ads1x15_index_table_t* table = &user_data->button_cfgs[i];
        device_ads1x15_index_item_t* btndef = &table->buttondef[table->button_start_index];
        int v, v0, md;

        cnt = table->pin_count;

        for (j = 0; j < cnt; j++ ){
            if (btndef->abs_input >= 0 && btndef->adc_channel >= 0) {
                if (value_weight >= 100) {
                    // 4.096 / 32767 / 3.3 = 4.096 / (32767 * 3.3) = 4096 / (32767 * 3300) = 0 ~ 1
                    v = __readADC_SingleEnded(i2c, btndef->adc_channel, adc_gain);
                    v *= adc_gain_milli_volt / ref_milli_volt;
                } else {
                    v = user_data->currentAdcValue[btndef->adc_channel];
                    for (k = 0; k < sampling_count; k++) {
                        // 4.096 / 32767 / 3.3 = 4.096 / (32767 * 3.3) = 4096 / (32767 * 3300) = 0 ~ 1
                        v0 = __readADC_SingleEnded(i2c, btndef->adc_channel, adc_gain);
                        v0 *= adc_gain_milli_volt / ref_milli_volt;
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
}


static void stop_input_device_for_ads1x15(input_device_data_t *device_data)
{
#if !defined(USE_I2C_DIRECT)
    device_ads1x15_data_t *user_data = (device_ads1x15_data_t *)device_data->data;
#endif    

    device_data->is_opend = FALSE;

#if defined(USE_I2C_DIRECT)
    i2c_close();
#else
	i2c_del_driver(&__ads1x15_driver);
    if (!IS_ERR_OR_NULL(user_data->i2c)) {
        i2c_unregister_device(user_data->i2c);
    }
#endif    
}


int register_input_device_for_ads1115(input_device_type_desc_t *device_desc)
{
    device_ads1x15_desc_config_t* desc = (device_ads1x15_desc_config_t *)device_desc->device_desc_data;

    strcpy(device_desc->device_type, "ads1115");
    desc->shift_size = 2;
    desc->value_size = 14;

    device_desc->init_input_dev = init_input_device_for_ads1x15;
    device_desc->start_input_dev = start_input_device_for_ads1x15;
    device_desc->check_input_dev = check_input_device_for_ads1x15;
    device_desc->stop_input_dev = stop_input_device_for_ads1x15;

    return 0;
}


int register_input_device_for_ads1015(input_device_type_desc_t *device_desc)
{
    device_ads1x15_desc_config_t* desc = (device_ads1x15_desc_config_t *)device_desc->device_desc_data;

    strcpy(device_desc->device_type, "ads1015");
    desc->shift_size = 2;
    desc->value_size = 14;

    device_desc->init_input_dev = init_input_device_for_ads1x15;
    device_desc->start_input_dev = start_input_device_for_ads1x15;
    device_desc->check_input_dev = check_input_device_for_ads1x15;
    device_desc->stop_input_dev = stop_input_device_for_ads1x15;

    return 0;
}
