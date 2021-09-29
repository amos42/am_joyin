#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include "../util/gpio_util.h"
#include "gpio_rpi2.h"

//#define GPIO_GET(i)   GPIO_READ(i)
//#define GPIO_GET_VALUE(i)   gpio_get_value(i)

#define INPUT_GPIO_CONFIG_TYPE_INDEX    (0)
#define INPUT_GPIO_CONFIG_TYPE_KEYCODE  (1)

typedef struct tag_device_gpio_index_item {
    int gpio;
    int button;
    int value;
} device_gpio_index_item_t;

typedef struct tag_device_gpio_index_table {
    device_gpio_index_item_t buttondef[MAX_INPUT_BUTTON_COUNT];
    int button_count;
} device_gpio_index_table_t;

typedef struct tag_device_gpio_data {
    int is_pullup;
    device_gpio_index_table_t items[1];
} device_gpio_data_t;


#define INPUT_GPIO_DEFAULT_KEYCODE_TABLE_ITEM_COUNT  (13)

static const device_gpio_index_table_t default_input1_gpio_config = {
    {
        {4,  ABS_Y,     -1},
        {17, ABS_Y,      1},
        {27, ABS_X,     -1},
        {22, ABS_X,      1},
        {10, BTN_START,  1},
        {9,  BTN_SELECT, 1},
        {25, BTN_A,      1},
        {24, BTN_B,      1},
        {15, BTN_X,      1},
        {18, BTN_Y,      1},
        {14, BTN_TL,     1},
        {23, BTN_TR,     1},
        {2,  BTN_MODE,   1},
    }, 
    INPUT_GPIO_DEFAULT_KEYCODE_TABLE_ITEM_COUNT
};

static const device_gpio_index_table_t default_input2_gpio_config = {
    {
        {11, ABS_Y,     -1},
        {5,  ABS_Y,      1},
        {6,  ABS_X,     -1},
        {13, ABS_X,      1},
        {19, BTN_START,  1},
        {26, BTN_SELECT, 1},
        {21, BTN_A,      1},
        {20, BTN_B,      1},
        {16, BTN_X,      1},
        {12, BTN_Y,      1},
        {7,  BTN_TL,     1},
        {8,  BTN_TR,     1},
        {3,  BTN_MODE,   1},
    }, 
    INPUT_GPIO_DEFAULT_KEYCODE_TABLE_ITEM_COUNT
};


// device_config_str : endpoint,config_type(default1 | default2 | custom), ...
//        default1 = default #1. add_params : default | key_cnt
//        default2 = default #2. add_params : default | key_cnt
// ex) device1=gpio;0,default1
//     device3=gpio;1,custom,code,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}
int init_input_device_for_gpio(input_device_data_t *device_data, char* device_config_str, char* endpoint_config_str[])
{
     int i;
    device_gpio_index_table_t *src, *des;
    int code_mode;
    char szText[512];
    char* pText;
    char* temp_p;

    device_gpio_data_t* user_data = (device_gpio_data_t *)kzalloc(sizeof(device_gpio_data_t) + sizeof(device_gpio_index_table_t) * (device_data->target_endpoint_count - 1), GFP_KERNEL);
    des = user_data->items;

    for (i = 0; i < device_data->target_endpoint_count; i++ ) {
        input_endpoint_data_t *ep = device_data->target_endpoints[i];
        char* cfgtype_p;
        int button_count;

        if (ep == NULL || endpoint_config_str[i] == NULL) continue;

        strcpy(szText, endpoint_config_str[i]); 
        pText = szText;

        cfgtype_p = strsep(&pText, ",");
        if (cfgtype_p == NULL || strcmp(cfgtype_p, "default1") == 0 || strcmp(cfgtype_p, "default") == 0 || strcmp(cfgtype_p, "") == 0){
            src = (device_gpio_index_table_t *)&default_input1_gpio_config;
            code_mode = 0;

            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                button_count = src->button_count;
            } else {
                button_count = simple_strtol(temp_p, NULL, 10);
            }
        } else if(strcmp(cfgtype_p, "default2") == 0){
            src = (device_gpio_index_table_t *)&default_input2_gpio_config;
            code_mode = 0;

            temp_p = strsep(&pText, ",");
            if (temp_p == NULL || strcmp(temp_p, "default") == 0 || strcmp(temp_p, "") == 0) {
                button_count = src->button_count;
            } else {
                button_count = simple_strtol(temp_p, NULL, 10);
            }
        } else if(strcmp(cfgtype_p, "custom") == 0){
            char* keycode_p = strsep(&pText, ",");
            code_mode = simple_strtol(keycode_p, NULL, 10);

            button_count = 0;
            src = &user_data->items[i];
            while (pText != NULL && button_count < MAX_INPUT_BUTTON_COUNT) {
                char *block_p, *gpio_p, *button_p, *value_p;
                int gpio, button, value;

                strsep(&pText, "{");
                block_p = strsep(&pText, "}");
                gpio_p = strsep(&block_p, ",");
                button_p = strsep(&block_p, ",");
                value_p = strsep(&block_p, ",");
                strsep(&pText, ",");

                gpio = simple_strtol(gpio_p, NULL, 0);
                button = simple_strtol(button_p, NULL, 0);
                value = simple_strtol(value_p, NULL, 0);

                // 키 설정 추가
                src->buttondef[button_count].gpio = gpio;
                src->buttondef[button_count].button = button;
                src->buttondef[button_count].value = value;
                button_count++;
            }
        } else {
            continue;
        }

        if (code_mode == 0) {
            int j, k;
            // if (button_count > ep->button_count) button_count = ep->button_count;
            for (j = 0; j < button_count; j++) {
                int code = src->buttondef[j].button;
                for (k = 0; k < ep->target_buttonset->button_count; k++) {
                    if (code == ep->target_buttonset->button_data[k].button_code) {
                        des->buttondef[j].button = k;
                        des->buttondef[j].gpio = src->buttondef[j].gpio;
                        des->buttondef[j].value = src->buttondef[j].value;
                        break;
                    }
                }
            }
            des->button_count = button_count;
        } else if (code_mode == 1) {
            int j;
            // if (button_count > ep->button_count) button_count = ep->button_count;
            for (j = 0; j < button_count; j++) {
                des->buttondef[j].button = src->buttondef[j].button;
                des->buttondef[j].gpio = src->buttondef[j].gpio;
                des->buttondef[j].value = src->buttondef[j].value;
            }
            des->button_count = button_count;
        }

        des++;
    }

    device_data->data = (void *)user_data;

    return 0;
}


static void start_input_device_for_gpio(input_device_data_t *device_data)
{
    int i, j;
    int pull_ups[32], pull_idx;
    device_gpio_index_table_t* table;
    device_gpio_data_t* user_data = (device_gpio_data_t *)device_data->data;

    if (user_data == NULL) return;
    table = user_data->items;

    pull_idx = 0;
    for (i = 0; i < device_data->target_endpoint_count; i++) {
        device_gpio_index_item_t* item = (device_gpio_index_item_t*)table->buttondef;

        for (j = 0; j < table->button_count; j++) {
            if (item->gpio != -1) {
                gpio_as_input(item->gpio);
                pull_ups[pull_idx++] = item->gpio;
            }
            item++;
        }
        table++;
    }

    gpio_pullups(pull_ups, pull_idx);

    device_data->is_opend = TRUE;
}


static void check_input_device_for_gpio(input_device_data_t* device_data)
{
    int i, j;
    device_gpio_index_table_t* table;
    device_gpio_data_t* user_data = (device_gpio_data_t *)device_data->data;
    
    if (user_data == NULL) return;
    table = user_data->items;
    
    for (i = 0; i < device_data->target_endpoint_count; i++) {
        input_endpoint_data_t* endpoint = device_data->target_endpoints[i];
        device_gpio_index_item_t* item = (device_gpio_index_item_t*)table->buttondef;

        for (j = 0; j < table->button_count; j++) {
            if (item->gpio != -1) {    // to avoid unused buttons
                if (gpio_get_value(item->gpio) == 0) {
                    endpoint->report_button_state[item->button] = item->value;
                }
            }
            item++;
        }
        table++;
    }
}


static void stop_input_device_for_gpio(input_device_data_t *device_data)
{
    device_data->is_opend = FALSE;
}


int register_input_device_for_gpio(input_device_type_desc_t *device_desc)
{
    strcpy(device_desc->device_type, "gpio");
    device_desc->init_input_dev = init_input_device_for_gpio;
    device_desc->start_input_dev = start_input_device_for_gpio;
    device_desc->check_input_dev = check_input_device_for_gpio;
    device_desc->stop_input_dev = stop_input_device_for_gpio;
    return 0;    
}
