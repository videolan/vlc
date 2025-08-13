/*****************************************************************************
 * pes.h: PES Packet helpers
 *****************************************************************************
 * Copyright (C) 2004-2015 VLC authors and VideoLAN
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
#ifndef VLC_MPEG_PES_H
#define VLC_MPEG_PES_H

#include "timestamps.h"

#define STREAM_ID_PROGRAM_STREAM_MAP         0xBC
#define STREAM_ID_PRIVATE_STREAM_1           0xBD
#define STREAM_ID_PADDING                    0xBE
#define STREAM_ID_PRIVATE_STREAM_2           0xBF
#define STREAM_ID_AUDIO_STREAM_0             0xC0
#define STREAM_ID_VIDEO_STREAM_0             0xE0
#define STREAM_ID_ECM                        0xF0
#define STREAM_ID_EMM                        0xF1
#define STREAM_ID_DSM_CC                     0xF2
#define STREAM_ID_H222_1_TYPE_E              0xF8
#define STREAM_ID_METADATA_STREAM            0xFC
#define STREAM_ID_EXTENDED_STREAM_ID         0xFD
#define STREAM_ID_PROGRAM_STREAM_DIRECTORY   0xFF

/* MPEG-2 PTS/DTS */
static inline ts_90khz_t GetPESTimestamp( const uint8_t *p_data )
{
    /* prefixed by 4 bits 0010 or 0011 */
    return  ((ts_90khz_t)(p_data[ 0]&0x0e ) << 29)|
             (ts_90khz_t)(p_data[1] << 22)|
            ((ts_90khz_t)(p_data[2]&0xfe) << 14)|
             (ts_90khz_t)(p_data[3] << 7)|
             (ts_90khz_t)(p_data[4] >> 1);
}

static inline bool ExtractPESTimestamp( const uint8_t *p_data, uint8_t i_flags, ts_90khz_t *ret )
{
    /* !warn broken muxers set incorrect flags. see #17773 and #19140 */
    /* check marker bits, and i_flags = b 0010, 0011 or 0001 */
    if((p_data[0] & 0xC1) != 0x01 ||
       (p_data[2] & 0x01) != 0x01 ||
       (p_data[4] & 0x01) != 0x01 ||
       (p_data[0] & 0x30) == 0 || /* at least needs one bit */
       (p_data[0] >> 5) > i_flags ) /* needs flags 1x => 1x or flags 01 => 01 */
        return false;


    *ret =  GetPESTimestamp( p_data );
    return true;
}

/* PS SCR timestamp as defined in H222 2.5.3.2 */
static inline ts_90khz_t ExtractPackHeaderTimestamp( const uint8_t *p_data )
{
    /* prefixed by 2 bits 01 */
    return  ((ts_90khz_t)(p_data[0]&0x38 ) << 27)|
            ((ts_90khz_t)(p_data[0]&0x03 ) << 28)|
             (ts_90khz_t)(p_data[1] << 20)|
            ((ts_90khz_t)(p_data[2]&0xf8 ) << 12)|
            ((ts_90khz_t)(p_data[2]&0x03 ) << 13)|
             (ts_90khz_t)(p_data[3] << 5) |
             (ts_90khz_t)(p_data[4] >> 3);
}

typedef struct
{
    ts_90khz_t i_dts;
    ts_90khz_t i_pts;
    uint8_t i_stream_id;
    bool b_scrambling;
    unsigned i_size;
} ts_pes_header_t;

static inline void ts_pes_header_init(ts_pes_header_t *h)
{
    h->i_dts = TS_90KHZ_INVALID;
    h->i_pts = TS_90KHZ_INVALID;
    h->i_stream_id = 0;
    h->b_scrambling = false;
    h->i_size = 0;
}

inline
static int ParsePESHeader( struct vlc_logger *p_logger, const uint8_t *p_header, size_t i_header,
                           ts_pes_header_t *h )
{
    unsigned i_skip;

    if ( i_header < 9 )
        return VLC_EGENERIC;

    if( p_header[0] != 0 || p_header[1] != 0 || p_header[2] != 1 )
        return VLC_EGENERIC;

    h->i_stream_id = p_header[3];

    switch( p_header[3] )
    {
    case STREAM_ID_PROGRAM_STREAM_MAP:
    case STREAM_ID_PADDING:
    case STREAM_ID_PRIVATE_STREAM_2:
    case STREAM_ID_ECM:
    case STREAM_ID_EMM:
    case STREAM_ID_PROGRAM_STREAM_DIRECTORY:
    case STREAM_ID_DSM_CC:
    case STREAM_ID_H222_1_TYPE_E:
        i_skip = 6;
        h->b_scrambling = false;
        break;
    default:
        if( ( p_header[6]&0xC0 ) == 0x80 )
        {
            /* mpeg2 PES */
            // 9 = syncword(3), stream ID(1), length(2), MPEG2 PES(1), flags(1), header_len(1)
            // p_header[8] = header_len(1)
            i_skip = p_header[8] + 9;

            h->b_scrambling = p_header[6]&0x30;

            if( p_header[7]&0x80 )    /* has pts */
            {
                if( i_header >= 9 + 5 )
                   (void) ExtractPESTimestamp( &p_header[9], p_header[7] >> 6, &h->i_pts );

                if( ( p_header[7]&0x40 ) &&    /* has dts */
                    i_header >= 14 + 5 )
                   (void) ExtractPESTimestamp( &p_header[14], 0x01, &h->i_dts );
            }
        }
        else
        {
            /* FIXME?: WTH do we have undocumented MPEG1 packet stuff here ?
               This code path should not be valid, but seems some ppl did
               put MPEG1 packets into PS or TS.
               Non spec reference for packet format on http://andrewduncan.net/mpeg/mpeg-1.html */
            i_skip = 6;

            h->b_scrambling = false;

            while( i_skip < 23 && p_header[i_skip] == 0xff )
            {
                i_skip++;
                if( i_header < i_skip + 1 )
                    return VLC_EGENERIC;
            }
            if( i_skip == 23 )
            {
                vlc_error( p_logger, "too much MPEG-1 stuffing" );
                return VLC_EGENERIC;
            }
            /* Skip STD buffer size */
            if( ( p_header[i_skip] & 0xC0 ) == 0x40 )
            {
                i_skip += 2;
            }

            if( i_header < i_skip + 1 )
                return VLC_EGENERIC;

            if(  p_header[i_skip]&0x20 )
            {
                if( i_header >= i_skip + 5 )
                    (void) ExtractPESTimestamp( &p_header[i_skip], p_header[i_skip] >> 4, &h->i_pts );

                if( ( p_header[i_skip]&0x10 ) &&     /* has dts */
                    i_header >= i_skip + 10 )
                {
                    (void) ExtractPESTimestamp( &p_header[i_skip+5], 0x01, &h->i_dts );
                    i_skip += 10;
                }
                else
                {
                    i_skip += 5;
                }
            }
            else
            {
                if( (p_header[i_skip] & 0xFF) != 0x0F ) /* No pts/dts, lowest bits set to 0x0F */
                    return VLC_EGENERIC;
                i_skip += 1;
            }
        }
        break;
    }

    h->i_size = i_skip;
    return VLC_SUCCESS;
}

#endif
