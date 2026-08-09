---
title: Status Bar
description: Configure mangobar and Waybar for mangowm.
---

## Recommended: mangobar

We recommend [mangobar](https://github.com/mangowm/mangobar), a dedicated status bar for mangowm built on `wlr-layer-shell`. It integrates directly with mangowm over IPC, so it stays in sync with your tags, layouts and windows. It ships with built-in modules for workspaces, layout, window title, keymode, keyboard layout, CPU/memory, brightness, volume, clock, network, battery, system tray, and user-defined `custom/<name>` modules.

### Installation

On Arch Linux, install the AUR package [`mangobar-git`](https://aur.archlinux.org/packages/mangobar-git):

```sh
yay -S mangobar-git
```

Or build from source:

```sh
git clone https://github.com/mangowm/mangobar.git
cd mangobar
meson setup build -Dprefix=/usr
ninja -C build
sudo ninja -C build install
```

### Usage

Start `mangobar` in your mangowm configuration:

```ini
exec-once=mangobar
```

It reads its configuration from `$MANGOBAR_CONFIG` or `~/.config/mangobar/config.jsonc`, and its styling from `~/.config/mangobar/style.css`. See the [mangobar repository](https://github.com/mangowm/mangobar) for a complete reference and example configuration.

---

## Waybar Module Configuration

### `config.jsonc`

Add the following to your Waybar configuration:

```jsonc
{
  "modules-left": [
    "mango/workspaces",
    "mango/layout",
    "mango/window"
  ],
  "modules-right": [
    "mango/language",
    "mango/keymode",
  ],
  "mango/workspaces": {
      "format": "{icon}",
      "hide-empty": true,
      "on-click": "activate",
      "on-click-right": "toggle",
      "overview-label": "OVERVIEW",
  },
  "mango/keymode": {
  	"format": "{}",
  	// "format-default": " Default",
    // "format-test": " Test",
  },
  "mango/window": {
    "format": "{}",
	  "icon-size": 20
  },
  "mango/layout": {
      "format": "{}",
      // "format-S": "Scroller",
      // "format-T": "Tile",
  },
  "mango/language": {
  "format": "{short}",
  },
}
```

## Styling Example

You can style the tags using standard CSS in `style.css`.

### `style.css`

```css
#workspaces {
  border-color: #c9b890;
  background: rgba(40, 40, 40, 0.76);
}

#workspaces button {
  background: none;
  color: #ddca9e;
}

#workspaces button.hidden {
  color: #9e906f;
  background-color: transparent;
}

#workspaces button.visible {
  color: #ddca9e;
}

#workspaces button:hover {
  color: #d79921;
}

#workspaces button.active {
  background-color: #ddca9e;
  color: #282828;
}

#workspaces button.urgent {
  background-color: #ef5e5e;
  color: #282828;
}

#workspaces button.overview {
  background-color: #ef5e5e;
  color: #282828;
}

#window {
  background-color: #CA9297;
  color: #282828;
}

window#waybar.empty #window {
    background: none;
    margin: 0px;
    padding: 0px;
}

#layout {
  background-color: #CA9297;
  color: #282828;
}

#language {
  background-color: #CA9297;
  color: #282828;
}

#keymode {
  background-color: #CA9297;
  color: #282828;
}

```

## Complete Configuration Example

> **Tip:** You can find a complete Waybar configuration for mangowm at [waybar-config](https://github.com/DreamMaoMao/waybar-config).
