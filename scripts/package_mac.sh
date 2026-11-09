#!/usr/bin/env bash
# Collects the built macOS plug-ins into dist/: a zip of the bundles and an installer .pkg.
# Usage: scripts/package_mac.sh [build-dir] [config]
set -euo pipefail

BUILD_DIR="${1:-build}"
CONFIG="${2:-Release}"
VERSION="$(grep -m1 -Eo 'VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | awk '{print $2}')"
DIST="dist"
STAGE="$(mktemp -d)"

rm -rf "$DIST"
mkdir -p "$DIST/Detroit Hallelujah (macOS)" \
         "$STAGE/Library/Audio/Plug-Ins/VST3" \
         "$STAGE/Library/Audio/Plug-Ins/Components" \
         "$STAGE/Applications"

copy_bundle () {   # $1 = bundle path, $2 = staging sub-folder
    local b="$1"
    # ad-hoc signature: required for Apple Silicon to load the binary
    codesign --force --deep --sign - "$b"
    cp -R "$b" "$DIST/Detroit Hallelujah (macOS)/"
    cp -R "$b" "$STAGE/$2/"
}

found=0
for target in HallelujahBlue HallelujahDuck; do
    ART="$BUILD_DIR/plugins/$target/${target}_artefacts/$CONFIG"
    for b in "$ART"/VST3/*.vst3;      do [ -e "$b" ] && copy_bundle "$b" "Library/Audio/Plug-Ins/VST3" && found=$((found+1)); done
    for b in "$ART"/AU/*.component;   do [ -e "$b" ] && copy_bundle "$b" "Library/Audio/Plug-Ins/Components" && found=$((found+1)); done
    for b in "$ART"/Standalone/*.app; do [ -e "$b" ] && copy_bundle "$b" "Applications" && found=$((found+1)); done
done

if [ "$found" -eq 0 ]; then
    echo "No built plug-ins found under $BUILD_DIR (config $CONFIG)." >&2
    exit 1
fi

cp README.md "$DIST/Detroit Hallelujah (macOS)/README.md"
( cd "$DIST" && ditto -c -k --keepParent "Detroit Hallelujah (macOS)" "DetroitHallelujah-$VERSION-macOS.zip" )

pkgbuild --root "$STAGE" \
         --identifier "com.detroithallelujah.plugins" \
         --version "$VERSION" \
         --install-location "/" \
         "$DIST/DetroitHallelujah-$VERSION-macOS-Installer.pkg"

rm -rf "$STAGE"
echo "Packaged $found bundles into $DIST/"
ls -la "$DIST"
