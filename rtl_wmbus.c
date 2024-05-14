/*-
 * Copyright (c) 2024 <xael.south@yandex.com>
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

#include <getopt.h>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <complex.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <fixedptc/fixedptc.h>

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define WINDOWS_BUILD 1
#else
#define WINDOWS_BUILD 0
#endif

#include "build/version.h"
#include "fir.h"
#include "iir.h"
#include "ppf.h"
#include "moving_average_filter.h"
#include "atan2.h"
#include "rtl_wmbus_util.h"
#include "t1_c1_packet_decoder.h"
#include "s1_packet_decoder.h"

#if WINDOWS_BUILD == 1
#define CHECK_FLOW 0

#include <io.h>

#warning "Compiling for Win discludes network support."

static inline void START_ALARM(void) {}
static inline void STOP_ALARM(void)  {}

#else
#define CHECK_FLOW 1

#include <signal.h>
#include <unistd.h>
#include "net_support.h"

static inline void START_ALARM(void) { alarm(2); }
static inline void STOP_ALARM(void)  { alarm(0); }

static void sig_alarm_handler(int signo)
{
    fprintf(stderr, "rtl_wmbus: exiting since incoming data stopped flowing!\n");
    exit(EXIT_FAILURE);
}
#endif

#ifndef TIME2_ALGORITHM_ENABLED
#define TIME2_ALGORITHM_ENABLED 1
#endif

#ifndef RUN_LENGTH_ALGORITHM_ENABLED
#define RUN_LENGTH_ALGORITHM_ENABLED 1
#endif

#ifndef T1_C1_DC_OFFSET_ALPHA
#define T1_C1_DC_OFFSET_ALPHA 0.999f
#endif

#ifndef S1_DC_OFFSET_ALPHA
#define S1_DC_OFFSET_ALPHA 0.999f
#endif

static const uint32_t ACCESS_CODE_T1_C1 = 0x543d;
static const uint32_t ACCESS_CODE_T1_C1_BITMASK = 0xFFFFu;
static const unsigned ACCESS_CODE_T1_C1_ERRORS = 0u; // 0 if no errors allowed

static const uint32_t ACCESS_CODE_S1 = 0x547696;
static const uint32_t ACCESS_CODE_S1_BITMASK = 0xFFFFFFu;
static const unsigned ACCESS_CODE_S1_ERRORS = 0u; // 0 if no errors allowed


/* deglitch_filter_t1_c1 has been calculated by a Python script as follows.
   The filter is counting "1" among 7 bits and saying "1" if count("1") >= 3 else "0".
   Notice here count("1") >= 3. (More intuitive in that case would be count("1") >= 3.5.)
   That forces the filter to put more "1" than "0" on the output, because RTL-SDR streams
   more "0" than "1" - i don't know why RTL-SDR do this.
x = 'static const uint8_t deglitch_filter_t1_c1[128] = {'
mod8 = 8

for i in range(2**7):
    s = '{0:07b};'.format(i)
    val = '1' if bin(i).count("1") >= 3 else '0'
    print(s[0] + ";" + s[1] + ";" + s[2] + ";" + s[3] + ";" + s[4] + ";" + s[5] + ";" + s[6] + ";;%d;;%s" % (bin(i).count("1"), val))

    if i % 8 == 0: x += '\n\t'
    x += val + ','

x += '};\n'

print(x)
*/
static const uint8_t deglitch_filter_t1_c1[128] =
{
    0,0,0,0,0,0,0,1,
    0,0,0,1,0,1,1,1,
    0,0,0,1,0,1,1,1,
    0,1,1,1,1,1,1,1,
    0,0,0,1,0,1,1,1,
    0,1,1,1,1,1,1,1,
    0,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    0,0,0,1,0,1,1,1,
    0,1,1,1,1,1,1,1,
    0,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    0,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1
};

/* 1) Force the filter to put more ones than zeros on the output.
   2) Zeros surrounded by ones are ones and vice versa.
*/
static const uint8_t deglitch_filter_s1[16] = {
   // 0000 0001 0010 0011 0100 0101 0110 0111
         0,   1,   0,   1,   0,   1,   1,   1,
    // 1000 1001 1010 1011 1100 1101 1110 1111
         0,   1,   1,   1,   1,   1,   1,   1
};


//static FILE *demod_out = NULL;
static FILE *demod_out2_t1_c1 = NULL;
static FILE *demod_out2_s1 = NULL;
//static FILE *clock_out = NULL;
//static FILE *bits_out = NULL;
//static FILE *rawbits_out = NULL;


static inline float moving_average_t1_c1(float sample, size_t i_or_q)
{
#define COEFFS 8
    static int i_hist[COEFFS];
    static int q_hist[COEFFS];

    static MAVGI_FILTER filter[2] =                                         // i/q
    {
        {.length = COEFFS, .hist = i_hist}, //  0
        {.length = COEFFS, .hist = q_hist}  //  1
    };
#undef COEFFS

    return mavgi(sample, &filter[i_or_q]);
}

static inline float moving_average_s1(float sample, size_t i_or_q)
{
#define COEFFS 16
    static int i_hist[COEFFS];
    static int q_hist[COEFFS];

    static MAVGI_FILTER filter[2] =                                         // i/q
    {
        {.length = COEFFS, .hist = i_hist}, //  0
        {.length = COEFFS, .hist = q_hist}  //  1
    };
#undef COEFFS

    return mavgi(sample, &filter[i_or_q]);
}

static inline float lp_fir_butter_1600kHz_160kHz_200kHz_t1_c1(float sample, size_t i_or_q)
{
#define COEFFS 23
    static float b[COEFFS] = {0.000140535927, 1.102280392e-05, 0.0001309279731, 0.001356012537, 0.00551787474, 0.01499414005, 0.03160167988, 0.05525973093, 0.08315031015, 0.1099887688, 0.1295143636, 0.1366692652, 0.1295143636, 0.1099887688, 0.08315031015, 0.05525973093, 0.03160167988, 0.01499414005, 0.00551787474, 0.001356012537, 0.0001309279731, 1.102280392e-05, 0.000140535927, };
    //static float b[COEFFS] = {0.001645672124, 0.0004733757463, -0.002542116469, -0.008572441674, -0.01545406295, -0.01651661113, -0.002914917097, 0.03113207374, 0.08317149659, 0.1410058012, 0.1866042197, 0.2039350204, 0.1866042197, 0.1410058012, 0.08317149659, 0.03113207374, -0.002914917097, -0.01651661113, -0.01545406295, -0.008572441674, -0.002542116469, 0.0004733757463, 0.001645672124, };

    static float i_hist[COEFFS] = {};
    static float q_hist[COEFFS] = {};

    static FIRF_FILTER filter[2] =                                                 // i/q
    {
        {.length = COEFFS, .b = b, .hist = i_hist}, //  0
        {.length = COEFFS, .b = b, .hist = q_hist}  //  1
    };
#undef COEFFS

    return firf(sample, &filter[i_or_q]);
}

static inline float lp_fir_butter_1600kHz_160kHz_200kHz_s1(float sample, size_t i_or_q)
{
#define COEFFS 23
    static float b[COEFFS] = {0.000140535927, 1.102280392e-05, 0.0001309279731, 0.001356012537, 0.00551787474, 0.01499414005, 0.03160167988, 0.05525973093, 0.08315031015, 0.1099887688, 0.1295143636, 0.1366692652, 0.1295143636, 0.1099887688, 0.08315031015, 0.05525973093, 0.03160167988, 0.01499414005, 0.00551787474, 0.001356012537, 0.0001309279731, 1.102280392e-05, 0.000140535927, };
    //static float b[COEFFS] = {0.001645672124, 0.0004733757463, -0.002542116469, -0.008572441674, -0.01545406295, -0.01651661113, -0.002914917097, 0.03113207374, 0.08317149659, 0.1410058012, 0.1866042197, 0.2039350204, 0.1866042197, 0.1410058012, 0.08317149659, 0.03113207374, -0.002914917097, -0.01651661113, -0.01545406295, -0.008572441674, -0.002542116469, 0.0004733757463, 0.001645672124, };

    static float i_hist[COEFFS] = {};
    static float q_hist[COEFFS] = {};

    static FIRF_FILTER filter[2] =                                                 // i/q
    {
        {.length = COEFFS, .b = b, .hist = i_hist}, //  0
        {.length = COEFFS, .b = b, .hist = q_hist}  //  1
    };
#undef COEFFS

    return firf(sample, &filter[i_or_q]);
}

static inline float lp_firfp_butter_1600kHz_160kHz_200kHz(float sample, size_t i_or_q)
{
#define COEFFS 23
    static const fixedpt b[COEFFS] = {fixedpt_rconst(0.000140535927),  fixedpt_rconst(1.102280392e-05), fixedpt_rconst(0.0001309279731), fixedpt_rconst(0.001356012537), fixedpt_rconst(0.00551787474),
                                      fixedpt_rconst(0.01499414005),   fixedpt_rconst(0.03160167988),   fixedpt_rconst(0.05525973093),   fixedpt_rconst(0.08315031015),  fixedpt_rconst(0.1099887688),
                                      fixedpt_rconst(0.1295143636),    fixedpt_rconst(0.1366692652),    fixedpt_rconst(0.1295143636),    fixedpt_rconst(0.1099887688),   fixedpt_rconst(0.08315031015),
                                      fixedpt_rconst(0.05525973093),   fixedpt_rconst(0.03160167988),   fixedpt_rconst(0.01499414005),   fixedpt_rconst(0.00551787474),  fixedpt_rconst(0.001356012537),
                                      fixedpt_rconst(0.0001309279731), fixedpt_rconst(1.102280392e-05), fixedpt_rconst(0.000140535927),
                                     };
    static fixedpt i_hist[COEFFS] = {};
    static fixedpt q_hist[COEFFS] = {};

    static FIRFP_FILTER filter[2] =                                                // i/q
    {
        {.length = COEFFS, .b = b, .hist = i_hist}, //  0
        {.length = COEFFS, .b = b, .hist = q_hist}  //  1
    };
#undef COEFFS

    return fixedpt_tofloat(firfp(fixedpt_fromint(sample), &filter[i_or_q]));
}


static inline float lp_ppf_butter_1600kHz_160kHz_200kHz(float sample, size_t i_or_q)
{
#define PHASES 2
#define COEFFS 12
    static float b[PHASES][COEFFS] =
    {
        {0.000140535927, 0.0001309279731, 0.00551787474, 0.03160167988, 0.08315031015, 0.1295143636, 0.1295143636, 0.08315031015, 0.03160167988, 0.00551787474, 0.0001309279731, 0.000140535927, },
        {1.102280392e-05, 0.001356012537, 0.01499414005, 0.05525973093, 0.1099887688, 0.1366692652, 0.1099887688, 0.05525973093, 0.01499414005, 0.001356012537, 1.102280392e-05, 0, },
    };

    static float i_hist[PHASES][COEFFS] = {};
    static float q_hist[PHASES][COEFFS] = {};

    static FIRF_FILTER fir[2][PHASES] =
    {
        {
            // i/q phase
            {.length = COEFFS, .b = b[1], .hist = i_hist[0]}, //  0    0
            {.length = COEFFS, .b = b[0], .hist = i_hist[1]}  //  0    1
        },
        {
            // i/q phase
            {.length = COEFFS, .b = b[1], .hist = q_hist[0]}, //  1    0
            {.length = COEFFS, .b = b[0], .hist = q_hist[1]}  //  1    1
        },
    };

    static PPF_FILTER filter[2] =
    {
        {.sum = 0, .phase = 0, .max_phase = PHASES, .fir = fir[0]}, // 0 =: i
        {.sum = 0, .phase = 0, .max_phase = PHASES, .fir = fir[1]}, // 1 =: q
    };
#undef COEFFS
#undef PHASES

    return ppf(sample, &filter[i_or_q]);
}


static inline float lp_ppffp_butter_1600kHz_160kHz_200kHz(float sample, size_t i_or_q)
{
#define PHASES 2
#define COEFFS 12
    static const fixedpt b[PHASES][COEFFS] =
    {
        {fixedpt_rconst(0.000140535927),  fixedpt_rconst(0.0001309279731), fixedpt_rconst(0.00551787474), fixedpt_rconst(0.03160167988), fixedpt_rconst(0.08315031015), fixedpt_rconst(0.1295143636), fixedpt_rconst(0.1295143636), fixedpt_rconst(0.08315031015), fixedpt_rconst(0.03160167988), fixedpt_rconst(0.00551787474),  fixedpt_rconst(0.0001309279731), fixedpt_rconst(0.000140535927), },
        {fixedpt_rconst(1.102280392e-05), fixedpt_rconst(0.001356012537),  fixedpt_rconst(0.01499414005), fixedpt_rconst(0.05525973093), fixedpt_rconst(0.1099887688),  fixedpt_rconst(0.1366692652), fixedpt_rconst(0.1099887688), fixedpt_rconst(0.05525973093), fixedpt_rconst(0.01499414005), fixedpt_rconst(0.001356012537), fixedpt_rconst(1.102280392e-05), fixedpt_rconst(0), },
    };

    static fixedpt i_hist[PHASES][COEFFS] = {};
    static fixedpt q_hist[PHASES][COEFFS] = {};

    static FIRFP_FILTER fir[2][PHASES] =
    {
        {
            // i/q phase
            {.length = COEFFS, .b = b[1], .hist = i_hist[0]}, //  0    0
            {.length = COEFFS, .b = b[0], .hist = i_hist[1]}  //  0    1
        },
        {
            // i/q phase
            {.length = COEFFS, .b = b[1], .hist = q_hist[0]}, //  1    0
            {.length = COEFFS, .b = b[0], .hist = q_hist[1]}  //  1    1
        },
    };

    static PPFFP_FILTER filter[2] =
    {
        {.sum = fixedpt_rconst(0), .phase = 0, .max_phase = PHASES, .fir = fir[0]}, // 0 =: i
        {.sum = fixedpt_rconst(0), .phase = 0, .max_phase = PHASES, .fir = fir[1]}, // 1 =: q
    };
#undef COEFFS
#undef PHASES

    return fixedpt_tofloat(ppffp(fixedpt_fromint(sample), &filter[i_or_q]));
}


static inline float bp_iir_cheb1_800kHz_90kHz_98kHz_102kHz_110kHz(float sample)
{
#define GAIN 1.874981046e-06
#define SECTIONS 3
    static const float b[3*SECTIONS] = {1, 1.999994649, 0.9999946492, 1, -1.99999482, 0.9999948196, 1, 1.703868036e-07, -1.000010531, };
    static const float a[3*SECTIONS] = {1, -1.387139203, 0.9921518712, 1, -1.403492665, 0.9845934971, 1, -1.430055639, 0.9923856172, };
    static float hist[3*SECTIONS] = {};

    static IIRF_FILTER filter = {.sections = SECTIONS, .b = b, .a = a, .gain = GAIN, .hist = hist};
#undef SECTIONS
#undef GAIN

    return iirf(sample, &filter);
}

static inline float bp_iir_cheb1_800kHz_22kHz_30kHz_34kHz_42kHz(float sample)
{
#define GAIN 1.874981046e-06
#define SECTIONS 3
    static const float b[3*SECTIONS] = {1, 1.999994187, 0.9999941867, 1, -1.999994026,0.9999940262, 1, -1.605750097e-07, -1.000011787, };
    static const float a[3*SECTIONS] = {1, -1.92151475, 0.9918135499, 1, -1.922481015,0.984593497, 1, -1.937432099, 0.9927241336, };
    static float hist[3*SECTIONS] = {};

    static IIRF_FILTER filter = {.sections = SECTIONS, .b = b, .a = a, .gain = GAIN, .hist = hist};

#undef SECTIONS
#undef GAIN

    return iirf(sample, &filter);
}



static inline float lp_fir_butter_800kHz_100kHz_160kHz(float sample)
{
#define COEFFS 11
    static float b[COEFFS] = {-0.00456638213, -0.002571450348, 0.02689425925, 0.1141330398, 0.2264456422, 0.2793297826, 0.2264456422, 0.1141330398, 0.02689425925, -0.002571450348, -0.00456638213, };
    static float hist[COEFFS];

    static FIRF_FILTER filter = {.length = COEFFS, .b = b, .hist = hist};
#undef COEFFS

    return firf(sample, &filter);
}

static inline float lp_fir_butter_800kHz_32kHz_36kHz(float sample)
{
#define COEFFS 46
    static float b[COEFFS] = {-0.000649081282, -0.0009491938209, -0.001361601657, -0.001910785234, -0.002570133495, -0.003251218426, -0.003801634695, -0.004012672882, -0.003636803575, -0.002413585945, -0.0001013597693, 0.003488892085, 0.008461671287, 0.01481127545, 0.02240598045, 0.03098477999, 0.0401679839, 0.04948137286, 0.05839197924, 0.06635211627, 0.07284719662, 0.07744230649, 0.07982251613, 0.07982251613, 0.07744230649, 0.07284719662, 0.06635211627, 0.05839197924, 0.04948137286, 0.0401679839, 0.03098477999, 0.02240598045, 0.01481127545, 0.008461671287, 0.003488892085, -0.0001013597693, -0.002413585945, -0.003636803575, -0.004012672882, -0.003801634695, -0.003251218426, -0.002570133495, -0.001910785234, -0.001361601657, -0.0009491938209, -0.000649081282, };

    static float hist[COEFFS];

    static FIRF_FILTER filter = {.length = COEFFS, .b = b, .hist = hist};
#undef COEFFS

    return firf(sample, &filter);
}

/* https://liquidsdr.org/blog/lms-equalizer/ */
static inline void equalizer_complex_t1_c1(float *i, float *q)
{
    static const float mu = 0.05f;
    static size_t buf_index = 0;

#define COEFFS 21
    static float complex w[COEFFS] = {
        0.f, 0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f, 0.f,
        1.f,
        0.f, 0.f, 0.f, 0.f, 0.f,
        0.f, 0.f, 0.f, 0.f, 0.f,
    };

    static float complex b[COEFFS];

    b[buf_index] = *i + *q * _Complex_I;
    buf_index = (buf_index + 1) % COEFFS;

    float complex r = 0.f;
    for (int k = 0; k < COEFFS; k++)
    {
        r += b[(buf_index+k)%COEFFS] * conjf(w[k]);
    }

    const float complex e = (*q >= 0.f) ? (127.5f * _Complex_I) : (-127.5f * _Complex_I);
    for (int k = 0; k < COEFFS; k++)
    {
        w[k] = w[k] - mu * conjf(e)*b[(buf_index+k)%COEFFS];
    }
#undef COEFFS

    //fprintf(stdout, "%8.3f, %8.3f, %8.3f, %8.3f\n", *i, crealf(r), *q, cimagf(r));

    *i = crealf(r);
    *q = cimagf(r);
}

static inline float equalizer_t1_c1(const float sample, const float d)
{
    static const float mu = 0.05f;

#define COEFFS 9
    static float b[COEFFS] = { [COEFFS/2 + 1] = 1.f, };

    static float hist[COEFFS];

    static FIRF_FILTER filter = {.length = COEFFS, .b = b, .hist = hist};
#undef COEFFS

    const float r = firf(sample, &filter);

    const float e = d - r;
    const float mu_e = mu * e;
    firf_lms(mu_e, &filter);

    return r;
}

static inline float equalizer_s1(const float sample, const float d)
{
    static const float mu = 0.05f;

#define COEFFS 19
    static float b[COEFFS] = { [COEFFS/2 + 1] = 1.f, };

    static float hist[COEFFS];

    static FIRF_FILTER filter = {.length = COEFFS, .b = b, .hist = hist};
#undef COEFFS

    const float r = firf(sample, &filter);

    const float e = d - r;
    const float mu_e = mu * e;
    firf_lms(mu_e, &filter);

    return r;
}

static float rssi_filter_t1_c1(float sample)
{
    static float old_sample;

#define ALPHA 0.6789f
    old_sample = ALPHA*sample + (1.0f - ALPHA)*old_sample;
#undef ALPHA

    return old_sample;
}

static float rssi_filter_s1(float sample)
{
    static float old_sample;

#define ALPHA 0.6789f
    old_sample = ALPHA*sample + (1.0f - ALPHA)*old_sample;
#undef ALPHA

    return old_sample;
}

static float s1_remove_dc_offset_demod(float x)
{
  static float x_old, y_old;

  y_old = (1.f + S1_DC_OFFSET_ALPHA)/2.f * (x - x_old) + S1_DC_OFFSET_ALPHA * y_old;
  x_old = x;

  return y_old;
}

static float t1_c1_remove_dc_offset_demod(float x)
{
  static float x_old, y_old;

  y_old = (1.f + T1_C1_DC_OFFSET_ALPHA)/2.f * (x - x_old) + T1_C1_DC_OFFSET_ALPHA * y_old;
  x_old = x;

  return y_old;
}

static inline float polar_discriminator_t1_c1(float i, float q)
{
    static float complex s_last;
    const float complex s = i + q * _Complex_I;
    const float complex y = s * conjf(s_last);

#if 1
    const float delta_phi = atan2_libm(y);
#elif 0
    const float delta_phi = atan2_approximation(y);
#else
    const float delta_phi = atan2_approximation2(y);
#endif

    s_last = s;

    return delta_phi;
}

static inline float polar_discriminator_t1_c1_inaccurate(float i, float q)
{
    // We are going to use only complex part of the phase difference
    // so avoid unnecesary computation of real part. The math behind:
    // cargf = atan (delta_phi_imag / delta_phi_real) / pi;
    // In the formula only the sign is of interest - we compute delta_phi_imag only.

    static float i_last, q_last;

    const float delta_phi_imag = i_last*q - i*q_last;

    i_last = i;
    q_last = q;

    return delta_phi_imag;
}

static inline float polar_discriminator_s1(float i, float q)
{
    static float complex s_last;
    const float complex s = i + q * _Complex_I;
    const float complex y = s * conjf(s_last);

#if 1
    const float delta_phi = atan2_libm(y);
#elif 0
    const float delta_phi = atan2_approximation(y);
#else
    const float delta_phi = atan2_approximation2(y);
#endif

    s_last = s;

    return delta_phi;
}

static inline float polar_discriminator_s1_inaccurate(float i, float q)
{
    // We are going to use only complex part of the phase difference
    // so avoid unnecesary computation of real part. The math behind:
    // cargf = atan (delta_phi_imag / delta_phi_real) / pi;
    // In the formula only the sign is of interest - we compute delta_phi_imag only.

    static float i_last, q_last;
    const float delta_phi_imag = i_last*q - i*q_last;

    i_last = i;
    q_last = q;

    return delta_phi_imag;
}

/** @brief Sparse Ones runs in time proportional to the number
 *         of 1 bits.
 *
 *   From: http://gurmeet.net/puzzles/fast-bit-counting-routines
 */
static inline unsigned count_set_bits_sparse_one(uint32_t n)
{
    unsigned count = 0;

    while (n)
    {
        count++;
        n &= (n - 1) ; // set rightmost 1 bit in n to 0
    }

    return count;
}


static inline unsigned count_set_bits(uint32_t n)
{
#if defined(__i386__) || defined(__arm__)
    return __builtin_popcount(n);
#else
    return count_set_bits_sparse_one(n);
#endif
}


struct runlength_algorithm_s1
{
    int run_length;
    unsigned state;
    uint32_t raw_bitstream;
    uint32_t bitstream;
    int samples_per_bit[2];
    struct s1_packet_decoder_work decoder;
};


static void runlength_algorithm_reset_s1(struct runlength_algorithm_s1 *algo)
{
    algo->run_length = 0;
    algo->state = 0u;
    algo->raw_bitstream = 0;
    algo->bitstream = 0;
    algo->samples_per_bit[0] = 24; // Data rate is 32768 bps which gives us approx. 24 samples
    algo->samples_per_bit[1] = 24; // at a sample rate of 800kHz (800kHz / 32768bps = 24.41 ~= 24 samples).
    reset_s1_packet_decoder(&algo->decoder);
}


static void runlength_algorithm_s1(unsigned raw_bit, unsigned rssi, struct runlength_algorithm_s1 *algo)
{
    algo->raw_bitstream = (algo->raw_bitstream << 1) | raw_bit;

    const unsigned state = deglitch_filter_s1[algo->raw_bitstream & 0xFu];

    // Edge detector.
    if (algo->state == state)
    {
        algo->run_length++;
    }
    else
    {
        // Get the current bit length expressed in samples as an
        // average of two preceeding symbols.
        const int samples_per_bit = (algo->samples_per_bit[0] + algo->samples_per_bit[1]) / 2;

        // Reset the state machine if the current bit length (in samples)
        // is less than 0.5 or more than 1.5 of the ideal symbol length.
        if (samples_per_bit <= 24/2 || samples_per_bit >= (24+24/2))
        {
            runlength_algorithm_reset_s1(algo);
            algo->state = state;
            algo->run_length = 1;
            return;
        }

        // Reset the state machine if the sequence of ones (or zeros)
        // is less than 0.5 symbol length that we assume.
        const int half_bit_length = samples_per_bit/2;
        const int run_length = algo->run_length;
        if (run_length <= half_bit_length)
        {
            runlength_algorithm_reset_s1(algo);
            algo->state = state;
            algo->run_length = 1;
            return;
        }

        int num_of_bits_rx;
        for (num_of_bits_rx = 0; algo->run_length > half_bit_length; num_of_bits_rx++)
        {
            algo->run_length -= samples_per_bit;

            unsigned bit = algo->state;

            algo->bitstream = (algo->bitstream << 1) | bit;

            if (count_set_bits((algo->bitstream & ACCESS_CODE_S1_BITMASK) ^ ACCESS_CODE_S1) <= ACCESS_CODE_S1_ERRORS)
            {
                bit |= (1u<<PACKET_PREAMBLE_DETECTED_SHIFT); // packet detected; mark the bit similar to "Access Code"-Block in GNU Radio
            }

            s1_packet_decoder(bit, rssi, &algo->decoder, "rla;");
        }

        //fprintf(stdout, "%u, %d, bits: %d, 0: %u, 1: %u\n", algo->state, run_length, num_of_bits_rx, algo->samples_per_bit[0], algo->samples_per_bit[1]);

        algo->samples_per_bit[algo->state] = run_length / num_of_bits_rx;
        algo->state = state;
        algo->run_length = 1;
    }
}


struct runlength_algorithm_t1_c1
{
    int run_length;
    int bit_length;
    int cum_run_length_error;
    unsigned state;
    uint32_t raw_bitstream;
    uint32_t bitstream;
    struct t1_c1_packet_decoder_work decoder;
};


static void runlength_algorithm_reset_t1_c1(struct runlength_algorithm_t1_c1 *algo)
{
    algo->run_length = 0;
    algo->bit_length = 8 * 256;
    algo->cum_run_length_error = 0;
    algo->state = 0u;
    algo->raw_bitstream = 0;
    algo->bitstream = 0;
    reset_t1_c1_packet_decoder(&algo->decoder);
}


static void runlength_algorithm_t1_c1(unsigned raw_bit, unsigned rssi, struct runlength_algorithm_t1_c1 *algo)
{
    algo->raw_bitstream = (algo->raw_bitstream << 1) | raw_bit;

    const unsigned state = deglitch_filter_t1_c1[algo->raw_bitstream & 0x3Fu];

    // Edge detector.
    if (algo->state == state)
    {
        algo->run_length++;
    }
    else
    {
        if (algo->run_length < 5)
        {
            runlength_algorithm_reset_t1_c1(algo);
            algo->state = state;
            algo->run_length = 1;
            return;
        }

        //const int unscaled_run_length = algo->run_length;

        algo->run_length *= 256; // resolution scaling up for fixed point calculation

        const int half_bit_length = algo->bit_length / 2;

        if (algo->run_length <= half_bit_length)
        {
            runlength_algorithm_reset_t1_c1(algo);
            algo->state = state;
            algo->run_length = 1;
            return;
        }

        int num_of_bits_rx;
        for (num_of_bits_rx = 0; algo->run_length > half_bit_length; num_of_bits_rx++)
        {
            algo->run_length -= algo->bit_length;

            unsigned bit = algo->state;

            algo->bitstream = (algo->bitstream << 1) | bit;

            if (count_set_bits((algo->bitstream & ACCESS_CODE_T1_C1_BITMASK) ^ ACCESS_CODE_T1_C1) <= ACCESS_CODE_T1_C1_ERRORS)
            {
                bit |= (1u<<PACKET_PREAMBLE_DETECTED_SHIFT); // packet detected; mark the bit similar to "Access Code"-Block in GNU Radio
            }

            t1_c1_packet_decoder(bit, rssi, &algo->decoder, "rla;");
        }

        #if 0
        const int bit_error_length = algo->run_length / num_of_bits_rx;
        if (in_rx_t1_c1_packet_decoder(&algo->decoder))
        {
            fprintf(stdout, "rl = %d, num_of_bits_rx = %d, bit_length = %d, old_bit_error_length = %d, new_bit_error_length = %d\n",
                    unscaled_run_length, num_of_bits_rx, algo->bit_length, algo->bit_error_length, bit_error_length);
        }
        #endif

        // Some kind of PI controller is implemented below: u[n] = u[n-1] + Kp * e[n] + Ki * sum(e[0..n]).
        // Kp and Ki were found by experiment; e[n] := algo->run_length; u[[n] is the new bit length; u[n-1] is the last known bit length
        algo->cum_run_length_error += algo->run_length; // sum(e[0..n])
        #define PI_KP  32
        #define PI_KI  16
        //algo->bit_length += (algo->run_length / PI_KP + algo->cum_run_length_error / PI_KI) / num_of_bits_rx;
        algo->bit_length += (algo->run_length + algo->cum_run_length_error / PI_KI) / (PI_KP * num_of_bits_rx);
        #undef PI_KI
        #undef PI_KP

        algo->state = state;
        algo->run_length = 1;
    }
}


struct time2_algorithm_t1_c1
{
    uint32_t bitstream;
    struct t1_c1_packet_decoder_work t1_c1_decoder;
};

static void time2_algorithm_t1_c1_reset(struct time2_algorithm_t1_c1 *algo)
{
    algo->bitstream = 0;
    reset_t1_c1_packet_decoder(&algo->t1_c1_decoder);
}

static void time2_algorithm_t1_c1(unsigned bit, unsigned rssi, struct time2_algorithm_t1_c1 *algo)
{
    algo->bitstream = (algo->bitstream << 1) | bit;

    if (count_set_bits((algo->bitstream & ACCESS_CODE_T1_C1_BITMASK) ^ ACCESS_CODE_T1_C1) <= ACCESS_CODE_T1_C1_ERRORS)
    {
        bit |= (1u<<PACKET_PREAMBLE_DETECTED_SHIFT); // packet detected; mark the bit similar to "Access Code"-Block in GNU Radio
    }

    t1_c1_packet_decoder(bit, rssi, &algo->t1_c1_decoder, "t2a;");
}

struct time2_algorithm_s1
{
    uint32_t bitstream;
    struct s1_packet_decoder_work s1_decoder;
};

static void time2_algorithm_s1_reset(struct time2_algorithm_s1 *algo)
{
    algo->bitstream = 0;
    reset_s1_packet_decoder(&algo->s1_decoder);
}

static void time2_algorithm_s1(unsigned bit, unsigned rssi, struct time2_algorithm_s1 *algo)
{
    algo->bitstream = (algo->bitstream << 1) | bit;

    if (count_set_bits((algo->bitstream & ACCESS_CODE_S1_BITMASK) ^ ACCESS_CODE_S1) <= ACCESS_CODE_S1_ERRORS)
    {
        bit |= (1u<<PACKET_PREAMBLE_DETECTED_SHIFT); // packet detected; mark the bit similar to "Access Code"-Block in GNU Radio
    }

    s1_packet_decoder(bit, rssi, &algo->s1_decoder, "t2a;");
}


static int opts_run_length_algorithm_enabled = 1;
static int opts_time2_algorithm_enabled = TIME2_ALGORITHM_ENABLED;
static unsigned opts_decimation_rate = 2u;
static int opts_s1_t1_c1_simultaneously = 0;
static int opts_accurate_atan = 1;
static int opts_remove_dc_offset = 0;
int opts_show_used_algorithm = 0;
static int opts_t1_c1_processing_enabled = 1;
static int opts_s1_processing_enabled = 1;
static int opts_check_flow = 0;
static const unsigned opts_CLOCK_LOCK_THRESHOLD_T1_C1 = 2; // Is not implemented as option yet.
static const unsigned opts_CLOCK_LOCK_THRESHOLD_S1 = 2; // Is not implemented as option yet.


static void print_usage(const char *program_name)
{
    fprintf(stdout, "rtl_wmbus: " VERSION "\n\n");
    fprintf(stdout, "Usage %s:\n", program_name);
    fprintf(stdout, "\t-o remove DC offset\n");
    fprintf(stdout, "\t-a accelerate (use an inaccurate atan version)\n");
    fprintf(stdout, "\t-r 0 to disable run length algorithm\n");
    fprintf(stdout, "\t-t 0 to disable time2 algorithm\n");
    fprintf(stdout, "\t-d 2 set decimation rate to 2 (defaults to 2 if omitted)\n");
    fprintf(stdout, "\t-v show used algorithm in the output\n");
    fprintf(stdout, "\t-V show version\n");
    fprintf(stdout, "\t-s receive S1 and T1/C1 datagrams simultaneously. rtl_sdr _MUST_ be set to 868.625MHz (-f 868.625M)\n");
    fprintf(stdout, "\t-p [T,S] to disable processing T1/C1 or S1 mode\n");
    fprintf(stdout, "\t-f exit if flow of incoming data stops\n");
    fprintf(stdout, "\t-h print this help\n");
}

static void print_version(void)
{
    fprintf(stdout, "rtl_wmbus: " VERSION "\n");
    fprintf(stdout, COMMIT "\n");
}

static void process_options(int argc, char *argv[])
{
    int option;

    while ((option = getopt(argc, argv, "ofad:p:r:vVst:")) != -1)
    {
        switch (option)
        {
        case 'o':
          opts_remove_dc_offset = 1;
          break;
        case 'f':
            opts_check_flow = 1;
#if CHECK_FLOW == 0
            fprintf(stderr, "rtl_wmbus: Warning! You supplied the option -f but this build of rtl_wmbus cannot check flow of incoming data!\n");
#endif
            break;
        case 'a':
            opts_accurate_atan = 0;
            break;
        case 'p':
            if (strcmp(optarg, "T") == 0 || strcmp(optarg, "t") == 0)
            {
                opts_t1_c1_processing_enabled = 0;
            }
            else if (strcmp(optarg, "S") == 0 || strcmp(optarg, "s") == 0)
            {
                opts_s1_processing_enabled = 0;
            }
            else
            {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'r':
            if (strcmp(optarg, "0") == 0)
            {
                opts_run_length_algorithm_enabled = 0;
            }
            else
            {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 't':
            if (strcmp(optarg, "0") == 0)
            {
                opts_time2_algorithm_enabled = 0;
            }
            else
            {
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            break;
        case 'd':
            opts_decimation_rate = strtoul(optarg, NULL, 10);
            break;
        case 's':
            opts_s1_t1_c1_simultaneously = 1;
            break;
        case 'v':
            opts_show_used_algorithm = 1;
            break;
        case 'V':
            print_version();
            exit(EXIT_SUCCESS);
            break;
        default:
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

static float *LUT_FREQUENCY_TRANSLATION_PLUS_COSINE = NULL;
static float *LUT_FREQUENCY_TRANSLATION_PLUS_SINE = NULL;
#define FREQ_STEP_KHZ (25)

/* fs_kHz is the sample rate in kHz. */
static void setup_lookup_tables_for_frequency_translation(int fs_kHz)
{
    const int ft_kHz = FREQ_STEP_KHZ;
    const size_t n_max = fs_kHz/ft_kHz;

    free(LUT_FREQUENCY_TRANSLATION_PLUS_COSINE);
    LUT_FREQUENCY_TRANSLATION_PLUS_COSINE = malloc(n_max *sizeof(LUT_FREQUENCY_TRANSLATION_PLUS_COSINE[0]));
    if (!LUT_FREQUENCY_TRANSLATION_PLUS_COSINE) exit(EXIT_FAILURE);

    free(LUT_FREQUENCY_TRANSLATION_PLUS_SINE);
    LUT_FREQUENCY_TRANSLATION_PLUS_SINE = malloc(n_max *sizeof(LUT_FREQUENCY_TRANSLATION_PLUS_SINE[0]));
    if (!LUT_FREQUENCY_TRANSLATION_PLUS_SINE) exit(EXIT_FAILURE);

    for (size_t n = 0; n < n_max; n++)
    {
        const double phi = (2. * M_PI * (ft_kHz * n)) / fs_kHz;
        LUT_FREQUENCY_TRANSLATION_PLUS_COSINE[n] = cosf(phi);
        LUT_FREQUENCY_TRANSLATION_PLUS_SINE[n] = -sinf(phi); // Minus sinf!
    }
}

/* Positive frequencies shift: ft = +325kHz the signal will shift from 868.625M right to 868.95M.
   Negative frequencies shift: ft = -325kHz the signal will shift from 868.625M right to 868.3M. */
static void shift_freq_plus_minus325(float *iplus, float *qplus, float *iminus, float *qminus, int fs_kHz)
{
    const int ft = 325;
    static size_t n = 0;
    const size_t n_max = fs_kHz/FREQ_STEP_KHZ;
    #if 0
    const float complex freq_shift = cosf(2.*M_PI*(ft*n)/fs_kHz) + sin(2.*M_PI*(ft*n)/fs_kHz) * _Complex_I;
    n++;
    #else
    const float x = LUT_FREQUENCY_TRANSLATION_PLUS_COSINE[n];
    const float z = LUT_FREQUENCY_TRANSLATION_PLUS_SINE[n];
    n += ft/FREQ_STEP_KHZ;
    #endif
    if (n >= n_max) n -= n_max;
    float ix, iz, qx, qz;

    // (i+Jq)*(x+Jz) =ix-qz + J(qx+iz) positive rotation
    // (i+Jq)*(x-Jz) =ix+qz + J(qx-iz) negative rotation
    // ix, qz, qx, iz are the same for boths shifts so we reuse them
    // totaling 4 mul and 4 sums instead of 8 mul 4 sum.
    // It works because iplus equals to iminus and qplus equals to qminus.

    ix = *iplus * x;
    qx = *qplus * x;
    iz = *iplus * z;
    qz = *qplus * z;

    // Symmetric positive shift.
    *iplus = ix - qz;
    *qplus = qx + iz;

    // Symmetric negative shift .
    *iminus = ix + qz;
    *qminus = qx - iz;
}

typedef void (*t1_c1_signal_chain_prototype)(float i_t1_c1, float q_t1_c1,
                                             struct time2_algorithm_t1_c1 *t2_algo_t1_c1,
                                             struct runlength_algorithm_t1_c1 *rl_algo_t1_c1,
                                             float (*polar_discriminator_t1_c1_function)(float i, float q));

void t1_c1_signal_chain(float i_t1_c1, float q_t1_c1,
                        struct time2_algorithm_t1_c1 *t2_algo_t1_c1,
                        struct runlength_algorithm_t1_c1 *rl_algo_t1_c1,
                        float (*polar_discriminator_t1_c1_function)(float i, float q))
{
    static int16_t old_clock_t1_c1 = INT16_MIN;
    static unsigned clock_lock_t1_c1 = 0;

    // Demodulate.
    const float _delta_phi_t1_c1 = polar_discriminator_t1_c1_function(i_t1_c1, q_t1_c1);
    //int16_t demodulated_signal = (INT16_MAX-1)*delta_phi;
    //fwrite(&demodulated_signal, sizeof(demodulated_signal), 1, demod_out);

    // Post-filtering to prevent bit errors because of signal jitter.
    float delta_phi_t1_c1 = lp_fir_butter_800kHz_100kHz_160kHz(_delta_phi_t1_c1);
    //float delta_phi_t1_c1 = equalizer_t1_c1(_delta_phi_t1_c1, _delta_phi_t1_c1 >= 0.f ? 1.f : -1.f);
    if (opts_remove_dc_offset) delta_phi_t1_c1 = t1_c1_remove_dc_offset_demod(delta_phi_t1_c1);
    //int16_t demodulated_signal = (INT16_MAX-1)*delta_phi_t1_c1;
    //fwrite(&demodulated_signal, sizeof(demodulated_signal), 1, demod_out2_t1_c1);

    // Get the bit!
    unsigned bit_t1_c1 = (delta_phi_t1_c1 >= 0) ? (1u<<PACKET_DATABIT_SHIFT) : (0u<<PACKET_DATABIT_SHIFT);
    //int16_t u = bit ? (INT16_MAX-1) : 0;
    //fwrite(&u, sizeof(u), 1, rawbits_out);

    // --- rssi filtering section begin ---
    // We are using one simple filter to rssi value in order to
    // prevent unexpected "splashes" in signal power.
    float rssi_t1_c1 = sqrtf(i_t1_c1*i_t1_c1 + q_t1_c1*q_t1_c1);
    rssi_t1_c1 = rssi_filter_t1_c1(rssi_t1_c1); // comment out, if rssi filtering is unwanted
    // --- rssi filtering section end ---

    // --- runlength algorithm section begin ---
    #if RUN_LENGTH_ALGORITHM_ENABLED
    if (opts_run_length_algorithm_enabled)
    {
        runlength_algorithm_t1_c1(bit_t1_c1, rssi_t1_c1, rl_algo_t1_c1);
    }
    #endif
    // --- runlength algorithm section end ---


    // --- time2 algorithm section begin ---
    #if TIME2_ALGORITHM_ENABLED
    if (opts_time2_algorithm_enabled)
    {
        // --- clock recovery section begin ---
        // The time-2 method is implemented: push squared signal through a bandpass
        // tuned close to the symbol rate. Saturating band-pass output produces a
        // rectangular pulses with the required timing information.
        // Clock-Signal is crossing zero in half period.
        const int16_t clock_t1_c1 = (bp_iir_cheb1_800kHz_90kHz_98kHz_102kHz_110kHz(delta_phi_t1_c1 * delta_phi_t1_c1) >= 0) ? INT16_MAX : INT16_MIN;
        //fwrite(&clock_t1_c1, sizeof(clock_t1_c1), 1, clock_out);

        if (clock_t1_c1 > old_clock_t1_c1)
        {   // Clock signal rising edge detected.
            clock_lock_t1_c1 = 1;
        }
        else if (clock_t1_c1 == INT16_MAX)
        {   // Clock signal is still high.
            if (clock_lock_t1_c1 < opts_CLOCK_LOCK_THRESHOLD_T1_C1)
            {   // Skip up to (opts_CLOCK_LOCK_THRESHOLD_T1_C1 - 1) clock bits
                // to get closer to the middle of the data bit.
                clock_lock_t1_c1++;
            }
            else if (clock_lock_t1_c1 == opts_CLOCK_LOCK_THRESHOLD_T1_C1)
            {   // Sample data bit at CLOCK_LOCK_THRESHOLD_T1_C1 clock bit position.
                clock_lock_t1_c1++;
                time2_algorithm_t1_c1(bit_t1_c1, rssi_t1_c1, t2_algo_t1_c1);
                //int16_t u = bit_t1_c1 ? (INT16_MAX-1) : 0;
                //fwrite(&u, sizeof(u), 1, bits_out);
            }
        }
        old_clock_t1_c1 = clock_t1_c1;
        // --- clock recovery section end ---
    }
    #endif
    // --- time2 algorithm section end ---
}

void t1_c1_signal_chain_empty(float i_t1_c1, float q_t1_c1,
                              struct time2_algorithm_t1_c1 *t2_algo_t1_c1,
                              struct runlength_algorithm_t1_c1 *rl_algo_t1_c1,
                              float (*polar_discriminator_t1_c1_function)(float i, float q))
{
}

typedef void (*s1_signal_chain_prototype)(float i_s1, float q_s1,
                                          struct time2_algorithm_s1 *t2_algo_s1,
                                          struct runlength_algorithm_s1 *rl_algo_s1,
                                          float (*polar_discriminator_s1_function)(float i, float q));

void s1_signal_chain(float i_s1, float q_s1,
                      struct time2_algorithm_s1 *t2_algo_s1,
                      struct runlength_algorithm_s1 *rl_algo_s1,
                      float (*polar_discriminator_s1_function)(float i, float q))
{
    static int16_t old_clock_s1 = INT16_MIN;
    static unsigned clock_lock_s1 = 0;

    // Demodulate.
    const float _delta_phi_s1 = polar_discriminator_s1_function(i_s1, q_s1);
    //int16_t demodulated_signal = (INT16_MAX-1)*delta_phi;
    //fwrite(&demodulated_signal, sizeof(demodulated_signal), 1, demod_out);

    // Post-filtering to prevent bit errors because of signal jitter.
    float delta_phi_s1 = lp_fir_butter_800kHz_32kHz_36kHz(_delta_phi_s1);
    //float delta_phi_s1 = equalizer_s1(_delta_phi_s1, _delta_phi_s1 >= 0.f ? 1.f : -1.f);
    if (opts_remove_dc_offset) delta_phi_s1 = s1_remove_dc_offset_demod(delta_phi_s1);
    //int16_t demodulated_signal = (INT16_MAX-1)*delta_phi_s1;
    //fwrite(&demodulated_signal, sizeof(demodulated_signal), 1, demod_out2_s1);

    // Get the bit!
    unsigned bit_s1 = (delta_phi_s1 >= 0) ? (1u<<PACKET_DATABIT_SHIFT) : (0u<<PACKET_DATABIT_SHIFT);
    //int16_t u = bit ? (INT16_MAX-1) : 0;
    //fwrite(&u, sizeof(u), 1, rawbits_out);

    // --- rssi filtering section begin ---
    // We are using one simple filter to rssi value in order to
    // prevent unexpected "splashes" in signal power.
    float rssi_s1 = sqrtf(i_s1*i_s1 + q_s1*q_s1);
    rssi_s1 = rssi_filter_s1(rssi_s1); // comment out, if rssi filtering is unwanted
    // --- rssi filtering section end ---

    // --- runlength algorithm section begin ---
    #if RUN_LENGTH_ALGORITHM_ENABLED
    if (opts_run_length_algorithm_enabled)
    {
        runlength_algorithm_s1(bit_s1, rssi_s1, rl_algo_s1);
    }
    #endif
    // --- runlength algorithm section end ---


    // --- time2 algorithm section begin ---
    #if TIME2_ALGORITHM_ENABLED
    if (opts_time2_algorithm_enabled)
    {
        // --- clock recovery section begin ---
        // The time-2 method is implemented: push squared signal through a bandpass
        // tuned close to the symbol rate. Saturating band-pass output produces a
        // rectangular pulses with the required timing information.
        // Clock-Signal is crossing zero in half period.
        const int16_t clock_s1 = (bp_iir_cheb1_800kHz_22kHz_30kHz_34kHz_42kHz(delta_phi_s1 * delta_phi_s1) >= 0) ? INT16_MAX : INT16_MIN;
        //fwrite(&clock_s1, sizeof(clock_s1), 1, clock_out);

        if (clock_s1 > old_clock_s1)
        {   // Clock signal rising edge detected.
            clock_lock_s1 = 1;
        }
        else if (clock_s1 == INT16_MAX)
        {   // Clock signal is still high.
            if (clock_lock_s1 < opts_CLOCK_LOCK_THRESHOLD_S1)
            {   // Skip up to (opts_CLOCK_LOCK_THRESHOLD_S1 - 1) clock bits
                // to get closer to the middle of the data bit.
                clock_lock_s1++;
            }
            else if (clock_lock_s1 == opts_CLOCK_LOCK_THRESHOLD_S1)
            {   // Sample data bit at CLOCK_LOCK_THRESHOLD_S1 clock bit position.
                clock_lock_s1++;
                time2_algorithm_s1(bit_s1, rssi_s1, t2_algo_s1);
                //int16_t u = bit ? (INT16_MAX-1) : 0;
                //fwrite(&u, sizeof(u), 1, bits_out);
            }
        }
        old_clock_s1 = clock_s1;
        // --- clock recovery section end ---
    }
    #endif
    // --- time2 algorithm section end ---
}

void s1_signal_chain_empty(float i_s1, float q_s1,
                           struct time2_algorithm_s1 *t2_algo_s1,
                           struct runlength_algorithm_s1 *rl_algo_s1,
                           float (*polar_discriminator_s1_function)(float i, float q))
{
}

int main(int argc, char *argv[])
{
    #if WINDOWS_BUILD == 1
    _setmode(_fileno(stdin), _O_BINARY);
    #else
    #ifndef DEBUG
    if (argc == 1 && isatty(0))
    {
        // Standard input is a terminal, print help.
        print_usage(argv[0]);
        exit(0);
    }
    #endif /* DEBUG */
    #endif /* WINDOWS_BUILD == 1 */

    process_options(argc, argv);

#if CHECK_FLOW == 1
    struct sigaction old_alarm;
    struct sigaction new_alarm;

    if (opts_check_flow)
    {
        new_alarm.sa_handler = sig_alarm_handler;
        sigemptyset(&new_alarm.sa_mask);
        new_alarm.sa_flags = 0;

        fprintf(stderr, "rtl_wmbus: monitoring flow\n");
        sigaction(SIGALRM, &new_alarm, &old_alarm);
    }
#endif

    __attribute__((__aligned__(16))) uint8_t samples[4096];
    const int fs_kHz = opts_decimation_rate*800; // Sample rate [kHz] as a multiple of 800 kHz.
    float i_t1_c1, q_t1_c1;
    float i_s1, q_s1;


    unsigned decimation_rate_index = 0;

    struct time2_algorithm_t1_c1 t2_algo_t1_c1;
    time2_algorithm_t1_c1_reset(&t2_algo_t1_c1);

    struct time2_algorithm_s1 t2_algo_s1;
    time2_algorithm_s1_reset(&t2_algo_s1);

    struct runlength_algorithm_t1_c1 rl_algo_t1_c1;
    runlength_algorithm_reset_t1_c1(&rl_algo_t1_c1);

    struct runlength_algorithm_s1 rl_algo_s1;
    runlength_algorithm_reset_s1(&rl_algo_s1);

    t1_c1_signal_chain_prototype process_t1_c1_chain = opts_t1_c1_processing_enabled ? t1_c1_signal_chain: t1_c1_signal_chain_empty;
    s1_signal_chain_prototype process_s1_chain = opts_s1_processing_enabled ? s1_signal_chain : s1_signal_chain_empty;

    float (*polar_discriminator_t1_c1_function)(float i, float q) = opts_accurate_atan ? polar_discriminator_t1_c1 : polar_discriminator_t1_c1_inaccurate;
    float (*polar_discriminator_s1_function)(float i, float q) = opts_accurate_atan ? polar_discriminator_s1 : polar_discriminator_s1_inaccurate;

    FILE *input = stdin;
    //input = fopen("samples/samples2.bin", "rb");
    //input = fopen("samples/kamstrup.bin", "rb");
    //input = fopen("samples/c1_mode_b.bin", "rb");
    //input = fopen("samples/t1_c1a_mixed.bin", "rb");
    //input = fopen("rtlsdr_868.95M_1M6_amiplus_notdecoded.bin.002", "rb");
    //input = get_net("localhost", 14423);

    if (input == NULL)
    {
        fprintf(stderr, "File open error.\n");
        return EXIT_FAILURE;
    }

    //demod_out = fopen("demod.bin", "wb");
    //demod_out2_t1_c1 = fopen("demod2_t1_c1.bin", "wb");
    //demod_out2_s1 = fopen("demod2_s1.bin", "wb");
    //clock_out = fopen("clock.bin", "wb");
    //bits_out = fopen("bits.bin", "wb");
    //rawbits_out = fopen("rawbits.bin", "wb");

    setup_lookup_tables_for_frequency_translation(fs_kHz);

    while (!feof(input))
    {
        if (opts_check_flow) START_ALARM();
        size_t read_items = fread(samples, sizeof(samples), 1, input);
        if (opts_check_flow) STOP_ALARM();

        if (1 != read_items)
        {
            // End of file?..
            break;
        }

        for (size_t k = 0; k < sizeof(samples)/sizeof(samples[0]); k += 2)   // +2 : i and q interleaved
        {
            const float i_unfilt = ((float)(samples[k])     - 127.5f);
            const float q_unfilt = ((float)(samples[k + 1]) - 127.5f);

            // rtl_sdr -f 868.35M -s 2400000 - 2>/dev/null | build/rtl_wmbus -d 3
            //shift_freq(&i_unfilt, &q_unfilt, 600, 2400);

            float i_t1_c1_unfilt = i_unfilt;
            float q_t1_c1_unfilt = q_unfilt;

            float i_s1_unfilt = i_unfilt;
            float q_s1_unfilt = q_unfilt;

            if (opts_s1_t1_c1_simultaneously)
            {
                shift_freq_plus_minus325(&i_t1_c1_unfilt, &q_t1_c1_unfilt, &i_s1_unfilt, &q_s1_unfilt, fs_kHz);
                //shift_freq_plus_minus325(&i_s1_unfilt, &q_s1_unfilt, &i_t1_c1_unfilt, &q_t1_c1_unfilt, fs_kHz); // Just to test T1/C1 at 869.275M.
            }

            // Low-Pass-Filtering before decimation is necessary, to ensure
            // that i and q signals don't contain frequencies above new sample
            // rate. Moving average can be viewed as a low pass filter.
            i_t1_c1 = moving_average_t1_c1(i_t1_c1_unfilt, 0);
            q_t1_c1 = moving_average_t1_c1(q_t1_c1_unfilt, 1);

            #if 0
            equalizer_complex_t1_c1(&i_t1_c1, &q_t1_c1);
            #endif

            // Low-Pass-Filtering before decimation is necessary, to ensure
            // that i and q signals don't contain frequencies above new sample
            // rate. Moving average can be viewed as a low pass filter.
            i_s1 = moving_average_s1(i_s1_unfilt, 0);
            q_s1 = moving_average_s1(q_s1_unfilt, 1);

            #if 0
            equalizer_complex_s1(&i_s1, &q_s1); // FIXME: Function does not exist.
            #endif

            ++decimation_rate_index;
            if (decimation_rate_index < opts_decimation_rate) continue;
            decimation_rate_index = 0;

            process_t1_c1_chain(i_t1_c1, q_t1_c1, &t2_algo_t1_c1, &rl_algo_t1_c1, polar_discriminator_t1_c1_function);
            process_s1_chain(i_s1, q_s1, &t2_algo_s1, &rl_algo_s1, polar_discriminator_s1_function);
        }
    }

    if (opts_check_flow)
    {
        #if CHECK_FLOW == 1
        sigaction(SIGALRM, &old_alarm, NULL);
        #endif
    }

    if (input != stdin) fclose(input);
    if (demod_out2_t1_c1 != NULL) fclose(demod_out2_t1_c1);
    if (demod_out2_s1 != NULL) fclose(demod_out2_s1);
    free(LUT_FREQUENCY_TRANSLATION_PLUS_COSINE);
    free(LUT_FREQUENCY_TRANSLATION_PLUS_SINE);
    return EXIT_SUCCESS;
}
