{
  description = "PiSCSI - Virtual SCSI device emulator for Raspberry Pi";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        # Cross-compilation target packages
        pkgsArmv7 = pkgs.pkgsCross.armv7l-hf-multiplatform;
        pkgsAarch64 = pkgs.pkgsCross.aarch64-multiplatform;

        # Common build function for PiSCSI
        mkPiscsi = { stdenv, protobuf, libpcap, spdlog, fmt, abseil-cpp, connectType ? "FULLSPEC", debug ? false }:
          stdenv.mkDerivation {
            pname = "piscsi";
            version = "24.10.01";

            src = ./cpp;

            # Copy the proto file into the build
            preBuild = ''
              mkdir -p ../proto
              cp ${./proto/piscsi_interface.proto} ../proto/piscsi_interface.proto
            '';

            nativeBuildInputs = [
              pkgs.protobuf  # Always use native protoc for code generation
              pkgs.gnumake
              pkgs.pkg-config
            ];

            buildInputs = [
              protobuf
              libpcap
              spdlog
              fmt
              abseil-cpp
            ];

            makeFlags = [
              "CONNECT_TYPE=${connectType}"
            ] ++ pkgs.lib.optionals debug [
              "DEBUG=1"
            ];

            # Skip docs generation (requires man command)
            buildPhase = ''
              runHook preBuild

              # Generate protobuf files using native protoc
              mkdir -p generated
              protoc --cpp_out=generated --proto_path=../proto ../proto/piscsi_interface.proto
              mv generated/piscsi_interface.pb.cc generated/piscsi_interface.pb.cpp

              # Build binaries (skip docs target which requires man)
              mkdir -p obj bin

              # Build targets
              targets="bin/piscsi bin/scsictl bin/scsimon bin/scsiloop"
              ${pkgs.lib.optionalString (connectType == "FULLSPEC") ''targets="$targets bin/scsidump"''}

              # Get abseil libs needed by protobuf
              ABSL_LIBS=$(pkg-config --libs protobuf 2>/dev/null | grep -oE '\-labsl[^ ]+' | tr '\n' ' ')

              make -j''${NIX_BUILD_CORES:-$(nproc)} \
                CXX=$CXX \
                CONNECT_TYPE=${connectType} \
                LDFLAGS="$ABSL_LIBS" \
                ${if debug then "DEBUG=1" else ""} \
                $targets

              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall

              mkdir -p $out/bin
              cp bin/* $out/bin/ 2>/dev/null || true

              # Install man pages if they exist
              mkdir -p $out/share/man/man1
              for f in ../doc/*.1; do
                [ -f "$f" ] && cp "$f" $out/share/man/man1/ || true
              done

              runHook postInstall
            '';

            meta = with pkgs.lib; {
              description = "Virtual SCSI device emulator for Raspberry Pi";
              homepage = "https://github.com/PiSCSI/piscsi";
              license = licenses.bsd3;
              platforms = platforms.linux;
              maintainers = [];
            };
          };

        # Helper to create cross-compiled package
        mkCrossPiscsi = crossPkgs: connectType:
          mkPiscsi {
            stdenv = crossPkgs.stdenv;
            protobuf = crossPkgs.protobuf;
            libpcap = crossPkgs.libpcap;
            spdlog = crossPkgs.spdlog;
            fmt = crossPkgs.fmt;
            abseil-cpp = crossPkgs.abseil-cpp;
            inherit connectType;
          };

      in {
        packages = {
          # Native build (for development/testing on x86_64)
          default = mkPiscsi {
            stdenv = pkgs.stdenv;
            protobuf = pkgs.protobuf;
            libpcap = pkgs.libpcap;
            spdlog = pkgs.spdlog;
            fmt = pkgs.fmt;
            abseil-cpp = pkgs.abseil-cpp;
          };

          # Cross-compiled for Raspberry Pi 2/3/Zero W (ARMv7)
          piscsi-armv7 = mkCrossPiscsi pkgsArmv7 "FULLSPEC";
          piscsi-armv7-standard = mkCrossPiscsi pkgsArmv7 "STANDARD";

          # Cross-compiled for Raspberry Pi 4/5 (AArch64)
          piscsi-aarch64 = mkCrossPiscsi pkgsAarch64 "FULLSPEC";
          piscsi-aarch64-standard = mkCrossPiscsi pkgsAarch64 "STANDARD";

          # Debug builds
          piscsi-armv7-debug = mkPiscsi {
            stdenv = pkgsArmv7.stdenv;
            protobuf = pkgsArmv7.protobuf;
            libpcap = pkgsArmv7.libpcap;
            spdlog = pkgsArmv7.spdlog;
            fmt = pkgsArmv7.fmt;
            abseil-cpp = pkgsArmv7.abseil-cpp;
            connectType = "FULLSPEC";
            debug = true;
          };

          piscsi-aarch64-debug = mkPiscsi {
            stdenv = pkgsAarch64.stdenv;
            protobuf = pkgsAarch64.protobuf;
            libpcap = pkgsAarch64.libpcap;
            spdlog = pkgsAarch64.spdlog;
            fmt = pkgsAarch64.fmt;
            abseil-cpp = pkgsAarch64.abseil-cpp;
            connectType = "FULLSPEC";
            debug = true;
          };
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            # Native build dependencies
            gnumake
            gcc
            protobuf
            libpcap
            spdlog
            fmt
            abseil-cpp
            pkg-config

            # Cross-compilation toolchains
            pkgsArmv7.buildPackages.gcc
            pkgsAarch64.buildPackages.gcc

            # Development tools
            gdb
            clang-tools  # for clang-format, clang-tidy

            # Testing
            gtest
            gmock
          ];

          shellHook = ''
            echo "PiSCSI Development Environment"
            echo "==============================="
            echo ""
            echo "Build targets:"
            echo "  nix build              - Build native (x86_64)"
            echo "  nix build .#piscsi-armv7    - Cross-compile for RPi 2/3/Zero W"
            echo "  nix build .#piscsi-aarch64  - Cross-compile for RPi 4/5"
            echo ""
            echo "Variants:"
            echo "  .#piscsi-armv7-standard    - ARMv7 STANDARD board type"
            echo "  .#piscsi-aarch64-standard  - AArch64 STANDARD board type"
            echo "  .#piscsi-armv7-debug       - ARMv7 debug build"
            echo "  .#piscsi-aarch64-debug     - AArch64 debug build"
            echo ""
            echo "Manual build from cpp/:"
            echo "  make all CONNECT_TYPE=FULLSPEC"
            echo ""
          '';
        };
      }
    );
}
