#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

/* Reuse the same synthetic input seeds and kernel API declarations
 * as the thesis WASM apps.
 */
#include "../../../../../product-mini/app-samples/thesis-example-cmdis-dsp-basicmath/thesis-lib.h"

/* Use the same cycle counter helper as lib_thesis_wrapper.c so that
 * native and WASM measurements are directly comparable.
 */
#include "../../../../../core/iwasm/libraries/lib-thesis/thesis_timer.h"

/* We call directly into the native wrappers used by WAMR. They don't
 * use the exec_env parameter for these kernels, so we pass NULL.
 */
#include "wasm_export.h"

extern void vec_abs_wrapper(wasm_exec_env_t exec_env, int *input, int len);
extern void vec_add_wrapper(wasm_exec_env_t exec_env, int *a, int len_a,
			      int *b, int len_b);
extern void vec_mul_wrapper(wasm_exec_env_t exec_env, int *a, int len_a,
			      int *b, int len_b);
extern void dot_product_wrapper(wasm_exec_env_t exec_env, int *a, int len_a,
				      int *b, int len_b);
extern void mat_mul_wrapper(wasm_exec_env_t exec_env, int *A, int m, int k,
			    int *B, int k2, int n, int *C);
extern void conv2d_small_wrapper(wasm_exec_env_t exec_env, int *input,
					 int in_h, int in_w, int *kernel,
					 int k_h, int k_w, int *output,
					 int *scratch_buf);
extern void softmax_wrapper(wasm_exec_env_t exec_env, int *input, int len,
				    int *output);
extern void rmsnorm_wrapper(wasm_exec_env_t exec_env, int *input, int len,
				    int *output);

typedef struct {
	const char *name;
	int vec_len;      /* length for vec_* */
	int mat_m, mat_k, mat_n;
	int img_h, img_w, k_h, k_w;
} bench_case_t;

/* Maximum length used across all benchmarks (elements).
 * Match the WASM apps: up to 1024 elements shared between
 * vector and matrix data (matrices use sqrt(N) x sqrt(N)).
 */
#define MAX_BENCH_LEN 1024
#define ITERATIONS 100

static float input1_buf[MAX_BENCH_LEN];
static float input2_buf[MAX_BENCH_LEN];
static float scratch_buf[MAX_BENCH_LEN];
static float ref_buf[MAX_BENCH_LEN];
static volatile float ref_sink_native;

/* Q15 buffers for vec_add, vec_mul, dot_product, mat_mul on THUMB/RISCV. */
static int16_t input1_q15[MAX_BENCH_LEN];
static int16_t input2_q15[MAX_BENCH_LEN];
static int16_t scratch_q15[MAX_BENCH_LEN];

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

static void
ref_vec_abs(float *data, int len)
{
	for (int i = 0; i < len; i++) {
		float v = data[i] < 0.0f ? -data[i] : data[i];
		data[i] = v;
		ref_sink_native += v;
	}
}

static void
compare_vec_f32(const char *name, const float *exp, const float *got,
		      int len, float tol)
{
	int mismatches = 0;
	float max_diff = 0.0f;

	for (int i = 0; i < len; i++) {
		float d = exp[i] - got[i];
		if (d < 0.0f)
			d = -d;
		if (d > tol) {
			mismatches++;
			if (d > max_diff)
				max_diff = d;
		}
	}

	if (mismatches == 0) {
		return 0;
		//printk("[%s][validate] PASS (len=%d)\n", name, len);
	} else {
		/* Scale max_diff to micro-units to avoid printing floats. */
		int max_diff_micro = (int)(max_diff * 1000000.0f);
		printk("[%s][validate] FAIL: mismatches=%d, max_diff_x1e-6=%d\n",
		       name, mismatches, max_diff_micro);
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

	printk("[%s][validate_s8] FAIL: mismatches=%d, max_abs_diff=%d (tol=%d)\n",
	       name, mismatches, max_diff, tol);
}

static void
ref_vec_add(float *a, int len_a, const float *b, int len_b)
{
	int n = len_a;
	(void)len_b;
	for (int i = 0; i < n; i++) {
		float v = a[i] + b[i];
		a[i] = v;
		ref_sink_native += v;
	}
}

static void
ref_vec_mul(float *a, int len_a, const float *b, int len_b)
{
	int n = len_a;
	(void)len_b;
	for (int i = 0; i < n; i++) {
		float v = a[i] * b[i];
		a[i] = v;
		ref_sink_native += v;
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
			ref_sink_native += sum;
		}
	}
}

static void
ref_rmsnorm(const float *x, float *y, int len)
{
	const float eps = 1e-5f;
	float sum_sq = 0.0f;

	for (int i = 0; i < len; i++) {
		float v = x[i];
		sum_sq += v * v;
	}

	float mean_sq = sum_sq / (float)len;
	float denom = sqrtf(mean_sq + eps);
	if (denom <= 0.0f)
		denom = 1.0f;
	float inv_denom = 1.0f / denom;

	for (int i = 0; i < len; i++) {
		float v = x[i] * inv_denom;
		y[i] = v;
		ref_sink_native += v;
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
		float p = tmp[i] * inv_sum;
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

		/* Pre-quantize Q15 inputs for vec_add, vec_mul, dot_product, mat_mul
		 * on THUMB/RISCV platforms.
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

/* Same benchmark shapes as the WASM apps.
 * Vector kernels use N in {4,16,64,256,1024} as their length.
 * Matrix kernels use square matrices with total size equal to
 * the vector size: 4->2x2, 16->4x4, 64->8x8, 256->16x16, 1024->32x32.
 */
static const bench_case_t bench_cases[] = {
	/*  name,  vec_len (vector N),  mat(m,k,n),     img(H,W,kH,kW) */
	{ "N=4",       4,    2,   2,   2,      2,   2, 1, 1 },
	{ "N=16",     16,    4,   4,   4,      4,   4, 1, 1 },
	{ "N=64",     64,    8,   8,   8,      8,   8, 1, 1 },
	{ "N=256",   256,   16,  16,  16,     16,  16, 1, 1 },
	{ "N=1024", 1024,   32,  32,  32,     32,  32, 1, 1 },
};

void
thesis_native_bench_run(void)
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

		printk("\n=== Benchmark case (native): %s ===\n", c->name);

		mem_time_t start, end;

		/* vec_abs timing (Q15 on THUMB/RISCV, float32 on AARCH64). */
#if defined(BUILD_TARGET_THUMB) || defined(BUILD_TARGET_RISCV32_ILP32)
		start = get_count();
		for (int i = 0; i < ITERATIONS; i++) {
			vec_abs_wrapper(NULL, (int *)input1_q15, c->vec_len);
		}
		end = get_count();
		if (end < start)
			end = start;
		printk("[vec_abs][native] Elapsed ticks: %u\n",
		       (unsigned int)((end - start) / ITERATIONS));
#else
		start = get_count();
		for (int i = 0; i < ITERATIONS; i++) {
			vec_abs_wrapper(NULL, (int *)input1_buf, c->vec_len);
		}
		end = get_count();
		if (end < start)
			end = start;
		printk("[vec_abs][native] Elapsed ticks: %u\n",
		       (unsigned int)((end - start) / ITERATIONS));
#endif

		/* vec_add timing (Q15 on THUMB/RISCV, float32 on AARCH64). */
#if defined(BUILD_TARGET_THUMB) || defined(BUILD_TARGET_RISCV32_ILP32)
		start = get_count();
		for (int i = 0; i < ITERATIONS; i++) {
			vec_add_wrapper(NULL, (int *)input1_q15, c->vec_len,
					(int *)input2_q15, c->vec_len);
		}
		end = get_count();
		if (end < start)
			end = start;
		printk("[vec_add][native] Elapsed ticks: %u\n",
		       (unsigned int)((end - start) / ITERATIONS));
#else
		start = get_count();
		for (int i = 0; i < ITERATIONS; i++) {
			vec_add_wrapper(NULL, (int *)input1_buf, c->vec_len,
					(int *)input2_buf, c->vec_len);
		}
		end = get_count();
		if (end < start)
			end = start;
		printk("[vec_add][native] Elapsed ticks: %u\n",
		       (unsigned int)((end - start) / ITERATIONS));
#endif

		/* vec_mul timing (Q15 on THUMB/RISCV, float32 on AARCH64). */
#if defined(BUILD_TARGET_THUMB) || defined(BUILD_TARGET_RISCV32_ILP32)
		start = get_count();
		for (int i = 0; i < ITERATIONS; i++) {
			vec_mul_wrapper(NULL, (int *)input1_q15, c->vec_len,
					(int *)input2_q15, c->vec_len);
		}
		end = get_count();
		if (end < start)
			end = start;
		printk("[vec_mul][native] Elapsed ticks: %u\n",
		       (unsigned int)((end - start) / ITERATIONS));
#else
		start = get_count();
		for (int i = 0; i < ITERATIONS; i++) {
			vec_mul_wrapper(NULL, (int *)input1_buf, c->vec_len,
					(int *)input2_buf, c->vec_len);
		}
		end = get_count();
		if (end < start)
			end = start;
		printk("[vec_mul][native] Elapsed ticks: %u\n",
		       (unsigned int)((end - start) / ITERATIONS));
#endif

		/* dot_product timing (Q15 on THUMB/RISCV, float32 on AARCH64). */
#if defined(BUILD_TARGET_THUMB) || defined(BUILD_TARGET_RISCV32_ILP32)
		start = get_count();
		for (int i = 0; i < ITERATIONS; i++) {
			dot_product_wrapper(NULL, (int *)input1_q15, c->vec_len,
					 (int *)input2_q15, c->vec_len);
		}
		end = get_count();
		if (end < start)
			end = start;
		printk("[dot_product][native] Elapsed ticks: %u\n",
		       (unsigned int)((end - start) / ITERATIONS));
#else
		start = get_count();
		for (int i = 0; i < ITERATIONS; i++) {
			dot_product_wrapper(NULL, (int *)input1_buf, c->vec_len,
					 (int *)input2_buf, c->vec_len);
		}
		end = get_count();
		if (end < start)
			end = start;
		printk("[dot_product][native] Elapsed ticks: %u\n",
		       (unsigned int)((end - start) / ITERATIONS));
#endif

		/* mat_mul timing (Q15 on THUMB/RISCV, float32 on AARCH64). */
#if defined(BUILD_TARGET_THUMB) || defined(BUILD_TARGET_RISCV32_ILP32)
		start = get_count();
		for (int i = 0; i < ITERATIONS; i++) {
			mat_mul_wrapper(NULL, (int *)input1_q15, c->mat_m, c->mat_k,
			       (int *)input2_q15, c->mat_k, c->mat_n,
			       (int *)scratch_q15);
		}
		end = get_count();
		if (end < start)
			end = start;
		printk("[mat_mul][native] Elapsed ticks: %u (m=%d,k=%d,n=%d)\n",
		       (unsigned int)((end - start) / ITERATIONS),
		       c->mat_m, c->mat_k, c->mat_n);
#else
		start = get_count();
		for (int i = 0; i < ITERATIONS; i++) {
			mat_mul_wrapper(NULL, (int *)input1_buf, c->mat_m, c->mat_k,
			       (int *)input2_buf, c->mat_k, c->mat_n,
			       (int *)scratch_buf);
		}
		end = get_count();
		if (end < start)
			end = start;
		printk("[mat_mul][native] Elapsed ticks: %u (m=%d,k=%d,n=%d)\n",
		       (unsigned int)((end - start) / ITERATIONS),
		       c->mat_m, c->mat_k, c->mat_n);
#endif

		/* rmsnorm timing - uses Q15 on THUMB/RISCV, float32 on AARCH64. */
		if (c->vec_len > 0) {
#if defined(BUILD_TARGET_THUMB) || defined(BUILD_TARGET_RISCV32_ILP32)
			start = get_count();
			for (int i = 0; i < ITERATIONS; i++) {
				rmsnorm_wrapper(NULL, (int *)input1_q15, c->vec_len,
				       (int *)scratch_q15);
			}
			end = get_count();
#else
			start = get_count();
			for (int i = 0; i < ITERATIONS; i++) {
				rmsnorm_wrapper(NULL, (int *)input1_buf, c->vec_len,
				       (int *)scratch_buf);
			}
			end = get_count();
#endif
			if (end < start)
				end = start;
			printk("[rmsnorm][native] Elapsed ticks: %u (len=%d)\n",
			       (unsigned int)((end - start) / ITERATIONS), c->vec_len);
		}

		/* conv2d_small validation and timing using pre-quantized int8 inputs. */
		{
			int out_h = c->img_h - c->k_h + 1;
			int out_w = c->img_w - c->k_w + 1;

			if (out_h > 0 && out_w > 0) {
				int out_len = out_h * out_w;

				/* Reference int8 conv2d. */
				ref_conv2d_s8(conv_input_s8, c->img_h, c->img_w,
				      conv_kernel_s8, c->k_h, c->k_w,
				      conv_ref_s8);

				/* Wrapper conv2d. */
				conv2d_small_wrapper(NULL, (int *)conv_input_s8,
						 c->img_h, c->img_w,
						 (int *)conv_kernel_s8,
						 c->k_h, c->k_w,
						 (int *)conv_out_s8,
						 (int *)conv_scratch_buf);

				compare_vec_s8("conv2d_small", conv_ref_s8, conv_out_s8,
				       out_len, 128);

				/* Timing. */
				start = get_count();
				for (int i = 0; i < ITERATIONS; i++) {
					conv2d_small_wrapper(NULL, (int *)conv_input_s8,
							 c->img_h, c->img_w,
							 (int *)conv_kernel_s8,
							 c->k_h, c->k_w,
							 (int *)conv_out_s8,
							 (int *)conv_scratch_buf);
				}
				end = get_count();
				if (end < start)
					end = start;
				printk("[conv2d_small][native] Elapsed ticks: %u (H=%d,W=%d,kH=%d,kW=%d)\n",
				       (unsigned int)((end - start) / ITERATIONS),
				       c->img_h, c->img_w, c->k_h, c->k_w);
			}
		}

		/* softmax validation and timing using pre-quantized int8 inputs. */
		if (c->vec_len > 0) {
			/* Reference int8 softmax. */
			ref_softmax_s8(softmax_in_s8, c->vec_len, softmax_ref_s8);

			/* Wrapper softmax. */
			softmax_wrapper(NULL, (int *)softmax_in_s8,
				       c->vec_len,
				       (int *)softmax_out_s8);

			compare_vec_s8("softmax", softmax_ref_s8, softmax_out_s8,
			       c->vec_len, 255);

			start = get_count();
			for (int i = 0; i < ITERATIONS; i++) {
				softmax_wrapper(NULL, (int *)softmax_in_s8,
					       c->vec_len,
					       (int *)softmax_out_s8);
			}
			end = get_count();
			if (end < start)
				end = start;
			printk("[softmax][native] Elapsed ticks: %u (len=%d)\n",
			       (unsigned int)((end - start) / ITERATIONS),
			       c->vec_len);
		}

	}
}
