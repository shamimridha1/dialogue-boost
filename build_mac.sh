#!/bin/bash
# Build Dialogue Boost VST3 (universal: Apple Silicon + Intel) and a .pkg installer. Runs on macOS.
set -e
cd "$(dirname "$0")"
SDK=vst3sdk
OUT=build-mac
VERSION=1.0.0
rm -rf $OUT && mkdir -p $OUT/obj dist

ARCH="-arch arm64 -arch x86_64 -mmacosx-version-min=10.13"
CXXFLAGS="$ARCH -std=c++17 -O2 -DNDEBUG -DRELEASE=1 -I$SDK -Isrc -fvisibility=hidden"

SRCS="
$SDK/base/source/baseiids.cpp
$SDK/base/source/fbuffer.cpp
$SDK/base/source/fdebug.cpp
$SDK/base/source/fobject.cpp
$SDK/base/source/fstreamer.cpp
$SDK/base/source/fstring.cpp
$SDK/base/source/updatehandler.cpp
$SDK/base/thread/source/flock.cpp
$SDK/pluginterfaces/base/coreiids.cpp
$SDK/pluginterfaces/base/funknown.cpp
$SDK/pluginterfaces/base/ustring.cpp
$SDK/public.sdk/source/common/commoniids.cpp
$SDK/public.sdk/source/common/pluginview.cpp
$SDK/public.sdk/source/main/pluginfactory.cpp
$SDK/public.sdk/source/main/macmain.cpp
$SDK/public.sdk/source/main/moduleinit.cpp
$SDK/public.sdk/source/vst/vstinitiids.cpp
$SDK/public.sdk/source/vst/vstbus.cpp
$SDK/public.sdk/source/vst/vstcomponentbase.cpp
$SDK/public.sdk/source/vst/vstparameters.cpp
$SDK/public.sdk/source/vst/vstsinglecomponenteffect.cpp
src/plugin.cpp
"

OBJS=""
for s in $SRCS; do
  o=$OUT/obj/$(echo $s | tr '/' '_').o
  clang++ $CXXFLAGS -c $s -o $o
  OBJS="$OBJS $o"
done
o=$OUT/obj/editor_mac.o
clang++ $CXXFLAGS -fno-objc-arc -ObjC++ -c src/editor_mac.mm -o $o
OBJS="$OBJS $o"

B="$OUT/Dialogue Boost.vst3"
mkdir -p "$B/Contents/MacOS" "$B/Contents/Resources"
clang++ $ARCH -bundle -o "$B/Contents/MacOS/Dialogue Boost" $OBJS \
  -framework Cocoa -framework CoreFoundation \
  -exported_symbols_list $SDK/public.sdk/source/main/macexport.exp
cp installer/mac/Info.plist "$B/Contents/Info.plist"
printf 'BNDL????' > "$B/Contents/PkgInfo"

# ad-hoc signature (required to load on Apple Silicon)
codesign --force --deep --sign - "$B"
codesign --verify --verbose "$B"
lipo -info "$B/Contents/MacOS/Dialogue Boost"

# .pkg installer -> /Library/Audio/Plug-Ins/VST3
ROOT=$OUT/pkgroot
mkdir -p "$ROOT/Library/Audio/Plug-Ins/VST3"
cp -R "$B" "$ROOT/Library/Audio/Plug-Ins/VST3/"
pkgbuild --root "$ROOT" --identifier com.abid.dialogueboost.vst3 --version $VERSION \
  --install-location / $OUT/component.pkg
productbuild --distribution installer/mac/distribution.xml --resources installer/mac/resources \
  --package-path $OUT "dist/DialogueBoost-$VERSION-Mac-Installer.pkg"
echo "built: dist/DialogueBoost-$VERSION-Mac-Installer.pkg"
