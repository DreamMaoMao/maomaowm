#

Master-Stack Layout

https://github.com/user-attachments/assets/a9d4776e-b50b-48fb-94ce-651d8a749b8a

Scroller Layout

https://github.com/user-attachments/assets/c9bf9415-fad1-4400-bcdc-3ad2d76de85a

# Maomaowm

This project is developed based on [dwl](https://codeberg.org/dwl/dwl/),
it is basically compatible with all `dwm` features.
In addition, it is added many operation that supported in `hyprland`, such as `smooth and customizable animation`、`External configuration that can be hot overloaded`,`pin mode`,`maximize window` etc...

See below for more features.

# separate window layout for each workspace(tags), with separate workspace parameters

## support layout

- tile
- scroller
- monocle
- grid
- dwindle
- spiral

# window open rules

## options

- appid: type-string if match it or title, the rule match
- title: type-string if match it or appid, the rule match
- tags: type-num(1-9) which tags to open the window
- isfloating: type-num(0 or 1)
- isfullscreen: type-num(0 or 1)
- scroller_proportion: type-float(0.1-1.0)
- animation_type_open : type-string(zoom,slide)
- animation_type_close : type-string(zoom,slide)
- isnoborder : type-num(0 or 1)
- monitor : type-num(0-99999)
- width : type-num(0-9999)
- height : type-num(0-9999)
- isterm : type-num(0 or 1) it will be swallowed by the sub window
- noswallow: type-num(0 or 1) don't swallow the isterm window
- globalkeybinding: type-string(for example-- alt-l or alt+super-l)

# some special feature

- hycov like overview
- foreign-toplevel protocol(dunst,waybar wlr taskbar)
- minimize window to waybar(like hych)
- sway scratchpad (minimize window to scratchpad)
- window pin mode/ maximize mode
- text-input-v2/v3 protocol for fcitx5
- window move/open/close animaition
- workspaces(tags) switch animaition
- fade/fadeout animation
- alt-tab switch window like gnome
- niri like scroller layout

# install

## depend

```bash
yay -S glibc wayland libinput libdrm pixman libxkbcommon git meson ninja wayland-protocols
```

## stable - wlroots(0.17)

Since 0.18 has some bugs that affect the experience,
I chose the more stable 0.17.4.

- Arch

```
yay -S maomaowm-git
```

- Other

```
git clone -b 0.17.4 https://gitlab.freedesktop.org/wlroots/wlroots.git
cd wlroots
meson build -Dprefix=/usr
sudo ninja -C build install

git clone https://github.com/DreamMaoMao/maomaowm.git
cd maomaowm
meson build -Dprefix=/usr
sudo ninja -C build install

```

## unstable wlroots(0.19)

This is the latest version of wlroots, it doesn't own enough test.
you can try it out if you want to help to test.

```
yay -S wlroots-git
git clone -b wlroots-0.19 https://github.com/DreamMaoMao/maomaowm.git
cd maomaowm
meson build -Dprefix=/usr
sudo ninja -C build install
```

## suggest tools

```
yay -S rofi foot xdg-desktop-portal-wlr swaybg waybar wl-clip-persist cliphist wl-clipboard wlsunset polkit-gnome

```

# config

```
cp /etc/maomao/config.conf ~/.config/maomao/config.conf
touch ~/.config/maomao/autostart.sh
chmod +x ~/.config/maomao/autostart.sh
```

you can use `MAOMAOCONFIG` env to set the config-folder-path and the autostart-folder-patch
like `MAOMAOCONFIG=/home/xxx/maomao`

- the only default keybinds is ctrl+alt+[F1-F12] to change tty

- the default config path is `~/.config/maomao/config.conf`

- the default autostart path is `~/.config/maomao/autostart.sh`

- the fallback config path is in `/etc/maomao/config.conf`, you can find the default config here

# keybinds notice

All mod keys(alt,ctrl,shift,super) are case insensitive, in addition to other key names are case sensitive, the name follows the xkb standard name, you can use the `xev` command to get the key name of the key you want, note that if your mod key contains the shift key, then it may not be the key name displayed on the keyboard. Real name Refer to the name displayed in the `xev` command.

for example:

### this is wrong:

```
bind=alt+shift,2,quit
```

### this is right:

```
bind=alt+shift,at,quit
```

because your keybinds contain shift, the `2` cover to `at`
![swappy-20250227-182157](https://github.com/user-attachments/assets/c4bca146-d1d7-42b1-aea5-a7e7e19e874b)

# custom animation

```
animation_curve_open=0.46,1.0,0.29,1.1
animation_curve_move=0.46,1.0,0.29,1
animation_curve_tag=0.46,1.0,0.29,1
animation_curve_close=0.46,1.0,0.29,1

```

You can design your animaition curve in:
[here, on cssportal.com](https://www.cssportal.com/css-cubic-bezier-generator/),

or you can just choice a curve in:
[easings.net](https://easings.net).

# overview mode

```
hotarea_size=10
enable_hotarea=1
ov_tab_mode=0
```

- enable_hotarea: when your cursor enter the bottom left corner of monitor, it will toggle overview.
- hotarea_size: the size of hotarea, 10x10 default.
- ov_tab_mode:
  - it will circle switch focus when you toggle overview.
  - and will leave ov mode when you release your mod key.

### notice

when you in ov mode, you can use right mouse button to close window, and left mouse button to jump to a window.

# About waybar

- you can also use the dwl moudle in waybar to show tags and window title
  refer to waybar wiki: [dwl-module](https://github.com/Alexays/Waybar/wiki/Module:-Dwl)

```json
"modules-left": ["dwl/tags","dwl/window"],
"dwl/tags": {
    "num-tags":9,
},

```

```css
#tags {
  background-color: transparent;
}

#tags button {
  background-color: #fff;
  color: #a585cd;
}

#tags button.occupied {
  background-color: #fff;
  color: #cdc885;
}

#tags button.focused {
  background-color: rgb(186, 142, 213);
  color: #fff;
}

#tags button.urgent {
  background: rgb(171, 101, 101);
  color: #fff;
}

#window {
  background-color: rgb(237, 196, 147);
  color: rgb(63, 37, 5);
}

window#waybar.empty #window {
  background-color: transparent;
  color: transparent;
  border-bottom: none;
  box-shadow: none;
  padding-right: 0px;
  padding-left: 0px;
  margin-left: 0px;
  margin-right: 0px;
}
```

# ipc

refer to [ipc](https://github.com/DreamMaoMao/mmsg)

# NixOS+Home-manager

The repo contains a flake that provides a NixOS module and a home-manager module for maomaowm.
Use the NixOS module to install maomaowm with other necessary components of a working wayland environment.
Use the home-manager module to declare configuration and autostart for maomaowm.

Here's an example of using the modules in a flake:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    home-manager = {
      url = "github:nix-community/home-manager";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    flake-parts.url = "github:hercules-ci/flake-parts";
    maomaowm.url = "github:DreamMaoMao/maomaowm";
  };
  outputs =
    inputs@{ self, flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      debug = true;
      systems = [ "x86_64-linux" ];
      flake = {
        nixosConfigurations = {
          hostname = inputs.nixpkgs.lib.nixosSystem {
            system = "x86_64-linux";
            modules = [
              inputs.home-manager.nixosModules.home-manager

              # Add maomaowm nixos module
              inputs.maomaowm.nixosModules.maomaowm
              {
                programs.maomaowm.enable = true;

                # mmsg is the ipc for maomamwm and is enabled by default
                programs.maomaowm.mmsg.enable = true;
              }
              {
                home-manager = {
                  useGlobalPkgs = true;
                  useUserPackages = true;
                  backupFileExtension = "backup";
                  users."username".imports =
                    [
                      (
                        { ... }:
                        {
                          wayland.windowManager.maomaowm = {
                            enable = true;
                            settings = ''
                              # see config.conf
                            '';
                            autostart_sh = ''
                              # see autostart.sh
                              # Note: here no need to add shebang
                            '';
                          };
                        }
                      )
                    ]
                    ++ [
                      # Add maomaowm hm module
                      inputs.maomaowm.hmModules.maomaowm
                    ];
                };
              }
            ];
          };
        };
      };
    };
}
```

# my dotfile

[maomao-config](https://github.com/DreamMaoMao/dotfile/tree/main/maomao)

# thanks for some refer repo

- https://gitlab.freedesktop.org/wlroots/wlroots - implementation of wayland protocol

- https://github.com/dqrk0jeste/owl - basal window animaition

- https://codeberg.org/dwl/dwl - basal dwl feature

- https://github.com/guyuming76/dwl - sample of text-input protocol

- https://github.com/swaywm/sway - sample of wayland protocol
