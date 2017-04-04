#ifndef IIR_H
#define IIR_H

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
 * Implementation of Infinite Response Filter.
*/

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    const size_t sections;

    const float gain;
    const float *const b; // 3 coeff
    const float *const a; // 3 coeff, first of these is assumed to be 1

    float *hist; // 3 taps
} IIRF_FILTER;

float iirf(float sample, IIRF_FILTER *filter);

float iirf(float sample, IIRF_FILTER *filter)
{
    float *hist;
    const float *a, *b;
    size_t i;

    // sample will be "y"

    for (i = 0; i < filter->sections; i++)
    {
        a = filter->a + 3 * i;
        b = filter->b + 3 * i;
        hist = filter->hist + 3 * i;

#if 0
        hist[0] = -sample;
        hist[0] = -(a[0]*hist[0] + a[1]*hist[1] + a[2]*hist[2]);
#else
        hist[0] = sample - (a[1]*hist[1] + a[2]*hist[2]);
#endif
        sample = b[0]*hist[0]  + b[1]*hist[1] + b[2]*hist[2];
        hist[2] = hist[1];
        hist[1] = hist[0];
    }

    sample *= filter->gain;

    return sample;
}

#endif /* IIR_H */

