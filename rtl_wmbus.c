/*-
 * Copyright (c) 2021 <xael.south@yandex.com>
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

static const uint32_t ACCESS_CODE = 0b0101010101010000111101u;
static const uint32_t ACCESS_CODE_BITMASK = 0x3FFFFFu;
static const unsigned ACCESS_CODE_ERRORS = 1u; // 0 if no errors allowed

/* deglitch_filter has been calculated by Python script as follows.
   The filter is counting "1" among 7 bits and saying "1" if count("1") >= 3 else "0".
   Notice here count("1") >= 3. (More intuitive in that case would be count("1") >= 3.5.)
   That forces the filter to put more "1" than "0" on the output, because RTL-SDR streams
   more "0" than "1" - i don't know why RTL-SDR do this.
x = 'static const uint8_t deglitch_filter[128] = {'
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
static const uint8_t deglitch_filter[128] =
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


struct runlength_algorithm
{
    int run_length;
    int bit_length;
    int cum_run_length_error;
    unsigned state;
    uint32_t raw_bitstream;
    uint32_t bitstream;
    struct t1_c1_packet_decoder_work decoder;
};

static void runlength_algorithm_reset(struct runlength_algorithm *algo)
{
    algo->run_length = 0;
    algo->bit_length = 8 * 256;
    algo->cum_run_length_error = 0;
    algo->state = 0u;
    algo->raw_bitstream = 0;
    algo->bitstream = 0;
    reset_t1_c1_packet_decoder(&algo->decoder);
}

static void runlength_algorithm(unsigned raw_bit, unsigned rssi, struct runlength_algorithm *algo)
{
    algo->raw_bitstream = (algo->raw_bitstream << 1) | raw_bit;

    const unsigned state = deglitch_filter[algo->raw_bitstream & 0x3Fu];

    if (algo->state == state)
    {
        algo->run_length++;
    }
    else
    {
        if (algo->run_length < 5)
        {
            runlength_algorithm_reset(algo);
            algo->state = state;
            algo->run_length = 1;
            return;
        }

        //const int unscaled_run_length = algo->run_length;

        algo->run_length *= 256; // resolution scaling up for fixed point calculation

        const int half_bit_length = algo->bit_length / 2;

        if (algo->run_length <= half_bit_length)
        {
            runlength_algorithm_reset(algo);
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

            if (count_set_bits((algo->bitstream & ACCESS_CODE_BITMASK) ^ ACCESS_CODE) <= ACCESS_CODE_ERRORS)
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
        // Kp and Ki were found by experiment; e[n] := algo->run_length
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

struct time2_algorithm
{
    uint32_t bitstream;
    struct t1_c1_packet_decoder_work decoder;
};

static void time2_algorithm_reset(struct time2_algorithm *algo)
{
    algo->bitstream = 0;
    reset_t1_c1_packet_decoder(&algo->decoder);
}

static void time2_algorithm(unsigned bit, unsigned rssi, struct time2_algorithm *algo)
{
    algo->bitstream = (algo->bitstream << 1) | bit;

    if (count_set_bits((algo->bitstream & ACCESS_CODE_BITMASK) ^ ACCESS_CODE) <= ACCESS_CODE_ERRORS)
    {
        bit |= (1u<<PACKET_PREAMBLE_DETECTED_SHIFT); // packet detected; mark the bit similar to "Access Code"-Block in GNU Radio
    }

    t1_c1_packet_decoder(bit, rssi, &algo->decoder, "t2a;");
}

static int opts_run_length_algorithm_enabled = 1;
static int opts_time2_algorithm_enabled = 1;

static void print_usage(const char *program_name)
{
    fprintf(stdout, "Usage %s:\n", program_name);
    fprintf(stdout, "\t-r 0 to disable run length algorithm\n");
    fprintf(stdout, "\t-t 0 to disable time2 algorithm\n");
}

static void process_options(int argc, char *argv[])
{
    int option;

    while ((option = getopt(argc, argv, "r:t:")) != -1)
    {
        switch (option)
        {
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
        default:
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char *argv[])
{
    process_options(argc, argv);

    // --- parameter section begin ---
    // The idea behind the variables in the section is to make parameters
    // configurable via command line.
    const unsigned CLOCK_LOCK_THRESHOLD = 2;

    const unsigned DECIMATION_RATE = 2;
    // --- parameter section end ---

    __attribute__((__aligned__(16))) uint8_t samples[4096];
    float i = 0, q = 0;
    unsigned decimation_rate_index = 0;
    int16_t old_clock = INT16_MIN;
    unsigned clock_lock = 0;

    struct time2_algorithm t2_algo;
    time2_algorithm_reset(&t2_algo);

    struct runlength_algorithm rl_algo;
    runlength_algorithm_reset(&rl_algo);

    //FILE *input = fopen("samples/samples2.bin", "rb");
    //FILE *input = fopen("samples/kamstrup.bin", "rb");
    //FILE *input = fopen("samples/c1_mode_b.bin", "rb");
    //FILE *input = fopen("samples/t1_c1a_mixed.bin", "rb");
    //FILE *input = get_net("localhost", 14423);
    FILE *input = stdin;

    if (input == NULL)
    {
        fprintf(stderr, "opening input error\n");
        return EXIT_FAILURE;
    }

    //FILE *demod_out = fopen("demod.bin", "wb");
    //FILE *demod_out2 = fopen("demod.bin", "wb");
    //FILE *clock_out = fopen("clock.bin", "wb");
    //FILE *bits_out= fopen("bits.bin", "wb");
    //FILE *rawbits_out = fopen("rawbits.bin", "wb");

    while (!feof(input))
    {
        size_t read_items = fread(samples, sizeof(samples), 1, input);
        if (1 != read_items)
        {
            // End of file?..
            return 2;
        }

        for (size_t k = 0; k < sizeof(samples)/sizeof(samples[0]); k += 2)   // +2 : i and q interleaved
        {
            const int i_unfilt = ((int)samples[k]     - 127);
            const int q_unfilt = ((int)samples[k + 1] - 127);

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

            // Get the bit!
            unsigned bit = (delta_phi >= 0) ? (1u<<PACKET_DATABIT_SHIFT) : (0u<<PACKET_DATABIT_SHIFT);
            //int16_t u = bit ? (INT16_MAX-1) : 0;
            //fwrite(&u, sizeof(u), 1, rawbits_out);

            // --- rssi filtering section begin ---
            // We are using one simple filter to rssi value in order to
            // prevent unexpected "splashes" in signal power.
            float rssi = sqrtf(i*i + q*q);
            rssi = rssi_filter(rssi); // comment out, if rssi filtering is unwanted
#if defined(USE_MOVING_AVERAGE)
            // If using moving average, we would have doubles of each of i- and q- signal components.
            rssi /= DECIMATION_RATE;
#endif
            // --- rssi filtering section end ---

            // --- runlength algorithm section begin ---
            if (opts_run_length_algorithm_enabled) runlength_algorithm(bit, rssi, &rl_algo);
            // --- runlength algorithm section end ---

            // --- clock recovery section begin ---
            // The time-2 method is implemented: push squared signal through a bandpass
            // tuned close to the symbol rate. Saturating band-pass output produces a
            // rectangular pulses with the required timing information.
            // Clock-Signal is crossing zero in half period.
            const int16_t clock = (bp_iir_cheb1_800kHz_90kHz_98kHz_102kHz_110kHz(delta_phi * delta_phi) >= 0) ? INT16_MAX : INT16_MIN;
            //fwrite(&clock, sizeof(clock), 1, clock_out);

            if (clock > old_clock)   // rising edge
            {
                clock_lock = 1;
            }
            else if (old_clock == clock && clock_lock < CLOCK_LOCK_THRESHOLD)
            {
                clock_lock++;
            }
            else if (clock_lock == CLOCK_LOCK_THRESHOLD)     // sample data bit on CLOCK_LOCK_THRESHOLD after rose up
            {
                clock_lock++;

                //fwrite(&bit, sizeof(bit), 1, bits_out);
                if (opts_time2_algorithm_enabled) time2_algorithm(bit, rssi, &t2_algo);
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

