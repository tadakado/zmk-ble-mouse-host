/*
 * Copyright (c) 2026 Tadashi Kadowaki
 * SPDX-License-Identifier: MIT
 */

#pragma once

/* Forget the bonded mouse (if any), disconnect it (if connected), and
 * actively scan to pair a new one. Intended for a keymap trigger. */
void zmk_ble_mouse_host_pair(void);

/* Forget the bonded mouse (if any), disconnect it (if connected), and stop
 * trying to reconnect until zmk_ble_mouse_host_pair() is called again.
 * Intended for a keymap trigger. */
void zmk_ble_mouse_host_unpair(void);

/* Log the mouse bond address, connection state, and (if known) the parsed HID
 * report layout to the console. Intended for a keymap trigger. */
void zmk_ble_mouse_host_dump(void);

/* Set scroll-wheel direction inversion for the forwarded mouse:
 * 0 = off, 1 = on, other = toggle. Normally driven automatically from the
 * active output endpoint (see the wheel-invert-* devicetree properties); this
 * is also exposed as the zmk,behavior-mouse-host-wheel-invert keymap trigger. */
void zmk_ble_mouse_host_wheel_invert(int action);
