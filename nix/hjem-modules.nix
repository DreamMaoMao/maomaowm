self:
{
  lib,
  config,
  pkgs,
  ...
}:
let
  cfg = config.desktops.mango;
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
  # hjem-rum calls these 'desktops', so let's do that here too.
  options.desktops.mango = common.options;

  config = lib.mkIf cfg.enable {
    packages = [ cfg.package ];
    xdg.config.files = common.configFiles;
    systemd.targets.mango-session = lib.mkIf cfg.systemd.enable {
      description = "mango compositor session";
      documentation = [ "man:systemd.special(7)" ];
      bindsTo = [ "graphical-session.target" ];
      wants = [
        "graphical-session-pre.target"
      ]
      ++ lib.optional cfg.systemd.xdgAutostart "xdg-desktop-autostart.target";
      after = [ "graphical-session-pre.target" ];
      before = lib.optional cfg.systemd.xdgAutostart "xdg-desktop-autostart.target";
    };
  };
}
