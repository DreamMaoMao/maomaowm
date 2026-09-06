---
title: Rules
description: Define behavior for specific windows, tags, and layers.
---

## Window Rules

Window rules allow you to set specific properties (floating, opacity, size, animations, etc.) for applications based on their `appid` or `title`. You can set all parameters in one line, and if you both set appid and title, the window will only follow the rules when appid and title both match.

**Format:**

```ini
windowrule=Parameter:Values,title:Values
windowrule=Parameter:Values,Parameter:Values,appid:Values,title:Values
```

### State & Behavior Parameters

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `appid` | string | Any | Match by application ID, supports regex |
| `title` | string | Any | Match by window title, supports regex |
| `isfloating` | integer | `0` / `1` | Force floating state |
| `isfullscreen` | integer | `0` / `1` | Force fullscreen state |
| `isfakefullscreen` | integer | `0` / `1` | Force fake-fullscreen state (window stays constrained) |
| `isglobal` | integer | `0` / `1` | Open as global window (sticky across tags) |
| `isoverlay` | integer | `0` / `1` | Make it always in top layer |
| `isopensilent` | integer | `0` / `1` | Open without focus |
| `istagsilent` | integer | `0` / `1` | Don't focus if client is not in current view tag |
| `force_fakemaximize` | integer | `0` / `1` (default 1) | The state of client set to fake maximized |
| `ignore_maximize` | integer | `0` / `1` (default 1) | Don't handle maximize request from client |
| `ignore_minimize` | integer | `0` / `1` (default 1) | Don't handle minimize request from client |
| `force_tiled_state` | integer | `0` / `1` | Deceive the window into thinking it is tiling, so it better adheres to assigned dimensions |
| `noopenmaximized` | integer | `0` / `1` | Window does not open as maximized mode |
| `single_scratchpad` | integer | `0` / `1` (default 1) | Only show one out of named scratchpads or the normal scratchpad |
| `allow_shortcuts_inhibit` | integer | `0` / `1` (default 1) | Allow shortcuts to be inhibited by clients |
| `idleinhibit_when_focus` | integer | `0` / `1` (default 0) | Automatically keep idle inhibit active when this window is focused |
| `vrr_only_fullscreen` | integer | `0` / `1` (default 0) | VRR only fullscreen,you need to turn `vrr` to `0` in monitor rule first |
| `shield_when_capture` | integer | `0` / `1` | Shield window when captured |
| `force_render` | integer | `0` / `1` | Force render frame even if the window is not visible |
| `activation_bypass` | integer | `0` / `1` | Bypass xdg-activation authentication: activation requests for this window are treated as authenticated, so the normal activation behavior applies regardless of token validity |


### Geometry & Position

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `width` | float | 0-9999 | Window width when it becomes a floating window,if the value below 1, it will be the percentage of the screen width,otherwise it will be the pixel value |
| `height` | float | 0-9999 | Window height when it becomes a floating window,if the value below 1, it will be the percentage of the screen height,otherwise it will be the pixel value |
| `offsetx` | integer | -999-999 | X offset from center (%), 100 is the edge of screen with outer gap |
| `offsety` | integer | -999-999 | Y offset from center (%), 100 is the edge of screen with outer gap |
| `monitor` | string | Any | Assign to monitor by [monitor spec](/docs/configuration/monitors#monitor-spec-format) (name, make, model, or serial) |
| `tags` | mask | `0-9` / `1\|3\|5` | Assign to specific one tag (use `0` for special workspace overlay) or multiple tags (use `\|` to split multiple tags) |
| `no_force_center` | integer | `0` / `1` | Window does not force center |
| `isnosizehint` | integer | `0` / `1` | Don't use min size and max size for size hints |

### Visuals & Decoration

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `noblur` | integer | `0` / `1` | Window does not have blur effect |
| `isnoborder` | integer | `0` / `1` | Remove window border |
| `isnoshadow` | integer | `0` / `1` | Not apply shadow |
| `isnoradius` | integer | `0` / `1` | Not apply corner radius |
| `focus_color` | color | `0xRRGGBBAA` | overrides the focused border color |
| `border_color` | color | `0xRRGGBBAA` | overrides the unfocused border color |
| `borderpx` | integer | `0-999` | overrides the border thickness (-1 to reset to default) |
| `border_radius` | integer | `0-99` | overrides the border radius (-1 to reset to default) |
| `active_texture(_top/_mid/_bot)` | `texture` | see [border texture](/docs/visuals/effects#border-textures) | sets the focused border texture, 3 slots are available for layering, not setting a slot sets top |
| `inactive_texture(_top/_mid/_bot)` | `texture` | see [border texture](/docs/visuals/effects#border-textures) | sets the unfocused texture, 3 slots are available for layering, not setting a slot sets top |
| `isnoanimation` | integer | `0` / `1` | Not apply animation |
| `focused_opacity` | integer | `0` / `1` | Window focused opacity |
| `unfocused_opacity` | integer | `0` / `1` | Window unfocused opacity |
| `allow_csd` | integer | `0` / `1` | Allow client side decoration |

> **Tip:** For detailed visual effects configuration, see the [Window Effects](/docs/visuals/effects) page for blur, shadows, and opacity settings.

### Textures

Window rules support assigning a [border texture](/docs/visuals/effects#border-textures) with `active_texture` and `inactive_texture`, using the same format as the global settings.

> **Note:** Texture values contain commas, so they absorb the rest of the rule. A texture option must be the last item in the rule, and only one texture option per rule is allowed — use two separate rules to set both an active and an inactive texture.

```ini
windowrule=appid:foot,active_texture_top:solid_color,FFAA00FF
windowrule=appid:foot,inactive_texture_bot:radial_gradient,FF0000FF|0000FFFF|1
```

### Layout & Scroller

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `scroller_proportion` | float | 0.1-1.0 | Set scroller proportion |
| `scroller_proportion_single` | float | 0.1-1.0 | Set scroller auto adjust proportion when it is single window |

> **Tip:** For comprehensive layout configuration, see the [Layouts](/docs/window-management/layouts) page for all layout options and detailed settings.

### Animation

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `animation_type_open` | string | zoom, slide, fade, none | Set open animation |
| `animation_type_close` | string | zoom, slide, fade, none | Set close animation |
| `nofadein` | integer | `0` / `1` | Window ignores fade-in animation |
| `nofadeout` | integer | `0` / `1` | Window ignores fade-out animation |

> **Tip:** For detailed animation configuration, see the [Animations](/docs/visuals/animations) page for available types and settings.

### Terminal & Swallowing

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `isterm` | integer | `0` / `1` | A new GUI window will replace the isterm window when it is opened |
| `noswallow` | integer | `0` / `1` | The window will not replace the isterm window |

### Global & Special Windows

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `globalkeybinding` | string | `[mod combination][-][key]` | Global keybinding (only works for Wayland apps) |
| `isunglobal` | integer | `0` / `1` | Open as unmanaged global window (for desktop pets or camera windows) |
| `isnamedscratchpad` | integer | `0` / `1` | 0: disable, 1: named scratchpad |

> **Tip:** For scratchpad usage, see the [Scratchpad](/docs/window-management/scratchpad) page for detailed configuration examples.

### Performance & Tearing

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `force_tearing` | integer | `0` / `1` | Set window to tearing state, refer to [Tearing](/docs/configuration/monitors#tearing-game-mode) |

### Examples

```ini
# Set specific window size and position
windowrule=width:1000,height:900,appid:yesplaymusic,title:Demons

# Global keybindings for OBS Studio
windowrule=globalkeybinding:ctrl+alt-o,appid:com.obsproject.Studio
windowrule=globalkeybinding:ctrl+alt-n,appid:com.obsproject.Studio
windowrule=isopensilent:1,appid:com.obsproject.Studio

# Force tearing for games
windowrule=force_tearing:1,title:vkcube

# Skip xdg-activation authentication for this app
windowrule=activation_bypass:1,appid:org.example.App
windowrule=force_tearing:1,title:Counter-Strike 2

# Named scratchpad for file manager
windowrule=isnamedscratchpad:1,width:1280,height:800,appid:st-yazi

# Custom opacity for specific apps
windowrule=focused_opacity:0.8,appid:firefox
windowrule=unfocused_opacity:0.6,appid:foot

# Disable blur for selection tools
windowrule=noblur:1,appid:slurp

# Position windows relative to screen center
windowrule=offsetx:20,offsety:-30,width:800,height:600,appid:alacritty

# Send to specific tag and monitor
windowrule=tags:9,monitor:HDMI-A-1,appid:discord

# Terminal swallowdby setup
windowrule=isterm:1,appid:st
windowrule=noswallow:1,appid:foot

# Disable client-side decorations
windowrule=allow_csd:1,appid:firefox

# Unmanaged global window (desktop pets, camera)
windowrule=isunglobal:1,appid:cheese

# Named scratchpad toggle
bind=alt,h,toggle_named_scratchpad,st-yazi,none,st -c st-yazi -e yazi
```

---

## Tag Rules

You can set all parameters in one line. If only `id` is set, the rule is followed when the id matches. If any of `monitor_name`, `monitor_make`, `monitor_model`, or `monitor_serial` are set, the rule is followed only if **all** of the set monitor fields match.

> **Warning:** Layouts set in tag rules have a higher priority than monitor rule layouts.

**Format:**

```ini
tagrule=id:Values,Parameter:Values,Parameter:Values
tagrule=id:Values,monitor_name:eDP-1,Parameter:Values,Parameter:Values
tagrule=id:Values,monitor_make:xxx,monitor_model:xxx,Parameter:Values
tagrule=id:*,Parameter:Values
```

> **Tip:** See [Layouts](/docs/window-management/layouts#supported-layouts) for detailed descriptions of each layout type.

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `id` | integer / wildcard | 0-9 / `*` | Match by tag id, 0 means the ~0 tag. Use `*` to match all tags at once |
| `monitor_name` | string | monitor name | Match by monitor name |
| `monitor_make` | string | monitor make | Match by monitor manufacturer |
| `monitor_model` | string | monitor model | Match by monitor model |
| `monitor_serial` | string | monitor serial | Match by monitor serial number |
| `layout_name` | string | layout name | Layout name to set |
| `no_render_border` | integer | `0` / `1` | Disable render border |
| `open_as_floating` | integer | `0` / `1` | New open window will be floating|
| `no_hide` | integer | `0` / `1` | Not hide even if the tag is empty |
| `nmaster` | integer | 0, 99 | Number of master windows |
| `mfact` | float | 0.1–0.9 | Master area factor |
| `scroller_default_proportion` | float | 0.1-1.0 | Set scroller  default proportion. |
| `scroller_default_proportion_single` | float | 0.1-1.0 | Set scroller auto adjust proportion when it is single window(only apply when set `scroller_ignore_proportion_single` to `0`) |
| `scroller_ignore_proportion_single` | integer | `0` / `1` | Ignore scroller single proportion setting. |

### Examples

```ini
# Set layout for all tags at once (equivalent to the two rules below)
tagrule=id:*,layout_name:scroller

# Set layout for specific tags
tagrule=id:1,layout_name:scroller
tagrule=id:2,layout_name:scroller

# Limit to specific monitor
tagrule=id:1,monitor_name:eDP-1,layout_name:scroller
tagrule=id:2,monitor_name:eDP-1,layout_name:scroller

# Persistent tags (1-4) with layout assignment
tagrule=id:1,no_hide:1,layout_name:scroller
tagrule=id:2,no_hide:1,layout_name:scroller
tagrule=id:3,monitor_name:eDP-1,no_hide:1,layout_name:scroller
tagrule=id:4,monitor_name:eDP-1,no_hide:1,layout_name:scroller

# Advanced tag configuration with master layout settings
tagrule=id:5,layout_name:tile,nmaster:2,mfact:0.6
tagrule=id:6,monitor_name:HDMI-A-1,layout_name:monocle,no_render_border:1

# set scroller proportion for specific tag
tagrule=id:1,layout_name:scroller,scroller_default_proportion_single:0.5,scroller_ignore_proportion_single:0,scroller_default_proportion:0.9,monitor_name:HDMI-A-1

```

> **Tip:** For Waybar configuration with persistent tags, see [Status Bar](/docs/visuals/status-bar) documentation.

---

## Layer Rules

You can set all parameters in one line. Target "layer shell" surfaces like status bars (`waybar`), launchers (`rofi`), or notification daemons.

**Format:**

```ini
layerrule=layer_name:Values,Parameter:Values,Parameter:Values
```

> **Tip:** You can use `mmsg get last_open_surface` to get the last open layer name for debugging.

| Parameter | Type | Values | Description |
| :--- | :--- | :--- | :--- |
| `layer_name` | string | layer name | Match name of layer, supports regex |
| `animation_type_open` | string | slide, zoom, fade, none | Set open animation |
| `animation_type_close` | string | slide, zoom, fade, none | Set close animation |
| `noblur` | integer | `0` / `1` | Disable blur |
| `noanim` | integer | `0` / `1` | Disable layer animation |
| `noshadow` | integer | `0` / `1` | Disable layer shadow |
| `shield_when_capture`| integer | `0` / `1` | Shield layer when captured.(it is better to combination with `noanim:1`) |

> **Tip:** For animation types, see [Animations](/docs/visuals/animations#animation-types). For visual effects, see [Window Effects](/docs/visuals/effects).

### Examples

```ini
# No blur or animation for slurp selection layer (avoids occlusion and ghosting in screenshots)
layerrule=noanim:1,noblur:1,layer_name:selection

# Zoom animation for Rofi with multiple parameters
layerrule=animation_type_open:zoom,noanim:0,layer_name:rofi

# Disable animations and shadows for notification daemon
layerrule=noanim:1,noshadow:1,layer_name:swaync

# Multiple effects for launcher
layerrule=animation_type_open:slide,animation_type_close:fade,noblur:1,layer_name:wofi
```
