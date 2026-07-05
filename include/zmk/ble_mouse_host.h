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

/* Temporarily stop all RF activity (scan / reconnect / the mouse link) without
 * forgetting the bond, then resume. Useful to keep the radio quiet during a
 * timing-critical operation elsewhere (the module uses this to go quiet during
 * an IR bit-bang via zmk-ir's zmk_ir_tx_active() hook). */
void zmk_ble_mouse_host_pause(void);
void zmk_ble_mouse_host_resume(void);

/* Set scroll-wheel direction inversion for the forwarded mouse:
 * 0 = off, 1 = on, other = toggle. Normally driven automatically from the
 * active output endpoint (see the wheel-invert-* devicetree properties); this
 * is also exposed as the zmk,behavior-mouse-host-wheel-invert keymap trigger. */
void zmk_ble_mouse_host_wheel_invert(int action);
