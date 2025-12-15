#include <stdio.h>
#include <math.h>

#include "thesis-lib.h"

/*
 * Pure WASM implementations of the benchmark kernels.
 * These run fully inside the WASM module and use thesis_get_ticks()
 * for timing, matching the counter used by the native side.
 */

#define MAX_BENCH_LEN 1024

typedef struct {
    const char *name;
    int vec_len;
    int mat_m, mat_k, mat_n;
    int img_h, img_w, k_h, k_w;
} bench_case_t;

static unsigned int input1_buf[MAX_BENCH_LEN];
static unsigned int input2_buf[MAX_BENCH_LEN];
static unsigned int scratch_buf[MAX_BENCH_LEN];
static volatile float wasm_sink; /* keep a visible side-effect */

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

/* ===================== Pure WASM kernels ===================== */

static void
wasm_vec_abs(float *x, int len)
{
    for (int i = 0; i < len; i++) {
        float v = x[i];
        x[i] = v < 0.0f ? -v : v;
    }
}

static void
wasm_vec_add(float *a, float *b, int len)
{
    for (int i = 0; i < len; i++)
        a[i] = a[i] + b[i];
}

static void
wasm_vec_mul(float *a, float *b, int len)
{
    for (int i = 0; i < len; i++)
        a[i] = a[i] * b[i];
}

static float
wasm_dot_product(float *a, float *b, int len)
{
    float acc = 0.0f;
    for (int i = 0; i < len; i++)
        acc += a[i] * b[i];
    return acc;
}

static void
wasm_mat_mul(float *A, int m, int k, float *B, int k2, int n)
{
    if (k2 != k)
        return;

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float acc = 0.0f;
            for (int kk = 0; kk < k; kk++) {
                float a_ik = A[i * k + kk];
                float b_kj = B[kk * n + j];
                acc += a_ik * b_kj;
            }
            ((float *)scratch_buf)[i * n + j] = acc;
        }
    }
}

static void
wasm_conv2d_small(float *img, int in_h, int in_w,
                  float *ker, int k_h, int k_w,
                  float *out)
{
    int out_h = in_h - k_h + 1;
    int out_w = in_w - k_w + 1;
    if (out_h <= 0 || out_w <= 0)
        return;

    for (int i = 0; i < out_h; i++) {
        for (int j = 0; j < out_w; j++) {
            float acc = 0.0f;
            for (int kh = 0; kh < k_h; kh++) {
                for (int kw = 0; kw < k_w; kw++) {
                    int ih = i + kh;
                    int iw = j + kw;
                    acc += img[ih * in_w + iw]
                           * ker[kh * k_w + kw];
                }
            }
            out[i * out_w + j] = acc;
        }
    }
}

/* Polynomial exp approximation: 1 + x + x^2/2 + x^3/6 + x^4/24.
 * We apply it to (x - max), so values are typically close to 0.
 */
static float
wasm_exp_approx(float x)
{
    float x2 = x * x;
    float x3 = x2 * x;
    float x4 = x2 * x2;
    return 1.0f + x + 0.5f * x2 + (1.0f / 6.0f) * x3 + (1.0f / 24.0f) * x4;
}

static void
wasm_softmax(float *x, int len, float *y)
{
    if (len <= 0)
        return;

    float max_val = x[0];
    for (int i = 1; i < len; i++) {
        if (x[i] > max_val)
            max_val = x[i];
    }

    float sum = 0.0f;
    for (int i = 0; i < len; i++) {
        float e = wasm_exp_approx(x[i] - max_val);
        y[i] = e;
        sum += e;
    }

    if (sum > 0.0f) {
        float inv_sum = 1.0f / sum;
        for (int i = 0; i < len; i++)
            y[i] *= inv_sum;
    }
}

/* RMSNorm: y = x / rms(x), where rms(x) = sqrt(mean(x^2)). */
static float
wasm_sqrt_approx(float x)
{
    if (x <= 0.0f)
        return 0.0f;

    float y = x;
    for (int i = 0; i < 5; i++)
        y = 0.5f * (y + x / y);

    return y;
}

static void
wasm_rmsnorm(float *x, int len, float *y)
{
    if (len <= 0)
        return;

    float sum_sq = 0.0f;
    for (int i = 0; i < len; i++) {
        float v = x[i];
        sum_sq += v * v;
    }

    float mean_sq = sum_sq / (float)len;
    float rms = wasm_sqrt_approx(mean_sq);
    if (rms == 0.0f)
        rms = 1.0f;

    float inv_rms = 1.0f / rms;
    for (int i = 0; i < len; i++)
        y[i] = x[i] * inv_rms;
}

/* ===================== Benchmark driver ===================== */

static const bench_case_t bench_cases[] = {
    /*  name,  vec_len,  mat(m,k,n),    img(H,W,kH,kW) */
    { "N=4",       4,    2,  2,  2,      4,  4, 3, 3 },
    { "N=16",     16,    4,  4,  4,      4,  4, 3, 3 },
    { "N=64",     64,    8,  8,  8,      8,  8, 3, 3 },
    { "N=256",   256,   16, 16, 16,     16, 16, 3, 3 },
    { "N=1024", 1024,   32, 32, 32,     32, 32, 3, 3 },
};

int
main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

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

        if (needed > MAX_BENCH_LEN)
            continue;

        init_buffers(needed);

        float *f_in1 = (float *)input1_buf;
        float *f_in2 = (float *)input2_buf;
        float *f_scratch = (float *)scratch_buf;

        printf("\n=== Benchmark case (pure WASM): %s ===\n", c->name);

        unsigned int start, end;

        start = thesis_get_ticks();
        wasm_vec_abs(f_in1, c->vec_len);
        end = thesis_get_ticks();
        printf("[vec_abs][wasm] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        wasm_vec_add(f_in1, f_in2, c->vec_len);
        end = thesis_get_ticks();
        printf("[vec_add][wasm] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        wasm_vec_mul(f_in1, f_in2, c->vec_len);
        end = thesis_get_ticks();
        printf("[vec_mul][wasm] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        /* Store into wasm_sink so the compiler keeps the loop. */
        wasm_sink += wasm_dot_product(f_in1, f_in2, c->vec_len);
        end = thesis_get_ticks();
        printf("[dot_product][wasm] Elapsed ticks: %u\n", end - start);

        start = thesis_get_ticks();
        wasm_mat_mul(f_in1, c->mat_m, c->mat_k,
                     f_in2, c->mat_k, c->mat_n);
        end = thesis_get_ticks();
        printf("[mat_mul][wasm] Elapsed ticks: %u (m=%d,k=%d,n=%d)\n",
               end - start, c->mat_m, c->mat_k, c->mat_n);

        start = thesis_get_ticks();
        wasm_conv2d_small(f_in1, c->img_h, c->img_w,
                          f_in2, c->k_h, c->k_w,
                          f_scratch);
        end = thesis_get_ticks();
        printf("[conv2d_small][wasm] Elapsed ticks: %u (H=%d,W=%d,kH=%d,kW=%d)\n",
               end - start, c->img_h, c->img_w, c->k_h, c->k_w);

        start = thesis_get_ticks();
        wasm_softmax(f_in1, c->vec_len, f_scratch);
        end = thesis_get_ticks();
        printf("[softmax][wasm] Elapsed ticks: %u (len=%d)\n",
               end - start, c->vec_len);

        start = thesis_get_ticks();
        wasm_rmsnorm(f_in1, c->vec_len, f_scratch);
        end = thesis_get_ticks();
        printf("[rmsnorm][wasm] Elapsed ticks: %u (len=%d)\n",
               end - start, c->vec_len);
    }

    return 0;
}
