---
title: Input Devices
description: Configure keyboard layouts, mouse sensitivity, and touchpad gestures.
---

## Device Configuration

The global settings below apply to every device of the corresponding type.
Per-device overrides are covered in the [Device Rules](#device-rules-advanced)
section at the end of this page.

### Keyboard Settings

Control key repeat rates and layout rules.

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `repeat_rate` | `int` | `25` | How many times a key repeats per second. |
| `repeat_delay` | `int` | `600` | Delay (ms) before a held key starts repeating. |
| `numlockon` | `0` or `1` | `0` | Enable NumLock on startup. |
| `xkb_rules_rules` | `string` | - | XKB rules file (e.g., `evdev`, `base`). Usually auto-detected. |
| `xkb_rules_model` | `string` | - | Keyboard model (e.g., `pc104`, `macbook`). |
| `xkb_rules_layout` | `string` | - | Keyboard layout code (e.g., `us`, `de`, `us,de`). |
| `xkb_rules_variant` | `string` | - | Layout variant (e.g., `dvorak`, `colemak`, `intl`). |
| `xkb_rules_options` | `string` | - | XKB options (e.g., `caps:escape`, `ctrl:nocaps`). |

**Example:**

```ini
repeat_rate=40
repeat_delay=300
numlockon=1
xkb_rules_layout=us,de
xkb_rules_variant=dvorak
xkb_rules_options=caps:escape,ctrl:nocaps
```

---

### Mouse Settings

Configuration for external mice.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `mouse_natural_scrolling` | `0` | Invert scrolling direction. |
| `mouse_accel_profile` | `2` | `0` (None), `1` (Flat), `2` (Adaptive). |
| `mouse_accel_speed` | `0.0` | Speed adjustment (-1.0 to 1.0). |
| `mouse_left_handed` | `0` | Swap left and right buttons. |
| `mouse_middle_button_emulation` | `0` | Emulate middle button. |
| `mouse_scroll_method` | `1` | `1` (Two-finger), `2` (Edge), `4` (Button). |
| `mouse_scroll_button` | `274` | The button used for button scrolling (272–279). |
| `mouse_click_method` | `1` | `1` (Button areas), `2` (Clickfinger). |
| `mouse_send_events_mode` | `0` | `0` (Enabled), `1` (Disabled), `2` (Disabled on external mouse). |
| `axis_scroll_factor` | `1.0` | Scroll factor for axis scroll speed (0.1–10.0). |
---

### Trackpad Settings

Specific settings for laptop touchpads. Some settings may require a relogin to take effect.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `disable_trackpad` | `0` | Set to `1` to disable the trackpad entirely. |
| `tap_to_click` | `1` | Tap to trigger a left click. |
| `tap_and_drag` | `1` | Tap and hold to drag items. |
| `trackpad_natural_scrolling` | `0` | Invert scrolling direction (natural scrolling). |
| `trackpad_accel_profile` | `2` | `0` (None), `1` (Flat), `2` (Adaptive). |
| `trackpad_accel_speed` | `0.0` | Speed adjustment (-1.0 to 1.0). |
| `trackpad_scroll_button` | `274` | The button used for button scrolling (272–279). |
| `trackpad_scroll_method` | `1` | `1` (Two-finger), `2` (Edge), `4` (Button). |
| `trackpad_click_method` | `1` | `1` (Button areas), `2` (Clickfinger). |
| `trackpad_send_events_mode` | `0` | `0` (Enabled), `1` (Disabled), `2` (Disabled on external mouse). |
| `drag_lock` | `1` | Lock dragging after tapping. |
| `trackpad_disable_while_typing` | `1` | Disable trackpad while typing. |
| `trackpad_left_handed` | `0` | Swap left/right buttons. |
| `trackpad_middle_button_emulation` | `0` | Emulate middle button. |
| `swipe_min_threshold` | `1` | Minimum swipe threshold when use gesture. |
| `gesture_live` | `1` | Drive tag/focus/overview transitions while the fingers are still moving (`1`), instead of only after release (`0`). |
| `gesture_swipe_distance` | `300` | Finger travel (px) that corresponds to one full page transition. |
| `gesture_swipe_cancel_ratio` | `0.5` | Releasing after the last page was dragged past this fraction commits it; below it, the transition animates back. |
| `gesture_swipe_min_speed_to_force` | `30` | Average per-event speed (px) that forces a commit even below the cancel ratio (for quick flicks). |
| `button_map` | `0` | `0` (Left/right/middle), `1` (Left/middle/right). |
| `trackpad_scroll_factor` | `1.0` | Scroll factor for trackpad scroll speed (0.1–10.0). |
---

### Touchscreen Settings

Settings for touchscreen devices. Touch input is forwarded to clients
that support the `wl_touch` protocol; otherwise it falls back to mouse
emulation so the touchscreen keeps working with non-touch clients.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `touch_enable` | `1` | Set to `0` to completely disable touchscreen support. |
| `touch_enable_mouse_emulation` | `0` | When `1`, touch events landing on surfaces that do not accept touch are emulated as left mouse button clicks/moves. Set to `0` to disable emulation (such touches are ignored). |
| `touch_map_to_mon` | *(unset)* | Restrict a touchscreen to one output. Accepts a [monitor spec](/docs/configuration/monitors#monitor-spec-format). Leave unset to map the touchscreen to the whole output layout. Useful on multi-monitor setups where the touchscreen would otherwise be stretched across all outputs. |

---

**Detailed descriptions:**

- `scroll_button` values (use `mouse_scroll_button` / `trackpad_scroll_button`):
  - `272` — Left button.
  - `273` — Right button.
  - `274` — Middle button.
  - `275` — Side button.
  - `276` — Extra button.
  - `277` — Forward button.
  - `278` — Back button.
  - `279` — Task button.

- `scroll_method` values (use `mouse_scroll_method` / `trackpad_scroll_method`):
  - `0` — Never send scroll events (no scrolling).
  - `1` — Two-finger scrolling: send scroll events when two fingers are logically down on the device.
  - `2` — Edge scrolling: send scroll events when a finger moves along the bottom or right edge.
  - `4` — Button scrolling: send scroll events when a button is held and the device moves along a scroll axis.

- `click_method` values (use `mouse_click_method` / `trackpad_click_method`):
  - `0` — No software click emulation.
  - `1` — Button areas: use software-defined areas on the touchpad to generate button events.
  - `2` — Clickfinger: the number of fingers determines which button is pressed.

- `mouse_accel_profile` or `trackpad_scroll_profile` values:
  - `0` — No acceleration.
  - `1` — Flat: no dynamic acceleration. Pointer speed = original input speed × (1 + `mouse_accel_speed`).
  - `2` — Adaptive: slow movement results in less acceleration, fast movement results in more.

- `button_map` values:
  - `0` — 1/2/3 finger tap maps to left / right / middle.
  - `1` — 1/2/3 finger tap maps to left / middle / right.

- `send_events_mode` values (use `mouse_send_events_mode` / `trackpad_send_events_mode`):
  - `0` — Send events from this device normally.
  - `1` — Do not send events from this device.
  - `2` — Disable this device when an external pointer device is plugged in.

---
---

## Keyboard Layout Switching

To bind multiple layouts and toggle between them, define the layouts in `xkb_rules_layout` and use `xkb_rules_options` to set a toggle key combination. Then bind `switch_keyboard_layout` to trigger a switch.

```ini
# Define two layouts: US QWERTY and US Dvorak
xkb_rules_layout=us,us
xkb_rules_variant=,dvorak
xkb_rules_options=grp:lalt_lshift_toggle
```

Or bind it manually to a key:

```ini
# Bind Alt+Shift_L to cycle keyboard layout
bind=alt,shift_l,switch_keyboard_layout
```

Use `mmsg get keyboardlayout` to query the current layout.

---

## Input Method Editor (IME)

To use Fcitx5 or IBus, set these environment variables in your config file.

> **Info:** These settings require a restart of the window manager to take effect.

**For Fcitx5:**

```ini
env=GTK_IM_MODULE,fcitx
env=QT_IM_MODULE,fcitx
env=QT_IM_MODULES,wayland;fcitx
env=SDL_IM_MODULE,fcitx
env=XMODIFIERS,@im=fcitx
env=GLFW_IM_MODULE,ibus
```

**For IBus:**

```ini
env=GTK_IM_MODULE,ibus
env=QT_IM_MODULE,ibus
env=XMODIFIERS,@im=ibus
```

---

## Device Rules (Advanced)

The global settings above apply to every device of the corresponding type.
Use `devicerule` to override parameters for a specific device.

**Finding device names:**

The easiest way to get a device's name is to watch for it: run

```bash
mmsg watch all-devices
```

then use the device (type on a keyboard, move a mouse, scroll a touchpad).
Each event prints the name of the device that triggered it, so you can match
every physical device to its name without guessing. `mmsg get all-devices`
also lists all connected devices at once if you prefer.

**Syntax:**

```ini
devicerule=name:<device-name>,option:value,option:value
devicerule=type:<device-type>,option:value
```

Put the printed `name` after `name:` (the `identifier` field,
`vendor:product:name`, also works). Use `type:` to match all devices of a
type: `keyboard`, `pointer`, `touchpad`, `touch`, `switch`, `tablet`, `pad`.

Exact `name:` matches take priority over `type:` matches; the first matching
rule wins. A rule with keyboard options (`kb_*`, `repeat_*`) turns that
keyboard into an independent keyboard with its own keymap and repeat settings;
unmatched devices stay in the shared, synchronized keyboard group.

**Examples:**

```ini
devicerule=name:AT Translated Set 2 keyboard,kb_layout:ru
devicerule=name:A4Tech USB Mouse,natural_scrolling:1,accel_speed:0.1
devicerule=type:touchpad,tap_to_click:1
```

Apply changes with `mmsg dispatch reload_config` or restart mango.

### Rule Options

All options are optional; unset options fall back to the global settings.
Keyboard `kb_*` options are independent of the global `xkb_rules_*` settings:
a rule's keymap is compiled only from the options it sets (unset fields use
the XKB defaults), so a rule like `kb_layout:pt` is not affected by a global
`xkb_rules_variant`.

| Category | Option | Description |
| :--- | :--- | :--- |
| Keyboard | `kb_layout` | Layout code, e.g. `us`, `ru`, `de` |
| Keyboard | `kb_variant` | Layout variant, e.g. `dvorak`, `colemak` |
| Keyboard | `kb_options` | XKB options, e.g. `caps:escape` |
| Keyboard | `kb_rules` / `kb_model` | XKB rules file / model |
| Keyboard | `repeat_rate` / `repeat_delay` | Key repeat rate / delay |
| Pointer | `accel_speed` | Pointer speed, `-1.0` to `1.0` |
| Pointer | `accel_profile` | `0` none, `1` flat, `2` adaptive |
| Pointer | `natural_scrolling` | `1` inverts scroll direction |
| Pointer | `left_handed` | `1` swaps left/right buttons |
| Touchpad | `tap_to_click` | `1` enables tap-to-click |
| Touchpad | `tap_and_drag` | `1` enables tap-and-drag |
| Touchpad | `scroll_method` | `1` two-finger, `2` edge, `4` button |
| Touchpad | `disable_while_typing` | `1` disables the touchpad while typing |
| Common | `middle_button_emulation` | `1` emulates the middle button |
| Common | `send_events_mode` | `0` enabled, `1` disabled, `2` disabled with external mouse |
| Common | `scroll_button` / `click_method` / `drag_lock` / `button_map` | libinput settings, see descriptions below |

> **Info:** If a rule's keyboard layout fails to compile (e.g. `kb_layout:ru`
> with `kb_variant:dvorak`), mango logs an error and falls back to the global
> layout instead of crashing.
