{
  lib,
  fetchFromGitHub,
  libX11,
  libinput,
  libxcb,
  libxkbcommon,
  pcre2,
  pixman,
  pkg-config,
  stdenv,
  wayland,
  wayland-protocols,
  wayland-scanner,
  xcbutilwm,
  xwayland,
  enableXWayland ? true,
  meson,
  ninja,
  wlroots,
  mmsg,
}: let
  pname = "maomaowm";
  # Use patched wlroots from github.com/DreamMaoMao/wlroots
  wlroots-git = wlroots.overrideAttrs (final: prev: {
    src = fetchFromGitHub {
      owner = "DreamMaoMao";
      repo = "wlroots";
      rev = "afbb5b7c2b14152730b57aa11119b1b16a299d5b";
      sha256 = "sha256-pVU+CuiqvduMTpsnDHX/+EWY2qxHX2lXKiVzdGtcnYY=";
    };
  });
in
  stdenv.mkDerivation {
    inherit pname;
    version = "nightly";

    src = builtins.path {
      path = ../.;
      name = "source";
    };

    nativeBuildInputs = [
      meson
      ninja
      pkg-config
      wayland-scanner
    ];

    buildInputs =
      [
        libinput
        libxcb
        libxkbcommon
        pcre2
        pixman
        wayland
        wayland-protocols
        wlroots-git
      ]
      ++ lib.optionals enableXWayland [
        libX11
        xcbutilwm
        xwayland
      ];

    passthru = {
      providedSessions = ["maomao"];
      inherit mmsg;
    };

    meta = {
      mainProgram = "maomao";
      description = "A streamlined but feature-rich Wayland compositor";
      homepage = "https://github.com/DreamMaoMao/maomaowm";
      license = lib.licenses.mit;
      maintainers = [];
      platforms = lib.platforms.unix;
    };
  }
