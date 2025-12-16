#ifndef WASM_TIMER_H
#define WASM_TIMER_H

#include <stdint.h>
#include <time.h>

/* System clock frequency from Zephyr config.
 * ESP32-C3: CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC = 16000000 (16 MHz)
 */
#define SYS_CLOCK_HW_CYCLES_PER_SEC  16000000ULL

/* Get time in nanoseconds using WASI clock */
static inline uint64_t wasm_get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Convert nanoseconds to hardware ticks.
 * ticks = ns * (clock_hz / 1e9)
 * For 16 MHz: ticks = ns * 16 / 1000 = ns / 62.5
 */
static inline uint32_t wasm_get_ticks(void) {
    uint64_t ns = wasm_get_time_ns();
    return (uint32_t)((ns * SYS_CLOCK_HW_CYCLES_PER_SEC) / 1000000000ULL);
}

/* Alias for compatibility with existing code */
#define thesis_get_ticks() wasm_get_ticks()

#endif /* WASM_TIMER_H */
