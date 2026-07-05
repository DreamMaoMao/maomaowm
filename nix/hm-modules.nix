self:
{
  lib,
  config,
  pkgs,
  ...
}:
let
  cfg = config.wayland.windowManager.mango;
  common = import ./home-common.nix self {
    inherit
      lib
      config
      pkgs
      cfg
      ;
  };
in
{
  options.wayland.windowManager.mango = common.options;

  config = lib.mkIf cfg.enable {
    # Backwards compatibility warning for old string-based config
    warnings = lib.optional (builtins.isString cfg.settings) ''
      wayland.windowManager.mango.settings: Using a string for settings is deprecated.
      Please migrate to the new structured attribute set format.
      See the module documentation for examples, or use the 'extraConfig' option for raw config strings.
      The old string format will be removed in a future release.
    '';

    home.packages = [ cfg.package ];
    xdg.configFile = common.configFiles;
    systemd.user.targets.mango-session = lib.mkIf cfg.systemd.enable {
      Unit = {
        Description = "mango compositor session";
        Documentation = [ "man:systemd.special(7)" ];
        BindsTo = [ "graphical-session.target" ];
        Wants = [
          "graphical-session-pre.target"
        ]
        ++ lib.optional cfg.systemd.xdgAutostart "xdg-desktop-autostart.target";
        After = [ "graphical-session-pre.target" ];
        Before = lib.optional cfg.systemd.xdgAutostart "xdg-desktop-autostart.target";
      };
    };
  };
}
