/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zephyr/drivers/display.h>
#include <zephyr/drivers/led.h>
#include <lvgl.h>

#include "theme.h"

#include <zmk/activity.h>
#include <zmk/usb.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/display/status_screen.h>

static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

#if DT_HAS_CHOSEN(zmk_display_led)
static const struct device *display_led = DEVICE_DT_GET(DT_PARENT(DT_CHOSEN(zmk_display_led)));
static const uint8_t display_led_idx = DT_NODE_CHILD_IDX(DT_CHOSEN(zmk_display_led));
#endif

static bool initialized = false;
static lv_obj_t *screen;

__attribute__((weak)) lv_obj_t *zmk_display_status_screen() { return NULL; }

void display_tick_cb(struct k_work *work) { lv_task_handler(); }

K_WORK_DEFINE(display_tick_work, display_tick_cb);

#if IS_ENABLED(CONFIG_ZMK_DISPLAY_WORK_QUEUE_DEDICATED)
K_THREAD_STACK_DEFINE(display_work_stack_area, CONFIG_ZMK_DISPLAY_DEDICATED_THREAD_STACK_SIZE);
static struct k_work_q display_work_q;
#endif

struct k_work_q *zmk_display_work_q() {
#if IS_ENABLED(CONFIG_ZMK_DISPLAY_WORK_QUEUE_DEDICATED)
    return &display_work_q;
#else
    return &k_sys_work_q;
#endif
}

void display_timer_cb() { k_work_submit_to_queue(zmk_display_work_q(), &display_tick_work); }

K_TIMER_DEFINE(display_timer, display_timer_cb, NULL);

void unblank_display_cb(struct k_work *work) {
#if DT_HAS_CHOSEN(zmk_display_led)
    led_on(display_led, display_led_idx);
#endif

    display_blanking_off(display);

#if !IS_ENABLED(CONFIG_ARCH_POSIX)
    k_timer_start(&display_timer, K_MSEC(CONFIG_ZMK_DISPLAY_TICK_PERIOD_MS),
                  K_MSEC(CONFIG_ZMK_DISPLAY_TICK_PERIOD_MS));
#endif
}

#if IS_ENABLED(CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE)
void blank_display_cb(struct k_work *work) {
#if !IS_ENABLED(CONFIG_ARCH_POSIX)
    k_timer_stop(&display_timer);
#endif

    display_blanking_on(display);

#if DT_HAS_CHOSEN(zmk_display_led)
    led_off(display_led, display_led_idx);
#endif
}

K_WORK_DEFINE(blank_display_work, blank_display_cb);
K_WORK_DEFINE(unblank_display_work, unblank_display_cb);

static void start_display_updates() {
    if (display == NULL) {
        return;
    }

    k_work_submit_to_queue(zmk_display_work_q(), &unblank_display_work);
}

static void stop_display_updates() {
    if (display == NULL) {
        return;
    }

    k_work_submit_to_queue(zmk_display_work_q(), &blank_display_work);
}

static bool display_usb_powered() {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    return zmk_usb_is_powered();
#else
    return false;
#endif
}
#endif

int zmk_display_is_initialized() { return initialized; }

static void initialize_theme() {
#if IS_ENABLED(CONFIG_LV_USE_THEME_MONO)
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *theme =
        lv_theme_mono_init(disp, IS_ENABLED(CONFIG_ZMK_DISPLAY_INVERT), CONFIG_LV_FONT_DEFAULT,
                           CONFIG_ZMK_LV_FONT_DEFAULT_SMALL, CONFIG_LV_FONT_DEFAULT);
    lv_display_set_theme(disp, theme);
#endif
}

void initialize_display(struct k_work *work) {
    LOG_DBG("");

    if (!device_is_ready(display)) {
        LOG_ERR("Failed to find display device");
        return;
    }

    initialized = true;
    initialize_theme();
    screen = zmk_display_status_screen();

    if (screen == NULL) {
        LOG_ERR("No status screen provided");
        return;
    }

    lv_scr_load(screen);
    unblank_display_cb(work);
}

K_WORK_DEFINE(init_work, initialize_display);

int zmk_display_init() {
#if IS_ENABLED(CONFIG_ZMK_DISPLAY_WORK_QUEUE_DEDICATED)
    struct k_work_queue_config config = {
#if IS_ENABLED(CONFIG_THREAD_NAME)
        .name = "display queue",
#endif
    };

    k_work_queue_start(&display_work_q, display_work_stack_area,
                       K_THREAD_STACK_SIZEOF(display_work_stack_area),
                       CONFIG_ZMK_DISPLAY_DEDICATED_THREAD_PRIORITY, &config);
#endif

#if IS_ENABLED(CONFIG_ARCH_POSIX)
    initialize_display(NULL);
#else
    k_work_submit_to_queue(zmk_display_work_q(), &init_work);
#endif

    LOG_DBG("");
    return 0;
}

#if IS_ENABLED(CONFIG_ZMK_DISPLAY_BLANK_ON_IDLE)
int display_event_handler(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *activity_ev = as_zmk_activity_state_changed(eh);

    if (activity_ev != NULL) {
        switch (activity_ev->state) {
        case ZMK_ACTIVITY_ACTIVE:
            start_display_updates();
            break;

        case ZMK_ACTIVITY_IDLE:
            /* Stay visible while USB-powered; blank normally on battery. */
            if (display_usb_powered()) {
                start_display_updates();
            } else {
                stop_display_updates();
            }
            break;

        case ZMK_ACTIVITY_SLEEP:
            /* Always blank before deep sleep, even if USB state changed unexpectedly. */
            stop_display_updates();
            break;

        default:
            LOG_WRN("Unhandled activity state: %d", activity_ev->state);
            return -EINVAL;
        }

        return 0;
    }

    if (as_zmk_usb_conn_state_changed(eh) != NULL) {
        if (display_usb_powered()) {
            /* USB connected: unblank and keep display updates running. */
            start_display_updates();
        } else if (zmk_activity_get_state() == ZMK_ACTIVITY_IDLE ||
                   zmk_activity_get_state() == ZMK_ACTIVITY_SLEEP) {
            /* USB removed while already inactive: blank immediately. */
            stop_display_updates();
        } else {
            /* USB removed while active: remain visible until normal idle timeout. */
            start_display_updates();
        }

        return 0;
    }

    return -ENOTSUP;
}

ZMK_LISTENER(display, display_event_handler);
ZMK_SUBSCRIPTION(display, zmk_activity_state_changed);
ZMK_SUBSCRIPTION(display, zmk_usb_conn_state_changed);
#endif

SYS_INIT(zmk_display_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
