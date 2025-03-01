# Copyright (C) 2019 Intel Corporation.  All rights reserved.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

WAMR_DIR=${PWD}/../../..

echo "Build wasm app .."
/opt/wasi-sdk/bin/clang -O3 \
        -I . \
        -I coremark/ \
        -z stack-size=4096 -Wl,--initial-memory=65536 \
        -o test.wasm coremark/core_list_join.c coremark/core_main.c coremark/core_matrix.c coremark/core_state.c coremark/core_util.c core_portme.c -DPERFORMANCE_RUN=1 -DITERATIONS=1000 \
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
cp test_wasm.h /home/okan/adi-zephyr-sdk/wasm-micro-runtime/product-mini/platforms/zephyr/thesis-example-fw/src # Fix me

echo "Done"
