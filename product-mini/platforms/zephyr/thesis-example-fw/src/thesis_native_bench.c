#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

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
				    int *B, int k2, int n);
extern void conv2d_small_wrapper(wasm_exec_env_t exec_env, int *input,
					 int in_h, int in_w, int *kernel,
					 int k_h, int k_w, int *output);
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
 * Match the WASM apps: N up to 1024.
 */
#define MAX_BENCH_LEN 1024

static unsigned int input1_buf[MAX_BENCH_LEN];
static unsigned int input2_buf[MAX_BENCH_LEN];
static unsigned int scratch_buf[MAX_BENCH_LEN];

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

/* Same benchmark shapes as the WASM apps. */
static const bench_case_t bench_cases[] = {
	/*  name,  vec_len,  mat(m,k,n),    img(H,W,kH,kW) */
	{ "N=4",       4,   2,  2,  2,      4,  4, 3, 3 },
	{ "N=16",     16,   4,  4,  4,      4,  4, 3, 3 },
	{ "N=64",     64,   8,  8,  8,      8,  8, 3, 3 },
	{ "N=256",   256,  16, 16, 16,     16, 16, 3, 3 },
	{ "N=1024", 1024,  32, 32, 32,     32, 32, 3, 3 },
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

		if (needed > MAX_BENCH_LEN)
			continue;

		init_buffers(needed);

		printk("\n=== Benchmark case (native): %s ===\n", c->name);

		mem_time_t start, end;

		/* Float32 kernels, same as default path in the WASM app. */
		start = get_count();
		vec_abs_wrapper(NULL, (int *)input1_buf, c->vec_len);
		end = get_count();
		if (end < start)
			end = start;
		printk("[vec_abs][native] Elapsed ticks: %u\n",
		       (unsigned int)(end - start));

		start = get_count();
		vec_add_wrapper(NULL, (int *)input1_buf, c->vec_len,
			       (int *)input2_buf, c->vec_len);
		end = get_count();
		if (end < start)
			end = start;
		printk("[vec_add][native] Elapsed ticks: %u\n",
		       (unsigned int)(end - start));

		start = get_count();
		vec_mul_wrapper(NULL, (int *)input1_buf, c->vec_len,
			       (int *)input2_buf, c->vec_len);
		end = get_count();
		if (end < start)
			end = start;
		printk("[vec_mul][native] Elapsed ticks: %u\n",
		       (unsigned int)(end - start));

		start = get_count();
		dot_product_wrapper(NULL, (int *)input1_buf, c->vec_len,
				       (int *)input2_buf, c->vec_len);
		end = get_count();
		if (end < start)
			end = start;
		printk("[dot_product][native] Elapsed ticks: %u\n",
		       (unsigned int)(end - start));

		start = get_count();
		mat_mul_wrapper(NULL, (int *)input1_buf, c->mat_m, c->mat_k,
			       (int *)input2_buf, c->mat_k, c->mat_n);
		end = get_count();
		if (end < start)
			end = start;
		printk("[mat_mul][native] Elapsed ticks: %u (m=%d,k=%d,n=%d)\n",
		       (unsigned int)(end - start),
		       c->mat_m, c->mat_k, c->mat_n);

		start = get_count();
		conv2d_small_wrapper(NULL, (int *)input1_buf, c->img_h, c->img_w,
				      (int *)input2_buf, c->k_h, c->k_w,
				      (int *)scratch_buf);
		end = get_count();
		if (end < start)
			end = start;
		printk("[conv2d_small][native] Elapsed ticks: %u (H=%d,W=%d,kH=%d,kW=%d)\n",
		       (unsigned int)(end - start),
		       c->img_h, c->img_w, c->k_h, c->k_w);

		start = get_count();
		softmax_wrapper(NULL, (int *)input1_buf, c->vec_len,
			       (int *)scratch_buf);
		end = get_count();
		if (end < start)
			end = start;
		printk("[softmax][native] Elapsed ticks: %u (len=%d)\n",
		       (unsigned int)(end - start), c->vec_len);

		start = get_count();
		rmsnorm_wrapper(NULL, (int *)input1_buf, c->vec_len,
			       (int *)scratch_buf);
		end = get_count();
		if (end < start)
			end = start;
		printk("[rmsnorm][native] Elapsed ticks: %u (len=%d)\n",
		       (unsigned int)(end - start), c->vec_len);
	}
}
