#ifndef S1_PACKET_DECODER_H
#define S1_PACKET_DECODER_H

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

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const uint8_t MANCHESTER_IEEE_802_3[4] = {
    0xFF, 0x01, 0x00, 0xFF // According to wireless MBus spec.: "01b” representing a “one”; "10b" representing a "zero".
};

struct s1_packet_decoder_work;
typedef void (*s1_packet_decoder_state)(unsigned bit, struct s1_packet_decoder_work *decoder);

static void s1_idle(unsigned bit, struct s1_packet_decoder_work *decoder);
static void s1_done(unsigned bit, struct s1_packet_decoder_work *decoder);
static void s1_rx_bit(unsigned bit, struct s1_packet_decoder_work *decoder);
static void s1_rx_bit2(unsigned bit, struct s1_packet_decoder_work *decoder);

static void s1_rx_first_mode_bit(unsigned bit, struct s1_packet_decoder_work *decoder);
static void s1_rx_last_mode_bit(unsigned bit, struct s1_packet_decoder_work *decoder);

static void s1_rx_first_lfield_bit(unsigned bit, struct s1_packet_decoder_work *decoder);
static void s1_rx_last_lfield_bit(unsigned bit, struct s1_packet_decoder_work *decoder);

static void s1_rx_first_data_bit(unsigned bit, struct s1_packet_decoder_work *decoder);
static void s1_rx_last_data_bit(unsigned bit, struct s1_packet_decoder_work *decoder);


static const s1_packet_decoder_state s1_decoder_states[] =
{
    s1_idle,                                       // 0

    s1_rx_first_lfield_bit,                        // 1
    s1_rx_bit2,                                    // 2
    s1_rx_bit,                                     // 3
    s1_rx_bit2,                                    // 4
    s1_rx_bit,                                     // 5
    s1_rx_bit2,                                    // 6
    s1_rx_bit,                                     // 7
    s1_rx_bit2,                                    // 8
    s1_rx_bit,                                     // 9
    s1_rx_bit2,                                    // 10
    s1_rx_bit,                                     // 11
    s1_rx_bit2,                                    // 12
    s1_rx_bit,                                     // 13
    s1_rx_bit2,                                    // 14
    s1_rx_bit,                                     // 15
    s1_rx_last_lfield_bit,                         // 16

    s1_rx_first_data_bit,                          // 17
    s1_rx_bit2,                                    // 18
    s1_rx_bit,                                     // 19
    s1_rx_bit2,                                    // 20
    s1_rx_bit,                                     // 21
    s1_rx_bit2,                                    // 22
    s1_rx_bit,                                     // 23
    s1_rx_bit2,                                    // 24
    s1_rx_bit,                                     // 25
    s1_rx_bit2,                                    // 26
    s1_rx_bit,                                     // 27
    s1_rx_bit2,                                    // 28
    s1_rx_bit,                                     // 29
    s1_rx_bit2,                                    // 30
    s1_rx_bit,                                     // 31
    s1_rx_last_data_bit,                           // 32

    s1_done,                                       // 33
};


struct s1_packet_decoder_work
{
    const s1_packet_decoder_state *state;
    unsigned current_rssi;
    unsigned packet_rssi;
    union
    {
        unsigned flags;
        struct
        {
            unsigned unused: 1;
            unsigned crc_ok: 1;
        };
    };
    unsigned l;
    unsigned L;
    unsigned mode;
    unsigned byte;
    __attribute__((__aligned__(16))) uint8_t packet[290]; // max. packet length with L- and all CRC-Fields
    char timestamp[64];
};

static int in_rx_s1_packet_decoder(struct s1_packet_decoder_work *decoder)
{
    return (decoder->state == &s1_decoder_states[0]) ? 0 : 1;
}

static void reset_s1_packet_decoder(struct s1_packet_decoder_work *decoder)
{
    memset(decoder, 0, sizeof(*decoder));
    decoder->state = &s1_decoder_states[0];
}

static void s1_idle(unsigned bit, struct s1_packet_decoder_work *decoder)
{
    if (!(bit & PACKET_PREAMBLE_DETECTED_MASK))
    {
        reset_s1_packet_decoder(decoder);
    }
}

static void s1_done(unsigned bit, struct s1_packet_decoder_work *decoder)
{
    (void)bit;
    (void)decoder;
}

static void s1_rx_bit(unsigned bit, struct s1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);
}

static void s1_rx_bit2(unsigned bit, struct s1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);

    const unsigned b = MANCHESTER_IEEE_802_3[decoder->byte & 0b11];

    if (b == 0xFFu)
    {
        reset_s1_packet_decoder(decoder);
    }
    else
    {
        decoder->byte >>= 2;
        decoder->byte <<= 1;
        decoder->byte |= b;
    }
}

static void s1_rx_first_lfield_bit(unsigned bit, struct s1_packet_decoder_work *decoder)
{
    decoder->byte = (bit & PACKET_DATABIT_MASK);
    decoder->packet_rssi = decoder->current_rssi;
}

static void s1_rx_last_lfield_bit(unsigned bit, struct s1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);

    const unsigned b = MANCHESTER_IEEE_802_3[decoder->byte & 0b11];

    if (b == 0xFFu)
    {
        reset_s1_packet_decoder(decoder);
    }
    else
    {
        decoder->byte >>= 2;
        decoder->byte <<= 1;
        decoder->byte |= b;
    }

    decoder->L = decoder->byte;
    decoder->l = 0;
    decoder->packet[decoder->l++] = decoder->L;
    decoder->L = FULL_TLG_LENGTH_FROM_L_FIELD[decoder->L];
}

static void s1_rx_first_data_bit(unsigned bit, struct s1_packet_decoder_work *decoder)
{
    decoder->byte = (bit & PACKET_DATABIT_MASK);
}

static void s1_rx_last_data_bit(unsigned bit, struct s1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);

    const unsigned b = MANCHESTER_IEEE_802_3[decoder->byte & 0b11];

    if (b == 0xFFu)
    {
        reset_s1_packet_decoder(decoder);
    }
    else
    {
        decoder->byte >>= 2;
        decoder->byte <<= 1;
        decoder->byte |= b;
    }

    decoder->packet[decoder->l++] = decoder->byte;

    if (decoder->l < decoder->L)
    {
        decoder->state = &s1_decoder_states[17]; // s1_rx_first_data_bit
    }
    else
    {
        time_t now;
        time(&now);

        struct tm *timeinfo = gmtime(&now);
        strftime(decoder->timestamp, sizeof(decoder->timestamp), "%Y-%m-%d %H:%M:%S.000", timeinfo);
    }
}

static void s1_packet_decoder(unsigned bit, unsigned rssi, struct s1_packet_decoder_work *decoder, const char *algorithm)
{
    decoder->current_rssi = rssi;

    (*decoder->state++)(bit, decoder);

    if (*decoder->state == s1_idle)
    {
        // nothing
    }
    else if (*decoder->state == s1_done)
    {
        decoder->crc_ok = check_calc_crc_wmbus(decoder->packet, decoder->L) ? 1 : 0;

        if (!opts_show_used_algorithm) algorithm = "";
        fprintf(stdout, "%s%s;%u;%u;%s;%u;%u;%08X;", algorithm, "S1",
               decoder->crc_ok,
               1,
               decoder->timestamp,
               decoder->packet_rssi,
                rssi,
                get_serial(decoder->packet));

#if 0
        fprintf(stdout, "0x");
        for (size_t l = 0; l < decoder->L; l++) fprintf(stdout, "%02x", decoder->packet[l]);
        fprintf(stdout, ";");
#endif

#if 1
        decoder->L = cook_pkt(decoder->packet, decoder->L);
        fprintf(stdout, "0x");
        for (size_t l = 0; l < decoder->L; l++) fprintf(stdout, "%02x", decoder->packet[l]);
#endif

        fprintf(stdout, "\n");
        fflush(stdout);

        reset_s1_packet_decoder(decoder);
    }
    else
    {
        // Stop receiving packet if current rssi below threshold.
        // The current packet seems to be collided with an another one.
        if (rssi < PACKET_CAPTURE_THRESHOLD)
        {
            reset_s1_packet_decoder(decoder);
        }
    }
}

#endif /* S1_PACKET_DECODER_H */

