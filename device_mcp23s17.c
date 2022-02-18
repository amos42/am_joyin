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
#if defined(USE_SPI_DIRECT)
#include "spi_util.h"
#else
#include <linux/spi/spi.h>
#endif
#include "parse_util.h"

//#define GPIO_GET(i)   GPIO_READ(i)
//#define GPIO_GET_VALUE(i)   getGpio(i)
//#define GPIO_SET_VALUE(i,v)   setGpio((i), (v))

//#define MAX_ADDR_IO_COUNT   (6)
#define MCP23S17_DEFAULT_SPI_CHANNEL    (0)

/*
 * MCP23S17 Defines
 */
#define MCP23S17_ADDRESS_WRITE   (0x40)
#define MCP23S17_ADDRESS_READ    (0x41)

#define MCP23S17_IODIRA    (0x00)      // MCP23x17 I/O Direction Register
#define MCP23S17_IODIRB    (0x01)      // 1 = Input (default), 0 = Output
#define MCP23S17_IPOLA     (0x02)      // MCP23x17 Input Polarity Register
#define MCP23S17_IPOLB     (0x03)      // 0 = Normal (default)(low reads as 0), 1 = Inverted (low reads as 1)
#define MCP23S17_GPINTENA  (0x04)      // MCP23x17 Interrupt on Change Pin Assignements
#define MCP23S17_GPINTENB  (0x05)      // 0 = No Interrupt on Change (default), 1 = Interrupt on Change
#define MCP23S17_DEFVALA   (0x06)      // MCP23x17 Default Compare Register for Interrupt on Change
#define MCP23S17_DEFVALB   (0x07)      // Opposite of what is here will trigger an interrupt (default = 0)
#define MCP23S17_INTCONA   (0x08)      // MCP23x17 Interrupt on Change Control Register
#define MCP23S17_INTCONB   (0x09)      // 1 = pin is compared to DEFVAL, 0 = pin is compared to previous state (default)
#define MCP23S17_IOCON     (0x0A)      // MCP23x17 Configuration Register
//                         (0x0B)      //     Also Configuration Register
#define MCP23S17_GPPUA     (0x0C)      // MCP23x17 Weak Pull-Up Resistor Register
#define MCP23S17_GPPUB     (0x0D)      // INPUT ONLY: 0 = No Internal 100k Pull-Up (default) 1 = Internal 100k Pull-Up 
#define MCP23S17_INTFA     (0x0E)      // MCP23x17 Interrupt Flag Register
#define MCP23S17_INTFB     (0x0F)      // READ ONLY: 1 = This Pin Triggered the Interrupt
#define MCP23S17_INTCAPA   (0x10)      // MCP23x17 Interrupt Captured Value for Port Register
#define MCP23S17_INTCAPB   (0x11)      // READ ONLY: State of the Pin at the Time the Interrupt Occurred
#define MCP23S17_GPIOA     (0x12)      // MCP23x17 GPIO Port Register
#define MCP23S17_GPIOB     (0x13)      // Value on the Port - Writing Sets Bits in the Output Latch
#define MCP23S17_OLATA     (0x14)      // MCP23x17 Output Latch Register
#define MCP23S17_OLATB     (0x15)      // 1 = Latch High, 0 = Latch Low (default) Reading Returns Latch State, Not Port Value!


typedef struct tag_device_mcp23s17_config {
    int spi_channel;
    int io_count;
    int pull_updown;
} device_mcp23s17_config_t;

typedef struct tag_device_mcp23s17_index_item {
    int button;
    int value;
} device_mcp23s17_index_item_t;

typedef struct tag_device_mcp23s17_index_table {
    device_mcp23s17_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int pin_count;
    int button_start_index;
    int io_skip_count;
} device_mcp23s17_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_mcp23s17_data {
    device_mcp23s17_config_t         device_cfg;
    device_mcp23s17_index_table_t    button_cfgs[1];
} device_mcp23s17_data_t;


#define INPUT_MCP23S17_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (16)

static const device_mcp23s17_index_table_t default_input_mcp23s17_config = {
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
    INPUT_MCP23S17_DEFAULT_KEYCODE_TABLE_ITEM_COUNT,
    0, 0
};


// device 파라미터 파싱
static int __parse_device_param_for_mcp23s17(device_mcp23s17_data_t* user_data, char* device_config_str)
{
    char szText[512];
    char* pText;

    if (device_config_str != NULL) {
        strcpy(szText, device_config_str); 
        pText = szText;

        user_data->device_cfg.spi_channel = parse_number(&pText, ",", 0, MCP23S17_DEFAULT_SPI_CHANNEL);
        user_data->device_cfg.io_count = parse_number(&pText, ",", 10, INPUT_MCP23S17_DEFAULT_KEYCODE_TABLE_ITEM_COUNT);
        user_data->device_cfg.pull_updown = parse_number(&pText, ",", 10, 0);
    } else {
        user_data->device_cfg.spi_channel = MCP23S17_DEFAULT_SPI_CHANNEL;
        user_data->device_cfg.io_count = INPUT_MCP23S17_DEFAULT_KEYCODE_TABLE_ITEM_COUNT;
        user_data->device_cfg.pull_updown = 0;
    }

    return 0;
}


// 각 endpoint 파라미터 파싱
static int __parse_endpoint_param_for_mcp23s17(device_mcp23s17_data_t* user_data, char* endpoint_config_str[], input_endpoint_data_t *endpoint_list[], int endpoint_count)
{
    int i, j;
    int code_mode;
    device_mcp23s17_index_table_t *src, *des;
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
            src = (device_mcp23s17_index_table_t *)&default_input_mcp23s17_config;
            code_mode = INPUT_CODE_TYPE_KEYCODE;

            pin_count = parse_number(&pText, ",", 10, src->pin_count);
            button_start_index = parse_number(&pText, ",", 10, 0);
            io_skip_count = parse_number(&pText, ",", 10, 0);
        } else if (strcmp(cfgtype_p, "custom") == 0){
            io_skip_count = parse_number(&pText, ",", 10, 0);

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


// device_config_str : spi_channel, io_count
// endpoint_config_str : endpoint, config_type (default | custom), ...
//        default: pin_count, start_index, io_skip_count
//        custom: io_skip_count, code_type (0|1), n * {button, value}
//            code_type = 0 : key code
//            code_type = 1 : index
//
// ex) device1=mcp23s17;0x20,16;0,default,12
//     device2=mcp23s17;0x20;1,custom,,0,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
static int init_input_device_for_mcp23s17(void* device_desc_data, input_device_data_t *device_data, char* device_config_str, char* endpoint_config_strs[])
{
    device_mcp23s17_data_t* user_data;
    int result;
    
    user_data = (device_mcp23s17_data_t *)kzalloc(sizeof(device_mcp23s17_data_t) + sizeof(device_mcp23s17_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);

    result = __parse_device_param_for_mcp23s17(user_data, device_config_str);
    if (result != 0) {
        kfree(user_data);
        return result;
    }

    result = __parse_endpoint_param_for_mcp23s17(user_data, endpoint_config_strs, device_data->target_endpoints, device_data->target_endpoint_count);
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

#if !defined(USE_SPI_DIRECT)
static int __mcp23s17_spi_probe(struct spi_device *spi)
{
	return 0;
}

static int __mcp23s17_spi_remove(struct spi_device *spi)
{
	return 0;
}

static struct of_device_id __mcp23s17_match_table[] = { 
    {.compatible = "mcp23s17",},
    {},
};
MODULE_DEVICE_TABLE(of, __mcp23s17_match_table);

static struct spi_driver __mcp23s17_spi_driver = {
    .driver = {
        .name = "mcp23s17",
        .owner = THIS_MODULE,
        .of_match_table = __mcp23s17_match_table,},
    .probe = __mcp23s17_spi_probe,
    .remove = __mcp23s17_spi_remove,
};
#endif

static uint8_t __mcp23s17_spi_read(uint8_t cmd)
{
#if defined(USE_SPI_DIRECT)
    unsigned char buf[3] = {0x41, cmd, 0};
    spi_transfern(buf, 3);
    return buf[2];
#else

#endif
}

static uint8_t __mcp23s17_spi_write(uint8_t cmd, uint8_t value)
{
#if defined(USE_SPI_DIRECT)
    unsigned char buf[3] = {0x40, cmd, value};
    spi_transfern(buf, 3);
    return buf[2];
#else

#endif
}

static void start_input_device_for_mcp23s17(input_device_data_t *device_data)
{
    device_mcp23s17_data_t *user_data = (device_mcp23s17_data_t *)device_data->data;
    //char FF = 0xFF;

#if defined(USE_SPI_DIRECT)
    spi_init(bcm_peri_base_probe(), 0xB0);
    spi_setClockDivider(256);
    udelay(100);

    spi_begin();
    spi_chipSelect(user_data->device_cfg.spi_channel);

    __mcp23s17_spi_write(MCP23017_GPIOA_MODE, 0xFF);
    //udelay(100);

    // read one byte on GPIOA (for dummy)
    //__mcp23s17_spi_read(MCP23017_GPIOA_READ);
    //udelay(100);

    __mcp23s17_spi_write(MCP23017_GPIOA_PULLUPS_MODE, 0xFF);
    //udelay(100);

    __mcp23s17_spi_write(MCP23017_GPIOB_MODE, 0xFF);
    //udelay(100);

    // read one byte on GPIOA (for dummy)
    //__mcp23s17_spi_read(MCP23017_GPIOB_READ);
    //udelay(100);

    __mcp23s17_spi_write(MCP23017_GPIOB_PULLUPS_MODE, 0xFF);

    spi_end();
#else
    spi_register_driver(&__mcp23s17_spi_driver);
#endif

    device_data->is_opend = TRUE;
}


static void check_input_device_for_mcp23s17(input_device_data_t *device_data)
{
    int i, j, cnt;
    int spi_channel, io_count;
    uint8_t resultA, resultB;
    unsigned io_value;
    device_mcp23s17_data_t *user_data = (device_mcp23s17_data_t *)device_data->data;

    if (user_data == NULL) return;

    spi_channel = user_data->device_cfg.spi_channel;
    io_count = user_data->device_cfg.io_count;

    if (io_count <= 0) return;

#if defined(USE_SPI_DIRECT)
    spi_begin();
    spi_chipSelect(spi_channel);

    resultA = __mcp23s17_spi_read(MCP23S17_GPIOA);
    resultB = __mcp23s17_spi_read(MCP23S17_GPIOB);

    spi_end();
#else

#endif
    io_value = ((unsigned)resultB << 8) | (unsigned)resultA;

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_mcp23s17_index_table_t* table = &user_data->button_cfgs[i];
        device_mcp23s17_index_item_t* btndef = &table->buttondef[table->button_start_index];

        cnt = table->pin_count;
        if (cnt > io_count) cnt = io_count;
        io_count -= cnt;

        io_value >>= table->io_skip_count;
        for (j = 0; j < cnt; j++ ){
            if ((io_value & 0x1) == 0) {
                endpoint->report_button_state[btndef->button] = btndef->value;
            }
            io_value >>= 1;

            btndef++;
        }

        if (io_count <= 0) break;
    }
}


static void stop_input_device_for_mcp23s17(input_device_data_t *device_data)
{
    device_data->is_opend = FALSE;
    
#if defined(USE_SPI_DIRECT)
    spi_close();
#else

#endif
}


int register_input_device_for_mcp23s17(input_device_type_desc_t *device_desc)
{
    strcpy(device_desc->device_type, "mcp23s17");
    device_desc->init_input_dev = init_input_device_for_mcp23s17;
    device_desc->start_input_dev = start_input_device_for_mcp23s17;
    device_desc->check_input_dev = check_input_device_for_mcp23s17;
    device_desc->stop_input_dev = stop_input_device_for_mcp23s17;
    return 0;
}
