/*
 * Copyright (C) 2019 Intel Corporation.  All rights reserved.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "thesis-lib.h"   // where the APIs are declared
 
int
main(int argc, char **argv)
{
    test_benchmark_vec_add_f32(f32_input1, sizeof(f32_input1), f32_input2, sizeof(f32_input2));
    test_benchmark_vec_sub_f32(f32_input1, sizeof(f32_input1), f32_input2, sizeof(f32_input2));
    test_benchmark_vec_mul_f32(f32_input1, sizeof(f32_input1), f32_input2, sizeof(f32_input2));
    test_benchmark_vec_abs_f32(f32_input1, sizeof(f32_input1));

    return 0;
}
