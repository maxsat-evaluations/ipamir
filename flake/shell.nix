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
          gnuplot
          just
          mpfr
          nushell
          unzip
          zip
          zlib
        ];
      };
    };
}
