// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tadashi Kadowaki
// Keymap behavior: control forwarded-mouse wheel-direction inversion.
// Parameter: 0 = OFF, 1 = ON, 2 = TOGGLE (see dt-bindings/zmk/mouse_host_wheel.h).

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/ble_mouse_host.h>

LOG_MODULE_DECLARE(ble_mouse_host, CONFIG_ZMK_BLE_MOUSE_HOST_LOG_LEVEL);

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    zmk_ble_mouse_host_wheel_invert((int)binding->param1);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_released(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_mouse_host_wheel_invert_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
};

static int behavior_mouse_host_wheel_invert_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define DT_DRV_COMPAT zmk_behavior_mouse_host_wheel_invert

BEHAVIOR_DT_INST_DEFINE(0, behavior_mouse_host_wheel_invert_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_mouse_host_wheel_invert_api);
