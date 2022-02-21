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


//#define MAX_ADDR_IO_COUNT   (6)

/*
 * MCP23017 Defines
 */
#define MCP23017_GPIOA_MODE	            (0x00)
#define MCP23017_GPIOB_MODE	            (0x01)
#define MCP23017_IOCON                  (0x0A)
#define MCP23017_GPIOA_PULLUPS_MODE	    (0x0C)
#define MCP23017_GPIOB_PULLUPS_MODE     (0x0D)
#define MCP23017_GPIOA_READ             (0x12)
#define MCP23017_GPIOB_READ             (0x13)

// default i2c addr
#define MCP23017_DEFAULT_I2C_ADDR       (0x20)


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
    int pin_count;
    int button_start_index;
    int io_skip_count;
} device_mcp23017_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_mcp23017_data {
    device_mcp23017_config_t         device_cfg;
#if !defined(USE_I2C_DIRECT)    
	struct i2c_client *i2c;
#endif    
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
    INPUT_MCP23017_DEFAULT_KEYCODE_TABLE_ITEM_COUNT,
    0, 0
};


// device 파라미터 파싱
static int __parse_device_param_for_mcp23017(device_mcp23017_data_t* user_data, char* device_config_str)
{
    char szText[512];
    char* pText;

    if (device_config_str != NULL) {
        strcpy(szText, device_config_str); 
        pText = szText;

        user_data->device_cfg.i2c_addr = parse_number(&pText, ",", 0, MCP23017_DEFAULT_I2C_ADDR);
        user_data->device_cfg.io_count = parse_number(&pText, ",", 10, INPUT_MCP23017_DEFAULT_KEYCODE_TABLE_ITEM_COUNT);
        user_data->device_cfg.pull_updown = parse_number(&pText, ",", 10, 0);
    } else {
        user_data->device_cfg.i2c_addr = MCP23017_DEFAULT_I2C_ADDR;
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
            src = (device_mcp23017_index_table_t *)&default_input_mcp23017_config;
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

// device_config_str : i2c_addr, io_count
// endpoint_config_str : endpoint, config_type (default | custom), ...
//        default: pin_count, start_index, io_skip_count
//        custom: io_skip_count, code_type (0|1), n * {button, value}
//            code_type = 0 : key code
//            code_type = 1 : index
//
// ex) device1=mcp23017;0x20,16;0,default,12
//     device2=mcp23017;0x20;1,custom,,0,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
static int init_input_device_for_mcp23017(void* device_desc_data, input_device_data_t *device_data, char* device_config_str, char* endpoint_config_strs[])
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


#if !defined(USE_I2C_DIRECT)
//static struct i2c_client *__mcp23017_i2c = NULL;
//static int __mcp23017_i2c_refcnt = 0;

static int __mcp23017_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
    // device_mcp23017_data_t *user_data = (device_mcp23017_data_t *)i2c->dev.platform_data;
  
    // if (++__mcp23017_i2c_refcnt == 1) {
    //     __mcp23017_i2c = i2c;
    // }

	// i2c_set_clientdata(i2c, user_data);
	// user_data->i2c = i2c;

	return 0;
}

static int __mcp23017_remove(struct i2c_client *i2c)
{
	// device_mcp23017_data_t* user_data = (device_mcp23017_data_t *)i2c_get_clientdata(i2c);
  
    // if (--__mcp23017_i2c_refcnt == 0) {
    //     __mcp23017_i2c = NULL;
    // }

	return 0;
}

static const struct of_device_id __mcp23017_of_ids[] = {
    { .compatible = "brcm,bcm2835" },
	{} /* sentinel */
};
MODULE_DEVICE_TABLE(of, __mcp23017_of_ids);

static const struct i2c_device_id __mcp23017_i2c_ids[] = {
	{ "mcp23017", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, __mcp23017_i2c_ids);

static struct i2c_driver __mcp23017_i2c_driver = {
	.driver = {
		.name = "mcp23017",
        .owner = THIS_MODULE,
		.of_match_table = of_match_ptr(__mcp23017_of_ids)
	},
	.probe = __mcp23017_probe,
	.remove = __mcp23017_remove,
    .id_table = __mcp23017_i2c_ids,
};
#endif

static void start_input_device_for_mcp23017(input_device_data_t *device_data)
{
    device_mcp23017_data_t *user_data = (device_mcp23017_data_t *)device_data->data;

#if defined(USE_I2C_DIRECT)
    char outval;

    i2c_init(bcm_peri_base_probe(), 0xB0);
    //udelay(1000);

    // outval = 0x00;
    // i2c_write(user_data->device_cfg.i2c_addr, MCP23017_IOCON, &outval, 1);
    // udelay(1000);

    outval = 0xFF;
    // Put all GPIOA inputs on MCP23017 in INPUT mode
    i2c_write(user_data->device_cfg.i2c_addr, MCP23017_GPIOA_MODE, &outval, 1);
    //udelay(1000);

    // read one byte on GPIOA (for dummy)
    //i2c_read_1byte(user_data->device_cfg.i2c_addr, MCP23017_GPIOA_READ);
    //udelay(1000);

    // Put all inputs on MCP23017 in pullup mode
    i2c_write(user_data->device_cfg.i2c_addr, MCP23017_GPIOA_PULLUPS_MODE, &outval, 1);
    //udelay(1000);

    // Put all GPIOB inputs on MCP23017 in INPUT mode
    i2c_write(user_data->device_cfg.i2c_addr, MCP23017_GPIOB_MODE, &outval, 1);
    //udelay(1000);

    // read one byte on GPIOB (for dummy)
    //i2c_read_1byte(user_data->device_cfg.i2c_addr, MCP23017_GPIOB_READ);
    //udelay(1000);

    // Put all inputs on MCP23017 in pullup mode
    i2c_write(user_data->device_cfg.i2c_addr, MCP23017_GPIOB_PULLUPS_MODE, &outval, 1);
    //udelay(1000);
#else
    // add driver
	int r = i2c_add_driver(&__mcp23017_i2c_driver);
    // printk("i2c_add_driver = %d", r);

    if (r >= 0) {
        struct i2c_board_info i2c_board_info = {
            I2C_BOARD_INFO("mcp23017", user_data->device_cfg.i2c_addr)
        };
        struct i2c_adapter* i2c_adap = i2c_get_adapter(1);
        if (i2c_adap == NULL) {
            return;
        }
        user_data->i2c = i2c_new_client_device(i2c_adap, &i2c_board_info);
        i2c_put_adapter(i2c_adap);
    }

    // printk("%p %p", user_data->i2c, __mcp23017_i2c);
    // if (!IS_ERR_OR_NULL(__mcp23017_i2c)) {
    //     user_data->i2c = __mcp23017_i2c;
    // }

    if (!IS_ERR_OR_NULL(user_data->i2c)) {
        // Put all GPIOA pins to input mode & all pullup
        i2c_smbus_write_byte_data(user_data->i2c, MCP23017_GPIOA_MODE, 0xFF);
        i2c_smbus_write_byte_data(user_data->i2c, MCP23017_GPIOA_PULLUPS_MODE, 0xFF);

        // Put all GPIOB pins to input mode & all pullup
        i2c_smbus_write_byte_data(user_data->i2c, MCP23017_GPIOB_MODE, 0xFF);
        i2c_smbus_write_byte_data(user_data->i2c, MCP23017_GPIOB_PULLUPS_MODE, 0xFF);
    }
#endif

    device_data->is_opend = TRUE;
}


static void check_input_device_for_mcp23017(input_device_data_t *device_data)
{
    int i, j, cnt;
    int io_count;
#if defined(USE_I2C_DIRECT)
    int i2c_addr;
#endif
    char resultA, resultB;
    unsigned io_value;
    device_mcp23017_data_t *user_data = (device_mcp23017_data_t *)device_data->data;

    if (user_data == NULL) return;

    io_count = user_data->device_cfg.io_count;
    if (io_count <= 0) return;

#if defined(USE_I2C_DIRECT)
    i2c_addr = user_data->device_cfg.i2c_addr;
    i2c_read(i2c_addr, MCP23017_GPIOA_READ, &resultA, 1);
    i2c_read(i2c_addr, MCP23017_GPIOB_READ, &resultB, 1);
#else
    if (!IS_ERR_OR_NULL(user_data->i2c)) {
        resultA = i2c_smbus_read_byte_data(user_data->i2c, MCP23017_GPIOA_READ);
        resultB = i2c_smbus_read_byte_data(user_data->i2c, MCP23017_GPIOB_READ);
    } else {
        resultA = resultB = 0xFF;
    }
#endif

    io_value = ((unsigned)resultB << 8) | (unsigned)resultA;

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_mcp23017_index_table_t* table = &user_data->button_cfgs[i];
        device_mcp23017_index_item_t* btndef = &table->buttondef[table->button_start_index];

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


static void stop_input_device_for_mcp23017(input_device_data_t *device_data)
{
#if !defined(USE_I2C_DIRECT)
    device_mcp23017_data_t *user_data = (device_mcp23017_data_t *)device_data->data;
#endif    

    device_data->is_opend = FALSE;

#if defined(USE_I2C_DIRECT)
    i2c_close();
#else
	i2c_del_driver(&__mcp23017_i2c_driver);
    if (!IS_ERR_OR_NULL(user_data->i2c)) {
        i2c_unregister_device(user_data->i2c);
    }
#endif
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
