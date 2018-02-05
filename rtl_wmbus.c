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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <complex.h>
#include <stdio.h>
#include <errno.h>
#include <fixedptc/fixedptc.h>
#include "fir.h"
#include "iir.h"
#include "ppf.h"
#include "moving_average_filter.h"
#include "atan2.h"
#include "net_support.h"
#include "t1_c1_packet_decoder.h"


#if defined(__SSE4_2__)
#include <immintrin.h>
#endif


static float lp_1600kHz_56kHz(int sample, size_t i_or_q)
{
    static float moving_average[2];

#define ALPHA 0.80259f // exp(-2.0 * M_PI * 56e3 / 1600e3)
    moving_average[i_or_q] = ALPHA * (moving_average[i_or_q] - sample) + sample;
#undef ALPHA

    return moving_average[i_or_q];
}


static inline float moving_average(int sample, size_t i_or_q)
{
#define COEFFS 12
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


static inline float lp_fir_butter_1600kHz_160kHz_200kHz(int sample, size_t i_or_q)
{
#define COEFFS 23
    static const float b[COEFFS] = {0.000140535927, 1.102280392e-05, 0.0001309279731, 0.001356012537, 0.00551787474, 0.01499414005, 0.03160167988, 0.05525973093, 0.08315031015, 0.1099887688, 0.1295143636, 0.1366692652, 0.1295143636, 0.1099887688, 0.08315031015, 0.05525973093, 0.03160167988, 0.01499414005, 0.00551787474, 0.001356012537, 0.0001309279731, 1.102280392e-05, 0.000140535927, };

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


static inline float lp_firfp_butter_1600kHz_160kHz_200kHz(int sample, size_t i_or_q)
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


static inline float lp_ppf_butter_1600kHz_160kHz_200kHz(int sample, size_t i_or_q)
{
#define PHASES 2
#define COEFFS 12
    static const float b[PHASES][COEFFS] =
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


static inline float lp_ppffp_butter_1600kHz_160kHz_200kHz(int sample, size_t i_or_q)
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


static inline float lp_fir_butter_800kHz_100kHz_10kHz(float sample)
{
#define COEFFS 4
    static const float b[COEFFS] = {0.04421550009, 0.4557844999, 0.4557844999, 0.04421550009, };

    static float hist[COEFFS];

    static FIRF_FILTER filter = {.length = COEFFS, .b = b, .hist = hist};
#undef COEFFS

    return firf(sample, &filter);
}


static float rssi_filter(int sample)
{
    static float old_sample;

#define ALPHA 0.6789f
    old_sample = ALPHA*sample + (1.0f - ALPHA)*old_sample;
#undef ALPHA

    return old_sample;
}


static inline float polar_discriminator(float i, float q)
{
    static float complex s_last;
    const float complex s = i + q * _Complex_I;
    const float complex y = s * conj(s_last);

#if 0
    const float delta_phi = atan2_libm(y);
#elif 1
    const float delta_phi = atan2_approximation(y);
#else
    const float delta_phi = atan2_approximation2(y);
#endif

    s_last = s;

    return delta_phi;
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


static inline int majority_votes_bitfilter(uint32_t unfilt_bitstream, uint32_t bits_in_unfilt_bitstream)
{
    const unsigned ones = count_set_bits(unfilt_bitstream & bits_in_unfilt_bitstream);
    const bool odd = (ones & 1) > 0;
    const uint32_t bits_in_unfilt_bitstream_half = count_set_bits(bits_in_unfilt_bitstream)/2;

    if (odd)
        return (ones <= bits_in_unfilt_bitstream_half) ? 0 : 1;

    if (ones < bits_in_unfilt_bitstream_half)
        return 0;

    if (ones > bits_in_unfilt_bitstream_half)
        return 1;

    return unfilt_bitstream & 1;
}


typedef void (*OutFunction)(unsigned bit, unsigned rssi);


static inline void to_stdout(unsigned bit, unsigned rssi)
{
    (void)rssi;

    const uint8_t tmp = bit;

    fwrite(&tmp, sizeof(tmp), 1, stdout);
}


static const OutFunction out_functions[] = { to_stdout, t1_c1_packet_decoder };


int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;


    // --- parameter section begin ---
    // The idea behind the variables in the section is to make parameters
    // configurable via command line.
    const unsigned CLOCK_LOCK_THRESHOLD = 2;

    const unsigned DECIMATION_RATE = 2;

    //#define USING_BITFILTER

    const uint32_t ACCESS_CODE = 0b0101010101010000111101u;
    const uint32_t ACCESS_CODE_BITMASK = 0x3FFFFFu;
    const unsigned ACCESS_CODE_ERRORS = 1u; // 0 if no errors allowed
    // --- parameter section end ---

    // Select function for output
    OutFunction out_function = out_functions[1];

    __attribute__((__aligned__(16))) uint8_t samples[4096];
    float i = 0, q = 0;
    unsigned decimation_rate_index = 0;
    int16_t old_clock = INT16_MIN;
    uint32_t bitstream = 0;
    unsigned clock_lock = 0;

#if defined(USING_BITFILTER)
    uint32_t unfilt_bitstream = 0;
    uint32_t bits_in_unfilt_bitstream = 0;
#endif

#if defined (__SSE4_2__)
    __attribute__((__aligned__(16))) int16_t iq_samples[sizeof(samples)];
    const __m128i dc_offset = _mm_set_epi16(-127, -127, -127, -127, -127, -127, -127, -127);
#endif

    //FILE *input = fopen("samples.bin", "rb");
    //FILE *input = get_net("localhost", 14423);
    FILE *input= stdin;

    if (input == NULL)
    {
        fprintf(stderr, "opening input error\n");
        return EXIT_FAILURE;
    }

    //FILE *demod_out = fopen("demod.bin", "wb");
    //FILE *demod_out2 = fopen("demod.bin", "wb");
    //FILE *clock_out = fopen("clock.bin", "wb");
    //FILE *bits_out= fopen("bits.bin", "wb");

    while (!feof(input))
    {
        size_t read_items = fread(samples, sizeof(samples), 1, input);
        if (1 != read_items)
        {
            // End of file?..
            return 2;
        }

#if defined (__SSE4_2__)
        for (size_t k = 0; k < sizeof(samples)/sizeof(samples[0]); k += 8)   // +2 : i and q interleaved
        {
            __m128i tmp = _mm_loadu_si128((__m128i const*)&samples[k]); // Hmmm, loading 8 byte besides of upper boundary?..
            __m128i cvt = _mm_add_epi16(_mm_cvtepu8_epi16(tmp), dc_offset);
            _mm_store_si128((__m128i *)&iq_samples[k], cvt);
        }
#endif

        for (size_t k = 0; k < sizeof(samples)/sizeof(samples[0]); k += 2)   // +2 : i and q interleaved
        {
#if defined (__SSE4_2__)
            const int i_unfilt = iq_samples[k];
            const int q_unfilt = iq_samples[k+1];
#else
            const int i_unfilt = ((int)samples[k]     - 127);
            const int q_unfilt = ((int)samples[k + 1] - 127);
#endif

            // Low-Pass-Filtering before decimation is necessary, to ensure
            // that i and q signals don't contain frequencies above new sample
            // rate.
            // The sample rate decimation is realised as sum over i and q,
            // which must not be divided by decimation factor before
            // demodulating (atan2(q,i)).
#if 0
            i = lp_fir_butter_1600kHz_160kHz_200kHz(i_unfilt, 0);
            q = lp_fir_butter_1600kHz_160kHz_200kHz(q_unfilt, 1);
#elif 0
            i = lp_ppf_butter_1600kHz_160kHz_200kHz(i_unfilt, 0);
            q = lp_ppf_butter_1600kHz_160kHz_200kHz(q_unfilt, 1);
#elif 0
            i = lp_firfp_butter_1600kHz_160kHz_200kHz(i_unfilt, 0);
            q = lp_firfp_butter_1600kHz_160kHz_200kHz(q_unfilt, 1);
#elif 0
            i = lp_ppffp_butter_1600kHz_160kHz_200kHz(i_unfilt, 0);
            q = lp_ppffp_butter_1600kHz_160kHz_200kHz(q_unfilt, 1);
#elif 0
            i += lp_1600kHz_58kHz(i_unfilt, 0);
            q += lp_1600kHz_58kHz(q_unfilt, 1);
#define USE_MOVING_AVERAGE
#else
            i += moving_average(i_unfilt, 0);
            q += moving_average(q_unfilt, 1);
#define USE_MOVING_AVERAGE
#endif

            ++decimation_rate_index;
            if (decimation_rate_index < DECIMATION_RATE) continue;
            decimation_rate_index = 0;

            // Demodulate.
            float delta_phi = polar_discriminator(i, q);

            //int16_t demodulated_signal = (INT16_MAX-1)*delta_phi;
            //fwrite(&demodulated_signal, sizeof(demodulated_signal), 1, demod_out);

            // Post-filtering to prevent bit errors because of signal jitter.
            delta_phi = lp_fir_butter_800kHz_100kHz_10kHz(delta_phi);
            //int16_t demodulated_signal = (INT16_MAX-1)*delta_phi;
            //fwrite(&demodulated_signal, sizeof(demodulated_signal), 1, demod_out2);

            // --- clock recovery section begin ---
            // The time-2 method is implemented: push squared signal through a bandpass
            // tuned close to the symbol rate. Saturating band-pass output produces a
            // rectangular pulses with the required timing information.
            // Clock-Signal is crossing zero in half period.
            const int16_t clock = (bp_iir_cheb1_800kHz_90kHz_98kHz_102kHz_110kHz(delta_phi * delta_phi) >= 0) ? INT16_MAX : INT16_MIN;
            //fwrite(&clock, sizeof(clock), 1, clock_out);

            unsigned bit = (delta_phi >= 0) ? (1u<<PACKET_DATABIT_SHIFT) : (0u<<PACKET_DATABIT_SHIFT);

#if defined(USING_BITFILTER)
            unfilt_bitstream = (unfilt_bitstream << 1) | bit;
            bits_in_unfilt_bitstream = (bits_in_unfilt_bitstream << 1) | 1;
#endif

            // We are using one simple filter to rssi value in order to
            // prevent unexpected "splashes" in signal power.
            float rssi = sqrtf(i*i + q*q);
            rssi = rssi_filter(rssi); // comment out, if rssi filtering is unwanted

            if (clock > old_clock)   // rising edge
            {
                clock_lock = 1;

#if defined(USING_BITFILTER)
                unfilt_bitstream = bit;
                bits_in_unfilt_bitstream = 1;
#endif
            }
            else if (old_clock == clock && clock_lock < CLOCK_LOCK_THRESHOLD)
            {
                clock_lock++;
            }
            else if (clock_lock == CLOCK_LOCK_THRESHOLD)     // sample data bit on CLOCK_LOCK_THRESHOLD after rose up
            {
                clock_lock++;

#if defined(USE_MOVING_AVERAGE)
                // If using moving average, we would habe doubles of each of i- and q- signal components.
                rssi /= DECIMATION_RATE;
#endif

#if defined(USING_BITFILTER)
                // Bitfilter can be used to remove unwanted spikes in the demodulated signal.
                bit = majority_votes_bitfilter(unfilt_bitstream, bits_in_unfilt_bitstream);
#endif

                bitstream = (bitstream << 1) | bit;

                if (count_set_bits((bitstream & ACCESS_CODE_BITMASK) ^ ACCESS_CODE) <= ACCESS_CODE_ERRORS)
                {
                    bit |= (1u<<PACKET_PREAMBLE_DETECTED_SHIFT); // packet detected; mark the bit similar to "Access Code"-Block in GNU Radio
                }

                //fwrite(&bit, sizeof(bit), 1, bits_out);
                out_function(bit, rssi);
            }
            old_clock = clock;
            // --- clock recovery section end ---

#if defined(USE_MOVING_AVERAGE)
            i = q = 0;
#endif
        }
    }

    return EXIT_SUCCESS;
}

