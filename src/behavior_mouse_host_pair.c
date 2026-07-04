// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tadashi Kadowaki
// Keymap behavior: forget the bonded BLE mouse (if any) and actively scan to
// pair a new one.

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/ble_mouse_host.h>

LOG_MODULE_DECLARE(ble_mouse_host, CONFIG_ZMK_BLE_MOUSE_HOST_LOG_LEVEL);

static int on_pressed(struct zmk_behavior_binding *binding,
                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    zmk_ble_mouse_host_pair();
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_released(struct zmk_behavior_binding *binding,
                       struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_mouse_host_pair_api = {
    .binding_pressed = on_pressed,
    .binding_released = on_released,
};

static int behavior_mouse_host_pair_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define DT_DRV_COMPAT zmk_behavior_mouse_host_pair

BEHAVIOR_DT_INST_DEFINE(0, behavior_mouse_host_pair_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_mouse_host_pair_api);
