#!/bin/bash
# Cross-compile Dialogue Boost VST3 for Windows x64 with mingw-w64
set -e
cd "$(dirname "$0")"
SDK=vst3sdk
OUT=build
mkdir -p $OUT/obj

CXX=x86_64-w64-mingw32-g++
FLAGS="-std=c++17 -O2 -DNDEBUG -DRELEASE=1 -D_UNICODE -DUNICODE -I$SDK -Isrc -ffast-math -fno-finite-math-only"

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
$SDK/public.sdk/source/main/dllmain.cpp
$SDK/public.sdk/source/main/moduleinit.cpp
$SDK/public.sdk/source/vst/vstinitiids.cpp
$SDK/public.sdk/source/vst/vstbus.cpp
$SDK/public.sdk/source/vst/vstcomponentbase.cpp
$SDK/public.sdk/source/vst/vstparameters.cpp
$SDK/public.sdk/source/vst/vstsinglecomponenteffect.cpp
src/plugin.cpp
src/editor.cpp
"

OBJS=""
for s in $SRCS; do
  o=$OUT/obj/$(echo $s | tr '/' '_' ).o
  $CXX $FLAGS -c $s -o $o
  OBJS="$OBJS $o"
done

BUNDLE="$OUT/Dialogue Boost.vst3/Contents/x86_64-win"
mkdir -p "$BUNDLE"
$CXX -shared -o "$BUNDLE/Dialogue Boost.vst3" $OBJS src/exports.def \
  -static -static-libgcc -static-libstdc++ -lgdiplus -lole32 -luuid -lgdi32 -luser32 -s
echo "built: $BUNDLE/Dialogue Boost.vst3"
