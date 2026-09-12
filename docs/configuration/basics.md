---
title: Basic Configuration
description: Learn how to configure mangowm files, environment variables, and autostart scripts.
---

## Configuration File

mangowm uses a simple configuration file format. By default, it looks for a configuration file in `~/.config/mango/`.

1. **Locate Default Config**

   A fallback configuration is provided at `/etc/mango/config.conf`. You can use this as a reference.

2. **Create User Config**

   Copy the default config to your local config directory to start customizing.

   ```bash
   mkdir -p ~/.config/mango
   cp /etc/mango/config.conf ~/.config/mango/config.conf
   ```

3. **Launch with Custom Config (Optional)**

   If you prefer to keep your config elsewhere, you can launch mango with the `-c` flag.

   ```bash
   mango -c /path/to/your_config.conf
   ```

### Sub-Configuration

To keep your configuration organized, you can split it into multiple files and include them using the `source` keyword.

```ini
# Import keybindings from a separate file
source=~/.config/mango/bind.conf

# Relative paths work too
source=./theme.conf

# Optional: ignore if file doesn't exist (useful for shared configs)
source-optional=~/.config/mango/optional.conf
```

### Validate Configuration

You can check your configuration for errors without starting mangowm:

```bash
mango -c /path/to/config.conf -p
```

Use with `source-optional` for shared configs across different setups.

## Environment Variables

You can define environment variables directly within your config file. These are set before the window manager fully initializes.

> **Warning:** Environment variables defined here will be **reset** every time you reload the configuration.

```ini
env=QT_IM_MODULES,wayland;fcitx
env=XMODIFIERS,@im=fcitx
```

## Configuration Variables

Define reusable values once and reference them anywhere in the config with `{name}`. This is handy for terminals, browsers, and other commands you bind more than once.

> **Note:** A variable must be **defined before** you use it, and definitions are **reset** every time you reload the configuration. Undefined `{name}` references are left as-is.

```ini
var=terminal,ghostty
var=browser,zen-browser

bind=SUPER,Return,spawn,{terminal}
bind=SUPER,B,spawn,{browser}
```

The value may itself contain commas (everything after the first comma is kept), so commands with arguments work too:

```ini
var=run,foot --alpha 90%
bind=SUPER,Return,spawn,{run}
```

## Autostart

mangowm can automatically run commands or scripts upon startup. There are two modes for execution:

| Command | Behavior | Usage Case |
| :--- | :--- | :--- |
| `exec-once` | Runs **only once** when mangowm starts. | Status bars, Wallpapers, Notification daemons |
| `exec` | Runs **every time** the config is reloaded. | Scripts that need to refresh settings |

### Example Setup

```ini
# Start the status bar once
exec-once=waybar

# Set wallpaper
exec-once=swaybg -i ~/.config/mango/wallpaper/room.png

# Reload a custom script on config change
exec=bash ~/.config/mango/reload-settings.sh
```
