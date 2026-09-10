{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    scenefx = {
      url = "github:wlrfx/scenefx";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      flake-parts,
      ...
    }@inputs:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [
        inputs.flake-parts.flakeModules.easyOverlay
      ];

      flake = {
        hmModules.mango = import ./nix/hm-modules.nix self;
        nixosModules.mango = import ./nix/nixos-modules.nix self;
      };

      perSystem =
        {
          config,
          pkgs,
          ...
        }:
        let
          inherit (pkgs) callPackage;
          # Temporary workaround for broken borders on NVIDIA proprietary drivers (https://github.com/wlrfx/scenefx/pull/177).
          # Once upstream merges/releases the fix, delete this override and uncomment the default below:
          # scenefx = inputs.scenefx.packages.${pkgs.stdenv.hostPlatform.system}.default;
          scenefx = inputs.scenefx.packages.${pkgs.stdenv.hostPlatform.system}.default.overrideAttrs (oldAttrs: {
            postPatch = (oldAttrs.postPatch or "") + ''
              substituteInPlace render/egl.c \
                --replace-fail 'attribs[atti++] = 2;' 'attribs[atti++] = 3;'
              substituteInPlace render/fx_renderer/shaders.c \
                --replace-fail 'glShaderSource(shader, 1, &src, NULL);' \
                  'const char *prefix = (type == GL_FRAGMENT_SHADER) ? "#ifndef GL_FRAGMENT_PRECISION_HIGH\n#define GL_FRAGMENT_PRECISION_HIGH 1\n#endif\n" : ""; const GLchar *sources[] = { prefix, src }; glShaderSource(shader, 2, sources, NULL);'
            '';
          });
          mango = callPackage ./nix {
            inherit scenefx;
          };
          shellOverride = old: {
            nativeBuildInputs = old.nativeBuildInputs ++ [ pkgs.clang-tools ];
            buildInputs = old.buildInputs ++ [ ];
          };
        in
        {
          packages.default = mango;
          overlayAttrs = {
            inherit (config.packages) mango;
          };
          packages = {
            inherit mango;
            hm-options-json = pkgs.callPackage (import ./nix/generate-options.nix self) {
              module = ./nix/hm-modules.nix;
              optionPrefix = "wayland.windowManager.mango.";
            };
            nixos-options-json = pkgs.callPackage (import ./nix/generate-options.nix self) {
              module = ./nix/nixos-modules.nix;
              optionPrefix = "programs.mango.";
            };
          };
          devShells.default = mango.overrideAttrs shellOverride;
          formatter = pkgs.nixfmt;
        };
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
    };
}
