#!/usr/bin/env bash
# Build InterzoneXXL for MPC OS Gen1 (armv7-a hard-float, glibc <= 2.31 so it also loads on MPC OS 2.x).
#   ./build.sh                     (MPC_VST=../mpc-vst-plugins to use another framework checkout)
# Needs: python3 + Pillow + cairosvg, a host gcc (for shadow_art), and Zig (pip install ziglang). No Docker.
# Output: build/package/ANDREALPHEUS - VST - InterzoneXXL/  (skin + interzonexxl.so) and build/pluginlist-entry.xml
set -euo pipefail
cd "$(dirname "$0")"
MPC_VST="${MPC_VST:-$PWD/third_party/mpc-vst-plugins}"
ZIG="${ZIG:-python3 -m ziglang}"
NAME="ANDREALPHEUS - VST - InterzoneXXL"
python3 tools/gen_params.py
python3 tools/layout.py --check   # layout.conf matches tools/panel.py (every control on a Q-Link)

mkdir -p build/host build/arm
[ -x build/host/shadow_art ] || gcc -O2 -w -I"$MPC_VST/tools/vendor/force-shadow/tools" -x c -o build/host/shadow_art "$MPC_VST/tools/shadow_art.c" -lm
rm -rf build/skin
SHADOW_TITLE_FONT="$PWD/art/fonts/TitilliumWeb-SemiBold.ttf" SHADOW_ART="$PWD/build/host/shadow_art" python3 "$MPC_VST/tools/gen_vst.py" vst.json
python3 tools/knob_art.py "build/skin/$NAME/Plugin Skins"   # the source pages' knobs: Valley, Bogaudio, VCV Rogan (PlateauXXL)
python3 tools/post_skin.py "build/skin/$NAME/Plugin Skins" layout.conf   # those pages in their modules' colours (PlateauXXL)
python3 tools/panel_art.py "build/skin/$NAME/Plugin Skins"   # Interzone's panel, sliders, knobs and switches
python3 tools/check_skin.py "build/skin/$NAME/Plugin Skins"   # touch boxes, edges, Q-Links, as MPC loads it
SRCS=$(python3 -c "import json; print(' '.join(json.load(open('vst.json'))['build']['sources']))")
CFLAGS=$(python3 -c "import json; print(' '.join(json.load(open('vst.json'))['build']['cflags']))")
TGT="-target arm-linux-gnueabihf.2.31 -mcpu=generic+v7a+vfp3d16-d32-neon+thumb2 -ffp-contract=off"
COMMON="-O2 -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -DNDEBUG -w"
OBJS=""
for f in $SRCS; do
  o="build/arm/$(echo "$f" | tr / _).o"
  $ZIG c++ $TGT $COMMON -std=gnu++11 $CFLAGS -Ibuild -I"$MPC_VST/wrapper" -c "$f" -o "$o"
  OBJS="$OBJS $o"
done
$ZIG cc $TGT $COMMON -std=gnu11 -Ibuild -c "$MPC_VST/wrapper/vst2_wrap.c" -o build/arm/vst2_wrap.o
printf '{\n  global: VSTPluginMain;\n  local: *;\n};\n' > build/arm/exports.map
$ZIG c++ $TGT -shared -fPIC -Wl,--no-undefined -Wl,--version-script=build/arm/exports.map -Wl,--gc-sections -Wl,-s \
  $OBJS build/arm/vst2_wrap.o -lm -ldl -lpthread -o build/arm/interzonexxl.so
PKG="build/package/$NAME"
rm -rf build/package && mkdir -p "$PKG"
cp -r "build/skin/$NAME/." "$PKG/"
cp build/arm/interzonexxl.so "$PKG/"
echo "-> $PKG"
