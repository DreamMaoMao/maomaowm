self:
{
  config,
  lib,
  pkgs,
  ...
}:
let
  cfg = config.programs.maomaowm;
in
{
  options = {
    programs.maomaowm = {
      enable = lib.mkEnableOption "maomaowm, a wayland compositor based on dwl";
      package = lib.mkOption {
        type = lib.types.package;
        default = self.packages.${pkgs.system}.maomaowm;
        description = "The maomaowm package to use";
      };
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [
      cfg.package
    ] ++ (if (builtins.hasAttr "mmsg" cfg.package) then [ cfg.package.mmsg ] else [ ]);

    xdg.portal = {
      enable = lib.mkDefault true;

      wlr.enable = lib.mkDefault true;

      configPackages = [ cfg.package ];
    };

    security.polkit.enable = lib.mkDefault true;

    programs.xwayland.enable = lib.mkDefault true;

    services = {
      displayManager.sessionPackages = [ cfg.package ];

      graphical-desktop.enable = lib.mkDefault true;
    };

  };
}
