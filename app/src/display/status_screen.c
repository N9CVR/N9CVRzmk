/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/display/widgets/output_status.h>
#include <zmk/display/widgets/peripheral_status.h>
#include <zmk/display/widgets/battery_status.h>
#include <zmk/display/widgets/layer_status.h>
#include <zmk/display/widgets/wpm_status.h>
#include <zmk/display/status_screen.h>

#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

extern const lv_img_dsc_t n9cvr_logo;

#if IS_ENABLED(CONFIG_ZMK_WIDGET_BATTERY_STATUS)
static struct zmk_widget_battery_status battery_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_OUTPUT_STATUS)
static struct zmk_widget_output_status output_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_PERIPHERAL_STATUS)
static struct zmk_widget_peripheral_status peripheral_status_widget;
static lv_obj_t *n9cvr_img;
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_LAYER_STATUS)
static struct zmk_widget_layer_status layer_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_WPM_STATUS)
static struct zmk_widget_wpm_status wpm_status_widget;
#endif

struct power_layout_state {
    bool usb_powered;
};

static struct power_layout_state power_layout_get_state(const zmk_event_t *eh) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    return (struct power_layout_state){
        .usb_powered = zmk_usb_is_powered(),
    };
#else
    return (struct power_layout_state){
        .usb_powered = false,
    };
#endif
}

static void power_layout_update_cb(struct power_layout_state state) {
#if IS_ENABLED(CONFIG_ZMK_WIDGET_BATTERY_STATUS)
    lv_obj_t *battery_obj =
        zmk_widget_battery_status_obj(&battery_status_widget);

    if (state.usb_powered) {
        lv_obj_align(battery_obj, LV_ALIGN_TOP_RIGHT, 0, 0);
    } else {
        lv_obj_align(battery_obj, LV_ALIGN_CENTER, 0, 0);
    }
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_OUTPUT_STATUS)
    lv_obj_t *output_obj =
        zmk_widget_output_status_obj(&output_status_widget);

    if (state.usb_powered) {
        lv_obj_clear_flag(output_obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(output_obj, LV_OBJ_FLAG_HIDDEN);
    }
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_PERIPHERAL_STATUS)
    lv_obj_t *peripheral_obj =
        zmk_widget_peripheral_status_obj(&peripheral_status_widget);

    if (state.usb_powered) {
        lv_obj_clear_flag(peripheral_obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(peripheral_obj, LV_OBJ_FLAG_HIDDEN);
    }

    if (n9cvr_img != NULL) {
        if (state.usb_powered) {
            lv_obj_clear_flag(n9cvr_img, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(n9cvr_img, LV_OBJ_FLAG_HIDDEN);
        }
    }
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_LAYER_STATUS)
    lv_obj_t *layer_obj =
        zmk_widget_layer_status_obj(&layer_status_widget);

    if (state.usb_powered) {
        lv_obj_clear_flag(layer_obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(layer_obj, LV_OBJ_FLAG_HIDDEN);
    }
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_WPM_STATUS)
    lv_obj_t *wpm_obj =
        zmk_widget_wpm_status_obj(&wpm_status_widget);

    if (state.usb_powered) {
        lv_obj_clear_flag(wpm_obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(wpm_obj, LV_OBJ_FLAG_HIDDEN);
    }
#endif
}

ZMK_DISPLAY_WIDGET_LISTENER(
    widget_power_layout,
    struct power_layout_state,
    power_layout_update_cb,
    power_layout_get_state
)

ZMK_SUBSCRIPTION(widget_power_layout, zmk_usb_conn_state_changed);

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    screen = lv_obj_create(NULL);

#if IS_ENABLED(CONFIG_ZMK_WIDGET_BATTERY_STATUS)
    zmk_widget_battery_status_init(&battery_status_widget, screen);
    lv_obj_align(
        zmk_widget_battery_status_obj(&battery_status_widget),
        LV_ALIGN_TOP_RIGHT,
        0,
        0
    );
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_OUTPUT_STATUS)
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(
        zmk_widget_output_status_obj(&output_status_widget),
        LV_ALIGN_TOP_LEFT,
        0,
        0
    );
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_PERIPHERAL_STATUS)
    zmk_widget_peripheral_status_init(&peripheral_status_widget, screen);
    lv_obj_align(
        zmk_widget_peripheral_status_obj(&peripheral_status_widget),
        LV_ALIGN_TOP_LEFT,
        0,
        0
    );
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_LAYER_STATUS)
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_set_style_text_font(
        zmk_widget_layer_status_obj(&layer_status_widget),
        lv_theme_get_font_small(screen),
        LV_PART_MAIN
    );
    lv_obj_align(
        zmk_widget_layer_status_obj(&layer_status_widget),
        LV_ALIGN_BOTTOM_LEFT,
        0,
        0
    );
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_WPM_STATUS)
    zmk_widget_wpm_status_init(&wpm_status_widget, screen);
    lv_obj_align(
        zmk_widget_wpm_status_obj(&wpm_status_widget),
        LV_ALIGN_BOTTOM_RIGHT,
        0,
        0
    );
#endif

#if IS_ENABLED(CONFIG_ZMK_WIDGET_PERIPHERAL_STATUS)
    n9cvr_img = lv_img_create(screen);
    lv_img_set_src(n9cvr_img, &n9cvr_logo);
    lv_obj_set_pos(n9cvr_img, 2, 8);
#endif

    /*
     * Initialize this listener only after every object it may manipulate
     * has been created.
     */
    widget_power_layout_init();

    return screen;
}
