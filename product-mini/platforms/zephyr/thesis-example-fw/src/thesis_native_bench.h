/* Native (non-WASM) benchmark harness for thesis kernels.
 *
 * This runs directly on Zephyr and calls the same native
 * implementations that lib_thesis_wrapper.c uses, but without
 * going through the WAMR interpreter. It uses the same synthetic
 * inputs and problem sizes as the WASM apps.
 */

#ifndef THESIS_NATIVE_BENCH_H
#define THESIS_NATIVE_BENCH_H

void thesis_native_bench_run(void);

#endif /* THESIS_NATIVE_BENCH_H */
