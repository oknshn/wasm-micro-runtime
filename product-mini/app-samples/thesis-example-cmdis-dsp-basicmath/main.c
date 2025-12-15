/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "thesis-lib.h"

typedef struct {
    const char *name;
    int vec_len;
    int mat_m, mat_k, mat_n;
    int img_h, img_w, k_h, k_w;
} bench_case_t;

#define MAX_BENCH_LEN 1024
#define THESIS_TARGET_M4    1
#define THESIS_TARGET_RISC_V32 1

static unsigned int input1_buf[MAX_BENCH_LEN];
static unsigned int input2_buf[MAX_BENCH_LEN];
static unsigned int scratch_buf[MAX_BENCH_LEN];

#if defined(THESIS_TARGET_M4) || defined(THESIS_TARGET_RISCV32)
static short q15_input1_buf[MAX_BENCH_LEN];
static short q15_input2_buf[MAX_BENCH_LEN];
static short q15_scratch_buf[MAX_BENCH_LEN];

static void
prepare_q15_buffers(int len)
{
    if (len > MAX_BENCH_LEN)
        len = MAX_BENCH_LEN;

    float *f1 = (float *)input1_buf;
    float *f2 = (float *)input2_buf;
    float max_abs1 = 0.0f;
    float max_abs2 = 0.0f;

    for (int i = 0; i < len; i++) {
        float v1 = f1[i];
        float v2 = f2[i];
        float a1 = fabsf(v1);
        float a2 = fabsf(v2);
        if (a1 > max_abs1)
            max_abs1 = a1;
        if (a2 > max_abs2)
            max_abs2 = a2;
    }

    float scale1 = max_abs1 > 0.0f ? 32767.0f / max_abs1 : 1.0f;
    float scale2 = max_abs2 > 0.0f ? 32767.0f / max_abs2 : 1.0f;

    for (int i = 0; i < len; i++) {
        float q1 = f1[i] * scale1;
        float q2 = f2[i] * scale2;
        if (q1 > 32767.0f)
            q1 = 32767.0f;
        if (q1 < -32768.0f)
            q1 = -32768.0f;
        if (q2 > 32767.0f)
            q2 = 32767.0f;
        if (q2 < -32768.0f)
            q2 = -32768.0f;

        /* Avoid lrintf to keep WASM-side dependencies minimal. */
        if (q1 >= 0.0f)
            q15_input1_buf[i] = (short)(q1 + 0.5f);
        else
            q15_input1_buf[i] = (short)(q1 - 0.5f);

        if (q2 >= 0.0f)
            q15_input2_buf[i] = (short)(q2 + 0.5f);
        else
            q15_input2_buf[i] = (short)(q2 - 0.5f);
    }
}
#endif

static void
init_buffers(int len)
{
    int seed_len = (int)(sizeof(f32_input1) / sizeof(f32_input1[0]));
    if (len > MAX_BENCH_LEN)
        len = MAX_BENCH_LEN;

    for (int i = 0; i < len; i++) {
        input1_buf[i] = f32_input1[i % seed_len];
        input2_buf[i] = f32_input2[i % seed_len];
    }
}

static const bench_case_t bench_cases[] = {
    /*  name,  vec_len,  mat(m,k,n),    img(H,W,kH,kW) */
    { "N=4",       4,   2,  2,  2,      4,  4, 3, 3 },
    { "N=16",     16,   4,  4,  4,      4,  4, 3, 3 },
    { "N=64",     64,   8,  8,  8,      8,  8, 3, 3 },
    { "N=256",   256,  16, 16, 16,     16, 16, 3, 3 },
    { "N=1024", 1024,  32, 32, 32,     32, 32, 3, 3 },
};

int
main(int argc, char **argv)
{
    size_t num_cases = sizeof(bench_cases) / sizeof(bench_cases[0]);

    for (size_t ci = 0; ci < num_cases; ci++) {
        const bench_case_t *c = &bench_cases[ci];
        int needed = c->vec_len;

        if (c->mat_m * c->mat_k > needed)
            needed = c->mat_m * c->mat_k;
        if (c->mat_k * c->mat_n > needed)
            needed = c->mat_k * c->mat_n;
        if (c->img_h * c->img_w > needed)
            needed = c->img_h * c->img_w;
        if (c->k_h * c->k_w > needed)
            needed = c->k_h * c->k_w;

        if (needed > MAX_BENCH_LEN) {
            continue;
        }

        init_buffers(needed);

        printf("\n=== Benchmark case: %s ===\n", c->name);

        unsigned int start, end;

#if defined(THESIS_TARGET_M4) || defined(THESIS_TARGET_RISCV32)
        /* Prepare q15 inputs for fixed-point kernels on MCU/RISC-V. */
        prepare_q15_buffers(needed);

        start = thesis_get_ticks();
        vec_abs((int *)input1_buf, c->vec_len);
        end = thesis_get_ticks();
        printf("[vec_abs][app] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        vec_add_q15((int *)q15_input1_buf, c->vec_len,
            (int *)q15_input2_buf, c->vec_len);
        end = thesis_get_ticks();
        printf("[vec_add_q15][app] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        vec_mul_q15((int *)q15_input1_buf, c->vec_len,
            (int *)q15_input2_buf, c->vec_len);
        end = thesis_get_ticks();
        printf("[vec_mul_q15][app] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        dot_product_q15((int *)q15_input1_buf, c->vec_len,
            (int *)q15_input2_buf, c->vec_len);
        end = thesis_get_ticks();
        printf("[dot_product_q15][app] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        mat_mul_q15((int *)q15_input1_buf, c->mat_m, c->mat_k,
            (int *)q15_input2_buf, c->mat_k, c->mat_n);
        end = thesis_get_ticks();
        printf("[mat_mul_q15][app] Elapsed ticks: %u (m=%d,k=%d,n=%d)\n",
               end - start, c->mat_m, c->mat_k, c->mat_n);

        start = thesis_get_ticks();
        conv2d_small((int *)input1_buf, c->img_h, c->img_w,
             (int *)input2_buf, c->k_h, c->k_w,
             (int *)scratch_buf);
        end = thesis_get_ticks();
        printf("[conv2d_small][app] Elapsed ticks: %u (H=%d,W=%d,kH=%d,kW=%d)\n",
               end - start, c->img_h, c->img_w, c->k_h, c->k_w);

        start = thesis_get_ticks();
        softmax((int *)input1_buf, c->vec_len, (int *)scratch_buf);
        end = thesis_get_ticks();
        printf("[softmax][app] Elapsed ticks: %u (len=%d)\n",
               end - start, c->vec_len);

        start = thesis_get_ticks();
        rmsnorm((int *)input1_buf, c->vec_len, (int *)scratch_buf);
        end = thesis_get_ticks();
        printf("[rmsnorm][app] Elapsed ticks: %u (len=%d)\n",
               end - start, c->vec_len);
#else
        start = thesis_get_ticks();
        vec_abs((int *)input1_buf, c->vec_len);
        end = thesis_get_ticks();
        printf("[vec_abs][app] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        vec_add((int *)input1_buf, c->vec_len,
            (int *)input2_buf, c->vec_len);
        end = thesis_get_ticks();
        printf("[vec_add][app] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        vec_mul((int *)input1_buf, c->vec_len,
            (int *)input2_buf, c->vec_len);
        end = thesis_get_ticks();
        printf("[vec_mul][app] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        dot_product((int *)input1_buf, c->vec_len,
            (int *)input2_buf, c->vec_len);
        end = thesis_get_ticks();
        printf("[dot_product][app] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        mat_mul((int *)input1_buf, c->mat_m, c->mat_k,
            (int *)input2_buf, c->mat_k, c->mat_n);
        end = thesis_get_ticks();
        printf("[mat_mul][app] Elapsed ticks: %u (m=%d,k=%d,n=%d)\n",
               end - start, c->mat_m, c->mat_k, c->mat_n);

        start = thesis_get_ticks();
        conv2d_small((int *)input1_buf, c->img_h, c->img_w,
             (int *)input2_buf, c->k_h, c->k_w,
             (int *)scratch_buf);
        end = thesis_get_ticks();
        printf("[conv2d_small][app] Elapsed ticks: %u (H=%d,W=%d,kH=%d,kW=%d)\n",
               end - start, c->img_h, c->img_w, c->k_h, c->k_w);

        start = thesis_get_ticks();
        softmax((int *)input1_buf, c->vec_len, (int *)scratch_buf);
        end = thesis_get_ticks();
        printf("[softmax][app] Elapsed ticks: %u (len=%d)\n",
               end - start, c->vec_len);

        start = thesis_get_ticks();
        rmsnorm((int *)input1_buf, c->vec_len, (int *)scratch_buf);
        end = thesis_get_ticks();
        printf("[rmsnorm][app] Elapsed ticks: %u (len=%d)\n",
               end - start, c->vec_len);
#endif

    }

    return 0;
}
