#include <linux/kernel.h>
#include <linux/module.h>


#include "indev_type.h"
#include "am_joyin.h"

/*
#include <stdlib.h>
#define simple_strtol(a,b,c)  strtol(a,b,c)

#include <string.h>
char *strsep(char **stringp, const char *delim)
{
    char *ptr = *stringp;
    if (ptr == NULL)
    {
        return NULL;
    }
    while (**stringp)
    {
        if (strchr(delim, **stringp) != NULL)
        {
            **stringp = 0x00;
            (*stringp)++;
            return ptr;
        }
        (*stringp)++;
    }
    *stringp = NULL;
    return ptr;
}
*/


#define MAX_INPUT_INPUT_DESC_COUNT (2)
#define MAX_INPUT_DEVICES_COUNT (4)


#define AM_PARAM_ARRAY_VARIABLE(N, I) static char *N[I] /*__initdata*/;
#define AM_PARAM_DEFINE(N, V)           \
    module_param_named(N, V, charp, 0); \
    MODULE_PARM_DESC(N, "Joystick device " #N " button config parameters");

#define AM_PARAM_DEFINE_WITH_VARIABLE(N, V) \
    static char *V /*__initdata*/;          \
    module_param_named(N, V, charp, 0);     \
    MODULE_PARM_DESC(N, "Joystick device " #N " button config parameters");

AM_PARAM_ARRAY_VARIABLE(am_buttonset_cfg, MAX_INPUT_INPUT_DESC_COUNT)
AM_PARAM_DEFINE(buttonset1, am_buttonset_cfg[0])
AM_PARAM_DEFINE(buttonset2, am_buttonset_cfg[1])

AM_PARAM_DEFINE_WITH_VARIABLE(endpoints, am_endpoints_cfg)

AM_PARAM_ARRAY_VARIABLE(am_device_cfg, MAX_INPUT_DEVICES_COUNT)
AM_PARAM_DEFINE(device1, am_device_cfg[0])
AM_PARAM_DEFINE(device2, am_device_cfg[1])
AM_PARAM_DEFINE(device3, am_device_cfg[2])
AM_PARAM_DEFINE(device4, am_device_cfg[3])


void __init prepocess_params(void)
{
    int i, cnt;

    // endpoint 생략시, 기본 파라미터 세팅
    if (am_endpoints_cfg == NULL)
    {
        am_endpoints_cfg = "default,default";
    }

    // device 모두 생략시, 기본 파라미터 세팅
    cnt = 0;    
    for (i = 0; i < MAX_INPUT_DEVICES_COUNT; i++)
    {
        if (am_device_cfg[i] != NULL)
            cnt++;
    }
    if (cnt <= 0)
    {
        am_device_cfg[0] = "gpio;;0,default1";
    }
}


void parsing_device_config_params(am_joyin_data_t* a_input)
{
    int i, j;
    char szText[512];
    char* pText;
    int idx;

    // 기본 키 설정 초기화
    input_buttonset_data_t* target_buttonset = &a_input->buttonset_list[a_input->input_buttonset_count++];
    for (i = 0; i < DEFAULT_INPUT_BUTTON_COUNT; i++) {
        target_buttonset->button_data[i] = default_buttonset[i];
    }
    target_buttonset->button_count = DEFAULT_INPUT_BUTTON_COUNT;

    // key 설정
    for (i = 0; i < MAX_INPUT_INPUT_DESC_COUNT; i++) {
        input_buttonset_data_t* target_buttonset;

        if (am_buttonset_cfg[i] == NULL) continue;

        target_buttonset = &a_input->buttonset_list[a_input->input_buttonset_count++];

        strcpy(szText, am_buttonset_cfg[i]);
        pText = szText;

        idx = 0;
        while (pText != NULL && idx < MAX_INPUT_BUTTON_COUNT) {
            char *block_p, *keycode_p, *minvalue_p, *maxvalue_p;
            int key_code, min_value, max_value;

            strsep(&pText, "{");
            block_p = strsep(&pText, "}");
            keycode_p = strsep(&block_p, ",");
            minvalue_p = strsep(&block_p, ",");
            maxvalue_p = strsep(&block_p, ",");
            strsep(&pText, ",");

            key_code = simple_strtol(keycode_p, NULL, 0);
            min_value = simple_strtol(minvalue_p, NULL, 0);
            max_value = simple_strtol(maxvalue_p, NULL, 0);

            // 키 설정 추가
            target_buttonset->button_data[idx].button_code = key_code;
            target_buttonset->button_data[idx].min_value = min_value;
            target_buttonset->button_data[idx].max_value = max_value;
            idx++;
        }
        target_buttonset->button_count = idx;
    }

    strcpy(szText, am_endpoints_cfg);
    pText = szText;

    // endpoint 설정
    idx = 0;
    while (pText != NULL && idx < MAX_INPUT_ENDPOINT_COUNT) {
        input_buttonset_data_t* btncfg_item;
        char *ptr, *keycfg_p, *buttoncnt_p;
        int keycfg_idx, button_cnt;

        ptr = strsep(&pText, ";");

        keycfg_p = strsep(&ptr, ",");
        buttoncnt_p = strsep(&ptr, ",");

        if (strcmp(keycfg_p, "default") == 0) {
            keycfg_idx = 0;
        }
        else {
            keycfg_idx = simple_strtol(keycfg_p, NULL, 10);
        }

        btncfg_item = &a_input->buttonset_list[keycfg_idx];

        if (buttoncnt_p == NULL || strcmp(buttoncnt_p, "default") == 0 || strcmp(buttoncnt_p, "") == 0) {
            button_cnt = 0;
        }
        else {
            button_cnt = simple_strtol(buttoncnt_p, NULL, 10);
        }
        if (button_cnt <= 0) {
            button_cnt = btncfg_item->button_count;
        }

        // endpoint 추가
        a_input->endpoint_list[idx].endpoint_id = idx;
        a_input->endpoint_list[idx].target_buttonset = btncfg_item;
        a_input->endpoint_list[idx].button_count = button_cnt;

        idx++;
    }
    a_input->input_endpoint_count = idx;

    // device들 설정
    for (i = 0; i < MAX_INPUT_DEVICE_COUNT; i++) {
        input_device_type_desc_t* type_desc;
        input_device_data_t* device;
        char *typeid_p, *device_params_p;
        char* endpoint_params[MAX_INPUT_ENDPOINT_COUNT];

        if (am_device_cfg[i] == NULL) continue;

        strcpy(szText, am_device_cfg[i]);
        pText = szText;

        typeid_p = strsep(&pText, ";");
        // if (typeid_p == NULL || (strcmp(typeid_p, "") == 0)) {
        //     typeid_p = (char *)"gpio";
        // }

        type_desc = NULL;
        for (j = 0; j < a_input->input_device_type_desc_count; j++) {
            input_device_type_desc_t* desc = &a_input->device_type_desc_list[j];
            if (strcmp(desc->device_type, typeid_p) == 0) {
                type_desc = desc;
                break;
            }
        }
        if (type_desc == NULL) continue;

        device = &a_input->device_list[a_input->input_device_count++];

        device->device_type_desc = type_desc;

        device_params_p = strsep(&pText, ";");

        while(pText != NULL){
            char* ptr = strsep(&pText, ";");

            char* endpoint_p = strsep(&ptr, ",");
            char* endpoint_cfg_p = ptr; // strsep(&ptr, ",");

            int endpoint_idx;
            if (strcmp(endpoint_p, "") == 0) {
                endpoint_idx = 0;
            }
            else {
                endpoint_idx = simple_strtol(endpoint_p, NULL, 10);
            }

            if (endpoint_idx < a_input->input_endpoint_count) {
                device->target_endpoints[device->target_endpoint_count] = &a_input->endpoint_list[endpoint_idx];
                endpoint_params[device->target_endpoint_count] = endpoint_cfg_p;
                device->target_endpoint_count++;
            }
        }

        if (type_desc->init_input_dev != NULL) {
            type_desc->init_input_dev(device, device_params_p, endpoint_params);
        }
    }
}

/*

keyset1_cfg={0x102,-1,1},{0x103,0,1},{0x102,-1,1},{0x103,0,1},{0x102,0,1},{0x103,0,1},{0x102,0,1},{0x103,0,1},{0x102,0,1},{0x103,0,1}

// endpoint 생략시 1개에 0번 키설정에 키 default (13) 개
endpoints=default,0;0,12;1,default // endpoint1_keyset_cfg=default, endpoint1_key_count=default(13), endpoint2_keyset_cfg=default, endpoint2_key_count=12, endpoint3_key_cfg=keyset1, endpoint3_key_count=default

// device_type; endpoint_id, ...; config1=default1
device1=gpio;;0,default1;1,default2

// type=gpio, endpoint1_id=1
device2=gpio;;1,custom,code,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1};2,custom,index,{10,0,-1},{10,0,1},{10,1,1},{10,2,1},{10,3,1},{10,4,1}

// type=gpio, endpoint1_id=1, endpoint2_id=2
device3=gpio;1,custom,code,{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1},{10,0x103,1}

// type=gpio, endpoint1_id=1, endpoint2_id=2
device4=gpio;2,custom,index,{10,0,-1},{10,0,1},{10,2,1},{10,3,1},{10,4,1},{10,5,1}

// type=74hc165, endpoint1_id=1
device2=74hc165;1,2;16,2,13,1;default,default

// type=74hc165, endpoint1_id=1, endpoint2_id=2
device2=74hc165;21,19,18,24,1;0,default,12;1,default,12

*/

