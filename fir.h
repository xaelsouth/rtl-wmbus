#ifndef FIR_H
#define FIR_H

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
 * Floating and fixed point implementations of Finite Response Filter.
*/

#include <stdint.h>
#include <stddef.h>
#include <fixedptc/fixedptc.h>

#include "rtlwmbus_settings.h"

#if FIR_SIMD && defined(__SSE__)
#include <xmmintrin.h>
#elif FIR_SIMD && defined(__ARM_NEON)
#include <arm_neon.h>
#endif

typedef struct
{
    const size_t length;
    float *const b;

    size_t i;
    float *hist;
} FIRF_FILTER;

float firf(float sample, FIRF_FILTER *filter);

float firf(float sample, FIRF_FILTER *filter)
{
    const float *b = filter->b;
    float *hist = &filter->hist[filter->i++];
    size_t i;

    *hist = sample;

    sample = 0; // will be "y"

    for (i = filter->i; i--;)
    {
        sample += *b++ * *hist--;
    }

    hist = &filter->hist[filter->length-1];
    for (i = filter->length; i-- > filter->i;)
    {
        sample += *b++ * *hist--;
    }

    if (filter->i >= filter->length) filter->i = 0;

    return sample;
}

void firf_lms(float mu_e, FIRF_FILTER *filter)
{
    float *b = &filter->b[filter->length-1];
    size_t i;
    float *hist;

    i = filter->i;
    hist = &filter->hist[i];
    while (i++ < filter->length)
    {
        *b-- += mu_e * *hist++;
    }

    i = 0;
    hist = &filter->hist[i];
    while (i++ < filter->i)
    {
        *b-- += mu_e * *hist++;
    }
}

/* Requires 2 * length history slots; keeps the dot product contiguous at wrap. */
float firf_mirrored(float sample, FIRF_FILTER *filter)
{
    const size_t position = filter->i;
    float *hist = filter->hist + position + filter->length;
    filter->hist[position] = *hist = sample;
    sample = 0;
    size_t i = 0;
#if FIR_SIMD && defined(__SSE__)
    __m128 sum = _mm_setzero_ps();
    for (; i + 4 <= filter->length; i += 4)
    {
        const __m128 history = _mm_loadu_ps(hist - (ptrdiff_t)i - 3);
        const __m128 reversed = _mm_shuffle_ps(history, history, _MM_SHUFFLE(0, 1, 2, 3));
        sum = _mm_add_ps(sum, _mm_mul_ps(_mm_loadu_ps(filter->b + i), reversed));
    }
    float lanes[4];
    _mm_storeu_ps(lanes, sum);
    sample = ((lanes[0] + lanes[1]) + lanes[2]) + lanes[3];
#elif FIR_SIMD && defined(__ARM_NEON)
    float32x4_t sum = vdupq_n_f32(0.f);
    for (; i + 4 <= filter->length; i += 4)
    {
        const float32x4_t history = vld1q_f32(hist - (ptrdiff_t)i - 3);
        const float32x4_t pairs = vrev64q_f32(history);
        const float32x4_t reversed = vcombine_f32(vget_high_f32(pairs), vget_low_f32(pairs));
        sum = vaddq_f32(sum, vmulq_f32(vld1q_f32(filter->b + i), reversed));
    }
    float lanes[4];
    vst1q_f32(lanes, sum);
    sample = ((lanes[0] + lanes[1]) + lanes[2]) + lanes[3];
#endif
    for (; i < filter->length; ++i)
        sample += filter->b[i] * hist[-(ptrdiff_t)i];
    filter->i = position + 1;
    if (filter->i == filter->length) filter->i = 0;
    return sample;
}

typedef struct
{
    const size_t length;
    const fixedpt *const b;

    size_t i;
    fixedpt *hist;
} FIRFP_FILTER;

fixedpt firfp(fixedpt sample, FIRFP_FILTER *filter);

fixedpt firfp(fixedpt sample, FIRFP_FILTER *filter)
{
    const fixedpt *b = filter->b;
    fixedpt *hist = &filter->hist[filter->i++];
    size_t i;

    *hist = sample;

    sample = 0; // will be "y"

    for (i = filter->i; i--;)
    {
        sample = fixedpt_add(sample, fixedpt_mul((*b++), (*hist--)));
    }

    hist = &filter->hist[filter->length-1];
    for (i = filter->length; i-- > filter->i;)
    {
        sample = fixedpt_add(sample, fixedpt_mul((*b++), (*hist--)));
    }

    if (filter->i >= filter->length) filter->i = 0;

    return sample;
}

#endif /* FIR_H */

