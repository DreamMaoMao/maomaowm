---
title: Overview
description: Configure the overview mode for window navigation.
---

## Overview Settings

| Setting | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `hotarea_size` | integer | `10` | Hot area size in pixels. |
| `enable_hotarea` | integer | `0` | Enable hot areas (0: disable, 1: enable). |
| `hotarea_corner` | integer | `2` | Hot area corner (0: top-left, 1: top-right, 2: bottom-left, 3: bottom-right). |
| `overviewgappi` | integer | `5` | Inner gap in overview mode. |
| `overviewgappo` | integer | `30` | Outer gap in overview mode. |
| `overcircle_center_ratio` | float | `0.5` | Width ratio of the centered window in the `overcircle` layout (0.1–0.9). |
| `jump_labels` | string | `HJKLASDFGQWERTYUIOPZXCVBNM` | Character sequence used for jump hints in overview mode. |

### Setting Descriptions

- `enable_hotarea` — Toggles overview when the cursor enters the configured corner.
- `hotarea_size` — Size of the hot area trigger zone in pixels.
- `hotarea_corner` — Corner that triggers the hot area (0: top-left, 1: top-right, 2: bottom-left, 3: bottom-right).
- `jump_labels` — Defines the ordered characters used for jump hints when in overview jump mode. Each visible window is assigned a label in this order, and pressing the corresponding key jumps to that window. The number of labels limits how many windows can be assigned hints at once.

[`overcircle`](/docs/bindings/keys#focus--movement) opens overview; while
overview is open, each trigger cycles focus to the next window on the current
monitor. Release a modifier key or run `toggleoverview` to close it.

By default overview temporarily views every tag on the monitor. Use
`overcircle` with `current_next`/`current_prev`, or run `toggleoverview,1`, to
keep the overview restricted to the current tagset's windows.

### Mouse Interaction in Overview

When in overview mode:

- **Left mouse button** — Jump to (focus) a window.
- **Right mouse button** — Close a window.
