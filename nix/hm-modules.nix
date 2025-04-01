self:
{
  lib,
  config,
  pkgs,
  ...
}:
let
  inherit (self.packages.${pkgs.system}) maomaowm;
  cfg = config.wayland.windowManager.maomaowm;
  variables = lib.concatStringsSep " " cfg.systemd.variables;
  extraCommands = lib.concatStringsSep " && " cfg.systemd.extraCommands;
  systemdActivation = ''${pkgs.dbus}/bin/dbus-update-activation-environment --systemd ${variables}; ${extraCommands}'';
  autostart_sh = pkgs.writeShellScript "autostart.sh" ''
    ${lib.optionalString cfg.systemd.enable systemdActivation}
    ${cfg.autostart_sh}
  '';
in
{
  options = {
    wayland.windowManager.maomaowm = with lib; {
      enable = mkOption {
        type = types.bool;
        default = false;
      };
      systemd = {
        enable = mkOption {
          type = types.bool;
          default = pkgs.stdenv.isLinux;
          example = false;
          description = ''
            Whether to enable {file}`maomao-session.target` on
            maomao startup. This links to
            {file}`graphical-session.target`.
            Some important environment variables will be imported to systemd
            and dbus user environment before reaching the target, including
            * {env}`DISPLAY`
            * {env}`WAYLAND_DISPLAY`
            * {env}`XDG_CURRENT_DESKTOP`
            * {env}`XDG_SESSION_TYPE`
            * {env}`NIXOS_OZONE_WL`
            You can extend this list using the `systemd.variables` option.
          '';
        };
        variables = mkOption {
          type = types.listOf types.str;
          default = [
            "DISPLAY"
            "WAYLAND_DISPLAY"
            "XDG_CURRENT_DESKTOP"
            "XDG_SESSION_TYPE"
            "NIXOS_OZONE_WL"
            "XCURSOR_THEME"
            "XCURSOR_SIZE"
          ];
          example = [ "--all" ];
          description = ''
            Environment variables imported into the systemd and D-Bus user environment.
          '';
        };
        extraCommands = mkOption {
          type = types.listOf types.str;
          default = [
            "systemctl --user reset-failed"
            "systemctl --user start maomao-session.target"
          ];
          description = ''
            Extra commands to run after D-Bus activation.
          '';
        };
        xdgAutostart = mkEnableOption ''
          autostart of applications using
          {manpage}`systemd-xdg-autostart-generator(8)`
        '';
      };
      settings = mkOption {
        description = "maomaowm config content";
        type = types.str;
        default = "";
        example = ''
          # menu and terminal
          bind=Alt,space,spawn,rofi -show drun
          bind=Alt,Return,spawn,foot
        '';
      };
      autostart_sh = mkOption {
        description = "WARRNING: This is a shell script, but no need to add shebang";
        type = types.str;
        default = "";
        example = ''
          waybar &
        '';
      };
    };
  };

  config = lib.mkIf cfg.enable {
    home.packages = [ maomaowm ];
    home.activation =
      lib.optionalAttrs (cfg.autostart_sh != "") {
        createMaomaoScript = lib.hm.dag.entryAfter [ "clearMaomaoConfig" ] ''
          cat ${autostart_sh} > $HOME/.config/maomao/autostart.sh
          chmod +x $HOME/.config/maomao/autostart.sh
        '';
      }
      // lib.optionalAttrs (cfg.settings != "") {
        createMaomaoConfig = lib.hm.dag.entryAfter [ "clearMaomaoConfig" ] ''
          cat > $HOME/.config/maomao/config.conf <<EOF
          ${cfg.settings}
          EOF
        '';
      }
      // {
        clearMaomaoConfig = lib.hm.dag.entryAfter [ "writeBoundary" ] ''
          rm -rf $HOME/.config/maomao
          mkdir -p $HOME/.config/maomao
        '';
      };
    systemd.user.targets.maomao-session = lib.mkIf cfg.systemd.enable {
      Unit = {
        Description = "maomao compositor session";
        Documentation = [ "man:systemd.special(7)" ];
        BindsTo = [ "graphical-session.target" ];
        Wants = [
          "graphical-session-pre.target"
        ] ++ lib.optional cfg.systemd.xdgAutostart "xdg-desktop-autostart.target";
        After = [ "graphical-session-pre.target" ];
        Before = lib.optional cfg.systemd.xdgAutostart "xdg-desktop-autostart.target";
      };
    };
  };
}
