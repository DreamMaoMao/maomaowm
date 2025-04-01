{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    nixpkgs-wayland.url = "github:nix-community/nixpkgs-wayland";
    flake-parts.url = "github:hercules-ci/flake-parts";
    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    mmsg = {
      url = "github:DreamMaoMao/mmsg";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      flake-parts,
      treefmt-nix,
      ...
    }@inputs:
    flake-parts.lib.mkFlake { inherit inputs; } {
      imports = [
        inputs.flake-parts.flakeModules.easyOverlay
      ];

      flake = {
        hmModules.maomaowm = import ./nix/hm-modules.nix self;
        nixosModules.maomaowm = import ./nix/nixos-modules.nix self;
      };

      perSystem =
        {
          config,
          pkgs,
          ...
        }:
        let
          inherit (pkgs)
            callPackage
            ;
          maomaowm = callPackage ./nix {
            inherit (inputs.nixpkgs-wayland.packages.${pkgs.system}) wlroots;
            inherit (inputs.mmsg.packages.${pkgs.system}) mmsg;
          };
          shellOverride = old: {
            nativeBuildInputs = old.nativeBuildInputs ++ [ ];
            buildInputs = old.buildInputs ++ [ ];
          };
          treefmtEval = treefmt-nix.lib.evalModule pkgs ./treefmt.nix;
        in
        {
          packages.default = maomaowm;
          overlayAttrs = {
            inherit (config.packages) maomaowm;
          };
          packages = {
            inherit maomaowm;
          };
          devShells.default = maomaowm.overrideAttrs shellOverride;
          formatter = treefmtEval.config.build.wrapper;
        };
      systems = [ "x86_64-linux" ];
    };
}
