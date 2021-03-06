/********************************************************************************
 * Copyright (C) 2021 Ju, Gyeong-min
 ********************************************************************************/

#ifndef __KERNEL__
#define __KERNEL__
#endif


#include "build_cfg.h"

#ifndef MODULE
#define MODULE
#endif


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#if defined(USE_REPORT_TIMER)
#include <linux/timer.h>
#else
#include <linux/kthread.h>
#endif

MODULE_AUTHOR("Amos42");
MODULE_DESCRIPTION("GPIO and Multiplexer and 74HC165 amd MCP23017 Arcade Joystick Driver");
MODULE_LICENSE("GPL");
#if !defined(USE_I2C_DIRECT)
MODULE_SOFTDEP("pre: i2c-dev");
#endif

/*============================================================*/

#include "am_joyin.h"
#include "log_util.h"

#include "bcm_peri.h"
#include "gpio_util.h"
#include "parse_util.h"
#include "indev_type.h"
#if defined(USE_I2C_DIRECT)
#include "i2c_util.h"
#endif
#if defined(USE_SPI_DIRECT)
#include "spi_util.h"
#endif


//==== 코드 포함 (begin) ====
#include "log_util.c"
#include "am_joyin_cfg.c"
#include "bcm_peri.c"
#include "gpio_util.c"
#if defined(USE_I2C_DIRECT)
#include "i2c_util.c"
#endif
#if defined(USE_SPI_DIRECT)
#include "spi_util.c"
#endif
#include "parse_util.c"
#include "indev_type.c"
#include "device_gpio_rpi2.c"
#include "device_74hc165.c"
#include "device_mcp23017.c"
#include "device_mcp23s17.c"
#include "device_mux.c"
#include "device_adc_mcp300x.c"
#include "device_adc_ads1x15.c"
#include "device_rotary_am_spinin.c"
//==== 코드 포함 (end) ====


#if defined(USE_REPORT_TIMER)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
#define HAVE_TIMER_SETUP
#endif
#endif

#define DEFAULT_REPORT_PERIOD	(100)
#if defined(USE_REPORT_TIMER)
#define DEFAULT_REFRESH_TIME	(HZ / DEFAULT_REPORT_PERIOD)
#endif


static am_joyin_data_t *a_input;


static void init_button_config(struct input_dev *indev, input_buttonset_data_t *buttonset_cfg, int button_count, int endpoint_type)
{
    int i;

    if (button_count > buttonset_cfg->button_count) {
        button_count = buttonset_cfg->button_count;
    }

    for (i = 0; i < button_count; i++) {
        input_button_data_t *btn = &buttonset_cfg->button_data[i];
        unsigned int code = btn->button_code;
        if (code < ABS_MAX) {
            if (endpoint_type == ENDPOINT_TYPE_JOYSTICK) {
                input_set_abs_params(indev, code, btn->min_value, btn->max_value, 0, 0);
            }
        } else {
            __set_bit(code, indev->keybit);
        }
    }
}


static void report_input(struct input_dev *indev, int endpoint_type, input_button_data_t *button_data, int button_count, int *data, int *cur_data)
{
    int i;
    BOOL is_changed = FALSE;

    for (i = 0; i < button_count; i++) {
        if (data[i] != cur_data[i]){
            unsigned int code = button_data[i].button_code;
            if (code < ABS_MAX) {
                if (endpoint_type == ENDPOINT_TYPE_MOUSE) {
                    input_report_rel(indev, code, data[i]);
                } else {
                    input_report_abs(indev, code, data[i]);
                }
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


#if defined(USE_REPORT_TIMER)
#if defined(HAVE_TIMER_SETUP)
static void am_timer(struct timer_list *t) {
    am_joyin_data_t *inp = from_timer(inp, t, report_timer);
#else
static void am_timer(unsigned long private) {
    am_joyin_data_t *inp = (void *) private;
#endif
    int i;
    unsigned long next_timer = jiffies + inp->timer_period; // 다음 타이머 주기를 미리 구해 놓는다.

    for (i = 0; i < inp->input_endpoint_count; i++) {
        input_endpoint_data_t *ep = &inp->endpoint_list[i];
        memset(ep->report_button_state, 0, sizeof(int) * MAX_INPUT_BUTTON_COUNT);
    }

    for (i = 0; i < inp->input_device_count; i++) {
        input_device_data_t *dev = &inp->device_list[i];
        if (dev != NULL && dev->is_opend && dev->device_type_desc != NULL) {
            dev->device_type_desc->check_input_dev(dev);
        }
    }

    for (i = 0; i < inp->input_endpoint_count; i++) {
        input_endpoint_data_t *ep = &inp->endpoint_list[i];
        report_input(ep->indev, ep->endpoint_type, ep->target_buttonset->button_data, ep->button_count, ep->report_button_state, ep->current_button_state);
    }

    // 만약 키체크 시간이 너무 길어서 다음 타이머 주기를 초과해 버리면 예외 처리
    if (next_timer <= jiffies) {
        next_timer = jiffies + inp->timer_period;

        // 타이머 주기를 초과하는 횟수가 100회를 넘으면 타이머 중단
        if (++inp->missing_timer_count > 100) {
            am_log_err("button check time is too late. Terminate the timer.");
            return;
        }
    }

    // 다음 타이머 트리거
    mod_timer(&inp->report_timer, next_timer);
}
#else
static int report_worker(void *data) {
    am_joyin_data_t *inp = (am_joyin_data_t *)data;

    unsigned int msleeptick = 1000 / inp->report_period;

    while (!kthread_should_stop()) {
        int i;

        for (i = 0; i < inp->input_endpoint_count; i++) {
            input_endpoint_data_t *ep = &inp->endpoint_list[i];
            memset(ep->report_button_state, 0, sizeof(int) * MAX_INPUT_BUTTON_COUNT);
        }

        for (i = 0; i < inp->input_device_count; i++) {
            input_device_data_t *dev = &inp->device_list[i];
            if (dev != NULL && dev->is_opend && dev->device_type_desc != NULL) {
                dev->device_type_desc->check_input_dev(dev);
            }
        }

        for (i = 0; i < inp->input_endpoint_count; i++) {
            input_endpoint_data_t *ep = &inp->endpoint_list[i];
            report_input(ep->indev, ep->endpoint_type, ep->target_buttonset->button_data, ep->button_count, ep->report_button_state, ep->current_button_state);
        }

        msleep(msleeptick);
    }

    return 0;
}
#endif

static void init_report_worker(void)
{
    if (a_input != NULL) {
#if defined(USE_REPORT_TIMER)
#if defined(HAVE_TIMER_SETUP)
        timer_setup(&a_input->report_timer, am_timer, 0);
#else
        setup_timer(&a_input->report_timer, am_timer, (long)a_input);
#endif
#else
        a_input->report_task = kthread_create(report_worker, a_input, "am_joyin_report_task-%d", current->pid);
#endif
    }
}


static void start_report_worker(void)
{
    if (a_input != NULL) {
        if (a_input->input_endpoint_count > 0 && a_input->input_device_count > 0){
#if defined(USE_REPORT_TIMER)
            mod_timer(&a_input->report_timer, jiffies + a_input->timer_period);
#else
            if (a_input->report_task != NULL) {
                wake_up_process(a_input->report_task);
            }
#endif
            am_log_debug("start report worker.");
        }
    }
}


static void stop_report_worker(void)
{
    if (a_input != NULL) {
#if defined(USE_REPORT_TIMER)
        del_timer_sync(&a_input->report_timer);
#else
        if (a_input->report_task != NULL) {
            //sleep_on(a_input->report_task);
        }
#endif
        am_log_debug("stop report worker.");
    }
}


static void remove_report_worker(void)
{
    if (a_input != NULL) {
#if defined(USE_REPORT_TIMER)
        del_timer_sync(&a_input->report_timer);
#else
        if (a_input->report_task != NULL) {
            kthread_stop(a_input->report_task);
            a_input->report_task = NULL;
        }
#endif
    }
}


static int __open_handler(struct input_dev *indev)
{
    input_endpoint_data_t *endpoint = input_get_drvdata(indev);
    int err;

    am_log_debug("joystick input endpoint %d open...", a_input->used);

    err = mutex_lock_interruptible(&a_input->mutex);
    if (err)
        return err;

    if (!endpoint->is_opened) {
        if (!a_input->used++) {
            start_report_worker();
        }

        endpoint->is_opened = TRUE;
        am_log_debug("joystick input endpoint opend");
    }

    mutex_unlock(&a_input->mutex);

    return 0;
}


static void __close_handler(struct input_dev *indev)
{
    input_endpoint_data_t *endpoint = input_get_drvdata(indev);

    am_log_debug("joystick input endpoint %d close...", a_input->used);

    mutex_lock(&a_input->mutex);

    if (endpoint->is_opened) {
        if (!--a_input->used) {
            stop_report_worker();
        }

        endpoint->is_opened = FALSE;
        am_log_debug("joystick input endpoint closed");
    }

    mutex_unlock(&a_input->mutex);
}


static int __endpoint_register(input_endpoint_data_t *endpoint)
{
    int err = 0;

    if (endpoint->indev == NULL) {
        struct input_dev *indev = input_allocate_device();
        indev->name = endpoint->endpoint_name;
        indev->id.bustype = BUS_PARPORT;
        indev->id.vendor = VENDOR_ID;
        indev->id.product = PRODUCT_ID;
        indev->id.version = PRODUCT_VERSION;

        input_set_drvdata(indev, endpoint);
        indev->open = __open_handler;
        indev->close = __close_handler;
        if (endpoint->endpoint_type == ENDPOINT_TYPE_JOYSTICK) {
            indev->phys = "joystick";
            indev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
            init_button_config(indev, endpoint->target_buttonset, endpoint->button_count, endpoint->endpoint_type);
        } else if (endpoint->endpoint_type == ENDPOINT_TYPE_MOUSE) {
            indev->phys = "mouse";
            indev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
            __set_bit(REL_X, indev->relbit);
            __set_bit(REL_Y, indev->relbit);
            __set_bit(BTN_MOUSE, indev->keybit);
            __set_bit(BTN_RIGHT, indev->keybit);
            __set_bit(BTN_MIDDLE, indev->keybit);
        }

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

        remove_report_worker();

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



static int __init am_joyin_init(void)
{
    int i;
    int err;

    am_log_info("am_joyin init...");

    // 커맨드 라인 파라미터 전처리
    prepocess_params();

    err = gpio_init(bcm_peri_base_probe());
    if (err < 0) {
        goto err_free_dev;
    }

    a_input = (am_joyin_data_t *)kzalloc(sizeof(am_joyin_data_t), GFP_KERNEL);

    init_report_worker();

    // 지원할 장치 목록들을 등록한다. 
    // 등록 가능한 최대 갯수는 MAX_INPUT_DEVICE_TYPE_DESC_COUNT 값을 따른다.
    register_input_device_for_gpio(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);       // 1
    register_input_device_for_74hc165(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);    // 2
    register_input_device_for_mcp23017(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);   // 3
    register_input_device_for_mcp23s17(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);   // 4
    register_input_device_for_mux(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);        // 5
    register_input_device_for_mcp3008(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);    // 6
    register_input_device_for_mcp3004(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);    // 7
    register_input_device_for_ads1115(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);    // 8
    register_input_device_for_ads1015(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);    // 9
    register_input_device_for_am_spinin(&a_input->device_type_desc_list[a_input->input_device_type_desc_count++]);  // 10

    // 커맨드 라인 파라미터들을 분석한다.
    parsing_device_config_params(a_input);

    // 타이머 주기 설정. 비어 있으면 기본 타이머 주기
    if (a_input->report_period <= 0 || a_input->report_period > 1000) {
        a_input->report_period = DEFAULT_REPORT_PERIOD;
    }
#if defined(USE_REPORT_TIMER)
    a_input->timer_period = HZ / a_input->report_period;
#endif

    am_log_debug("start register input dev...");

    // 설정으로 지정한 모든 장치를 초기화 한다.
    for (i = 0; i < a_input->input_device_count; i++) {
        input_device_data_t *dev = &a_input->device_list[i];

        am_log_info("register input dev #%d : %s", i, dev->device_type_desc->device_type);

        dev->device_type_desc->start_input_dev(dev);

        err = __input_dev_register(dev);
        if (err < 0) {
            goto err_free_dev;
        }
    }

    am_log_debug("end register input dev.");

    // 한개라도 endpoint가 등록되어 있다면 정상 종료
    for (i = 0; i < a_input->input_endpoint_count; i++) {
        if (a_input->endpoint_list[i].indev != NULL) {
            am_log_info("success register input dev.");
            return 0;
        }
    }

    // 단 한개도 enpoint가 없다면 에러
    am_log_err("nothing input endpoint.");
    err = -ENODEV;

err_free_dev:
    am_log_err("fail register input dev.");
    cleanup();
    return err;
}


static void __exit am_joyin_exit(void)
{
    cleanup();
    am_log_info("am_joyin exit.");
}


module_init(am_joyin_init);
module_exit(am_joyin_exit);
