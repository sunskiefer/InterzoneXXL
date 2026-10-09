#!/usr/bin/env bash
# Offline tests (x86, ASan + UBSan): the framework's host test, test/sources_test.cc on the modulation sources
# (PlateauXXL's), then test/interzone_test.cc on the engine, which also runs Valley's own Interzone module code
# (test/make_ref.py) as the reference.
#   test/run_tests.sh              (MPC_VST=../mpc-vst-plugins to use another framework checkout)
#   test/run_tests.sh --wav DIR    also write the renders to listen to
set -euo pipefail
cd "$(dirname "$0")/.."
MPC_VST="${MPC_VST:-$PWD/third_party/mpc-vst-plugins}"
python3 tools/gen_params.py
python3 tools/layout.py --check
bash "$MPC_VST/tools/test_port.sh" vst.json
SRCS=$(python3 -c "import json; print(' '.join(json.load(open('vst.json'))['build']['sources']))")
CFLAGS=$(python3 -c "import json; print(' '.join(json.load(open('vst.json'))['build']['cflags']))")
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -g -O1"
mkdir -p build/dsptest
python3 test/make_ref.py build/dsptest/interzone_ref.cc
OBJS=""
# the reference also needs the scalar OTA filter's calcGTable() (the module calls it; the plugin does not)
for f in $SRCS third_party/valley/src/dsp/filters/OTAFilter.cpp build/dsptest/interzone_ref.cc test/interzone_test.cc; do
  o="build/dsptest/$(echo "$f" | tr / _).o"
  g++ $SAN -std=gnu++11 -Wall -Wno-unused-function -Wno-unused-variable -Itest $CFLAGS -Ibuild -I"$MPC_VST/wrapper" \
    -c "$f" -o "$o"
  OBJS="$OBJS $o"
done
g++ $SAN $OBJS -lm -ldl -lpthread -o build/dsptest/interzone_test
o="build/dsptest/test_sources_test.cc.o"
g++ $SAN -std=gnu++11 -Wall -Wno-unused-function -Wno-unused-variable -Itest $CFLAGS -Ibuild -I"$MPC_VST/wrapper" \
  -c test/sources_test.cc -o "$o"
g++ $SAN $(echo $OBJS | tr ' ' '\n' | grep -v -E "interzone_test|interzone_ref|OTAFilter.cpp" | grep -v engine.cc) "$o" \
  -lm -ldl -lpthread -o build/dsptest/sources_test
build/dsptest/sources_test
build/dsptest/interzone_test "$@"
