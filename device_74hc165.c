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

static const int default_74hc165_gpio_maps[3] = {16, 20, 21};

//#define DEFAULT_74HC165_BUTTON_COUNT  (16)

// static const device_74hc165_index_item_t default_74hc165_button_config[DEFAULT_74HC165_BUTTON_COUNT] = {
//     {ABS_Y,      -1},
//     {ABS_Y,       1},
//     {ABS_X,      -1},
//     {ABS_X,       1}, 
//     {BTN_START,   1},
//     {BTN_SELECT,  1},
//     {BTN_A,       1},
//     {BTN_B,       1},
//     {BTN_X,       1},
//     {BTN_Y,       1},
//     {BTN_TL,      1},
//     {BTN_TR,      1},
//     {BTN_MODE,    1},
//     {BTN_TL2,     1},
//     {BTN_TR2,     1},
//     {BTN_TRIGGER, 1}
// };

typedef struct tag_device_74hc165_config {
    int gpio_ck;
    int gpio_ld;
    int gpio_dt;
    int io_count;
    int bit_order;
    int is_pullup;
} device_74hc165_config_t;

typedef struct tag_device_74hc165_index_item {
    int button;
    int value;
} device_74hc165_index_item_t;

typedef struct tag_device_74hc165_index_table {
    device_74hc165_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int button_start_index;
    int button_count;
} device_74hc165_index_table_t;

// device.data에 할당 될 구조체
typedef struct tag_device_74hc165_data {
    device_74hc165_config_t         device_cfg;
    device_74hc165_index_table_t    button_cfg[1];
} device_74hc165_data_t;


#define INPUT_74HC165_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (16)

static const device_74hc165_index_table_t default_input_74hc165_config = {
    {
        {ABS_Y,      -1},
        {ABS_Y,       1},
        {ABS_X,      -1},
        {ABS_X,       1}, 
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
    INPUT_74HC165_DEFAULT_KEYCODE_TABLE_ITEM_COUNT
};



// device_config_str : ck, ld, rd, io_count, bit_order (0 | 1)
//        bit_order: 0 = assending, 1 = descending
// endpoint_count_str : endpoint1_keycode(default | custom), code_type(code | index), n * {keycode1, value1}
int init_input_device_for_74hc165(input_device_data_t *device_data, char* device_config_str, char* endpoint_config_str[])
{
    int i, j;
    int code_mode;
    device_74hc165_index_table_t *src, *des;
    char szText[512];
    char* pText;
    char* temp_p;
    
    device_74hc165_data_t* user_data = (device_74hc165_data_t *)kzalloc(sizeof(device_74hc165_data_t) + sizeof(device_74hc165_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);

    if (device_config_str != NULL) {
        strcpy(szText, device_config_str); 
        pText = szText;

        temp_p = strsep(&pText, ",");
        user_data->device_cfg.gpio_ck = simple_strtol(temp_p, NULL, 0);
        temp_p = strsep(&pText, ",");
        user_data->device_cfg.gpio_ld = simple_strtol(temp_p, NULL, 0);
        temp_p = strsep(&pText, ",");
        user_data->device_cfg.gpio_dt = simple_strtol(temp_p, NULL, 0);
        temp_p = strsep(&pText, ",");
        if (temp_p == NULL || strcmp(temp_p, "") == 0 || strcmp(temp_p, "default") == 0) {
            user_data->device_cfg.io_count = INPUT_74HC165_DEFAULT_KEYCODE_TABLE_ITEM_COUNT;
        } else {
            user_data->device_cfg.io_count = simple_strtol(temp_p, NULL, 10);
        }
        temp_p = strsep(&pText, ",");
        if (temp_p == NULL || strcmp(temp_p, "") == 0) {
            user_data->device_cfg.bit_order = 0;
        } else {
            user_data->device_cfg.bit_order = simple_strtol(temp_p, NULL, 0);
        }
    }

    des = user_data->button_cfg;

    for (i = 0; i < device_data->target_endpoint_count; i++ ) {
        input_endpoint_data_t *ep = device_data->target_endpoints[i];
        char* cfgtype_p;
        int button_start_index, button_count;

        if (ep == NULL || endpoint_config_str[i] == NULL) continue;

        strcpy(szText, endpoint_config_str[i]); 
        pText = szText;

        cfgtype_p = strsep(&pText, ",");
        if (strcmp(cfgtype_p, "default") == 0){
            src = (device_74hc165_index_table_t *)&default_input_74hc165_config;
            code_mode = 0;            

            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                button_start_index = 0;
            } else {
                button_start_index = simple_strtol(temp_p, NULL, 10);
            }
            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                button_count = src->button_count;
            } else {
                button_count = simple_strtol(temp_p, NULL, 10);
            }
        } else if(strcmp(cfgtype_p, "custom") == 0){
            char* keycode_p = strsep(&pText, ";");
            code_mode = simple_strtol(keycode_p, NULL, 10);

            button_start_index = 0;
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
            des->button_start_index = button_start_index;
            des->button_count = button_count;
        } else if (code_mode == 1) {
            // if (button_count > ep->button_count) button_count = ep->button_count;
            for (j = 0; j < button_count; j++) {
                des->buttondef[j].button = src->buttondef[j].button;
                des->buttondef[j].value = src->buttondef[j].value;
            }
            des->button_start_index = button_start_index;
            des->button_count = button_count;
        }

        des++;
    }

    device_data->data = (void *)user_data;

    return 0;
}


static void start_input_device_for_74hc165(input_device_data_t *device_data)
{
    device_74hc165_data_t *user_data = (device_74hc165_data_t *)device_data->data;

    gpio_as_output(user_data->device_cfg.gpio_ck);
    gpio_as_output(user_data->device_cfg.gpio_ld);
    gpio_as_input(user_data->device_cfg.gpio_dt);

    device_data->is_opend = TRUE;
}


static void check_input_device_for_74hc165(input_device_data_t *device_data)
{
    int i, j, cnt;
    int ld, ck, dt, io_count;
    device_74hc165_data_t *user_data = (device_74hc165_data_t *)device_data->data;

    if (user_data == NULL) return;

    ck = user_data->device_cfg.gpio_ck;
    ld = user_data->device_cfg.gpio_ld;
    dt = user_data->device_cfg.gpio_dt;
    io_count = user_data->device_cfg.io_count;

    if (io_count <= 0) return;

    gpio_set_value(ld, 0);
    udelay(5);
    gpio_set_value(ld, 1);

    if (user_data->device_cfg.bit_order == 0) {
        for (i = 0; i < device_data->target_endpoint_count; i++) {
            input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
            device_74hc165_index_table_t* table = &user_data->button_cfg[i];
            device_74hc165_index_item_t* btndef = &table->buttondef[table->button_start_index];

            cnt = table->button_count;
            if (cnt > io_count) cnt = io_count;
            io_count -= cnt;

            for (j = 0; j < cnt; j++ ){
                if (gpio_get_value(dt) == 0) {
                    endpoint->report_button_state[btndef->button] = btndef->value;
                }

                gpio_set_value(ck, 1);
                udelay(5);
                gpio_set_value(ck, 0);

                btndef++;
            }

            if (io_count <= 0) break;
        }
    } else if (user_data->device_cfg.bit_order == 1) {
        int endpoint_idx = 0;
        input_endpoint_data_t* endpoint = device_data->target_endpoints[endpoint_idx];
        device_74hc165_index_table_t* table = &user_data->button_cfg[endpoint_idx];
        device_74hc165_index_item_t* btndef = (device_74hc165_index_item_t *)table->buttondef;

        int chips = io_count / 8 + ((io_count % 8)? 1 : 0);

        int idx = 0;
        for (i = 0; i < chips; i++ ){
            int data[8];

            for (j = 0; j < 8; j++ ){
                data[7 - j] = gpio_get_value(dt);

                gpio_set_value(ck, 1);
                udelay(5);
                gpio_set_value(ck, 0);
            }

            cnt = (io_count > 8)? 8 : io_count;
            io_count -= cnt;

            for (j = 0; j < cnt; j++ ){
                if (data[j] == 0) {
                    endpoint->report_button_state[btndef->button] = btndef->value;
                }                
                btndef++;
                if (++idx >= table->button_count) {
                    endpoint_idx++;
                    endpoint = device_data->target_endpoints[endpoint_idx];
                    table = &user_data->button_cfg[endpoint_idx];
                    btndef = (device_74hc165_index_item_t *)table->buttondef;
                    idx = 0; 
                }
            }
            
            if (io_count <= 0) break;
        }
    }
}


static void stop_input_device_for_74hc165(input_device_data_t *device_data)
{
    device_data->is_opend = FALSE;
}


int register_input_device_for_74hc165(input_device_type_desc_t *device_desc)
{
    strcpy(device_desc->device_type, "74hc165");
    device_desc->init_input_dev = init_input_device_for_74hc165;
    device_desc->start_input_dev = start_input_device_for_74hc165;
    device_desc->check_input_dev = check_input_device_for_74hc165;
    device_desc->stop_input_dev = stop_input_device_for_74hc165;
    return 0;
}
