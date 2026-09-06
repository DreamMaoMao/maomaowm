---
title: Key Bindings
description: Define keyboard shortcuts and modes.
---

## Syntax

Key bindings follow this format:

```ini
bind[flags]=MODIFIERS,KEY,COMMAND,PARAMETERS
```

- **Modifiers**: `SUPER`, `CTRL`, `ALT`, `SHIFT`, `NONE` (combine with `+`, e.g. `SUPER+CTRL+ALT`).
- **Key**: Key name (from `xev` or `wev`) or keycode (e.g., `code:24` for `q`).

> **Info:** `bind` automatically converts keysym to keycode for comparison. This makes it compatible with all keyboard layouts, but the matching may not always be precise. If a key combination doesn't work on your keyboard layout, use a keycode instead (e.g., `code:24` instead of `q`).

### Flags

- `l`: Works even when screen is locked.
- `s`: Uses keysym instead of keycode to bind.
- `r`: Triggers on key release instead of press.
- `p`: Pass key event to client.
- `c`: allow keybind conflict(need set in all conflict key).

**Examples:**

```ini
bind=SUPER,Q,killclient
bindl=SUPER,L,spawn,swaylock

# Using keycode instead of key name
bind=ALT,code:24,killclient

# Combining keycodes for modifiers and keys
bind=code:64,code:24,killclient
bind=code:64+code:133,code:24,killclient

# Bind with no modifier
bind=NONE,XF86MonBrightnessUp,spawn,brightnessctl set +5%

# Bind a modifier key itself as the trigger key
bind=alt,shift_l,switch_keyboard_layout

# Allow keybind conflict
bindc=SUPER,a,resizewin,+10,0
bindc=SUPER,a,centerwin
```

## Key Modes (Submaps)

You can divide key bindings into named modes. Rules:

1. Set `keymode=<name>` before a group of `bind` lines — those binds only apply in that mode.
2. If no `keymode` is set before a bind, it belongs to the `default` mode.
3. The special `common` keymode applies its binds **across all modes**.

> **Info:** Key modes also apply to the other input bindings — `mousebind`, `axisbind`, `gesturebind` and `switchbind` — which share the exact same `keymode` rules as `bind`.

Use `setkeymode` to switch modes, and `mmsg get keymode` to query the current mode.

```ini
# Binds in 'common' apply in every mode
keymode=common
bind=SUPER,r,reload_config

# Default mode bindings
keymode=default
bind=ALT,Return,spawn,foot
bind=SUPER,F,setkeymode,resize

# 'resize' mode bindings
keymode=resize
bind=NONE,Left,resizewin,-10,0
bind=NONE,Right,resizewin,+10,0
bind=NONE,Escape,setkeymode,default
```

### Single Modifier Key Binding

When binding a modifier key itself, use `NONE` for press and the modifier name for release:

```ini
# Trigger on press of Super key
bind=none,Super_L,spawn,rofi -show run

# Trigger on release of Super key
bindr=Super,Super_L,spawn,rofi -show run
```

## Dispatchers List

### Window Management

| Command | Param | Description |
| :--- | :--- | :--- |
| `killclient` | `force` | Close the focused window. If `force` is specified, sends `SIGKILL`. With `smartkill=1`, a window on multiple tags is first detached from all viewed tags and only closed if no other tags remain (`force` always kills). |
| `togglefloating` | - | Toggle floating state. |
| `toggle_all_floating` | - | Toggle all visible clients floating state. |
| `togglefullscreen` | - | Toggle fullscreen. |
| `togglefakefullscreen` | - | Toggle "fake" fullscreen (remains constrained). |
| `togglemaximizescreen` | - | Maximize window (keep decoration/bar). |
| `toggleglobal` | - | Pin window to all tags. |
| `toggle_render_border` | - | Toggle border rendering. |
| `centerwin` | - | Center the floating window. |
| `minimized` | - | Minimize window to scratchpad. |
| `restore_minimized` | - | Restore minimized window to the currently focused tag. |
| `toggle_scratchpad` | - | Toggle scratchpad. |
| `toggle_named_scratchpad` | `appid,title,cmd` | Toggle named scratchpad. Launches app if not running, otherwise shows/hides it. |
| `toggle_special_tag` | - | Toggle special workspace overlay (tiling scratchpad). |
| `tag_special_tag` | - | Move focused window to/from the special workspace overlay. |
| `tag_special_silent` | - | Silently move focused window to/from the special workspace overlay. |

### Focus & Movement

| Command | Param | Description |
| :--- | :--- | :--- |
| `focusid` | - | Focus window (can target any window via IPC: `mmsg dispatch focusid client,<id>`) |
| `focusdir` | `left/right/up/down` | Focus window in direction. |
| `focus_window_or_workspace` | `left/right/up/down` | Focus window in direction; otherwise jump to the nearest adjacent tag that has clients, falling back to the next/previous tag if none. |
| `focusstack` | `next/prev` | Cycle focus within the stack. |
| `overcircle` | `next/prev` | Open overview when closed; while it is open, cycle focus to the next/previous window on the current monitor. |
| `focuslast` | - | Focus the previously active window. |
| `switcher` | `next/prev`, `all_tag_next/all_tag_prev`, `all_next/all_prev` | Open or cycle the thumbnail switcher. `next`/`prev` list the current tag's windows, `all_tag_next`/`all_tag_prev` list all tags on the current monitor, `all_next`/`all_prev` list all monitors and tags. Releasing any modifier key selects. |
| `exchange_client` | `left/right/up/down` | Swap window with neighbor in direction. |
| `exchange_stack_client` | `next/prev` | Exchange window position in stack. |
| `zoom` | - | Swap focused window with Master. |

### Group
| Command | Param | Description |
| :--- | :--- | :--- |
| `groupjoin` | `left/right/up/down`  | Join group by direction. |
| `groupfocus` | `prev/next`  | Focus group member by direction. |
| `groupleave` | -  | Leave group. |

### Tags & Monitors

| Command | Param | Description |
| :--- | :--- | :--- |
| `view` | `mask[,synctag]` | View tag(s). Accepts a [tag mask](/docs/bindings/keys#tag-mask-format). Additionally, `0` shows all tags, `-1` shows the previous tagset. Optional `synctag` (0/1) syncs the action to all monitors. |
| `viewtoleft` | `[synctag]` | View previous tag. Optional `synctag` (0/1) syncs to all monitors. |
| `viewtoright` | `[synctag]` | View next tag. Optional `synctag` (0/1) syncs to all monitors. |
| `view_insert` | `prev`/`next` | View the adjacent tag if it is empty; otherwise insert an empty tag before/after the current one and switch to it. |
| `viewtoleft_have_client` | `[synctag]` | View left tag and focus client if present. Optional `synctag` (0/1). |
| `viewtoright_have_client` | `[synctag]` | View right tag and focus client if present. Optional `synctag` (0/1). |
| `viewcrossmon` | `mask,monitor_spec` | View specified tag(s) on specified monitor. Accepts a [tag mask](/docs/bindings/keys#tag-mask-format) and a [monitor spec](/docs/configuration/monitors#monitor-spec-format). |
| `tag` | `mask[,synctag]` | Move window to tag(s). Accepts a [tag mask](/docs/bindings/keys#tag-mask-format). Optional `synctag` (0/1) syncs to all monitors. |
| `tagsilent` | `mask` | Move window to tag(s) without focusing it. Accepts a [tag mask](/docs/bindings/keys#tag-mask-format). |
| `tagtoleft` | `[synctag]` | Move window to left tag. Optional `synctag` (0/1). |
| `tagtoright` | `[synctag]` | Move window to right tag. Optional `synctag` (0/1). |
| `tagcrossmon` | `mask,monitor_spec` | Move window to tag(s) on specified monitor. Accepts a [tag mask](/docs/bindings/keys#tag-mask-format) and a [monitor spec](/docs/configuration/monitors#monitor-spec-format). |
| `toggletag` | `mask` | Toggle tag(s) on window. Accepts a [tag mask](/docs/bindings/keys#tag-mask-format). `0` toggles all tags. |
| `toggleview` | `mask` | Toggle view of tag(s). Accepts a [tag mask](/docs/bindings/keys#tag-mask-format). |
| `comboview` | `mask` | View multiple tags simultaneously. Accepts a [tag mask](/docs/bindings/keys#tag-mask-format) (typically built by pressing keys, e.g., `1|3`). |
| `focusmon` | `left/right/up/down/monitor_spec` | Focus monitor by direction or [monitor spec](/docs/configuration/monitors#monitor-spec-format). |
| `tagmon` | `left/right/up/down/monitor_spec,[keeptag]` | Move window to monitor by direction or [monitor spec](/docs/configuration/monitors#monitor-spec-format). `keeptag` is 0 or 1. |

#### Tag Mask Format

A tag mask specifies one or more tags for commands that operate on tags.  
It is formed by tag numbers `1`–`9`, optionally combined with `|`.

- `3` – single tag 3
- `1|3|5` – tags 1, 3, and 5

### Layouts

| Command | Param | Description |
| :--- | :--- | :--- |
| `setlayout` | `name` | Switch to layout (e.g., `scroller`, `tile`). |
| `switch_layout` | - | Cycle through available layouts. |
| `incnmaster` | `+1/-1` | Increase/Decrease number of master windows. |
| `setmfact` | `+0.05` | Increase/Decrease master area size. |
| `set_proportion` | `float` | Set scroller window proportion (0.0–1.0). |
| `switch_proportion_preset` | - | Cycle proportion presets of scroller window. |
| `scroller_stack` | `left/right/up/down` | Move window inside/outside scroller stack by direction. |
| `incgaps` | `+/-value` | Adjust gap size. |
| `togglegaps` | - | Toggle gaps. |
|  `dwindle_toggle_split_direction` | - | Toggle split direction in dwindle layout. |
| `dwindle_split_horizontal` | - | Set split window direction to horizontal in dwindle layout. |
| `dwindle_split_vertical` | - | Set split window direction to vertical in dwindle layout. |
| `dwindle_toggle_current_split` | - | Toggle split direction of current window in dwindle layout. |

### System

| Command | Param | Description |
| :--- | :--- | :--- |
| `spawn` | `cmd` | Execute a command. |
| `spawn_shell` | `cmd` | Execute shell command (supports pipes `\|`). |
| `spawn_on_empty` | `cmd, tagmask` | Open command on empty tag.Accepts a cmd string and [tagmask](/docs/bindings/keys#tag-mask-format) |
| `reload_config` | - | Hot-reload configuration. |
| `load_config_file` | `file path` | Load configuration from the specified file. Empty path resets to default config location. |
| `quit` | - | Exit mangowm. |
| `toggleoverview` | - | Toggle overview mode. |
| `togglejump` | - | Toggle overview with jump mode. |
| `create_virtual_output` | - | Create a headless monitor (for VNC/Sunshine). |
| `destroy_all_virtual_output` | - | Destroy all virtual monitors. |
| `toggleoverlay` | - | Toggle overlay state for the focused window. |
| `toggle_trackpad_enable` | - | Toggle trackpad enable. |
| `setkeymode` | `mode` | Set keymode. |
| `switch_keyboard_layout` | `[index]` | Switch keyboard layout. Optional index (0, 1, 2...) to switch to specific layout. |
| `setoption` | `key,value` | Set config option temporarily. |
| `sleep_monitor` | `monitor_spec` | Shutdown monitor power but not remove. Accepts a [monitor spec](/docs/configuration/monitors#monitor-spec-format). |
| `wakeup_monitor` | `monitor_spec` | Turn on monitor power. Accepts a [monitor spec](/docs/configuration/monitors#monitor-spec-format). |
| `sleep_toggle_monitor` | `monitor_spec` | Toggle monitor power but not remove. Accepts a [monitor spec](/docs/configuration/monitors#monitor-spec-format).
| `disable_monitor` | `monitor_spec` | remove monitor. Accepts a [monitor spec](/docs/configuration/monitors#monitor-spec-format). |
| `enable_monitor` | `monitor_spec` | add monitor. Accepts a [monitor spec](/docs/configuration/monitors#monitor-spec-format). |
| `toggle_monitor` | `monitor_spec` | Toggle monitor add/remove. Accepts a [monitor spec](/docs/configuration/monitors#monitor-spec-format). |

### Media Controls

> **Warning:** Some keyboards don't send standard media keys. Run `wev` and press your key to check the exact key name.

#### Brightness

Requires: `brightnessctl`

```ini
bind=NONE,XF86MonBrightnessUp,spawn,brightnessctl s +2%
bind=SHIFT,XF86MonBrightnessUp,spawn,brightnessctl s 100%
bind=NONE,XF86MonBrightnessDown,spawn,brightnessctl s 2%-
bind=SHIFT,XF86MonBrightnessDown,spawn,brightnessctl s 1%
```

#### Volume

Requires: `wpctl` (WirePlumber)

```ini
bind=NONE,XF86AudioRaiseVolume,spawn,wpctl set-volume @DEFAULT_SINK@ 5%+
bind=NONE,XF86AudioLowerVolume,spawn,wpctl set-volume @DEFAULT_SINK@ 5%-
bind=NONE,XF86AudioMute,spawn,wpctl set-mute @DEFAULT_SINK@ toggle
bind=SHIFT,XF86AudioMute,spawn,wpctl set-mute @DEFAULT_SOURCE@ toggle
```

#### Playback

Requires: `playerctl`

```ini
bind=NONE,XF86AudioNext,spawn,playerctl next
bind=NONE,XF86AudioPrev,spawn,playerctl previous
bind=NONE,XF86AudioPlay,spawn,playerctl play-pause
```

### Floating Window Movement

| Command | Param | Description |
| :--- | :--- | :--- |
| `smartmovewin` | `left/right/up/down` | Move floating window by snap distance. |
| `smartresizewin` | `left/right/up/down` | Resize floating window by snap distance. |
| `movewin` | `(x,y)` | Move floating window. |
| `resizewin` | `(width,height)` | Resize window. |
