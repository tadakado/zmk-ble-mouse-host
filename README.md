# zmk-ble-mouse-host

A [ZMK](https://zmk.dev) module that lets a keyboard act as a **BLE HID host
(central)** for an external Bluetooth mouse, and forwards the mouse's pointer
events into ZMK's own pointing pipeline.

Because the events ride ZMK's normal mouse-HID path, they go out over whichever
output the keyboard is currently using — a USB cable or one of the BLE host
profiles. **Switching the keyboard's output moves the mouse with it**, so a
single mouse follows the keyboard between your computers without re-pairing the
mouse itself.

```
   ┌─────────┐   BLE    ┌──────────────────────┐  USB / BLE  ┌────────┐
   │  mouse  │─────────▶│ keyboard (ZMK central│────────────▶│  host  │
   │ (HOGP)  │  (this   │  + this module)      │  (follows   │  PC    │
   └─────────┘  module) └──────────────────────┘   output)   └────────┘
```

## Requirements

- A ZMK keyboard on an nRF52-class board (tested on Seeed XIAO nRF52840).
- On a **split** keyboard, the mouse host runs on the **central** half only.

ZMK's pointing subsystem (`CONFIG_ZMK_POINTING`) is enabled automatically — the
module `select`s it — so you don't need to set it yourself.

## Installation

Add the module to your `zmk-config`'s `config/west.yml`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: tadakado
      url-base: https://github.com/tadakado
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-ble-mouse-host
      remote: tadakado
      revision: main
```

## Devicetree wiring

Add these nodes to your board/shield `.overlay` (on a split, the **central**
half's overlay). The `zmk,input-listener` is what actually forwards the mouse
device's events into ZMK.

```dts
/ {
    mouse_host: mouse_host {
        compatible = "zmk,ble-mouse-host";
    };

    mouse_host_listener {
        compatible = "zmk,input-listener";
        device = <&mouse_host>;
    };

    behaviors {
        mouse_pair: mouse_pair {
            compatible = "zmk,behavior-mouse-host-pair";
            #binding-cells = <0>;
        };
        mouse_unpair: mouse_unpair {
            compatible = "zmk,behavior-mouse-host-unpair";
            #binding-cells = <0>;
        };
        mouse_dump: mouse_dump {
            compatible = "zmk,behavior-mouse-host-dump";
            #binding-cells = <0>;
        };
    };
};
```

Then bind the behaviors somewhere in your keymap (e.g. on an adjust layer):

```dts
&mouse_pair    // forget any bonded mouse and scan/pair a new one
&mouse_unpair  // forget the bonded mouse and stop reconnecting
&mouse_dump    // log bond / connection / report-layout state to the console
```

### Scroll-wheel direction (per output)

Different hosts want different scroll directions (e.g. "natural scrolling").
The wheel direction can be inverted **per active output endpoint**, set on the
`mouse_host` node:

```dts
mouse_host: mouse_host {
    compatible = "zmk,ble-mouse-host";
    wheel-invert-usb;                    // invert while on USB
    wheel-invert-ble-profiles = <0 1>;   // invert on BLE profiles 0 and 1
};
```

This is applied automatically whenever the keyboard's output changes — and again
after a reboot when the saved profile reconnects — so the direction follows the
target device with no key press and survives power cycles. Only the vertical
wheel is affected; horizontal (AC Pan) is left alone.

A manual override is also available as a behavior (`0`=off, `1`=on, `2`=toggle):

```dts
#include <dt-bindings/zmk/mouse_host_wheel.h>
// ...
behaviors {
    wheel_inv: wheel_inv {
        compatible = "zmk,behavior-mouse-host-wheel-invert";
        #binding-cells = <1>;
    };
};
// in the keymap:  &wheel_inv WHEEL_INV_TOGGLE
```

A manual change lasts until the next output switch re-applies the per-endpoint
setting.

### Split keyboards

The mouse host only builds on the central half (its keymap is the only one
compiled). If your left/right halves **share a single keymap file**, the labels
`&mouse_pair` / `&mouse_unpair` / `&mouse_dump` must still resolve when the
peripheral half is built. Provide stub behaviors with the same labels in the
peripheral's overlay (any zero-parameter behavior works — the peripheral never
executes them), or guard those bindings out of the shared keymap.

### Connection budget (`.conf`)

The central now maintains up to three simultaneous links — the split peripheral,
the mouse, and a BLE host — and stores more bonds. On the central half:

```ini
# split peripheral + mouse + one BLE host
CONFIG_BT_MAX_CONN=3
# host profiles + peripheral + mouse
CONFIG_BT_MAX_PAIRED=8
# BT_ATT_TX_COUNT is a single shared ATT request pool across ALL connections;
# the default (3) can be exhausted by the mouse's multi-characteristic GATT
# discovery running alongside the host link (discovery fails -ENOMEM and the
# mouse connects but is never subscribed). Raise it.
CONFIG_BT_ATT_TX_COUNT=8
```

On a non-split board, `CONFIG_BT_MAX_CONN=2` (mouse + host) is enough.

## Usage

1. Flash the firmware. On first boot there is **no** bonded mouse and the module
   stays idle — it does **not** scan on its own (see "Design notes").
2. Press the key bound to `&mouse_pair`, then put your mouse into pairing mode.
   The module scans for a device advertising the HID *Mouse* appearance,
   connects, bonds, and subscribes to its input reports.
3. Move the mouse — the pointer now moves through the keyboard's active output.
4. The bond is saved. After any reboot or power-cycle (of either the mouse or the
   keyboard) the module reconnects to the bonded mouse automatically.
5. `&mouse_unpair` forgets the mouse and stops reconnecting until you pair again.

> The mouse must be bonded **only to the keyboard**, not simultaneously to a
> computer, or the computer may grab it first.

## Kconfig options

| Option | Default | Meaning |
| --- | --- | --- |
| `CONFIG_ZMK_BLE_MOUSE_HOST` | `y` (when the DT node exists) | Enable the module. |
| `CONFIG_ZMK_BLE_MOUSE_HOST_MAX_REPORTS` | `6` | Max notifying HID input-report characteristics tracked on the mouse. |
| `CONFIG_ZMK_BLE_MOUSE_HOST_LOG_REPORTS` | `n` | Verbose: log every decoded pointer report + a raw hexdump as the mouse moves. Noisy; for reverse-engineering a new mouse only. |
| `CONFIG_ZMK_BLE_MOUSE_HOST_FORGET_ON_BOOT` | `n` | One-shot helper: clear all BLE **host** profiles on boot (keeps the mouse bond) so a computer can re-pair without a `&bt BT_CLR` key. Leave off for normal use. |

## How it works / design notes

- **Reconnect via direct connection, not scanning.** Reconnecting to a bonded
  mouse calls `bt_conn_le_create()` at the known address ("Initiating State"),
  not `bt_le_scan_start()`. This is required: the mouse reconnects via directed
  advertising, which this scan config never surfaces to the scan callback
  (root-caused with an RF sniffer to a TargetA address-type mismatch). Fresh
  pairing (address unknown) uses an active scan matched by the HID *Mouse* GAP
  appearance.
- **No automatic pairing scan.** The module never scans on its own — pairing
  happens only when you press `&mouse_pair`. This avoids a boot-time scan
  competing with a split's own peripheral (re)pairing.
- **Generic HID report parsing.** The mouse's HID Report Map is read over GATT
  and parsed to locate the buttons / X / Y / wheel / AC-pan fields, so mice with
  differing report layouts (8/12/16-bit axes, report IDs, etc.) work without
  device-specific code. See
  [`include/zmk_ble_mouse_host/hid_parser.h`](include/zmk_ble_mouse_host/hid_parser.h). If a
  mouse exposes several input reports, only the one whose length matches the
  parsed mouse report is decoded.

## Known limitations

- **One mouse at a time.** Only a single external mouse is tracked.
- **Buttons.** Up to 5 buttons are forwarded (left / right / middle / back /
  forward), matching ZMK's mouse HID.
- **Discovery on fresh pairing needs a GAP *Mouse* appearance** (`0x03C2`).
  A mouse that doesn't advertise its appearance won't be found by `&mouse_pair`.
  (Reconnecting to an already-bonded mouse doesn't rely on this.)
- **No reconnect back-off.** While the mouse is off/absent, the central keeps
  initiating to reconnect (there's no low-duty fallback). Harmless on a
  USB-powered central, but on a battery-powered wireless central this draws
  power the whole time the mouse is away.
- Tested with a Logitech M650 (mouse) and MX Ergo (trackball). Other
  HID-over-GATT pointing devices should work via the generic parser but haven't
  all been verified — reports welcome.

## Tests

The HID report-descriptor parser is hardware-independent and has host-side unit
tests:

```sh
make -C test        # builds & runs the tests with your host cc
```

## License

MIT — see [LICENSE](LICENSE). The HID parser (`src/hid_parser.c`,
`include/zmk_ble_mouse_host/hid_parser.h`) is reused from the author's own USB mouse-host project
and is likewise MIT-licensed here.
