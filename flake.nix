{
  description = "A simple TUI Minesweeper game made in C.";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    systems.url = "github:nix-systems/default";
  };

  outputs =
    inputs@{
      self,
      flake-parts,
      ...
    }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = import inputs.systems;

      perSystem =
        {
          self',
          pkgs,
          system,
          ...
        }:
        let
          inherit (pkgs) lib;
        in
        {
          packages = {
            mines = pkgs.stdenv.mkDerivation {
              pname = "mines";
              version = "main";

              src = lib.cleanSource ./.;

              nativeBuildInputs = with pkgs; [
                meson
                ninja
                pkg-config
              ];

              doCheck = true;

              meta = with lib; {
                description = "A simple TUI Minesweeper game made in C.";
                homepage = "https://codeberg.org/Grazen0/mines";
                license = licenses.gpl3;
              };
            };

            default = self'.packages.mines;
          };

          devShells.default = pkgs.mkShell {
            inputsFrom = lib.attrValues self'.packages;
            hardeningDisable = [ "fortify" ];
          };
        };
    };
}
