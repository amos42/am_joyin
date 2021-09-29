#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include "../util/gpio_util.h"
#include "mux.h"

//#define GPIO_GET(i)   GPIO_READ(i)
//#define GPIO_GET_VALUE(i)   getGpio(i)
//#define GPIO_SET_VALUE(i,v)   setGpio((i), (v))

//#define DEFAULT_MUX_BUTTON_COUNT  (16)

// static const device_mux_index_item_t default_mux_button_config[DEFAULT_MUX_BUTTON_COUNT] = {
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

#define MAX_MUX_ADDR_REG_COUNT (6)

typedef struct tag_device_mux_config {
    int rw_gpio;
    int addr_gpio[MAX_MUX_ADDR_REG_COUNT];

    int io_count;
    int start_offset;
    int is_pullup;
} device_mux_config_t;

typedef struct tag_device_mux_index_item {
    int button;
    int value;
} device_mux_index_item_t;

typedef struct tag_device_mux_index_table {
    device_mux_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int button_count;
} device_mux_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_mux_data {
    device_mux_config_t         device_cfg;
    device_mux_index_table_t    button_cfg[1];
} device_mux_data_t;


#define INPUT_MUX_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (16)

static const device_mux_index_table_t default_input_mux_config = {
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
    INPUT_MUX_DEFAULT_KEYCODE_TABLE_ITEM_COUNT
};


// device_config_str :  read_gpio,{addr0_gpio, addr1_gpio, ...},io_count,start_offset
// endpoint_count_str : endpoint1_keycode(default | custom), code_type(code | index), n * {keycode1, value1}
int init_input_device_for_mux(input_device_data_t *device_data, char* device_config_str, char* endpoint_config_str[])
{
    int i, j, idx;
    int code_mode;
    device_mux_index_table_t *src, *des;
    char szText[512];
    char* pText;
    char* temp_p, *block_p;
    
    device_mux_data_t* user_data = (device_mux_data_t *)kzalloc(sizeof(device_mux_data_t) + sizeof(device_mux_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);

    if (device_config_str != NULL) {
        strcpy(szText, device_config_str); 
        pText = szText;

        temp_p = strsep(&pText, ",");
        user_data->device_cfg.rw_gpio = temp_p != NULL ? simple_strtol(temp_p, NULL, 0) : -1;

        strsep(&pText, "{");
        block_p = strsep(&pText, "}");
        for (i = 0; i < MAX_MUX_ADDR_REG_COUNT; i++) {
            temp_p = strsep(&block_p, ",");
            user_data->device_cfg.addr_gpio[i] = temp_p != NULL ? simple_strtol(temp_p, NULL, 0) : -1;
        }
        strsep(&pText, ",");

        temp_p = strsep(&pText, ",");
        if (temp_p == NULL || strcmp(temp_p, "") == 0 || strcmp(temp_p, "default") == 0) {
            user_data->device_cfg.io_count = INPUT_MUX_DEFAULT_KEYCODE_TABLE_ITEM_COUNT;
        } else {
            user_data->device_cfg.io_count = simple_strtol(temp_p, NULL, 10);
        }
        temp_p = strsep(&pText, ",");
        user_data->device_cfg.start_offset = temp_p != NULL ? simple_strtol(temp_p, NULL, 10) : 0;
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
            src = (device_mux_index_table_t *)&default_input_mux_config;
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

            idx = 0;
            src = &user_data->button_cfg[i];
            while (pText != NULL && idx < MAX_INPUT_BUTTON_COUNT) {
                char *ptr, *button_p, *value_p;
                int button, value;

                ptr = strsep(&pText, "}");
                strsep(&pText, ",");

                strsep(&ptr, "{");
                button_p = strsep(&ptr, ",");
                value_p = strsep(&ptr, ",");

                button = simple_strtol(button_p, NULL, 0);
                value = simple_strtol(value_p, NULL, 0);

                // 키 설정 추가
                src->buttondef[idx].button = button;
                src->buttondef[idx].value = value;
                idx++;
            }

            button_count = idx;
        } else {
            continue;
        }

        if (code_mode == 0) {
            if (button_count > ep->button_count) button_count = ep->button_count;
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
        }

        des++;
    }

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

    addr = user_data->device_cfg.start_offset;

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_mux_index_table_t* table = &user_data->button_cfg[i];
        device_mux_index_item_t* btndef = (device_mux_index_item_t *)table->buttondef;

        cnt = table->button_count;
        if (cnt > io_count) cnt = io_count;
        io_count -= cnt;

        for (j = 0; j < cnt; j++ ){
            int addr_reg = addr;
            int *addr_gpio = user_data->device_cfg.addr_gpio;
            for (k = 0; k < MAX_MUX_ADDR_REG_COUNT; k++ ){
                int gpio = *addr_gpio++;
                if (gpio == -1) break;
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
