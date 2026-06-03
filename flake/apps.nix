{
  perSystem =
    {
      lib,
      pkgs,
      self',
      ...
    }:
    {
      packages =
        let
          apps = [
            { name = "ipamirapp"; }
            { name = "ipamiradaboost"; }
            {
              name = "ipamirextenf";
              sourceRegex = [
                "sat"
                "sat/minisat220.*"
              ];
              preBuild = ''
                make -C sat/minisat220
              '';
              buildInputs = [ pkgs.zlib ];
            }
            {
              name = "ipamirbioptsat";
              sourceRegex = [
                "sat"
                "sat/minisat220.*"
              ];
              buildInputs = [ pkgs.zlib ];
            }
            { name = "ipamiric3ref"; }
          ];
          solvers = [
            {
              name = "solver2022";
              sourceRegex = [
                "sat"
                "sat/minisat220.*"
              ];
              buildInputs = with pkgs; [
                cmake
                zlib
              ];
              postInstall = ''
                cp sat/minisat220/libipasirminisat220.a $out/lib/
              '';
            }
            {
              name = "EvalMaxSAT2022";
              buildInputs = [ pkgs.zlib ];
            }
            {
              name = "uwrmaxsat14";
              sourceRegex = [
                "sat"
                "sat/cominisatps.*"
              ];
              buildInputs = with pkgs; [
                gmp
                zlib
              ];
              nativeBuildInputs = [ pkgs.unzip ];
              postInstall = ''
                cp sat/cominisatps/libipasircominisatps.a $out/lib/
              '';
            }
            {
              name = "uwrmaxsat14scip";
              sourceRegex = [
                "sat"
                "sat/cominisatps.*"
              ];
              buildInputs = with pkgs; [
                gmp
                zlib
              ];
              nativeBuildInputs = with pkgs; [
                cmake
                unzip
              ];
              postInstall = ''
                cp sat/cominisatps/libipasircominisatps.a $out/lib/
                cp maxsat/uwrmaxsat14scip/scipoptsuite-8.0.0/build/lib/*.a $out/lib/
              '';
            }
          ];
        in
        (builtins.listToAttrs (
          builtins.map (solver: {
            name = solver.name;
            value = pkgs.stdenv.mkDerivation {
              name = solver.name;

              src = lib.sources.sourceByRegex ../. (
                [
                  "ipamir.h"
                  "ipasir.h"
                  "maxsat"
                  "maxsat/${solver.name}.*"
                ]
                ++ lib.optionals (builtins.hasAttr "sourceRegex" solver) solver.sourceRegex
              );

              dontUseCmakeConfigure = true;

              buildInputs = [ ] ++ lib.optionals (builtins.hasAttr "buildInputs" solver) solver.buildInputs;

              nativeBuildInputs = [
                pkgs.gnumake
              ]
              ++ lib.optionals (builtins.hasAttr "nativeBuildInputs" solver) solver.nativeBuildInputs;

              buildPhase = ''
                make -C maxsat/${solver.name}
              '';

              installPhase = ''
                mkdir -p $out/lib/
                cp maxsat/${solver.name}/libipamir${solver.name}.a $out/lib/
                runHook postInstall
              '';

              postInstall = if builtins.hasAttr "postInstall" solver then solver.postInstall else "";
            };
          }) solvers
        ))
        // (builtins.listToAttrs (
          lib.lists.crossLists
            (app: solver: {
              name = "${app.name}-${solver.name}";
              value = pkgs.stdenv.mkDerivation {
                name = "${app.name}-${solver.name}";

                src = lib.sources.sourceByRegex ../. (
                  [
                    "ipamir.h"
                    "ipasir.h"
                    "app"
                    "app/${app.name}.*"
                    "maxsat"
                    "maxsat/${solver.name}"
                    "maxsat/${solver.name}/LIBS"
                    "maxsat/${solver.name}/LINK"
                  ]
                  ++ lib.optionals (builtins.hasAttr "sourceRegex" app) app.sourceRegex
                );

                dontUseCmakeConfigure = true;

                buildInputs = [
                  self'.packages."${solver.name}"
                ]
                ++ lib.optionals (builtins.hasAttr "buildInputs" app) app.buildInputs
                ++ lib.optionals (builtins.hasAttr "buildInputs" solver) solver.buildInputs;

                nativeBuildInputs = [
                  pkgs.gnumake
                ]
                ++ lib.optionals (builtins.hasAttr "nativeBuildInputs" app) app.nativeBuildInputs;

                env.IPAMIRSOLVER = solver.name;

                preBuild = if builtins.hasAttr "preBuild" app then app.preBuild else "";

                buildPhase = ''
                  runHook preBuild
                  cp ${self'.packages."${solver.name}"}/lib/*.a maxsat/${solver.name}/
                  make -C app/${app.name}
                '';

                installPhase = ''
                  mkdir -p $out/bin/
                  mv app/${app.name}/${app.name} $out/bin/${app.name}-${solver.name}
                '';
              };
            })
            [
              apps
              solvers
            ]
        ))
        // {
          all-apps = pkgs.stdenvNoCC.mkDerivation {
            name = "all-apps";
            src = pkgs.coreutils;
            buildInputs = lib.lists.crossLists (app: solver: self'.packages."${app.name}-${solver.name}") [
              apps
              solvers
            ];
            installPhase = ''
              mkdir -p $out/bin/
              ${lib.strings.concatLines (
                lib.lists.crossLists
                  (
                    app: solver:
                    "cp ${self'.packages."${app.name}-${solver.name}"}/bin/${app.name}-${solver.name} $out/bin/"
                  )
                  [
                    apps
                    solvers
                  ]
              )}
            '';
          };
        };
    };
}
