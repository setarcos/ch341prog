{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };
  outputs = { self, nixpkgs, ... }: let
    pkgs = import nixpkgs {
      system = "x86_64-linux";
    };
  in {
    devShells."x86_64-linux".default = pkgs.mkShell {
      nativeBuildInputs = with pkgs; let 
        fixWrapper = pkgs.runCommand "fix-wrapper" {} ''
          mkdir -p $out/bin
          for i in ${pkgs.gcc.cc}/bin/*-gnu-gcc*; do
            ln -s ${pkgs.gcc}/bin/gcc $out/bin/$(basename "$i")
          done
          for i in ${pkgs.gcc.cc}/bin/*-gnu-{g++,c++}*; do
            ln -s ${pkgs.gcc}/bin/g++ $out/bin/$(basename "$i")
          done
        '';
      in[
        gnumake pkg-config systemDlibs libusb1
      ];
      hardeningDisable = [ "all" ];
    };
    packages."x86_64-linux".default = pkgs.stdenv.mkDerivation {
      name = "ch341prog";
      pname = "ch341prog";
      version = "ch341prog";
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
