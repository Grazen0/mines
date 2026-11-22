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
          unity = rec {
            rev = "2.7.0";
            src = pkgs.fetchFromGitHub {
              owner = "ThrowTheSwitch";
              repo = "Unity";
              rev = "v${rev}";
              hash = "sha256-9vCVNdu7e1w0AsCwcLOJXI6EXOaO6zG/PmGjcpsCllw=";
            };
          };
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

                # Required for Unity
                python3
                ruby
              ];

              # Taken from https://github.com/Alexays/Waybar/blob/master/nix/default.nix#L42
              postUnpack = ''
                pushd "$sourceRoot"
                cp -R --no-preserve=ownership ${unity.src} subprojects/Unity-${unity.rev}
                patchShebangs .
                popd
              '';

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
