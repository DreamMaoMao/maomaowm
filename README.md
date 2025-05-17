# Maomaowm

This project is developed based on [dwl](https://codeberg.org/dwl/dwl/),

"Since many people have asked about the meaning of this compositor's name,  'Maomao' is an online alias I've been using for years - it comes from the first two characters of the Chinese word for 'caterpillar' (毛毛虫). You can basically think of it as meaning 'caterpillar'.

1. **Lightweight & Fast Build**  
   - *Maomao* is as lightweight as *dwl*, and its build can be completed within few seconds. Despite this, *maomao* does not compromise on functionality.  

2. **Feature Integration**  
   - In addition to inheriting *dwl*'s core features and common functions found in other compositors, *maomao* also supports unique functionalities from other projects. These include:  
     - Hyprland’s *animations*,  
     - Sway’s *additional protocols*,  
     - Labwc’s *text input method*,  
     - Niri’s *scrolling layout*.  
   - However, these are implemented in a deliberately lightweight and simplistic manner to avoid bloated code.  

3. **Philosophy**  
   - *Maomao* has no rigid design ideology—you could call it an eclectic hybrid of various compositors.

Master-Stack Layout

https://github.com/user-attachments/assets/a9d4776e-b50b-48fb-94ce-651d8a749b8a

Scroller Layout

https://github.com/user-attachments/assets/c9bf9415-fad1-4400-bcdc-3ad2d76de85a


# maomao based on tags instead of workspace, so it support separate window layout for each tags, with separate tags parameters

## support layout

- tile
- scroller
- monocle
- grid
- dwindle
- spiral

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
yay -S glibc wayland libinput libdrm pixman libxkbcommon git meson ninja wayland-protocols libdisplay-info libliftoff hwdata seatd pcre2
```

## arch
```bash
yay -S maomaowm-git

```

## other
```bash
yay -S wlroots-0.19-git
git clone https://github.com/DreamMaoMao/maomaowm.git
cd maomaowm
meson build -Dprefix=/usr
sudo ninja -C build install
```

## suggest tools

```
yay -S rofi foot xdg-desktop-portal-wlr swaybg waybar wl-clip-persist cliphist wl-clipboard wlsunset polkit-gnome swaync

```

## Some common default key bindings
- alt+return: open foot terminal
- alt+q: kill client
- alt+left/right/up/down: focus direction
- super+m: quit maomao

## My dotfile
- depend
```
yay -S pamixer lavalauncher-mao-git wlr-dpms sway-audio-idle-inhibit-git swayidle dimland-git brightnessctl swayosd wlr-randr grim slurp satty swaylock-effects-git wlogout eww
```
### maomao config
[maomao-config](https://github.com/DreamMaoMao/dotfile/tree/main/maomao)
#### other
[foot](https://github.com/DreamMaoMao/dotfile/tree/main/foot)
[swaylock](https://github.com/DreamMaoMao/dotfile/tree/main/swaylock)
[wlogout](https://github.com/DreamMaoMao/dotfile/tree/main/wlogout)
[swaync](https://github.com/DreamMaoMao/dotfile/tree/main/swaync)
[eww](https://github.com/DreamMaoMao/dotfile/tree/main/eww)

## Config document
refer to [wiki](https://github.com/DreamMaoMao/maomaowm/wiki/)


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

# thanks for some refer repo

- https://gitlab.freedesktop.org/wlroots/wlroots - implementation of wayland protocol

- https://github.com/dqrk0jeste/owl - basal window animaition

- https://codeberg.org/dwl/dwl - basal dwl feature

- https://github.com/labwc/labwc - sample of text-input protocol

- https://github.com/swaywm/sway - sample of wayland protocol
