/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __KERNEL__
#define __KERNEL__
#endif

#ifndef MODULE
#define MODULE
#endif

#define VENDOR_ID       (0xA042)
#define PRODUCT_ID      (0x0001)
#define PRODUCT_VERSION (0x0100)
#define DEVICE_NAME     "Amos Joystick"


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/slab.h>


MODULE_AUTHOR("Amos42");
MODULE_DESCRIPTION("GPIO and Multiplexer and 74HC165 amd MCP23017 Arcade Joystick Driver");
MODULE_LICENSE("GPL");

/*============================================================*/

#include "am_joyin.h"

/** parameters (begin) **/
#include "am_joyin_cfg.c"
/** parameters (end) **/

#include "bcm_gpio.c"

//#include "gpio_util.h"
#include "gpio_util.c"
//#include "i2c_util.h"
#include "i2c_util.c"
//#include "parse_util.h"
#include "parse_util.c"

//#include "indev_type.h"
#include "indev_type.c"

#include "device_gpio_rpi2.c"
#include "device_74hc165.c"
#include "device_mcp23017.c"
#include "device_mux.c"


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
#define HAVE_TIMER_SETUP
#endif

#define DEFAULT_REFRESH_TIME	(HZ/100)


static am_joyin_data_t *a_input;


static void initButtonConfig(struct input_dev *indev, input_buttonset_data_t *buttonset_cfg, int button_count)
{
    int i;
   
    if (button_count > buttonset_cfg->button_count) {
        button_count = buttonset_cfg->button_count;
    }

    for (i = 0; i < button_count; i++) {
        input_button_data_t *btn = &buttonset_cfg->button_data[i];
        unsigned int code = btn->button_code;
        if (code < ABS_MAX) {
            input_set_abs_params(indev, code, btn->min_value, btn->max_value, 0, 0);
        } else {
            __set_bit(code, indev->keybit);
        }
    }
}


static void reportInput(struct input_dev *indev, input_button_data_t *button_data, int button_count, int *data, int *cur_data)
{
    int i;
    BOOL is_changed = FALSE;

    for (i = 0; i < button_count; i++) {
        if (data[i] != cur_data[i]){
            unsigned int code = button_data[i].button_code;
            if (code < ABS_MAX) {
                input_report_abs(indev, code, data[i]);
            } else {
                input_report_key(indev, code, data[i]);
            }
            cur_data[i] = data[i];
            is_changed = TRUE;
        }
    }

    if (is_changed) {
        input_sync(indev);
    }
}


#ifdef HAVE_TIMER_SETUP
static void am_timer(struct timer_list *t) {
    am_joyin_data_t *inp = from_timer(inp, t, timer);
#else
static void am_timer(unsigned long private) {
    am_joyin_data_t *inp = (void *) private;
#endif
    int i, j;
    unsigned long next_timer = jiffies + inp->timer_period; // 다음 타이머 주기를 미리 구해 놓는다.

    for (i = 0; i < inp->input_endpoint_count; i++) {
        input_endpoint_data_t *ep = &inp->endpoint_list[i];
        for (j = 0; j < ep->button_count; j++) ep->report_button_state[j] = 0;
    }

    for (i = 0; i < inp->input_device_count; i++) {
        input_device_data_t *dev = &inp->device_list[i];
        if (dev != NULL && dev->is_opend && dev->device_type_desc != NULL) {
            dev->device_type_desc->check_input_dev(dev);
        }
    }

    for (i = 0; i < inp->input_endpoint_count; i++) {
        input_endpoint_data_t *ep = &inp->endpoint_list[i];
        reportInput(ep->indev, ep->target_buttonset->button_data, ep->button_count, ep->report_button_state, ep->current_button_state);
    }

    // 만약 키체크 시간이 너무 길어서 다음 타이머 주기를 초과해 버리면 예외 처리
    if (next_timer <= jiffies) {
        next_timer = jiffies + inp->timer_period;
        
        // 타이머 주기를 초과하는 횟수가 100회를 넘으면 타이머 중단
        if (++inp->missing_timer_count > 100) {
            printk(KERN_ERR"Button check time is too late. Terminate the timer.");
            return;
        }
    }

    // 다음 타이머 트리거
    mod_timer(&inp->timer, next_timer);
}


static void initTimer(void)
{
    if (a_input != NULL) {
#ifdef HAVE_TIMER_SETUP
        timer_setup(&a_input->timer, am_timer, 0);
#else
        setup_timer(&a_input->timer, am_timer, (long)a_input);
#endif
    }
}


static void startTimer(void)
{
    if (a_input != NULL) {
        if (a_input->input_endpoint_count > 0 && a_input->input_device_count > 0){
            mod_timer(&a_input->timer, jiffies + a_input->timer_period);
            //printk("start dev input timer");
        }
    }
}


static void stopTimer(void)
{
    if (a_input != NULL) {
        del_timer_sync(&a_input->timer);
        //printk("stop dev input timer");
    }
}


static int __open_handler(struct input_dev *indev) 
{
    input_endpoint_data_t *endpoint = input_get_drvdata(indev);
    int err;

    //printk("amos joystick input endpoint %d open...", a_input->used);

    err = mutex_lock_interruptible(&a_input->mutex);
    if (err)
        return err;

    if (!endpoint->is_opened) {
        if (!a_input->used++) {
            startTimer();
        }

        endpoint->is_opened = TRUE;
        //printk("amos joystick input endpoint opend");
    }

    mutex_unlock(&a_input->mutex);

    return 0;
}


static void __close_handler(struct input_dev *indev) 
{
    input_endpoint_data_t *endpoint = input_get_drvdata(indev);

    //printk("amos joystick input endpoint %d close...", a_input->used);

    mutex_lock(&a_input->mutex);

    if (endpoint->is_opened) {
        if (!--a_input->used) {
            stopTimer();
        }

        endpoint->is_opened = FALSE;
        //printk("amos joystick input endpoint closed");
    }

    mutex_unlock(&a_input->mutex);
}


static int __endpoint_register(input_endpoint_data_t *endpoint)
{
    int err = 0;

    if (endpoint->indev == NULL) {
        struct input_dev *indev = input_allocate_device();
        indev->name = endpoint->endpoint_name;
        indev->phys = "joystick";
        indev->id.bustype = BUS_PARPORT;
        indev->id.vendor = VENDOR_ID;
        indev->id.product = PRODUCT_ID;
        indev->id.version = PRODUCT_VERSION;
        input_set_drvdata(indev, endpoint);
        indev->open = __open_handler;
        indev->close = __close_handler;
        indev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

        initButtonConfig(indev, endpoint->target_buttonset, endpoint->button_count);

        err = input_register_device(indev);
        if (err >= 0){
            endpoint->indev = indev;
        } else {
            input_free_device(indev);
            endpoint->indev = NULL;
        }
    }

    return err;
}

static int __endpoint_unregister(input_endpoint_data_t *endpoint, BOOL force)
{
    int err = 0;

    if (endpoint->indev != NULL) {
        input_unregister_device(endpoint->indev);
        input_free_device(endpoint->indev);
        endpoint->indev = NULL;
    }

    return err;
}

static int __input_dev_register(input_device_data_t *device_data) 
{
    int i;
    int err;

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        err = __endpoint_register(device_data->target_endpoints[i]);
        if (err < 0) break;
    }

    return err;
}


static void __input_dev_unregister(input_device_data_t *device_data) 
{
    int i;

    for (i = 0; i < device_data->target_endpoint_count; i++) {
        __endpoint_unregister(device_data->target_endpoints[i], FALSE);
    }

    if (device_data->data != NULL) {
        kfree(device_data->data);
        device_data->data = NULL;
    }    
}


static void cleanup(void)
{
    if (a_input != NULL) {
        int i;

        stopTimer();

        // 장치들을 제거한다.
        for (i = 0; i < a_input->input_device_count; i++) {
            input_device_data_t *dev = &a_input->device_list[i];
            if (dev != NULL && dev->is_opend) {
                dev->device_type_desc->stop_input_dev(dev);
                __input_dev_unregister(dev);
                a_input->device_list[i].is_opend = FALSE;
            }
        }

        // 혹시 덜 닫힌 endpoint가 있으면 강제로 닫는다.
        for (i = 0; i < a_input->input_endpoint_count; i++) {
            __endpoint_unregister(&a_input->endpoint_list[i], TRUE);
        }

        kfree(a_input);
        a_input = NULL;
    }

    gpio_clean();
}



static int am_joyin_init(void)
{
    int i;
    int err;

    //printk("init input dev.....");

    // 커맨드 라인 파라미터 전처리
    prepocess_params();

    err = gpio_init(bcm_peri_base_probe());
    if (err < 0) {
        goto err_free_dev;
    }

    a_input = (am_joyin_data_t *)kzalloc(sizeof(am_joyin_data_t), GFP_KERNEL);

    initTimer();

    // 지원할 장치 목록들을 등록한다.
    register_input_device_for_gpio(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);
    register_input_device_for_74hc165(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);
    register_input_device_for_mcp23017(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);
    register_input_device_for_mux(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);

    // 커맨드 라인 파라미터들을 분석한다.
    parsing_device_config_params(a_input);

    // 타이머 주기 설정. 비어 있으면 기본 타이머 주기
    if (a_input->timer_period > 0) {
        a_input->timer_period = HZ / a_input->timer_period;
    } else {
        a_input->timer_period = DEFAULT_REFRESH_TIME;
    }

    //printk("start register input dev...");

    // 설정으로 지정한 모든 장치를 초기화 한다.
    for (i = 0; i < a_input->input_device_count; i++) {
        input_device_data_t *dev = &a_input->device_list[i];

        printk(KERN_INFO"register input dev #%d : %s", i, dev->device_type_desc->device_type);

        dev->device_type_desc->start_input_dev(dev);

        err = __input_dev_register(dev);
        if (err < 0) {
            goto err_free_dev;
        }
    }

    //printk("end register input dev.");

    // 한개라도 endpoint가 등록되어 있다면 정상 종료
    for (i = 0; i < a_input->input_endpoint_count; i++) {
        if (a_input->endpoint_list[i].indev != NULL) {
            return 0;
        }
    }

    // 단 한개도 enpoint가 없다면 에러
    printk(KERN_ERR"nothing input endpoint");
    err = -ENODEV;

err_free_dev:
    printk(KERN_ERR"fail register input dev");
    cleanup();
    return err;
}


static void am_joyin_exit(void)
{
    cleanup();
    //printk("unregister input dev");
}


module_init(am_joyin_init);
module_exit(am_joyin_exit);
