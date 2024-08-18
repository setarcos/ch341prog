{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };
  outputs = { self, nixpkgs, ... }: {
    devShells."x86_64-linux".default = let
      pkgs = import nixpkgs {
        system = "x86_64-linux";
      };
    in pkgs.mkShell {
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
        binutils
        #fixWrapper
        clang
        git
        #glibc.static
        musl
        gnumake
        pkg-config
        util-linux
      ];
      hardeningDisable = [ "all" ];
    };
  };
}
