{
  lib,
  libX11,
  libinput,
  libxcb,
  libdrm,
  libxkbcommon,
  pcre2,
  pango,
  cjson,
  pixman,
  pkg-config,
  stdenv,
  wayland,
  wayland-protocols,
  wayland-scanner,
  libxcb-wm,
  xwayland,
  meson,
  ninja,
  wlroots_0_20,
  enableXWayland ? true,
  debug ? false,
}:
stdenv.mkDerivation {
  pname = "mango";
  version = "nightly";

  src = lib.cleanSourceWith {
    src = ../.;
    filter =
      path: type:
      let
        base = baseNameOf (toString path);
      in
      (lib.cleanSourceFilter path type) && base != "build" && base != "result";
    name = "source";
  };

  mesonFlags = [
    (lib.mesonEnable "xwayland" enableXWayland)
    (lib.mesonBool "asan" debug)
  ];

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    wayland-scanner
  ];

  buildInputs = [
    libinput
    libxcb
    libxkbcommon
    pcre2
    pango
    cjson
    pixman
    wayland
    wayland-protocols
    wlroots_0_20
    libdrm
  ]
  ++ lib.optionals enableXWayland [
    libX11
    libxcb-wm
    xwayland
  ];

  passthru = {
    providedSessions = [ "mango" ];
  };

  meta = {
    mainProgram = "mango";
    description = "Practical and Powerful wayland compositor (dwm but wayland)";
    homepage = "https://github.com/mangowm/mango";
    license = lib.licenses.gpl3Plus;
    maintainers = [ ];
    platforms = lib.platforms.unix;
  };
}
