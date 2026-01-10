{
  description = "WASM builds of MQuickJS with finalized exception handling (exnref)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        mquickjs-rev = "767e5db54241f0b53193a04aea193c823a9e3121";
        mquickjs-date = "2026-01-03";  # Date of the pinned commit
        mquickjs-hash = "sha256-Lb0Vp5v+MCoE/T7DnoTC9ejXlB2O/ljuZMI1l94zXyM=";

        mquickjs-src = pkgs.fetchFromGitHub {
          owner = "bellard";
          repo = "mquickjs";
          rev = mquickjs-rev;
          hash = mquickjs-hash;
        };

        # Version format: YYYY.MM.DD+<short-commit-hash>
        version = "${mquickjs-date}+${builtins.substring 0 7 mquickjs-rev}";

        mkMquickjsWasm = { optLevel, suffix, description, wrapperFile ? ./src/wasm_wrapper.c }:
          pkgs.stdenv.mkDerivation {
            pname = "mquickjs-wasm${suffix}";
            inherit version;

            src = mquickjs-src;

            nativeBuildInputs = [ pkgs.emscripten ];

            configurePhase = ''
              runHook preConfigure

              export EM_CACHE="$TMPDIR/emscripten_cache"
              mkdir -p "$EM_CACHE"

              cp ${wrapperFile} wasm_wrapper.c
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild

              echo "Building stdlib generator..."
              $CC -Wall -O2 -D_GNU_SOURCE -fno-math-errno \
                  -fno-trapping-math -c -o mqjs_stdlib.host.o mqjs_stdlib.c
              $CC -Wall -O2 -D_GNU_SOURCE -fno-math-errno \
                  -fno-trapping-math -c -o mquickjs_build.host.o mquickjs_build.c
              $CC -o mqjs_stdlib_gen mqjs_stdlib.host.o mquickjs_build.host.o

              echo "Generating stdlib headers (32-bit for WASM)..."
              ./mqjs_stdlib_gen -m32 > mqjs_stdlib.h
              ./mqjs_stdlib_gen -a -m32 > mquickjs_atom.h

              echo "Compiling to WASM at ${optLevel} with finalized exception handling (exnref)..."
              emcc \
                ${optLevel} \
                -s WASM=1 \
                -s STANDALONE_WASM=1 \
                -fwasm-exceptions \
                -s SUPPORT_LONGJMP=wasm \
                -s WASM_LEGACY_EXCEPTIONS=0 \
                -s "EXPORTED_FUNCTIONS=['_sandbox_init','_sandbox_free','_sandbox_eval','_sandbox_get_error','_malloc','_free']" \
                --no-entry \
                -D_GNU_SOURCE \
                -I. \
                -o mquickjs_exnref${suffix}.wasm \
                wasm_wrapper.c mquickjs.c dtoa.c libm.c cutils.c

              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p $out/lib
              cp mquickjs_exnref${suffix}.wasm $out/lib/
              runHook postInstall
            '';

            meta = with pkgs.lib; {
              inherit description;
              homepage = "https://github.com/bellard/mquickjs";
              license = licenses.mit;
            };
          };

        # Standard build (-O2): faster execution, deterministic
        mquickjs-wasm = mkMquickjsWasm {
          optLevel = "-O2";
          suffix = "";
          description = "mquickjs JavaScript engine compiled to WASM (standard)";
        };

        # Size-optimized build (-Oz): ~35% smaller, deterministic
        mquickjs-wasm-small = mkMquickjsWasm {
          optLevel = "-Oz";
          suffix = "_small";
          description = "mquickjs JavaScript engine compiled to WASM (size-optimized)";
        };

        # WASI build (-O2): real time/random via WASI imports
        mquickjs-wasm-wasi = mkMquickjsWasm {
          optLevel = "-O2";
          suffix = "_wasi";
          wrapperFile = ./src/wasm_wrapper_wasi.c;
          description = "mquickjs JavaScript engine compiled to WASM (WASI, real time/random)";
        };

        # WASI build size-optimized (-Oz): real time/random, smaller
        mquickjs-wasm-wasi-small = mkMquickjsWasm {
          optLevel = "-Oz";
          suffix = "_wasi_small";
          wrapperFile = ./src/wasm_wrapper_wasi.c;
          description = "mquickjs JavaScript engine compiled to WASM (WASI, size-optimized)";
        };

        # Combined package with all variants
        mquickjs-wasm-all = pkgs.symlinkJoin {
          name = "mquickjs-wasm-all-${version}";
          paths = [ mquickjs-wasm mquickjs-wasm-small mquickjs-wasm-wasi mquickjs-wasm-wasi-small ];
        };

      in {
        packages = {
          default = mquickjs-wasm-all;
          mquickjs-wasm = mquickjs-wasm;
          mquickjs-wasm-small = mquickjs-wasm-small;
          mquickjs-wasm-wasi = mquickjs-wasm-wasi;
          mquickjs-wasm-wasi-small = mquickjs-wasm-wasi-small;
          all = mquickjs-wasm-all;
        };

        devShells.default = pkgs.mkShell {
          packages = [ pkgs.emscripten pkgs.git ];

          shellHook = ''
            echo "mquickjs WASM build environment"
            echo "  nix build                               - build all variants"
            echo "  nix build .#mquickjs-wasm               - build standard (-O2) variant"
            echo "  nix build .#mquickjs-wasm-small         - build size-optimized (-Oz) variant"
            echo "  nix build .#mquickjs-wasm-wasi          - build WASI (-O2) variant"
            echo "  nix build .#mquickjs-wasm-wasi-small    - build WASI size-optimized (-Oz) variant"
          '';
        };
      }
    );
}
