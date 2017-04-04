#ifndef MOVING_AVERAGE_FILTER_H
#define MOVING_AVERAGE_FILTER_H

/*-
 * Copyright (c) 2017 <xael.south@yandex.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Moving average filter implementation.
*/

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    size_t i;
    int *hist;

    const size_t length;
    int sum;
} MAVGI_FILTER;

float mavgi(int sample, MAVGI_FILTER *filter);

float mavgi(int sample, MAVGI_FILTER *filter)
{
    filter->sum = filter->sum - filter->hist[filter->i] + sample;
    filter->hist[filter->i++] = sample;
    if (filter->i >= filter->length) filter->i = 0;

    return (float)filter->sum / filter->length;
}

#if 0
static int test_mavgi(void)
{
#define COEFFS 5
    static int hist[COEFFS];

    static MAVGI_FILTER filter = { .length = COEFFS, .hist = hist };
#undef COEFFS

    for (int sample = 0; sample < 20; sample++)
    {
        printf("%f, ", mavgi(sample, &filter));
    }
    printf("\n");

    return 0;
}
#endif

#endif /* MOVING_AVERAGE_FILTER_H */

