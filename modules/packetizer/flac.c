/*****************************************************************************
 * flac.c: flac packetizer module.
 *****************************************************************************
 * Copyright (C) 1999-2017 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Sigmund Augdal Helberg <dnumgis@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>

#include <vlc_block_helper.h>
#include "packetizer_helper.h"
#include "flac.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_SOUT)
    set_subcategory(SUBCAT_SOUT_PACKETIZER)
    set_description(N_("Flac audio packetizer"))
    set_capability("packetizer", 50)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * decoder_sys_t : FLAC decoder descriptor
 *****************************************************************************/
typedef struct
{
    /*
     * Input properties
     */
    int i_state;

    block_bytestream_t bytestream;
    size_t i_offset;

    /*
     * FLAC properties
     */
    struct flac_stream_info stream_info;
    bool b_stream_info;

    /*
     * Common properties
     */
    date_t pts;
    struct flac_header_info headerinfo;

    size_t i_frame_size;
    size_t i_last_frame_size;
    uint16_t crc;
    size_t i_buf;
    uint8_t *p_buf;

    int i_next_block_flags;
} decoder_sys_t;

static const int pi_channels_maps[9] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
     | AOUT_CHAN_MIDDLERIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
     | AOUT_CHAN_LFE
};


/*****************************************************************************
 * ProcessHeader: process Flac header.
 *****************************************************************************/
static void ProcessHeader(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    int i_extra = p_dec->fmt_in.i_extra;
    char *p_extra = p_dec->fmt_in.p_extra;

    if (i_extra > 8 && !memcmp(p_extra, "fLaC", 4)) {
        i_extra -= 8;
        p_extra += 8;
    }

    if (p_dec->fmt_in.i_extra < FLAC_STREAMINFO_SIZE)
        return;

    FLAC_ParseStreamInfo( (uint8_t *) p_extra, &p_sys->stream_info );

    p_sys->b_stream_info = true;

    p_dec->fmt_out.i_extra = i_extra;
    free(p_dec->fmt_out.p_extra);
    p_dec->fmt_out.p_extra = malloc(i_extra);
    if (p_dec->fmt_out.p_extra)
        memcpy(p_dec->fmt_out.p_extra, p_extra, i_extra);
    else
        p_dec->fmt_out.i_extra = 0;
}

/* CRC-8, poly = x^8 + x^2 + x^1 + x^0, init = 0 */
static const uint8_t flac_crc8_table[256] = {
        0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
        0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
        0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
        0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
        0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
        0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
        0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
        0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
        0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
        0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
        0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
        0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
        0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
        0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
        0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
        0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
        0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
        0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
        0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
        0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
        0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
        0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
        0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
        0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
        0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
        0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
        0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
        0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
        0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
        0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
        0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
        0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

static uint8_t flac_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;

    while (len--)
        crc = flac_crc8_table[crc ^ *data++];

    return crc;
}

/* CRC-16, poly = x^16 + x^15 + x^2 + x^0, init = 0 */
static const uint16_t flac_crc16_table[256] = {
    0x0000,  0x8005,  0x800f,  0x000a,  0x801b,  0x001e,  0x0014,  0x8011,
    0x8033,  0x0036,  0x003c,  0x8039,  0x0028,  0x802d,  0x8027,  0x0022,
    0x8063,  0x0066,  0x006c,  0x8069,  0x0078,  0x807d,  0x8077,  0x0072,
    0x0050,  0x8055,  0x805f,  0x005a,  0x804b,  0x004e,  0x0044,  0x8041,
    0x80c3,  0x00c6,  0x00cc,  0x80c9,  0x00d8,  0x80dd,  0x80d7,  0x00d2,
    0x00f0,  0x80f5,  0x80ff,  0x00fa,  0x80eb,  0x00ee,  0x00e4,  0x80e1,
    0x00a0,  0x80a5,  0x80af,  0x00aa,  0x80bb,  0x00be,  0x00b4,  0x80b1,
    0x8093,  0x0096,  0x009c,  0x8099,  0x0088,  0x808d,  0x8087,  0x0082,
    0x8183,  0x0186,  0x018c,  0x8189,  0x0198,  0x819d,  0x8197,  0x0192,
    0x01b0,  0x81b5,  0x81bf,  0x01ba,  0x81ab,  0x01ae,  0x01a4,  0x81a1,
    0x01e0,  0x81e5,  0x81ef,  0x01ea,  0x81fb,  0x01fe,  0x01f4,  0x81f1,
    0x81d3,  0x01d6,  0x01dc,  0x81d9,  0x01c8,  0x81cd,  0x81c7,  0x01c2,
    0x0140,  0x8145,  0x814f,  0x014a,  0x815b,  0x015e,  0x0154,  0x8151,
    0x8173,  0x0176,  0x017c,  0x8179,  0x0168,  0x816d,  0x8167,  0x0162,
    0x8123,  0x0126,  0x012c,  0x8129,  0x0138,  0x813d,  0x8137,  0x0132,
    0x0110,  0x8115,  0x811f,  0x011a,  0x810b,  0x010e,  0x0104,  0x8101,
    0x8303,  0x0306,  0x030c,  0x8309,  0x0318,  0x831d,  0x8317,  0x0312,
    0x0330,  0x8335,  0x833f,  0x033a,  0x832b,  0x032e,  0x0324,  0x8321,
    0x0360,  0x8365,  0x836f,  0x036a,  0x837b,  0x037e,  0x0374,  0x8371,
    0x8353,  0x0356,  0x035c,  0x8359,  0x0348,  0x834d,  0x8347,  0x0342,
    0x03c0,  0x83c5,  0x83cf,  0x03ca,  0x83db,  0x03de,  0x03d4,  0x83d1,
    0x83f3,  0x03f6,  0x03fc,  0x83f9,  0x03e8,  0x83ed,  0x83e7,  0x03e2,
    0x83a3,  0x03a6,  0x03ac,  0x83a9,  0x03b8,  0x83bd,  0x83b7,  0x03b2,
    0x0390,  0x8395,  0x839f,  0x039a,  0x838b,  0x038e,  0x0384,  0x8381,
    0x0280,  0x8285,  0x828f,  0x028a,  0x829b,  0x029e,  0x0294,  0x8291,
    0x82b3,  0x02b6,  0x02bc,  0x82b9,  0x02a8,  0x82ad,  0x82a7,  0x02a2,
    0x82e3,  0x02e6,  0x02ec,  0x82e9,  0x02f8,  0x82fd,  0x82f7,  0x02f2,
    0x02d0,  0x82d5,  0x82df,  0x02da,  0x82cb,  0x02ce,  0x02c4,  0x82c1,
    0x8243,  0x0246,  0x024c,  0x8249,  0x0258,  0x825d,  0x8257,  0x0252,
    0x0270,  0x8275,  0x827f,  0x027a,  0x826b,  0x026e,  0x0264,  0x8261,
    0x0220,  0x8225,  0x822f,  0x022a,  0x823b,  0x023e,  0x0234,  0x8231,
    0x8213,  0x0216,  0x021c,  0x8219,  0x0208,  0x820d,  0x8207,  0x0202
};

static uint16_t flac_crc16(uint16_t crc, uint8_t byte)
{
    return (crc << 8) ^ flac_crc16_table[(crc >> 8) ^ byte];
}
#if 0
/* Gives the previous CRC value, before hashing last_byte through it */
static uint16_t flac_crc16_undo(uint16_t crc, const uint8_t last_byte)
{
    /*
     * Given a byte b, gives a position X in flac_crc16_table, such as:
     *      flac_crc16_rev_table[flac_crc16_table[X] & 0xff] == X
     * This works because flac_crc16_table[i] & 0xff yields 256 unique values.
     */
    static const uint8_t flac_crc16_rev_table[256] = {
        0x00, 0x7f, 0xff, 0x80, 0x7e, 0x01, 0x81, 0xfe,
        0xfc, 0x83, 0x03, 0x7c, 0x82, 0xfd, 0x7d, 0x02,
        0x78, 0x07, 0x87, 0xf8, 0x06, 0x79, 0xf9, 0x86,
        0x84, 0xfb, 0x7b, 0x04, 0xfa, 0x85, 0x05, 0x7a,
        0xf0, 0x8f, 0x0f, 0x70, 0x8e, 0xf1, 0x71, 0x0e,
        0x0c, 0x73, 0xf3, 0x8c, 0x72, 0x0d, 0x8d, 0xf2,
        0x88, 0xf7, 0x77, 0x08, 0xf6, 0x89, 0x09, 0x76,
        0x74, 0x0b, 0x8b, 0xf4, 0x0a, 0x75, 0xf5, 0x8a,
        0x60, 0x1f, 0x9f, 0xe0, 0x1e, 0x61, 0xe1, 0x9e,
        0x9c, 0xe3, 0x63, 0x1c, 0xe2, 0x9d, 0x1d, 0x62,
        0x18, 0x67, 0xe7, 0x98, 0x66, 0x19, 0x99, 0xe6,
        0xe4, 0x9b, 0x1b, 0x64, 0x9a, 0xe5, 0x65, 0x1a,
        0x90, 0xef, 0x6f, 0x10, 0xee, 0x91, 0x11, 0x6e,
        0x6c, 0x13, 0x93, 0xec, 0x12, 0x6d, 0xed, 0x92,
        0xe8, 0x97, 0x17, 0x68, 0x96, 0xe9, 0x69, 0x16,
        0x14, 0x6b, 0xeb, 0x94, 0x6a, 0x15, 0x95, 0xea,
        0xc0, 0xbf, 0x3f, 0x40, 0xbe, 0xc1, 0x41, 0x3e,
        0x3c, 0x43, 0xc3, 0xbc, 0x42, 0x3d, 0xbd, 0xc2,
        0xb8, 0xc7, 0x47, 0x38, 0xc6, 0xb9, 0x39, 0x46,
        0x44, 0x3b, 0xbb, 0xc4, 0x3a, 0x45, 0xc5, 0xba,
        0x30, 0x4f, 0xcf, 0xb0, 0x4e, 0x31, 0xb1, 0xce,
        0xcc, 0xb3, 0x33, 0x4c, 0xb2, 0xcd, 0x4d, 0x32,
        0x48, 0x37, 0xb7, 0xc8, 0x36, 0x49, 0xc9, 0xb6,
        0xb4, 0xcb, 0x4b, 0x34, 0xca, 0xb5, 0x35, 0x4a,
        0xa0, 0xdf, 0x5f, 0x20, 0xde, 0xa1, 0x21, 0x5e,
        0x5c, 0x23, 0xa3, 0xdc, 0x22, 0x5d, 0xdd, 0xa2,
        0xd8, 0xa7, 0x27, 0x58, 0xa6, 0xd9, 0x59, 0x26,
        0x24, 0x5b, 0xdb, 0xa4, 0x5a, 0x25, 0xa5, 0xda,
        0x50, 0x2f, 0xaf, 0xd0, 0x2e, 0x51, 0xd1, 0xae,
        0xac, 0xd3, 0x53, 0x2c, 0xd2, 0xad, 0x2d, 0x52,
        0x28, 0x57, 0xd7, 0xa8, 0x56, 0x29, 0xa9, 0xd6,
        0xd4, 0xab, 0x2b, 0x54, 0xaa, 0xd5, 0x55, 0x2a,
    };
    uint8_t idx = flac_crc16_rev_table[crc & 0xff];
    return ((idx ^ last_byte) << 8) | ((crc ^ flac_crc16_table[idx]) >> 8);
}
#endif

static void Flush(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_sys->i_state = STATE_NOSYNC;
    p_sys->i_offset = 0;
    date_Set( &p_sys->pts, VLC_TICK_INVALID );
    block_BytestreamEmpty(&p_sys->bytestream);
}

static const uint8_t * FLACStartcodeHelper(const uint8_t *p, const uint8_t *end)
{
    while( p && p < end )
    {
        if( (p = memchr(p, 0xFF, end - p)) )
        {
            if( end - p > 1 && (p[1] & 0xFE) == 0xF8 )
                return p;
            else
                p++;
        }
    }
    return NULL;
}

static bool FLACStartcodeMatcher(uint8_t i, size_t i_pos, const uint8_t *p_startcode)
{
    VLC_UNUSED(p_startcode);
    return (i_pos == 0) ? i == 0xFF : (i & 0xFE) == 0xF8;
}

/* */
static block_t *Packetize(decoder_t *p_dec, block_t **pp_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    uint8_t p_header[FLAC_HEADER_SIZE_MAX];
    block_t *out = NULL, *in = NULL;

    if ( pp_block && *pp_block)
    {
        in = *pp_block;
        *pp_block = NULL;
        if (in->i_flags&(BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED)) {
            Flush(p_dec);
            p_sys->i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;
            if (in->i_flags&BLOCK_FLAG_CORRUPTED) {
                block_Release(in);
                return NULL;
            }
        }
    }

    if (!p_sys->b_stream_info)
        ProcessHeader(p_dec);

    if (p_sys->stream_info.channels > 8) {
        msg_Err(p_dec, "This stream uses too many audio channels (%d > 8)",
            p_sys->stream_info.channels);
        return NULL;
    }

    if ( in )
        block_BytestreamPush(&p_sys->bytestream, in);

    while (1) switch (p_sys->i_state) {
    case STATE_NOSYNC:
        if(block_FindStartcodeFromOffset(&p_sys->bytestream, &p_sys->i_offset,
                                         NULL, 2,
                                         FLACStartcodeHelper,
                                         FLACStartcodeMatcher) == VLC_SUCCESS)
        {
            p_sys->i_state = STATE_SYNC;
        }

        block_SkipBytes(&p_sys->bytestream, p_sys->i_offset);
        block_BytestreamFlush(&p_sys->bytestream);
        p_sys->i_offset = 0;

        if( p_sys->i_state != STATE_SYNC )
            return NULL; /* Need more data */
        /* fallthrough */

    case STATE_SYNC:
        /* Sync state is unverified until we have read frame header and checked CRC
           Once validated, we'll send data from NEXT_SYNC state where we'll
           compute frame size */
        p_sys->i_state = STATE_HEADER;
        /* fallthrough */

    case STATE_HEADER:
        /* Get FLAC frame header (MAX_FLAC_HEADER_SIZE bytes) */
        if (block_PeekBytes(&p_sys->bytestream, p_header, FLAC_HEADER_SIZE_MAX))
            return NULL; /* Need more data */

        /* Check if frame is valid and get frame info */
        int i_ret = FLAC_ParseSyncInfo(p_header, FLAC_HEADER_SIZE_MAX,
                             p_sys->b_stream_info ? &p_sys->stream_info : NULL,
                             flac_crc8, &p_sys->headerinfo);
        if (!i_ret) {
            msg_Dbg(p_dec, "emulated sync word");
            block_SkipByte(&p_sys->bytestream);
            p_sys->i_offset = 0;
            p_sys->i_state = STATE_NOSYNC;
            break;
        }

        p_sys->i_state = STATE_NEXT_SYNC;
        p_sys->i_offset = 1;
        p_sys->i_frame_size = 0;
        p_sys->crc = 0;

        /* We have to read until next frame sync code to compute current frame size
         * from that boundary.
         * The confusing part below is that sync code needs to be verified in case
         * it would appear in data, so we also need to check next frame header CRC
         */
        /* fallthrough */

    case STATE_NEXT_SYNC:
    {
        if(block_FindStartcodeFromOffset(&p_sys->bytestream, &p_sys->i_offset,
                                         NULL, 2,
                                         FLACStartcodeHelper,
                                         FLACStartcodeMatcher) != VLC_SUCCESS)
        {
            if( pp_block == NULL ) /* EOF/Drain */
            {
                p_sys->i_offset = block_BytestreamRemaining( &p_sys->bytestream );
                p_sys->i_state = STATE_GET_DATA;
                continue;
            }
            return NULL;
        }

        /* Check next header */
        uint8_t nextheader[FLAC_HEADER_SIZE_MAX];
        if (block_PeekOffsetBytes(&p_sys->bytestream, p_sys->i_offset,
                                  nextheader, FLAC_HEADER_SIZE_MAX))
            return NULL; /* Need more data */

        struct flac_header_info dummy;
        /* Check if frame is valid and get frame info */
        if(FLAC_ParseSyncInfo(nextheader, FLAC_HEADER_SIZE_MAX,
                              p_sys->b_stream_info ? &p_sys->stream_info : NULL,
                              NULL, &dummy) == 0)
        {
            p_sys->i_offset++;
            continue;
        }

        p_sys->i_state = STATE_GET_DATA;
        continue;
    }

    case STATE_GET_DATA:
        if( p_sys->i_offset < FLAC_FRAME_SIZE_MIN ||
            ( p_sys->b_stream_info &&
              p_sys->stream_info.min_framesize > p_sys->i_offset ) )
        {
            p_sys->i_offset += 1;
            p_sys->i_state = STATE_NEXT_SYNC;
            break;
        }
        else if( p_sys->b_stream_info &&
                 p_sys->stream_info.max_framesize > FLAC_FRAME_SIZE_MIN &&
                 p_sys->stream_info.max_framesize < p_sys->i_offset )
        {
            /* Something went wrong, truncate stream head and restart */
            block_SkipBytes( &p_sys->bytestream, FLAC_HEADER_SIZE_MAX + 2 );
            block_BytestreamFlush( &p_sys->bytestream );
            p_sys->i_frame_size = 0;
            p_sys->crc = 0;
            p_sys->i_offset = 0;
            p_sys->i_state = STATE_NOSYNC;
            p_sys->i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;
            break;
        }
        else
        {
            /* Allocate enough for storage */
            if( p_sys->i_offset > p_sys->i_buf )
            {
                size_t i_min_alloc = __MAX(p_sys->i_last_frame_size, p_sys->i_offset);
                uint8_t *p_realloc = realloc( p_sys->p_buf, i_min_alloc );
                if( p_realloc )
                {
                    p_sys->i_buf = i_min_alloc;
                    p_sys->p_buf = p_realloc;
                }

                if ( !p_sys->p_buf )
                    return NULL;
            }

            /* Copy from previous sync point (frame_size) up to to current (offset) */
            block_PeekOffsetBytes( &p_sys->bytestream, p_sys->i_frame_size,
                                   &p_sys->p_buf[p_sys->i_frame_size],
                                    p_sys->i_offset - p_sys->i_frame_size );

            /* update crc to include this data chunk */
            for( size_t i = p_sys->i_frame_size; i < p_sys->i_offset - 2; i++ )
                p_sys->crc = flac_crc16( p_sys->crc, p_sys->p_buf[i] );

            p_sys->i_frame_size = p_sys->i_offset;

            uint16_t stream_crc = GetWBE(&p_sys->p_buf[p_sys->i_offset - 2]);
            if( stream_crc != p_sys->crc )
            {
                /* False positive syncpoint as the CRC does not match */
                /* Add the 2 last bytes which were not the CRC sum, and go for next sync point */
                p_sys->crc = flac_crc16( p_sys->crc, p_sys->p_buf[p_sys->i_offset - 2] );
                p_sys->crc = flac_crc16( p_sys->crc, p_sys->p_buf[p_sys->i_offset - 1] );
                p_sys->i_offset += 1;
                p_sys->i_state = !pp_block ? STATE_NOSYNC : STATE_NEXT_SYNC;
                break; /* continue */
            }

            p_sys->i_state = STATE_SEND_DATA;

            /* clean */
            block_SkipBytes( &p_sys->bytestream, p_sys->i_offset );
            block_BytestreamFlush( &p_sys->bytestream );
            p_sys->i_last_frame_size = p_sys->i_frame_size;
            p_sys->i_offset = 0;
            p_sys->crc = 0;

            if( block_BytestreamRemaining(&p_sys->bytestream) > 0 )
                p_sys->i_state = STATE_SEND_DATA;
            else
                p_sys->i_state = STATE_NOSYNC;
        }
        break;

    case STATE_SEND_DATA:
        p_dec->fmt_out.audio.i_rate = p_sys->headerinfo.i_rate;
        p_dec->fmt_out.audio.i_channels = p_sys->headerinfo.i_channels;
        p_dec->fmt_out.audio.i_physical_channels = pi_channels_maps[p_sys->stream_info.channels];

        if( p_sys->bytestream.p_block->i_pts > date_Get( &p_sys->pts ) &&
            p_sys->bytestream.p_block->i_pts != VLC_TICK_INVALID )
        {
            date_Init( &p_sys->pts, p_sys->headerinfo.i_rate, 1 );
            date_Set( &p_sys->pts, p_sys->bytestream.p_block->i_pts );
            p_sys->bytestream.p_block->i_pts = VLC_TICK_INVALID;
        }


        out = block_heap_Alloc( p_sys->p_buf, p_sys->i_frame_size );
        if( out )
        {
            out->i_dts = out->i_pts = date_Get( &p_sys->pts );
            out->i_flags = p_sys->i_next_block_flags;
            p_sys->i_next_block_flags = 0;
        }
        else
            p_sys->p_buf = NULL;

        date_Increment( &p_sys->pts, p_sys->headerinfo.i_frame_length );
        if( out )
            out->i_length = date_Get( &p_sys->pts ) - out->i_pts;
        else
            free( p_sys->p_buf );

        p_sys->i_buf = 0;
        p_sys->p_buf = NULL;
        p_sys->i_frame_size = 0;
        p_sys->i_offset = 0;
        p_sys->i_state = STATE_NOSYNC;

        /* So p_block doesn't get re-added several times */
        if ( pp_block )
            *pp_block = block_BytestreamPop(&p_sys->bytestream);

        return out;
    }

    return NULL;
}

static int Open(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_FLAC)
        return VLC_EGENERIC;


    /* */
    p_dec->p_sys = p_sys = malloc(sizeof(*p_sys));
    if (!p_sys)
        return VLC_ENOMEM;

    p_sys->i_state       = STATE_NOSYNC;
    p_sys->i_offset      = 0;
    p_sys->b_stream_info = false;
    p_sys->i_last_frame_size = FLAC_FRAME_SIZE_MIN;
    p_sys->i_frame_size  = 0;
    p_sys->headerinfo.i_pts  = VLC_TICK_INVALID;
    p_sys->i_buf         = 0;
    p_sys->p_buf         = NULL;
    p_sys->i_next_block_flags = 0;
    block_BytestreamInit(&p_sys->bytestream);
    date_Init( &p_sys->pts, 1, 1 );

    /* */
    es_format_Copy(&p_dec->fmt_out, &p_dec->fmt_in);
    p_dec->fmt_out.i_codec = VLC_CODEC_FLAC;
    p_dec->fmt_out.b_packetized = true;

    /* */
    p_dec->pf_packetize = Packetize;
    p_dec->pf_flush     = Flush;
    p_dec->pf_get_cc    = NULL;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    block_BytestreamRelease(&p_sys->bytestream);
    free(p_sys->p_buf);
    free(p_sys);
}
