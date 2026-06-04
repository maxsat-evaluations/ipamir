{
  perSystem =
    { pkgs, ... }:
    {
      devShells.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          autoconf
          cmake
          gmp
          gnumake
          just
          unzip
          zlib
        ];
      };
    };
}
