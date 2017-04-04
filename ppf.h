#ifndef PPF_H
#define PPF_H

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
 * Floating and fixed point implementations of Polyphase Filter.
*/

#include "fir.h"
#include <fixedptc/fixedptc.h>

typedef struct
{
    float sum;
    size_t phase;
    const size_t max_phase;
    FIRF_FILTER *fir;
} PPF_FILTER;

float ppf(float sample, PPF_FILTER *filter);

float ppf(float sample, PPF_FILTER *filter)
{
    if (filter->phase == filter->max_phase)
    {
        filter->phase = 0;
        filter->sum = 0;
    }

    filter->sum += firf(sample, filter->fir + filter->phase);

    filter->phase++;

    return filter->sum;
}


typedef struct
{
    fixedpt sum;
    size_t phase;
    const size_t max_phase;
    FIRFP_FILTER *fir;
} PPFFP_FILTER;

fixedpt ppffp(fixedpt sample, PPFFP_FILTER *filter);

fixedpt ppffp(fixedpt sample, PPFFP_FILTER *filter)
{
    if (filter->phase == filter->max_phase)
    {
        filter->phase = 0;
        filter->sum = 0;
    }

    filter->sum = fixedpt_add(filter->sum, firfp(sample, filter->fir + filter->phase));

    filter->phase++;

    return filter->sum;
}

#if 0
static int test_ppf(void)
{
#define PHASES 2
#define COEFFS 5
    static const float b[PHASES][COEFFS] =
    {
        {0.01208900045, 0.1180545517, 0.2457748215, 0.1180545517, 0.01208900045, },
        {0.04038886734, 0.2065801697, 0.2065801697, 0.04038886734, 0, },
    };

    static float hist[PHASES][COEFFS] = {};

    static FIRF_FILTER fir[PHASES] =
    {
        {.length = COEFFS, .b = b[1], .hist = hist[0]}, // !inverted indexing of fir!
        {.length = COEFFS, .b = b[0], .hist = hist[1]}, // !inverted indexing of fir!
    };

    static PPF_FILTER filter =
    {
        .sum = 0, .phase = 0, .max_phase = PHASES, .fir = fir,
    };
#undef COEFFS
#undef PHASES


    for (int sample = 1, phase = 0; sample <= 22; sample++, phase ^= 1)
    {
        float x = ppf(sample, &filter);
        if (phase == 1) printf("%f, ", x);
    }

    return 0;
}
#endif

#endif /* PPF_H */

