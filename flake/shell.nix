{
  perSystem =
    { pkgs, ... }:
    {
      devShells.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
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
