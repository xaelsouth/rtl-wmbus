#ifndef T1_C1_PACKET_DECODER_H
#define T1_C1_PACKET_DECODER_H

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

#if !defined(PACKET_CAPTURE_THRESHOLD)
#define PACKET_CAPTURE_THRESHOLD  5u
#endif

#define C1_MODE_A          0b010101001100
#define C1_MODE_B          0b010101000011
#define C1_MODE_AB_TRAILER             0b1101

#define PACKET_DATABIT_SHIFT           (0u)
#define PACKET_PREAMBLE_DETECTED_SHIFT (1u)

#define PACKET_DATABIT_MASK            (1u<<PACKET_DATABIT_SHIFT)
#define PACKET_PREAMBLE_DETECTED_MASK  (1u<<PACKET_PREAMBLE_DETECTED_SHIFT)


static const uint8_t HIGH_NIBBLE_3OUTOF6[] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x30, 0xFF, 0x10, 0x20, 0xFF,
    0xFF, 0xFF, 0xFF, 0x70, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0x50, 0x60, 0xFF, 0x40, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xB0, 0xFF, 0x90, 0xA0, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0x80, 0xFF, 0xFF, 0xFF,
    0xFF, 0xD0, 0xE0, 0xFF, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};


static const uint8_t LOW_NIBBLE_3OUTOF6[] =
{
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x03, 0xFF, 0x01, 0x02, 0xFF,
    0xFF, 0xFF, 0xFF, 0x07, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0x05, 0x06, 0xFF, 0x04, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x0B, 0xFF, 0x09, 0x0A, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF, 0x08, 0xFF, 0xFF, 0xFF,
    0xFF, 0x0D, 0x0E, 0xFF, 0x0C, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};


static const uint16_t FULL_TLG_LENGTH_FROM_L_FIELD[] =
{
    3,   4,   5,   6,   7,   8,   9,  10,  11,  12,
    15,  16,  17,  18,  19,  20,  21,  22,  23,  24,
    25,  26,  27,  28,  29,  30,  33,  34,  35,  36,
    37,  38,  39,  40,  41,  42,  43,  44,  45,  46,
    47,  48,  51,  52,  53,  54,  55,  56,  57,  58,
    59,  60,  61,  62,  63,  64,  65,  66,  69,  70,
    71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
    81,  82,  83,  84,  87,  88,  89,  90,  91,  92,
    93,  94,  95,  96,  97,  98,  99, 100, 101, 102,
    105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
    115, 116, 117, 118, 119, 120, 123, 124, 125, 126,
    127, 128, 129, 130, 131, 132, 133, 134, 135, 136,
    137, 138, 141, 142, 143, 144, 145, 146, 147, 148,
    149, 150, 151, 152, 153, 154, 155, 156, 159, 160,
    161, 162, 163, 164, 165, 166, 167, 168, 169, 170,
    171, 172, 173, 174, 177, 178, 179, 180, 181, 182,
    183, 184, 185, 186, 187, 188, 189, 190, 191, 192,
    195, 196, 197, 198, 199, 200, 201, 202, 203, 204,
    205, 206, 207, 208, 209, 210, 213, 214, 215, 216,
    217, 218, 219, 220, 221, 222, 223, 224, 225, 226,
    227, 228, 231, 232, 233, 234, 235, 236, 237, 238,
    239, 240, 241, 242, 243, 244, 245, 246, 249, 250,
    251, 252, 253, 254, 255, 256, 257, 258, 259, 260,
    261, 262, 263, 264, 267, 268, 269, 270, 271, 272,
    273, 274, 275, 276, 277, 278, 279, 280, 281, 282,
    285, 286, 287, 288, 289, 290
};


static const uint16_t CRC16_DNP_TABLE[] =
{
    0x0000, 0x3d65, 0x7aca, 0x47af, 0xf594, 0xc8f1, 0x8f5e, 0xb23b,
    0xd64d, 0xeb28, 0xac87, 0x91e2, 0x23d9, 0x1ebc, 0x5913, 0x6476,
    0x91ff, 0xac9a, 0xeb35, 0xd650, 0x646b, 0x590e, 0x1ea1, 0x23c4,
    0x47b2, 0x7ad7, 0x3d78, 0x001d, 0xb226, 0x8f43, 0xc8ec, 0xf589,
    0x1e9b, 0x23fe, 0x6451, 0x5934, 0xeb0f, 0xd66a, 0x91c5, 0xaca0,
    0xc8d6, 0xf5b3, 0xb21c, 0x8f79, 0x3d42, 0x0027, 0x4788, 0x7aed,
    0x8f64, 0xb201, 0xf5ae, 0xc8cb, 0x7af0, 0x4795, 0x003a, 0x3d5f,
    0x5929, 0x644c, 0x23e3, 0x1e86, 0xacbd, 0x91d8, 0xd677, 0xeb12,
    0x3d36, 0x0053, 0x47fc, 0x7a99, 0xc8a2, 0xf5c7, 0xb268, 0x8f0d,
    0xeb7b, 0xd61e, 0x91b1, 0xacd4, 0x1eef, 0x238a, 0x6425, 0x5940,
    0xacc9, 0x91ac, 0xd603, 0xeb66, 0x595d, 0x6438, 0x2397, 0x1ef2,
    0x7a84, 0x47e1, 0x004e, 0x3d2b, 0x8f10, 0xb275, 0xf5da, 0xc8bf,
    0x23ad, 0x1ec8, 0x5967, 0x6402, 0xd639, 0xeb5c, 0xacf3, 0x9196,
    0xf5e0, 0xc885, 0x8f2a, 0xb24f, 0x0074, 0x3d11, 0x7abe, 0x47db,
    0xb252, 0x8f37, 0xc898, 0xf5fd, 0x47c6, 0x7aa3, 0x3d0c, 0x0069,
    0x641f, 0x597a, 0x1ed5, 0x23b0, 0x918b, 0xacee, 0xeb41, 0xd624,
    0x7a6c, 0x4709, 0x00a6, 0x3dc3, 0x8ff8, 0xb29d, 0xf532, 0xc857,
    0xac21, 0x9144, 0xd6eb, 0xeb8e, 0x59b5, 0x64d0, 0x237f, 0x1e1a,
    0xeb93, 0xd6f6, 0x9159, 0xac3c, 0x1e07, 0x2362, 0x64cd, 0x59a8,
    0x3dde, 0x00bb, 0x4714, 0x7a71, 0xc84a, 0xf52f, 0xb280, 0x8fe5,
    0x64f7, 0x5992, 0x1e3d, 0x2358, 0x9163, 0xac06, 0xeba9, 0xd6cc,
    0xb2ba, 0x8fdf, 0xc870, 0xf515, 0x472e, 0x7a4b, 0x3de4, 0x0081,
    0xf508, 0xc86d, 0x8fc2, 0xb2a7, 0x009c, 0x3df9, 0x7a56, 0x4733,
    0x2345, 0x1e20, 0x598f, 0x64ea, 0xd6d1, 0xebb4, 0xac1b, 0x917e,
    0x475a, 0x7a3f, 0x3d90, 0x00f5, 0xb2ce, 0x8fab, 0xc804, 0xf561,
    0x9117, 0xac72, 0xebdd, 0xd6b8, 0x6483, 0x59e6, 0x1e49, 0x232c,
    0xd6a5, 0xebc0, 0xac6f, 0x910a, 0x2331, 0x1e54, 0x59fb, 0x649e,
    0x00e8, 0x3d8d, 0x7a22, 0x4747, 0xf57c, 0xc819, 0x8fb6, 0xb2d3,
    0x59c1, 0x64a4, 0x230b, 0x1e6e, 0xac55, 0x9130, 0xd69f, 0xebfa,
    0x8f8c, 0xb2e9, 0xf546, 0xc823, 0x7a18, 0x477d, 0x00d2, 0x3db7,
    0xc83e, 0xf55b, 0xb2f4, 0x8f91, 0x3daa, 0x00cf, 0x4760, 0x7a05,
    0x1e73, 0x2316, 0x64b9, 0x59dc, 0xebe7, 0xd682, 0x912d, 0xac48,
};


struct t1_c1_packet_decoder_work;
typedef void (*t1_c1_packet_decoder_state)(unsigned bit, struct t1_c1_packet_decoder_work *decoder);

static void idle(unsigned bit, struct t1_c1_packet_decoder_work *decoder);
static void done(unsigned bit, struct t1_c1_packet_decoder_work *decoder);
static void rx_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);

static void rx_high_nibble_first_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);
static void rx_high_nibble_last_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);

static void rx_low_nibble_first_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);
static void rx_low_nibble_last_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);

static void rx_high_nibble_first_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);
static void rx_high_nibble_last_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);

static void rx_low_nibble_first_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);
static void rx_low_nibble_last_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);

static void c1_rx_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);

static void c1_rx_first_mode_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);
static void c1_rx_last_mode_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);

static void c1_rx_first_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);
static void c1_rx_last_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);

static void c1_rx_first_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);
static void c1_rx_last_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder);


static const t1_c1_packet_decoder_state states[] =
{
    idle,                                          // 0

    rx_high_nibble_first_lfield_bit,               // 1
    rx_bit,                                        // 2
    rx_bit,                                        // 3
    rx_bit,                                        // 4
    rx_bit,                                        // 5
    rx_high_nibble_last_lfield_bit,                // 6

    rx_low_nibble_first_lfield_bit,                // 7
    rx_bit,                                        // 8
    rx_bit,                                        // 9
    rx_bit,                                        // 10
    rx_bit,                                        // 11
    rx_low_nibble_last_lfield_bit,                 // 12

    rx_high_nibble_first_data_bit,                 // 13
    rx_bit,                                        // 14
    rx_bit,                                        // 15
    rx_bit,                                        // 16
    rx_bit,                                        // 17
    rx_high_nibble_last_data_bit,                  // 18

    rx_low_nibble_first_data_bit,                  // 19
    rx_bit,                                        // 20
    rx_bit,                                        // 21
    rx_bit,                                        // 22
    rx_bit,                                        // 23
    rx_low_nibble_last_data_bit,                   // 24

    done,                                          // 25

    c1_rx_first_mode_bit,                          // 26
    c1_rx_bit,                                     // 27
    c1_rx_bit,                                     // 28
    c1_rx_last_mode_bit,                           // 29

    c1_rx_first_lfield_bit,                        // 30
    c1_rx_bit,                                     // 31
    c1_rx_bit,                                     // 32
    c1_rx_bit,                                     // 33
    c1_rx_bit,                                     // 34
    c1_rx_bit,                                     // 35
    c1_rx_bit,                                     // 36
    c1_rx_last_lfield_bit,                         // 37

    c1_rx_first_data_bit,                          // 38
    c1_rx_bit,                                     // 39
    c1_rx_bit,                                     // 40
    c1_rx_bit,                                     // 41
    c1_rx_bit,                                     // 42
    c1_rx_bit,                                     // 43
    c1_rx_bit,                                     // 44
    c1_rx_last_data_bit,                           // 45

    done,                                          // 46
};


struct t1_c1_packet_decoder_work
{
    const t1_c1_packet_decoder_state *state;
    unsigned current_rssi;
    unsigned packet_rssi;
    union
    {
        unsigned flags;
        struct
        {
            unsigned err_3outof: 1;
            unsigned crc_ok: 1;
            unsigned c1_packet: 1;
            unsigned b_frame_type: 1;
        };
    };
    unsigned l;
    unsigned L;
    unsigned mode;
    unsigned byte;
    __attribute__((__aligned__(16))) uint8_t packet[290]; // max. packet length with L- and all CRC-Fields
    char timestamp[64];
};

int get_mode_a_tlg_length(uint8_t lfield)
{
    return 1 + (int)lfield + (((int)lfield - 10 + 15)/16 + 1) * 2;
}

int get_mode_b_tlg_length(uint8_t lfield)
{
    /* L-field of frame type B is given including data and CRC bytes but the length byte itself. */
    return 1 + (int)lfield;
}

static int in_rx_t1_c1_packet_decoder(struct t1_c1_packet_decoder_work *decoder)
{
    return (decoder->state == &states[0]) ? 0 : 1;
}

static void reset_t1_c1_packet_decoder(struct t1_c1_packet_decoder_work *decoder)
{
    memset(decoder, 0, sizeof(*decoder));
    decoder->state = &states[0];
}

static void idle(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    if (!(bit & PACKET_PREAMBLE_DETECTED_MASK))
    {
        reset_t1_c1_packet_decoder(decoder);
    }
}

static void done(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    (void)bit;
    (void)decoder;
}

static void rx_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);
}

static void rx_high_nibble_first_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte = (bit & PACKET_DATABIT_MASK);
    decoder->packet_rssi = decoder->current_rssi;
}

static void rx_high_nibble_last_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);
    decoder->mode = decoder->byte;

    decoder->L = HIGH_NIBBLE_3OUTOF6[decoder->byte];
    decoder->flags = 0;
}

static void rx_low_nibble_first_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte = (bit & PACKET_DATABIT_MASK);
}

static void rx_low_nibble_last_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);
    decoder->mode <<= 6;
    decoder->mode |= decoder->byte;

    const unsigned byte = LOW_NIBBLE_3OUTOF6[decoder->byte];

    if (decoder->L == 0xFFu || byte == 0xFFu)
    {
        if (decoder->mode == C1_MODE_A)
        {
            decoder->b_frame_type = 0;
            decoder->state = &states[26]; // c1_rx_first_mode_bit
        }
        else if (decoder->mode == C1_MODE_B)
        {
            decoder->b_frame_type = 1;
            decoder->state = &states[26]; // c1_rx_first_mode_bit
        }
        else
        {
            reset_t1_c1_packet_decoder(decoder);
        }
    }
    else
    {
        decoder->b_frame_type = 0;
        decoder->c1_packet = 0;

        decoder->L |= byte;
        decoder->l = 0;
        decoder->packet[decoder->l++] = decoder->L;
        decoder->L = FULL_TLG_LENGTH_FROM_L_FIELD[decoder->L];
    }
}

static void rx_high_nibble_first_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte = (bit & PACKET_DATABIT_MASK);
}

static void rx_high_nibble_last_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);

    const unsigned byte = HIGH_NIBBLE_3OUTOF6[decoder->byte];

    if (byte == 0xFFu) decoder->err_3outof = 1;

    decoder->packet[decoder->l] = byte;
}

static void rx_low_nibble_first_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte = (bit & PACKET_DATABIT_MASK);
}

static void rx_low_nibble_last_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);

    const unsigned byte = LOW_NIBBLE_3OUTOF6[decoder->byte];

    if (byte == 0xFFu) decoder->err_3outof = 1;

    decoder->packet[decoder->l++] |= byte;

    if (decoder->l < decoder->L)
    {
        decoder->state = &states[13]; // rx_high_nibble_first_data_bit
    }
    else
    {
        time_t now;
        time(&now);

        struct tm *timeinfo = gmtime(&now);
        strftime(decoder->timestamp, sizeof(decoder->timestamp), "%Y-%m-%d %H:%M:%S.000", timeinfo);
    }
}

static void c1_rx_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);
}

static void c1_rx_first_mode_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte = (bit & PACKET_DATABIT_MASK);
}

static void c1_rx_last_mode_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);

    decoder->mode <<= 4;
    decoder->mode |= decoder->byte;

    if (decoder->byte == C1_MODE_AB_TRAILER)
    {
        decoder->c1_packet = 1;
    }
    else
    {
        reset_t1_c1_packet_decoder(decoder);
    }
}

static void c1_rx_first_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte = (bit & PACKET_DATABIT_MASK);
}

static void c1_rx_last_lfield_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);

    decoder->L = decoder->byte;
    decoder->l = 0;
    decoder->packet[decoder->l++] = decoder->L;
    if (decoder->b_frame_type)
    {
        decoder->L = get_mode_b_tlg_length(decoder->L); 
    }
    else
    {
        decoder->L = FULL_TLG_LENGTH_FROM_L_FIELD[decoder->L];
    }
}

static void c1_rx_first_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte = (bit & PACKET_DATABIT_MASK);
}

static void c1_rx_last_data_bit(unsigned bit, struct t1_c1_packet_decoder_work *decoder)
{
    decoder->byte <<= 1;
    decoder->byte |= (bit & PACKET_DATABIT_MASK);

    decoder->packet[decoder->l++] = decoder->byte;

    if (decoder->l < decoder->L)
    {
        decoder->state = &states[38]; // c1_rx_first_data_bit
    }
    else
    {
        time_t now;
        time(&now);

        struct tm *timeinfo = gmtime(&now);
        strftime(decoder->timestamp, sizeof(decoder->timestamp), "%Y-%m-%d %H:%M:%S.000", timeinfo);
    }
}


static uint16_t calc_crc_wmbus(const uint8_t *data, size_t datalen)
{
    uint16_t crc = 0;
    while (datalen--) crc = CRC16_DNP_TABLE[*data++ ^ (crc >> 8)] ^ (crc << 8);
    crc = ~crc;
    return crc;
}

static bool check_calc_crc_wmbus(const uint8_t *data, size_t datalen)
{
    bool crc_ok = false;
    uint16_t crc1, crc2;

    if (datalen >= 12)
    {
        crc1 = calc_crc_wmbus(data, 10);
        crc2 = (data[10] << 8) | (data[11]);
        data += 12;
        datalen -= 12;
        crc_ok = (crc1 == crc2);

        while (crc_ok && datalen)
        {
            if (datalen >= 18)
            {
                crc1 = calc_crc_wmbus(data, 16);
                crc2 = (data[16] << 8) | (data[17]);
                data += 18;
                datalen -= 18;
                crc_ok = (crc1 == crc2);
            }
            else
            {
                crc1 = calc_crc_wmbus(data, datalen-2);
                crc2 = (data[datalen-2] << 8) | (data[datalen-1]);
                data += datalen;
                datalen -= datalen;
                crc_ok = (crc1 == crc2);
            }
        }
    }

    return crc_ok;
}

static bool check_calc_crc_wmbus_b_frame_type(const uint8_t *data, size_t datalen)
{
    bool crc_ok = (datalen >= 12);
    uint16_t crc1, crc2;

    /* The CRC field of Block 2 is calculated on the _concatenation_
       of Block 1 and Block 2 data. */
    while (crc_ok && datalen)
    {
        if (datalen >= 128)
        {
            crc1 = calc_crc_wmbus(data, 126);
            crc2 = (data[126] << 8) | (data[127]);
            data += 128;
            datalen -= 128;
            crc_ok = (crc1 == crc2);
        }
        else
        {
            crc1 = calc_crc_wmbus(data, datalen-2);
            crc2 = (data[datalen-2] << 8) | (data[datalen-1]);
            data += datalen;
            datalen -= datalen;
            crc_ok = (crc1 == crc2);
        }
    }

    return crc_ok;
}

/** @brief Strip CRCs in place. */
static unsigned cook_pkt(uint8_t *data, unsigned datalen)
{
    uint8_t *dst = data;
    unsigned dstlen = 0;

    if (datalen >= 12)
    {
        dst += 10;
        dstlen += 10;

        data += 12;
        datalen -= 12;

        while (datalen)
        {
            if (datalen >= 18)
            {
                memmove(dst, data, 16);

                dst += 16;
                dstlen += 16;

                data += 18;
                datalen -= 18;
            }
            else
            {
                memmove(dst, data, datalen-2);

                dst += (datalen-2);
                dstlen += (datalen-2);

                data += datalen;
                datalen -= datalen;
            }
        }
    }

    return dstlen;
}

/** @brief Strip CRCs in place. */
static unsigned cook_pkt_b_frame_type(uint8_t *data, unsigned datalen)
{
    uint8_t *const L = data;
    uint8_t *dst = data;
    unsigned dstlen = 0;

    if (datalen >= 12)
    {
        while (datalen)
        {
            /* The CRC field of Block 2 is calculated on the _concatenation_
               of Block 1 and Block 2 data. */        
            if (datalen >= 128)
            {
                memmove(dst, data, 126);

                dst += 126;
                dstlen += 126;

                data += 128;
                datalen -= 128;
            }
            else
            {
                memmove(dst, data, datalen-2);

                dst += (datalen-2);
                dstlen += (datalen-2);

                data += datalen;
                datalen -= datalen;
            }
        }

        // L field has to be a number of data bytes in the datagram without CRC bytes.
        // dstlen is this "number of data bytes" already but has an add-on of L field itself,
        // which is going to be subtracted in the next step.
        *L = dstlen - 1;
    }

    return dstlen;
}

static inline uint32_t get_serial(const uint8_t *const packet)
{
    uint32_t serial;

    memcpy(&serial, &packet[4], sizeof(serial));

    return serial;
}

static void t1_c1_packet_decoder(unsigned bit, unsigned rssi, struct t1_c1_packet_decoder_work *decoder, const char *algorithm)
{
    decoder->current_rssi = rssi;

    (*decoder->state++)(bit, decoder);

    if (*decoder->state == idle)
    {
        // nothing
    }
    else if (*decoder->state == done)
    {
        if (decoder->b_frame_type)
        {
            decoder->crc_ok = check_calc_crc_wmbus_b_frame_type(decoder->packet, decoder->L) ? 1 : 0;
        }
        else
        {
            decoder->crc_ok = check_calc_crc_wmbus(decoder->packet, decoder->L) ? 1 : 0;
        }

        algorithm = ""; // uncomment of want to see which algorithm is executed right now
        fprintf(stdout, "%s%s;%u;%u;%s;%u;%u;%08X;", algorithm, decoder->c1_packet ? "C1": "T1",
               decoder->crc_ok,
               decoder->err_3outof^1,
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
        if (decoder->b_frame_type)
        {
            decoder->L = cook_pkt_b_frame_type(decoder->packet, decoder->L);
        }
        else
        {
            decoder->L = cook_pkt(decoder->packet, decoder->L);
        }
        fprintf(stdout, "0x");
        for (size_t l = 0; l < decoder->L; l++) fprintf(stdout, "%02x", decoder->packet[l]);
#endif

        fprintf(stdout, "\n");
        fflush(stdout);

        reset_t1_c1_packet_decoder(decoder);
    }
    else
    {
        // Stop receiving packet if current rssi below threshold.
        // The current packet seems to be collided with an another one.
        if (rssi < PACKET_CAPTURE_THRESHOLD)
        {
            reset_t1_c1_packet_decoder(decoder);
        }
    }
}

#endif /* T1_C1_PACKET_DECODER_H */

