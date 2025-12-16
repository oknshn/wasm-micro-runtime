/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "thesis-lib.h"

/* Define USE_Q15 for MCU targets (MAX32690, ESP32-C3).
 * Undefine or comment out for QEMU (uses float32).
 */

//#define USE_Q15
typedef struct {
    const char *name;
    int vec_len;
    int mat_m, mat_k, mat_n;
    int img_h, img_w, k_h, k_w;
} bench_case_t;

#define MAX_BENCH_LEN 1024
#define ITERATIONS 100
static float input1_buf[MAX_BENCH_LEN];
static float input2_buf[MAX_BENCH_LEN];
static float scratch_buf[MAX_BENCH_LEN];

/* Q15 buffers for vec_abs, vec_add, vec_mul, dot_product, mat_mul
 * (used by native Q15 kernels on THUMB/RISCV).
 */
static int16_t input1_q15[MAX_BENCH_LEN];
static int16_t input2_q15[MAX_BENCH_LEN];
static int16_t scratch_q15[MAX_BENCH_LEN];

static float ref_buf[MAX_BENCH_LEN];
static volatile float ref_sink;
static volatile int32_t ref_sink_q15;

/* Int8 buffers for conv2d and softmax (used by native int8 kernels). */
static int8_t conv_input_s8[MAX_BENCH_LEN];
static int8_t conv_kernel_s8[MAX_BENCH_LEN];
static int8_t conv_out_s8[MAX_BENCH_LEN];
static int8_t softmax_in_s8[MAX_BENCH_LEN];
static int8_t softmax_out_s8[MAX_BENCH_LEN];
static int8_t conv_ref_s8[MAX_BENCH_LEN];
static int8_t softmax_ref_s8[MAX_BENCH_LEN];
static uint8_t conv_scratch_buf[48];

static int8_t
quantize_f32_to_s8(float v, float scale)
{
    float scaled = v * scale;
    int q = (int)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
    if (q > 127)
        q = 127;
    else if (q < -128)
        q = -128;
    return (int8_t)q;
}

static int16_t
quantize_f32_to_q15(float v, float scale)
{
    float scaled = v * scale;
    int q = (int)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
    if (q > 32767)
        q = 32767;
    else if (q < -32768)
        q = -32768;
    return (int16_t)q;
}

static float
thesis_expf_approx(float x)
{
    return 1.0f + x * (1.0f + x * (0.5f + x * ((1.0f/6.0f) + x * (1.0f/24.0f))));
}

static void
ref_vec_abs(float *data, int len)
{
    for (int i = 0; i < len; i++) {
        float v = fabsf(data[i]);
        data[i] = v;
    }
}

static void
compare_vec_f32(const char *name, const float *exp, const float *got,
                int len, float tol)
{
    int mismatches = 0;
    float max_diff = 0.0f;

    for (int i = 0; i < len; i++) {
        float d = fabsf(exp[i] - got[i]);
        if (d > tol) {
            mismatches++;
            if (d > max_diff)
                max_diff = d;
        }
    }

    if (mismatches == 0) {
        return;
/*
        printf("[%s][validate] PASS (len=%d, tol=%g)\n",
               name, len, (double)tol);
*/
    }
    else {
        printf("[%s][validate] FAIL: mismatches=%d, max_diff=%g (tol=%g)\n",
               name, mismatches, (double)max_diff, (double)tol);
    }
}

static void
compare_vec_s8(const char *name, const int8_t *exp, const int8_t *got,
               int len, int tol)
{
    int mismatches = 0;
    int max_diff = 0;

    for (int i = 0; i < len; i++) {
        int d = (int)exp[i] - (int)got[i];
        if (d < 0)
            d = -d;
        if (d > tol) {
            mismatches++;
            if (d > max_diff)
                max_diff = d;
        }
    }

    if (mismatches == 0) {
        return;
    }

    printf("[%s][validate_s8] FAIL: mismatches=%d, max_abs_diff=%d (tol=%d)\n",
           name, mismatches, max_diff, tol);
}

static void
ref_vec_add(float *a, int len_a, const float *b, int len_b)
{
    int n = len_a;
    (void)len_b;
    for (int i = 0; i < n; i++) {
        a[i] = a[i] + b[i];
    }
}

static void
ref_vec_mul(float *a, int len_a, const float *b, int len_b)
{
    int n = len_a;
    (void)len_b;
    for (int i = 0; i < n; i++) {
        a[i] = a[i] * b[i];
    }
}

static float
ref_dot_product(float *a, int len_a, const float *b, int len_b)
{
    int n = len_a;
    (void)len_b;
    float acc = 0.0f;
    for (int i = 0; i < n; i++) {
        acc += a[i] * b[i];
    }
    a[0] = acc;
    return acc;
}

static void
ref_mat_mul(const float *A, const float *B, float *C,
            int m, int k, int n)
{
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int kk = 0; kk < k; kk++) {
                sum += A[i * k + kk] * B[kk * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

static void
ref_rmsnorm(const float *x, float *y, int len)
{
    const float eps = 1e-5f;
    float sum_sq = 0.0f;

    for (int i = 0; i < len; i++) {
        sum_sq += x[i] * x[i];
    }

    float mean_sq = sum_sq / (float)len;
    float denom = sqrtf(mean_sq + eps);
    if (denom <= 0.0f)
        denom = 1.0f;
    float inv_denom = 1.0f / denom;

    for (int i = 0; i < len; i++) {
        float v = x[i] * inv_denom;
        y[i] = v;
    }
}

/* Q15 reference functions for MCU targets. */
static void
ref_vec_abs_q15(int16_t *a, int len)
{
    for (int i = 0; i < len; i++) {
        if (a[i] < 0)
            a[i] = (a[i] == -32768) ? 32767 : -a[i];
    }
}

static void
ref_vec_add_q15(int16_t *a, int len_a, const int16_t *b, int len_b)
{
    int n = len_a;
    (void)len_b;
    for (int i = 0; i < n; i++) {
        int32_t sum = (int32_t)a[i] + (int32_t)b[i];
        if (sum > 32767) sum = 32767;
        else if (sum < -32768) sum = -32768;
        a[i] = (int16_t)sum;
    }
}

static void
ref_vec_mul_q15(int16_t *a, int len_a, const int16_t *b, int len_b)
{
    int n = len_a;
    (void)len_b;
    for (int i = 0; i < n; i++) {
        int32_t prod = ((int32_t)a[i] * (int32_t)b[i]) >> 15;
        if (prod > 32767) prod = 32767;
        else if (prod < -32768) prod = -32768;
        a[i] = (int16_t)prod;
    }
}

static int32_t
ref_dot_product_q15(int16_t *a, int len_a, const int16_t *b, int len_b)
{
    int n = len_a;
    (void)len_b;
    int64_t acc = 0;
    for (int i = 0; i < n; i++) {
        acc += (int32_t)a[i] * (int32_t)b[i];
    }
    int32_t result = (int32_t)(acc >> 15);
    if (result > 32767) result = 32767;
    else if (result < -32768) result = -32768;
    a[0] = (int16_t)result;
    return result;
}

static void
ref_mat_mul_q15(const int16_t *A, const int16_t *B, int16_t *C,
                int m, int k, int n)
{
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            int64_t sum = 0;
            for (int kk = 0; kk < k; kk++) {
                sum += (int32_t)A[i * k + kk] * (int32_t)B[kk * n + j];
            }
            int32_t result = (int32_t)(sum >> 15);
            if (result > 32767) result = 32767;
            else if (result < -32768) result = -32768;
            C[i * n + j] = (int16_t)result;
            ref_sink_q15 += result;
        }
    }
}

static void
ref_rmsnorm_q15(const int16_t *x, int16_t *y, int len)
{
    /* Step 1: Compute sum of squares in Q30 (Q15 * Q15 = Q30). */
    int64_t sum_sq = 0;
    for (int i = 0; i < len; i++) {
        sum_sq += (int32_t)x[i] * (int32_t)x[i];
    }

    /* Step 2: Compute mean_sq and convert to float for sqrt. */
    float mean_sq = (float)(sum_sq >> 15) / (float)len / 32768.0f;

    /* Step 3: Compute RMS = sqrt(mean_sq). */
    float rms = sqrtf(mean_sq);
    if (rms < 1e-10f)
        rms = 1e-10f; /* Avoid division by zero */

    /* Step 4: Compute scale = 1/rms in Q15. */
    int32_t scale_q15 = (int32_t)(32768.0f / rms);
    if (scale_q15 > 32767)
        scale_q15 = 32767;

    /* Step 5: Scale each element: y = x * scale (Q15 * Q15 >> 15 = Q15). */
    for (int i = 0; i < len; i++) {
        int32_t prod = ((int32_t)x[i] * scale_q15) >> 15;
        if (prod > 32767)
            prod = 32767;
        else if (prod < -32768)
            prod = -32768;
        y[i] = (int16_t)prod;
    }
}

static void
ref_conv2d_s8(const int8_t *img, int in_h, int in_w,
              const int8_t *ker, int k_h, int k_w,
              int8_t *out)
{
    int out_h = in_h - k_h + 1;
    int out_w = in_w - k_w + 1;

    for (int i = 0; i < out_h; i++) {
        for (int j = 0; j < out_w; j++) {
            int32_t acc = 0;
            for (int kh = 0; kh < k_h; kh++) {
                for (int kw = 0; kw < k_w; kw++) {
                    int ih = i + kh;
                    int iw = j + kw;
                    acc += (int32_t)img[ih * in_w + iw]
                           * (int32_t)ker[kh * k_w + kw];
                }
            }
            if (acc > 127)
                acc = 127;
            else if (acc < -128)
                acc = -128;
            out[i * out_w + j] = (int8_t)acc;
        }
    }
}

static void
ref_softmax_s8(const int8_t *input, int len, int8_t *output)
{
    /* Interpret int8 logits as floats and compute softmax,
     * then requantize probabilities back to Q7.
     */
    float max_val = (float)input[0];
    for (int i = 1; i < len; i++) {
        float v = (float)input[i];
        if (v > max_val)
            max_val = v;
    }

    float sum = 0.0f;
    float tmp[MAX_BENCH_LEN];

    for (int i = 0; i < len; i++) {
        float e = expf((float)input[i] - max_val);
        tmp[i] = e;
        sum += e;
    }

    if (sum <= 0.0f) {
        for (int i = 0; i < len; i++)
            output[i] = 0;
        return;
    }

    float inv_sum = 1.0f / sum;
    for (int i = 0; i < len; i++) {
        float p = tmp[i] * inv_sum; /* in [0,1] */
        float q = p * 127.0f;
        int qi = (int)(q >= 0.0f ? q + 0.5f : q - 0.5f);
        if (qi > 127)
            qi = 127;
        else if (qi < -128)
            qi = -128;
        output[i] = (int8_t)qi;
    }
}


static void
init_buffers(int len)
{
    int seed_len = (int)(sizeof(f32_input1) / sizeof(f32_input1[0]));
    const float *seed1 = (const float *)f32_input1;
    const float *seed2 = (const float *)f32_input2;

    for (int i = 0; i < len; i++) {
        float v1 = seed1[i % seed_len];
        float v2 = seed2[i % seed_len];

        input1_buf[i] = v1;
        input2_buf[i] = v2;

        /* Pre-quantize Q15 inputs for vec_abs, vec_add, vec_mul,
         * dot_product, mat_mul on THUMB/RISCV platforms.
         */
        input1_q15[i] = quantize_f32_to_q15(v1, 16384.0f);
        input2_q15[i] = quantize_f32_to_q15(v2, 16384.0f);

        /* Pre-quantize int8 inputs for conv2d and softmax so that
         * the native int8 kernels can be called directly without
         * any per-call quantization overhead.
         */
        conv_input_s8[i] = quantize_f32_to_s8(v1, 32.0f);
        conv_kernel_s8[i] = quantize_f32_to_s8(v2, 32.0f);
        softmax_in_s8[i] = quantize_f32_to_s8(v1, 16.0f);
    }
}

static const bench_case_t bench_cases[] = {
    /*  name,  vec_len (vector N),   mat(m,k,n) with total size = vec_len, img(H,W,kH,kW) */
    /* Matrices are square with dimension sqrt(N): 4->2x2, 16->4x4, 64->8x8, 256->16x16, 1024->32x32. */
    { "N=4",       4,    2,   2,   2,      2,   2, 1, 1 },
    { "N=16",     16,    4,   4,   4,      4,   4, 1, 1 },
    { "N=64",     64,    8,   8,   8,      8,   8, 1, 1 },
    { "N=256",   256,   16,  16,  16,     16,  16, 1, 1 },
    { "N=1024", 1024,   32,  32,  32,     32,  32, 1, 1 },
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

        init_buffers(needed);

        printf("\n=== Benchmark case: %s ===\n", c->name);

        unsigned int start, end;

         /* Pure WASM reference timings (scalar C compiled to WASM). */
         start = thesis_get_ticks();
         for (int i = 0; i < ITERATIONS; i++) {
    #ifdef USE_Q15
             ref_vec_abs_q15(input1_q15, c->vec_len);
    #else
             ref_vec_abs(input1_buf, c->vec_len);
    #endif
         }
         end = thesis_get_ticks();
         printf("[vec_abs_ref][app] Elapsed ticks: %u\n",
             (end - start) / ITERATIONS);

         start = thesis_get_ticks();
         for (int i = 0; i < ITERATIONS; i++) {
    #ifdef USE_Q15
             ref_vec_add_q15(input1_q15, c->vec_len, input2_q15, c->vec_len);
    #else
             ref_vec_add(input1_buf, c->vec_len, input2_buf, c->vec_len);
    #endif
         }
         end = thesis_get_ticks();
         printf("[vec_add_ref][app] Elapsed ticks: %u\n",
             (end - start) / ITERATIONS);

         start = thesis_get_ticks();
         for (int i = 0; i < ITERATIONS; i++) {
    #ifdef USE_Q15
             ref_vec_mul_q15(input1_q15, c->vec_len, input2_q15, c->vec_len);
    #else
             ref_vec_mul(input1_buf, c->vec_len, input2_buf, c->vec_len);
    #endif
         }
         end = thesis_get_ticks();
         printf("[vec_mul_ref][app] Elapsed ticks: %u\n",
             (end - start) / ITERATIONS);

         start = thesis_get_ticks();
         for (int i = 0; i < ITERATIONS; i++) {
    #ifdef USE_Q15
             ref_sink += ref_dot_product_q15(input1_q15, c->vec_len,
                               input2_q15, c->vec_len);
    #else
             ref_sink += ref_dot_product(input1_buf, c->vec_len,
                              input2_buf, c->vec_len);
    #endif
         }
         end = thesis_get_ticks();
         printf("[dot_product_ref][app] Elapsed ticks: %u\n",
             (end - start) / ITERATIONS);

         start = thesis_get_ticks();
         for (int i = 0; i < ITERATIONS; i++) {
    #ifdef USE_Q15
             ref_mat_mul_q15(input1_q15, input2_q15, scratch_q15,
                    c->mat_m, c->mat_k, c->mat_n);
    #else
             ref_mat_mul(input1_buf, input2_buf, scratch_buf,
                   c->mat_m, c->mat_k, c->mat_n);
    #endif
         }
         end = thesis_get_ticks();
         printf("[mat_mul_ref][app] Elapsed ticks: %u (m=%d,k=%d,n=%d)\n",
             (end - start) / ITERATIONS,
             c->mat_m, c->mat_k, c->mat_n);

         start = thesis_get_ticks();
         for (int i = 0; i < ITERATIONS; i++) {
    #ifdef USE_Q15
             ref_rmsnorm_q15(input1_q15, scratch_q15, c->vec_len);
    #else
             ref_rmsnorm(input1_buf, scratch_buf, c->vec_len);
    #endif
         }
         end = thesis_get_ticks();
         printf("[rmsnorm_ref][app] Elapsed ticks: %u (len=%d)\n",
             (end - start) / ITERATIONS, c->vec_len);

         /* Pure WASM reference timings for int8 conv2d and softmax. */
         {
             int out_h = c->img_h - c->k_h + 1;
             int out_w = c->img_w - c->k_w + 1;

             start = thesis_get_ticks();
             for (int i = 0; i < ITERATIONS; i++) {
              ref_conv2d_s8(conv_input_s8, c->img_h, c->img_w,
                      conv_kernel_s8, c->k_h, c->k_w,
                      conv_ref_s8);
             }
             end = thesis_get_ticks();
             printf("[conv2d_small_ref][app] Elapsed ticks: %u (H=%d,W=%d,kH=%d,kW=%d)\n",
                 (end - start) / ITERATIONS,
                 c->img_h, c->img_w, c->k_h, c->k_w);
         }

         start = thesis_get_ticks();
         for (int i = 0; i < ITERATIONS; i++) {
             ref_softmax_s8(softmax_in_s8, c->vec_len, softmax_ref_s8);
         }
         end = thesis_get_ticks();
         printf("[softmax_ref][app] Elapsed ticks: %u (len=%d)\n",
             (end - start) / ITERATIONS, c->vec_len);

    }

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

        init_buffers(needed);

        printf("\n=== Benchmark case: %s ===\n", c->name);

        unsigned int start, end;

        /* Timing for vec_abs. */
        start = thesis_get_ticks();
        for (int i = 0; i < ITERATIONS; i++) {
#ifdef USE_Q15
            vec_abs((int *)input1_q15, c->vec_len);
#else
            vec_abs((int *)input1_buf, c->vec_len);
#endif
        }
        end = thesis_get_ticks();
        printf("[vec_abs][app] Elapsed ticks: %u\n", (end - start) / ITERATIONS);

        /* Timing for vec_add. */
        start = thesis_get_ticks();
        for (int i = 0; i < ITERATIONS; i++) {
#ifdef USE_Q15
            vec_add((int *)input1_q15, c->vec_len,
                (int *)input2_q15, c->vec_len);
#else
            vec_add((int *)input1_buf, c->vec_len,
                (int *)input2_buf, c->vec_len);
#endif
        }
        end = thesis_get_ticks();
        printf("[vec_add][app] Elapsed ticks: %u\n",
               (end - start) / ITERATIONS);

        /* Timing for vec_mul. */
        start = thesis_get_ticks();
        for (int i = 0; i < ITERATIONS; i++) {
#ifdef USE_Q15
            vec_mul((int *)input1_q15, c->vec_len,
                (int *)input2_q15, c->vec_len);
#else
            vec_mul((int *)input1_buf, c->vec_len,
                (int *)input2_buf, c->vec_len);
#endif
        }
        end = thesis_get_ticks();
        printf("[vec_mul][app] Elapsed ticks: %u\n",
               (end - start) / ITERATIONS);

        /* Timing for dot_product. */
        start = thesis_get_ticks();
        for (int i = 0; i < ITERATIONS; i++) {
#ifdef USE_Q15
            dot_product((int *)input1_q15, c->vec_len,
                (int *)input2_q15, c->vec_len);
#else
            dot_product((int *)input1_buf, c->vec_len,
                (int *)input2_buf, c->vec_len);
#endif
        }
        end = thesis_get_ticks();
        printf("[dot_product][app] Elapsed ticks: %u\n",
               (end - start) / ITERATIONS);

        /* Timing for mat_mul. */
        start = thesis_get_ticks();
        for (int i = 0; i < ITERATIONS; i++) {
#ifdef USE_Q15
            mat_mul((int *)input1_q15, c->mat_m, c->mat_k,
                (int *)input2_q15, c->mat_k, c->mat_n,
                (int *)scratch_q15);
#else
            mat_mul((int *)input1_buf, c->mat_m, c->mat_k,
                (int *)input2_buf, c->mat_k, c->mat_n,
                (int *)scratch_buf);
#endif
        }
        end = thesis_get_ticks();
        printf("[mat_mul][app] Elapsed ticks: %u (m=%d,k=%d,n=%d)\n",
               (end - start) / ITERATIONS, c->mat_m, c->mat_k, c->mat_n);

        /* Timing for rmsnorm. */
        start = thesis_get_ticks();
        for (int i = 0; i < ITERATIONS; i++) {
#ifdef USE_Q15
            rmsnorm((int *)input1_q15, c->vec_len, (int *)scratch_q15);
#else
            rmsnorm((int *)input1_buf, c->vec_len, (int *)scratch_buf);
#endif
        }
        end = thesis_get_ticks();
        printf("[rmsnorm][app] Elapsed ticks: %u (len=%d)\n",
               (end - start) / ITERATIONS, c->vec_len);

        /* conv2d_small validation and timing using pre-quantized int8 inputs. */
        {
            int out_h = c->img_h - c->k_h + 1;
            int out_w = c->img_w - c->k_w + 1;
            int out_len = out_h * out_w;

            /* Reference int8 conv2d. */
            ref_conv2d_s8(conv_input_s8, c->img_h, c->img_w,
                          conv_kernel_s8, c->k_h, c->k_w,
                          conv_ref_s8);

            /* Wrapper conv2d. */
            conv2d_small((int *)conv_input_s8, c->img_h, c->img_w,
                         (int *)conv_kernel_s8, c->k_h, c->k_w,
                         (int *)conv_out_s8, (int *)conv_scratch_buf);

            compare_vec_s8("conv2d_small", conv_ref_s8, conv_out_s8,
                           out_len, 128);

            /* Timing. */
            start = thesis_get_ticks();
            for (int i = 0; i < ITERATIONS; i++) {
                conv2d_small((int *)conv_input_s8, c->img_h, c->img_w,
                             (int *)conv_kernel_s8, c->k_h, c->k_w,
                             (int *)conv_out_s8, (int *)conv_scratch_buf);
            }
            end = thesis_get_ticks();
            printf("[conv2d_small][app] Elapsed ticks: %u (H=%d,W=%d,kH=%d,kW=%d)\n",
                   (end - start) / ITERATIONS,
                   c->img_h, c->img_w, c->k_h, c->k_w);
        }

        /* softmax validation and timing using pre-quantized int8 inputs. */
        /* Reference int8 softmax. */
        ref_softmax_s8(softmax_in_s8, c->vec_len, softmax_ref_s8);

        /* Wrapper softmax. */
        softmax((int *)softmax_in_s8, c->vec_len,
                (int *)softmax_out_s8);

        compare_vec_s8("softmax", softmax_ref_s8, softmax_out_s8,
                       c->vec_len, 255);

        start = thesis_get_ticks();
        for (int i = 0; i < ITERATIONS; i++) {
            softmax((int *)softmax_in_s8, c->vec_len,
                    (int *)softmax_out_s8);
        }
        end = thesis_get_ticks();
        printf("[softmax][app] Elapsed ticks: %u (len=%d)\n",
               (end - start) / ITERATIONS, c->vec_len);
    }

    return 0;
}
