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

typedef struct
{
    const size_t length;
    const float *const b;

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

