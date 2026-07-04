// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tadashi Kadowaki
//
// Scroll-wheel direction inversion for the forwarded BLE mouse. The invert flag
// is read by ble_mouse_host.c's emit_motion() (it negates the vertical wheel).
// It is set automatically from the active output endpoint (per the
// wheel-invert-* devicetree properties, re-applied on every endpoint change) and
// manually via the zmk,behavior-mouse-host-wheel-invert keymap behavior.

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <zmk/ble_mouse_host.h>

LOG_MODULE_DECLARE(ble_mouse_host, CONFIG_ZMK_BLE_MOUSE_HOST_LOG_LEVEL);

/* Read by emit_motion() in ble_mouse_host.c. */
bool mh_wheel_inverted;

void zmk_ble_mouse_host_wheel_invert(int action) {
    switch (action) {
    case 0: mh_wheel_inverted = false; break;              /* OFF */
    case 1: mh_wheel_inverted = true; break;               /* ON */
    default: mh_wheel_inverted = !mh_wheel_inverted; break; /* TOGGLE */
    }
    LOG_INF("wheel invert = %d", (int)mh_wheel_inverted);
}

/* Auto per-endpoint direction, configured on the mouse_host node. Endpoints
 * exist on the central (or a non-split board) and the config lives on that node,
 * so compile the listener only where both are present. */
#define DT_DRV_COMPAT zmk_ble_mouse_host

#if (!defined(CONFIG_ZMK_SPLIT) || defined(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)) &&                       \
    DT_HAS_COMPAT_STATUS_OKAY(zmk_ble_mouse_host)

#include <zmk/event_manager.h>
#include <zmk/events/endpoint_changed.h>

static bool ble_profile_inverted(int profile) {
#if DT_INST_NODE_HAS_PROP(0, wheel_invert_ble_profiles)
    static const int list[] = DT_INST_PROP(0, wheel_invert_ble_profiles);
    for (int i = 0; i < (int)ARRAY_SIZE(list); i++) {
        if (list[i] == profile) {
            return true;
        }
    }
#else
    ARG_UNUSED(profile);
#endif
    return false;
}

static int mh_wheel_endpoint_listener(const zmk_event_t *eh) {
    const struct zmk_endpoint_changed *ev = as_zmk_endpoint_changed(eh);
    if (!ev) {
        return 0;
    }

    bool invert;
    switch (ev->endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        invert = DT_INST_PROP(0, wheel_invert_usb);
        break;
    case ZMK_TRANSPORT_BLE:
        invert = ble_profile_inverted(ev->endpoint.ble.profile_index);
        break;
    default:
        return 0; /* disconnected: keep the last direction until reconnect */
    }

    zmk_ble_mouse_host_wheel_invert(invert ? 1 : 0);
    return 0;
}

ZMK_LISTENER(mh_wheel_ep, mh_wheel_endpoint_listener);
ZMK_SUBSCRIPTION(mh_wheel_ep, zmk_endpoint_changed);

#endif
