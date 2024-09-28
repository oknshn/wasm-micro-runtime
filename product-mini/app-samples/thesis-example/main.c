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
#ifdef __GNUC__
    printf("GCC version: %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
 
    struct timespec start, end;
    double elapsed_time;
    int res;
    int x = 2, y = 6;
    //clock_gettime(CLOCK_MONOTONIC, &start);
    res = get_pow(x, y);
    //clock_gettime(CLOCK_MONOTONIC, &end);
    //elapsed_time = (end.tv_nsec - start.tv_nsec);
    //printf("Test get_pow(%d, %d) = %d Elapsed Time = %.9f\n", x, y, res, elapsed_time);
    printf("Res = %d\n", res);
    return 0;
}
