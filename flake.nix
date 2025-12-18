{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };
  outputs = { nixpkgs, ... }: let
    pkgs = import nixpkgs {
      system = "x86_64-linux";
    };
  in {
    devShells."x86_64-linux".default = pkgs.mkShell {
      nativeBuildInputs = with pkgs; [
        gnumake pkg-config systemDlibs libusb1
      ];
      hardeningDisable = [ "all" ];
    };
    packages."x86_64-linux".default = pkgs.stdenv.mkDerivation {
      name = "ch341prog";
      pname = "ch341prog";
      version = "9999";
      src = ./.;
      buildInputs = with pkgs; [
        gcc gnumake libusb1 systemdLibs
      ];
      buildPhase = ''
        gcc ch341a.c main.c -o ch341prog -lusb-1.0
      '';
      installPhase = ''
        mkdir -p $out/bin 
        cp ch341prog $out/bin
      '';
    };
  };
}
