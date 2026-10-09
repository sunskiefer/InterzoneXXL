#!/usr/bin/env bash
# CPU check on the device (mpc-vst-plugins docs/BENCH.md), for Mono and for Poly with 16 voices.
#   tools/device_bench.sh <device-ip>          build what is needed, copy it to /tmp on the device, run, clean up
#   tools/device_bench.sh --build-only DIR     only build: DIR gets bench-armhf, the two .so files and run.sh
# Needs ssh/scp to the device as root (MockbaMod). Nothing is installed and MPC keeps running; the bench runs pinned
# to core 1 at normal priority, so MPC's audio always wins.
set -euo pipefail
cd "$(dirname "$0")/.."
MPC_VST="${MPC_VST:-$PWD/third_party/mpc-vst-plugins}"
ZIG="${ZIG:-python3 -m ziglang}"
OUT="build/device_bench"
[ "${1:-}" = "--build-only" ] && OUT="$2"
mkdir -p "$OUT" build/bench
TGT="-target arm-linux-gnueabihf.2.31 -mcpu=generic+v7a+vfp3d16-d32-neon+thumb2"
$ZIG cc $TGT -O2 -w -o "$OUT/bench-armhf" "$MPC_VST/tools/bench.c" -ldl -lm
[ -f build/arm/interzonexxl.so ] || ./build.sh
cp build/arm/interzonexxl.so "$OUT/interzonexxl-mono.so"
SRCS=$(python3 -c "import json; print(' '.join(json.load(open('vst.json'))['build']['sources']))")
CFLAGS=$(python3 -c "import json; print(' '.join(json.load(open('vst.json'))['build']['cflags']))")
COMMON="-O2 -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -DNDEBUG -w -ffp-contract=off"
OBJS=""
for f in $SRCS; do
  o="build/bench/$(echo "$f" | tr / _).o"
  $ZIG c++ $TGT $COMMON -DIXXL_BENCH_POLY -std=gnu++11 $CFLAGS -Ibuild -I"$MPC_VST/wrapper" -c "$f" -o "$o"
  OBJS="$OBJS $o"
done
printf '{\n  global: VSTPluginMain;\n  local: *;\n};\n' > build/bench/exports.map
$ZIG c++ $TGT -shared -fPIC -Wl,--no-undefined -Wl,--version-script=build/bench/exports.map -Wl,--gc-sections -Wl,-s \
  $OBJS build/arm/vst2_wrap.o -lm -ldl -lpthread -o "$OUT/interzonexxl-poly16.so"
cat > "$OUT/run.sh" <<'RUN'
#!/usr/bin/env bash
# InterzoneXXL CPU bench on the device: ./run.sh <device-ip>   (nothing is installed; MPC can keep running)
set -euo pipefail
ip="$1"; here="$(cd "$(dirname "$0")" && pwd)"
scp -q "$here/bench-armhf" "$here/interzonexxl-mono.so" "$here/interzonexxl-poly16.so" "root@$ip:/tmp/"
ssh "root@$ip" 'cd /tmp && chmod +x bench-armhf &&
  echo "== Mono (default)"; taskset 2 ./bench-armhf /tmp/interzonexxl-mono.so -s 4;
  echo "== Poly, 16 voices (chords of 1, 4, 8, 16 notes)"; taskset 2 ./bench-armhf /tmp/interzonexxl-poly16.so -s 4;
  rm -f bench-armhf interzonexxl-mono.so interzonexxl-poly16.so'
RUN
chmod +x "$OUT/run.sh"
[ "${1:-}" = "--build-only" ] && { echo "-> $OUT"; exit 0; }
"$OUT/run.sh" "$1"
