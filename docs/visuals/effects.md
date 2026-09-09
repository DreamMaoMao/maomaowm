---
title: Window Effects
description: Add visual polish with blur, shadows, and opacity.
---

## Blur

Blur creates a frosted glass effect for transparent windows.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `blur` | `0` | Enable blur for windows. |
| `blur_layer` | `0` | Enable blur for layer surfaces (like bars/docks). |
| `blur_optimized` | `1` | Caches the wallpaper and blur background, significantly reducing GPU usage. Disabling it will significantly increase GPU consumption and may cause rendering lag. **Highly recommended.** |
| `blur_params_radius` | `5` | The strength (radius) of the blur. |
| `blur_params_num_passes` | `1` | Number of passes. Higher = smoother but more expensive. |
| `blur_params_noise` | `0.02` | Blur noise level. |
| `blur_params_brightness` | `0.9` | Blur brightness adjustment. |
| `blur_params_contrast` | `0.9` | Blur contrast adjustment. |
| `blur_params_saturation` | `1.2` | Blur saturation adjustment. |

> **Warning:** Blur has a relatively high impact on performance. If your hardware is limited, it is not recommended to enable it. If you experience lag with blur on, ensure `blur_optimized=1` — disabling it will significantly increase GPU consumption and may cause rendering lag. To disable blur entirely, set `blur=0`.

---

## Shadows

Drop shadows help distinguish floating windows from the background.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `shadows` | `0` | Enable shadows. |
| `layer_shadows` | `0` | Enable shadows for layer surfaces. |
| `shadow_only_floating` | `1` | Only draw shadows for floating windows (saves performance). |
| `shadows_size` | `10` | Size of the shadow. |
| `shadows_blur` | `15` | Shadow blur amount. |
| `shadows_position_x` | `0` | Shadow X offset. |
| `shadows_position_y` | `0` | Shadow Y offset. |
| `shadowscolor` | `0x000000ff` | Color of the shadow. |

```ini
# Example shadows configuration
shadows=1
layer_shadows=1
shadow_only_floating=1
shadows_size=12
shadows_blur=15
shadows_position_x=0
shadows_position_y=0
shadowscolor=0x000000ff
```

---

## Opacity & Corner Radius

Control the transparency and roundness of your windows.

| Setting | Default | Description |
| :--- | :--- | :--- |
| `border_radius` | `0` | Window corner radius in pixels. |
| `border_radius_location_default` | `0` | Corner radius location: `0` (all), `1` (top-left), `2` (top-right), `3` (bottom-left), `4` (bottom-right), `5` (closest corner). |
| `no_radius_when_single` | `0` | Disable radius if only one window is visible. |
| `focused_opacity` | `1.0` | Opacity for the active window (0.0 - 1.0). |
| `unfocused_opacity` | `1.0` | Opacity for inactive windows (0.0 - 1.0). |

```ini
# Window corner radius in pixels
border_radius=0

# Corner radius location (0=all, 1=top-left, 2=top-right, 3=bottom-left, 4=bottom-right)
border_radius_location_default=0

# Disable radius if only one window is visible
no_radius_when_single=0

# Opacity for the active window (0.0 - 1.0)
focused_opacity=1.0

# Opacity for inactive windows
unfocused_opacity=1.0
```
---

## Border Textures
> **Note:** Depending on the settings you apply, border textures can have a significant impact on mango's RAM and CPU usage, measured impact in power-user cases is a doubling of both RAM and CPU usage compared to not using the feature at all.

Mango supports various ways of texturing your borders through Cairo software rendering, the following options are available:

| Setting | Default | Description |
| :--- | :--- | :--- |
|active_texture(_top/_mid/_bot) | - | sets the active texture (type,opts) |
|inactive_texture(_top/_mid/_bot) | - | sets the inactive texture (type,opts) |

### Window rules & dispatchers

Textures can be assigned per-window with window rules, or swapped at runtime with the `set_active_texture`, `set_inactive_texture`, and `rerender_texture` dispatchers.

there are 6 total texture slots for each window, a top, middle and bottom slot, these are composited over top of eachother and merged together as two borders (active and inactive).

you can set each slot individually, allowing you to for example have a gradient, a solid color over the whole window (for dimming) and a texture with partial transparency to show the gradient through. 

the slot is entirely optional both in the settings/windowrules (just leave `_top`/`_mid`/`_bot` out), as well as in the dispatcher.
in the case of leaving out the slot, the `top` slot is used. 

for the dispatcher, slot names are `top`, `mid` and `bot`

- Window-rule texture values contain commas, so they absorb the rest of the rule. They must be the **last item** in the rule, and only one texture option per rule is allowed — use two separate rules to set both an active and an inactive texture.
- When using a dispatcher through `mmsg dispatch`, pipes (`|`) must be escaped with a backslash (`\`).
```sh
mmsg dispatch set_active_texture,linear_gradient,FF0000FF\|0000FFFF\|55 
```

### Renderers

Multiple types of renderers are available to use:

| Renderer Type | Option Format | Example | Description |
| :--- | :--- | :--- | :--- |
| `linear_gradient` | `RRGGBBAA|RRGGBBAA|(0.0-360.0)` | `FF0000FF|0000FFFF|45` | A linear edge-to-edge gradient between two colors, drawn at the given angle (degrees). |
| `radial_gradient` | `RRGGBBAA|RRGGBBAA|(0.0-2.0)` | `FF0000FF|0000FFFF|1` | A radial gradient that starts at the center of the window and expands outward, scaled by the given factor. |
| `conic_gradient` | `RRGGBBAA:(0.0-360.0)|...` | `FF0000FF:45|FF00FFFF:135|0000FFFF:225|00FF00FF:315` | A pseudo-conic gradient that cycles through any number of color stops at the given degrees. |
| `static_image` | `/path/to/image.png` | `/home/MyUser/pictures/sky.png` | Uses the image directly as the border source, centered without resizing. |
| `tiled_image` | `/path/to/image.png` | `/home/MyUser/pictures/checkerboard.png` | Tiles the image across the border. |
| `fit_image` | `/path/to/image.png` | `/home/MyUser/pictures/gradient.png` | Stretches and squashes the image to fit the window. |
| `fit_overlay` | `/path/to/image.png:(0.0-1.0)` | `/home/MyUser/pictures/overlay.png:0.5` | fits the image, does not clip to border. allows you to overlay images on your window|
| `tile_overlay` | `/path/to/image.png:(0.0-1.0)` | `/home/MyUser/pictures/overlay.png:0.5` | tiles the image, does not clip to border. allows you to overlay images on your window|
| `segment_image` | `/path/to/image.png` | `/home/MyUser/pictures/ninepatch.png` | Decorates the border using ninepatch-style images, allowing overlap. |
| `segment_color` | `RRGGBBAA|RRGGBBAA|RRGGBBAA|RRGGBBAA` | `FF0000FF|00FF00FF|0000FFFF|FF00FFFF` | Sets each side of the border separately (top/right/bottom/left). |
| `solid_color` | `RRGGBBAA` | `FFAA00FF` | A flat, solid-color border. |
| `solid_color_noclip` | `RRGGBBAA` | `00000040` | Overlays a colored box over the entire window (use an alpha value to dim or colorize). |

> **Note:** The image-based renderers (`static_image`, `tiled_image`, `fit_image`, `tile_overlay`, `fit_overlay`, `segment_image`) only support **PNG** images.

### Renderer look

- **`linear_gradient`** — Paints a straight band across the border so the color transitions from the first value to the second, tilted by the angle you pick (e.g. `45` blends it diagonally).
- **`radial_gradient`** — Starts at the center of the window and fades outward, so the first color hugs the center and bleeds into the second at the edges.
- **`conic_gradient`** — Sweeps around the border in a circle, cycling through each color stop as it goes around, letting you build rainbow-style borders from multiple stops.
- **`static_image`** — Draws the image without scaling, so the portion of the image sitting under the border band is what shows; best large general texture images.
- **`tiled_image`** — Repeats the image across the border like wallpaper, good for repeating patterns.
- **`fit_image`** — Stretches the image to the full width and height of the border, so the tim of the picture is visible but gets squashed on squashed windows.
- **`fit_overlay`** — Renders exactly like `fit_image` but multiplies the entire canvas's alpha by the second value before drawing, letting you fade a fitted image;
- **`tile_overlay`** — Renders exactly like `tile_image` but multiplies the entire canvas's alpha by the second value before drawing, letting you fade a fitted image; unlike most renderers it keeps the center of the image instead of cutting out a border ring.
- **`segment_image`** — Keeps the four corners of the image intact and tiles the middle of each edge, like a nine-patch Android drawable, so texture is not warped.
- **`segment_color`** — Fills each side of the border individually (top, right, bottom, left) with its own solid color, meeting at the corners.
- **`solid_color`** — A single flat color across the border; the alpha channel of the hex makes it translucent.
- **`solid_color_noclip`** — Fills the whole window instead of just the border; with a translucent alpha it dims or tints everything underneath.


