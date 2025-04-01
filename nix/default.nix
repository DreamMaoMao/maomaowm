{
  lib,
  libX11,
  libinput,
  libxcb,
  libxkbcommon,
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
}:
let
  pname = "maomaowm";
in
stdenv.mkDerivation {
  inherit pname;
  version = "nightly";

  src = ../.;

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
      pixman
      wayland
      wayland-protocols
      wlroots
    ]
    ++ lib.optionals enableXWayland [
      libX11
      xcbutilwm
      xwayland
    ];

  passthru = {
    providedSessions = [ "maomao" ];
    inherit mmsg;
  };

  meta = {
    mainProgram = "maomao";
    description = "A streamlined but feature-rich Wayland compositor";
    homepage = "https://github.com/DreamMaoMao/maomaowm";
    license = lib.licenses.mit;
    maintainers = with lib.maintainers; [ ];
    platforms = lib.platforms.unix;
  };
}
