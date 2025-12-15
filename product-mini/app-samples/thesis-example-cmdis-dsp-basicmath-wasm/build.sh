#!/usr/bin/env bash
# Pure-WASM variant of the thesis example app: all kernels are
# implemented inside the WASM module and timed using thesis_get_ticks.

WAMR_DIR=${PWD}/../../..

echo "Build pure WASM app .."
/opt/wasi-sdk/bin/clang -O3 \
        -I../thesis-example-cmdis-dsp-basicmath \
        -z stack-size=4096 -Wl,--initial-memory=131072 \
        -o test.wasm main.c \
        -Wl,--export=main -Wl,--export=__main_argc_argv \
        -Wl,--export=__data_end -Wl,--export=__heap_base \
        -Wl,--strip-all,--no-entry \
        -Wl,--allow-undefined \
        -nostdlib \

echo "Build binarydump tool .."
rm -fr build && mkdir build && cd build
cmake ../../../../test-tools/binarydump-tool
make
cd ..

echo "Generate test_wasm.h .."
./build/binarydump -o test_wasm.h -n wasm_test_file test.wasm

echo "Copy test_wasm.h"
cp test_wasm.h /home/vboxuser/zephyrproject/wasm-micro-runtime/product-mini/platforms/zephyr/thesis-example-fw/src

echo "Done"
