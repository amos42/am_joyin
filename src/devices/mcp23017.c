#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "../util/gpio_util.h"
#include "mcp23017.h"

//#define GPIO_GET(i)   GPIO_READ(i)
//#define GPIO_GET_VALUE(i)   getGpio(i)
//#define GPIO_SET_VALUE(i,v)   setGpio((i), (v))

#define MAX_ADDR_IO_COUNT   (6)

/*
 * MCP23017 Defines
 */
#define MPC23017_GPIOA_MODE	            (0x00)
#define MPC23017_GPIOB_MODE	            (0x01)
#define MPC23017_GPIOA_PULLUPS_MODE	    (0x0C)
#define MPC23017_GPIOB_PULLUPS_MODE     (0x0D)
#define MPC23017_GPIOA_READ             (0x12)
#define MPC23017_GPIOB_READ             (0x13)


static const int default_mcp23017_gpio_maps[MAX_ADDR_IO_COUNT] = {16, 20, 21, };

//#define DEFAULT_MCP23017_BUTTON_COUNT  (16)

// static const device_mcp23017_index_item_t default_mcp23017_button_config[DEFAULT_MCP23017_BUTTON_COUNT] = {
//     {ABS_Y,     -1},
//     {ABS_Y,      1},
//     {ABS_X,     -1},
//     {ABS_X,      1}, 
//     {BTN_START,  1},
//     {BTN_SELECT, 1},
//     {BTN_A,      1},
//     {BTN_B,      1},
//     {BTN_X,      1},
//     {BTN_Y,      1},
//     {BTN_TL,     1},
//     {BTN_TR,     1},
//     {BTN_MODE,   1},
//     {BTN_TL2,    1},
//     {BTN_TR2,    1},
//     {BTN_Z,      1}
// };

typedef struct tag_device_mcp23017_config {
    int i2c_addr;
    int io_count;
    int is_pullup;
} device_mcp23017_config_t;

typedef struct tag_device_mcp23017_index_item {
    int button;
    int value;
} device_mcp23017_index_item_t;

typedef struct tag_device_mcp23017_index_table {
    device_mcp23017_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int button_count;
} device_mcp23017_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_mcp23017_data {
    device_mcp23017_config_t         device_cfg;
    device_mcp23017_index_table_t    button_cfg[1];
} device_mcp23017_data_t;


#define INPUT_MCP23017_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (16)

static const device_mcp23017_index_table_t default_input_mcp23017_config = {
    {
        {ABS_Y,     -1},
        {ABS_Y,      1},
        {ABS_X,     -1},
        {ABS_X,      1}, 
        {BTN_START,  1},
        {BTN_SELECT, 1},
        {BTN_A,      1},
        {BTN_B,      1},
        {BTN_X,      1},
        {BTN_Y,      1},
        {BTN_TL,     1},
        {BTN_TR,     1},
        {BTN_MODE,   1},
        {BTN_TL2,    1},
        {BTN_TR2,    1},
        {BTN_Z,      1}
    }, 
    INPUT_MCP23017_DEFAULT_KEYCODE_TABLE_ITEM_COUNT
};



// device_config_str : i2c_addr, io_count
//        bit_order: 0 = assending, 1 = descending
// endpoint_count_str : endpoint1_keycode(default | custom), code_type(code | index), n * {keycode1, value1}
int init_input_device_for_mcp23017(input_device_data_t *device_data, char* device_config_str, char* endpoint_config_str[])
{
    int i, j;
    int code_mode;
    device_mcp23017_index_table_t *src, *des;
    char szText[512];
    char* pText;
    char* temp_p;
    
    device_mcp23017_data_t* user_data = (device_mcp23017_data_t *)kzalloc(sizeof(device_mcp23017_data_t) + sizeof(device_mcp23017_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);

    if (device_config_str != NULL) {
        strcpy(szText, device_config_str); 
        pText = szText;

        temp_p = strsep(&pText, ",");
        user_data->device_cfg.i2c_addr = temp_p != NULL ? simple_strtol(temp_p, NULL, 0) : 0x20;
        temp_p = strsep(&pText, ",");
        if (temp_p == NULL || strcmp(temp_p, "") == 0 || strcmp(temp_p, "default") == 0) {
            user_data->device_cfg.io_count = INPUT_MCP23017_DEFAULT_KEYCODE_TABLE_ITEM_COUNT;
        } else {
            user_data->device_cfg.io_count = simple_strtol(temp_p, NULL, 10);
        }
    }

    des = user_data->button_cfg;

    for (i = 0; i < device_data->target_endpoint_count; i++ ) {
        input_endpoint_data_t *ep = device_data->target_endpoints[i];
        char* cfgtype_p;
        int button_count;

        if (ep == NULL || endpoint_config_str[i] == NULL) continue;

        strcpy(szText, endpoint_config_str[i]); 
        pText = szText;

        cfgtype_p = strsep(&pText, ",");
        if (strcmp(cfgtype_p, "default") == 0){
            src = (device_mcp23017_index_table_t *)&default_input_mcp23017_config;
            code_mode = 0;            

            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                button_count = src->button_count;
            } else {
                button_count = simple_strtol(temp_p, NULL, 10);
            }
        } else if(strcmp(cfgtype_p, "custom") == 0){
            char* keycode_p = strsep(&pText, ";");
            code_mode = simple_strtol(keycode_p, NULL, 10);

            button_count = 0;
            src = &user_data->button_cfg[i];
            while (pText != NULL && button_count < MAX_INPUT_BUTTON_COUNT) {
                char *block_p, *button_p, *value_p;
                int button, value;

                strsep(&pText, "{");
                block_p = strsep(&pText, "}");
                button_p = strsep(&block_p, ",");
                value_p = strsep(&block_p, ",");
                strsep(&pText, ",");

                button = simple_strtol(button_p, NULL, 0);
                value = simple_strtol(value_p, NULL, 0);

                // 키 설정 추가
                src->buttondef[button_count].button = button;
                src->buttondef[button_count].value = value;
                button_count++;
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
        } else if (code_mode == 1) {
            // if (button_count > ep->button_count) button_count = ep->button_count;
            for (j = 0; j < button_count; j++) {
                des->buttondef[j].button = src->buttondef[j].button;
                des->buttondef[j].value = src->buttondef[j].value;
            }
            des->button_count = button_count;
        }

        des++;
    }

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
        device_mcp23017_index_table_t* table = &user_data->button_cfg[i];
        device_mcp23017_index_item_t* btndef = (device_mcp23017_index_item_t *)table->buttondef;

        cnt = table->button_count;
        if (cnt > io_count) cnt = io_count;
        io_count -= cnt;

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
