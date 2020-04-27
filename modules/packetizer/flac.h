/*****************************************************************************
 * flac.h: fLAC audio headers
 *****************************************************************************
 * Copyright (C) 2001-2018 VLC authors, VideoLabs and VideoLAN
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
#include <vlc_common.h>

#define FLAC_HEADER_SIZE_MIN 9
#define FLAC_HEADER_SIZE_MAX 16
#define FLAC_STREAMINFO_SIZE 34
#define FLAC_FRAME_SIZE_MIN ((48+(8 + 4 + 1*4)+FLAC_HEADER_SIZE_MAX)/8)

struct flac_stream_info
{
    unsigned min_blocksize, max_blocksize;
    unsigned min_framesize, max_framesize;
    unsigned sample_rate;
    unsigned channels;
    unsigned bits_per_sample;
    uint64_t total_samples;
};

struct flac_header_info
{
    vlc_tick_t i_pts;
    unsigned i_rate;
    unsigned i_channels;
    unsigned i_bits_per_sample;
    unsigned i_frame_length;
};

static inline void FLAC_ParseStreamInfo( const uint8_t *p_buf,
                                         struct flac_stream_info *stream_info )
{
    stream_info->min_blocksize = GetWBE( &p_buf[0] );
    stream_info->min_blocksize = VLC_CLIP( stream_info->min_blocksize, 16, 65535 );

    stream_info->max_blocksize = GetWBE( &p_buf[2] );
    stream_info->max_blocksize = VLC_CLIP( stream_info->max_blocksize, 16, 65535 );

    stream_info->min_framesize = GetDWBE( &p_buf[3] ) & 0x00FFFFFF;
    stream_info->min_framesize = __MAX( stream_info->min_framesize, FLAC_FRAME_SIZE_MIN );

    stream_info->max_framesize = GetDWBE( &p_buf[6] ) & 0x00FFFFFF;

    stream_info->sample_rate = GetDWBE( &p_buf[10] ) >> 12;
    stream_info->channels = (p_buf[12] & 0x0F >> 1) + 1;
    stream_info->bits_per_sample = (((p_buf[12] & 0x01) << 4) | p_buf[13] >> 4) + 1;

    stream_info->total_samples = GetQWBE(&p_buf[4+6]) & ((INT64_C(1)<<36)-1);
}

/* Will return INT64_MAX for an invalid utf-8 sequence */
static inline int64_t read_utf8(const uint8_t *p_buf, unsigned i_buf, int *pi_read)
{
    /* Max coding bits is 56 - 8 */
    /* Value max precision is 36 bits */
    int64_t i_result = 0;
    unsigned i;

    if(i_buf < 1)
        return INT64_MAX;

    if (!(p_buf[0] & 0x80)) { /* 0xxxxxxx */
        i_result = p_buf[0];
        i = 0;
    } else if (p_buf[0] & 0xC0 && !(p_buf[0] & 0x20)) { /* 110xxxxx */
        i_result = p_buf[0] & 0x1F;
        i = 1;
    } else if (p_buf[0] & 0xE0 && !(p_buf[0] & 0x10)) { /* 1110xxxx */
        i_result = p_buf[0] & 0x0F;
        i = 2;
    } else if (p_buf[0] & 0xF0 && !(p_buf[0] & 0x08)) { /* 11110xxx */
        i_result = p_buf[0] & 0x07;
        i = 3;
    } else if (p_buf[0] & 0xF8 && !(p_buf[0] & 0x04)) { /* 111110xx */
        i_result = p_buf[0] & 0x03;
        i = 4;
    } else if (p_buf[0] & 0xFC && !(p_buf[0] & 0x02)) { /* 1111110x */
        i_result = p_buf[0] & 0x01;
        i = 5;
    } else if (p_buf[0] & 0xFE && !(p_buf[0] & 0x01)) { /* 11111110 */
        i_result = 0;
        i = 6;
    } else {
        return INT64_MAX;
    }

    if(i_buf < i + 1)
        return INT64_MAX;

    for (unsigned j = 1; j <= i; j++) {
        if (!(p_buf[j] & 0x80) || (p_buf[j] & 0x40)) { /* 10xxxxxx */
            return INT64_MAX;
        }
        i_result <<= 6;
        i_result |= (p_buf[j] & 0x3F);
    }

    *pi_read = i;
    return i_result;
}

/*****************************************************************************
 * FLAC_ParseSyncInfo: parse FLAC sync info
 * - stream_info can be NULL
 * - pf_crc8 can be NULL to skip crc check
 * Returns: 1 on success, 0 on failure, and -1 if could be incorrect
 *****************************************************************************/
static inline int FLAC_ParseSyncInfo(const uint8_t *p_buf, unsigned i_buf,
                                     const struct flac_stream_info *stream_info,
                                     uint8_t(*pf_crc8)(const uint8_t *, size_t),
                                     struct flac_header_info *h)
{
    bool b_guessing = false;

    if(unlikely(i_buf < FLAC_HEADER_SIZE_MIN))
        return 0;

    /* Check syncword */
    if (p_buf[0] != 0xFF || (p_buf[1] & 0xFE) != 0xF8)
        return 0;

    /* Check there is no emulated sync code in the rest of the header */
    if (p_buf[2] == 0xff || p_buf[3] == 0xFF)
        return 0;

    /* Find blocksize (framelength) */
    int blocksize_hint = 0;
    unsigned blocksize = p_buf[2] >> 4;
    if (blocksize >= 8) {
        blocksize = 256 << (blocksize - 8);
    } else if (blocksize == 0) { /* value 0 is reserved */
        b_guessing = true;
        if (stream_info &&
            stream_info->min_blocksize == stream_info->max_blocksize)
            blocksize = stream_info->min_blocksize;
        else
            return 0; /* We can't do anything with this */
    } else if (blocksize == 1) {
        blocksize = 192;
    } else if (blocksize == 6 || blocksize == 7) {
        blocksize_hint = blocksize;
        blocksize = 0;
    } else /* 2, 3, 4, 5 */ {
        blocksize = 576 << (blocksize - 2);
    }

    if (stream_info && !blocksize_hint)
        if (blocksize < stream_info->min_blocksize ||
            blocksize > stream_info->max_blocksize)
            return 0;

    /* Find samplerate */
    int samplerate_hint = p_buf[2] & 0xf;
    unsigned int samplerate;
    if (samplerate_hint == 0) {
        if (stream_info)
            samplerate = stream_info->sample_rate;
        else
            return 0; /* We can't do anything with this */
    } else if (samplerate_hint == 15) {
        return 0; /* invalid */
    } else if (samplerate_hint < 12) {
        static const int16_t flac_samplerate[12] = {
            0,    8820, 17640, 19200,
            800,  1600, 2205,  2400,
            3200, 4410, 4800,  9600,
        };
        samplerate = flac_samplerate[samplerate_hint] * 10;
    } else {
        samplerate = 0; /* at end of header */
    }

    /* Find channels */
    unsigned channels = p_buf[3] >> 4;
    if (channels >= 8) {
        if (channels >= 11) /* reserved */
            return 0;
        channels = 2;
    } else
        channels++;


    /* Find bits per sample */
    static const int8_t flac_bits_per_sample[8] = {
        0, 8, 12, -1, 16, 20, 24, -1
    };
    int bits_per_sample = flac_bits_per_sample[(p_buf[3] & 0x0e) >> 1];
    if (bits_per_sample == 0) {
        if (stream_info)
            bits_per_sample = stream_info->bits_per_sample;
        else
            return 0;
    } else if (bits_per_sample < 0)
        return 0;


    /* reserved for future use */
    if (p_buf[3] & 0x01)
        return 0;

    /* End of fixed size header */
    unsigned i_header = 4;

    /* > FLAC_HEADER_SIZE_MIN checks start here */

    /* Check Sample/Frame number */
    int i_read;
    int64_t i_fsnumber = read_utf8(&p_buf[i_header++], i_buf - 4, &i_read);
    if ( i_fsnumber == INT64_MAX )
        return 0;

    i_header += i_read;

    /* Read blocksize */
    if (blocksize_hint) {
        if(i_header == i_buf)
            return 0;
        blocksize = p_buf[i_header++];
        if (blocksize_hint == 7) {
            blocksize <<= 8;
            blocksize |= p_buf[i_header++];
        }
        blocksize++;
    }

    /* Read sample rate */
    if (samplerate == 0) {
        if(i_header == i_buf)
            return 0;
        samplerate = p_buf[i_header++];
        if (samplerate_hint != 12) { /* 16 bits */
            if(i_header == i_buf)
                return 0;
            samplerate <<= 8;
            samplerate |= p_buf[i_header++];
        }

        if (samplerate_hint == 12)
            samplerate *= 1000;
        else if (samplerate_hint == 14)
            samplerate *= 10;
    }

    if ( !samplerate )
        return 0;

    if(i_header == i_buf) /* no crc space */
        return 0;

    /* Check the CRC-8 byte */
    if (pf_crc8 &&
        pf_crc8(p_buf, i_header) != p_buf[i_header])
        return 0;

    /* Sanity check using stream info header when possible */
    if (stream_info) {
        if (blocksize < stream_info->min_blocksize ||
            blocksize > stream_info->max_blocksize)
            return 0;
        if ((unsigned)bits_per_sample != stream_info->bits_per_sample)
            return 0;
        if (samplerate != stream_info->sample_rate)
            return 0;
    }

    /* Compute from frame absolute time */
    if ( (p_buf[1] & 0x01) == 0  ) /* Fixed blocksize stream / Frames */
        h->i_pts = VLC_TICK_0 + vlc_tick_from_samples(blocksize * i_fsnumber, samplerate);
    else /* Variable blocksize stream / Samples */
        h->i_pts = VLC_TICK_0 + vlc_tick_from_samples(i_fsnumber, samplerate);

    h->i_bits_per_sample = bits_per_sample;
    h->i_rate = samplerate;
    h->i_channels = channels;
    h->i_frame_length = blocksize;

    return b_guessing ? -1 : 1;
}
