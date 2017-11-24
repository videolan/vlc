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

static inline bool ExtractPESTimestamp( const uint8_t *p_data, uint8_t i_flags, mtime_t *ret )
{
    /* !warn broken muxers set incorrect flags. see #17773 and #19140 */
    /* check marker bits, and i_flags = b 0010, 0011 or 0001 */
    if((p_data[0] & 0xC1) != 0x01 ||
       (p_data[2] & 0x01) != 0x01 ||
       (p_data[4] & 0x01) != 0x01 ||
       (p_data[0] & 0x30) == 0 || /* at least needs one bit */
       (p_data[0] >> 5) > i_flags ) /* needs flags 1x => 1x or flags 01 => 01 */
        return false;


    *ret =  ((mtime_t)(p_data[ 0]&0x0e ) << 29)|
             (mtime_t)(p_data[1] << 22)|
            ((mtime_t)(p_data[2]&0xfe) << 14)|
             (mtime_t)(p_data[3] << 7)|
             (mtime_t)(p_data[4] >> 1);
    return true;
}

/* PS SCR timestamp as defined in H222 2.5.3.2 */
static inline mtime_t ExtractPackHeaderTimestamp( const uint8_t *p_data )
{
    return ((mtime_t)(p_data[ 0]&0x38 ) << 27)|
            ((mtime_t)(p_data[0]&0x03 ) << 28)|
             (mtime_t)(p_data[1] << 20)|
            ((mtime_t)(p_data[2]&0xf8 ) << 12)|
            ((mtime_t)(p_data[2]&0x03 ) << 13)|
             (mtime_t)(p_data[3] << 5) |
             (mtime_t)(p_data[4] >> 3);
}

inline
static int ParsePESHeader( vlc_object_t *p_object, const uint8_t *p_header, size_t i_header,
                           unsigned *pi_skip, mtime_t *pi_dts, mtime_t *pi_pts,
                           uint8_t *pi_stream_id, bool *pb_pes_scambling )
{
    unsigned i_skip;

    if ( i_header < 9 )
        return VLC_EGENERIC;

    *pi_stream_id = p_header[3];

    switch( p_header[3] )
    {
    case 0xBC:  /* Program stream map */
    case 0xBE:  /* Padding */
    case 0xBF:  /* Private stream 2 */
    case 0xF0:  /* ECM */
    case 0xF1:  /* EMM */
    case 0xFF:  /* Program stream directory */
    case 0xF2:  /* DSMCC stream */
    case 0xF8:  /* ITU-T H.222.1 type E stream */
        i_skip = 6;
        if( pb_pes_scambling )
            *pb_pes_scambling = false;
        break;
    default:
        if( ( p_header[6]&0xC0 ) == 0x80 )
        {
            /* mpeg2 PES */
            i_skip = p_header[8] + 9;

            if( pb_pes_scambling )
                *pb_pes_scambling = p_header[6]&0x30;

            if( p_header[7]&0x80 )    /* has pts */
            {
                if( i_header >= 9 + 5 )
                   (void) ExtractPESTimestamp( &p_header[9], p_header[7] >> 6, pi_pts );

                if( ( p_header[7]&0x40 ) &&    /* has dts */
                    i_header >= 14 + 5 )
                   (void) ExtractPESTimestamp( &p_header[14], 0x01, pi_dts );
            }
        }
        else
        {
            /* FIXME?: WTH do we have undocumented MPEG1 packet stuff here ?
               This code path should not be valid, but seems some ppl did
               put MPEG1 packets into PS or TS.
               Non spec reference for packet format on http://andrewduncan.net/mpeg/mpeg-1.html */
            i_skip = 6;

            if( pb_pes_scambling )
                *pb_pes_scambling = false;

            if( i_header < i_skip + 1 )
                return VLC_EGENERIC;
            while( i_skip < 23 && p_header[i_skip] == 0xff )
            {
                i_skip++;
                if( i_header < i_skip + 1 )
                    return VLC_EGENERIC;
            }
            if( i_skip == 23 )
            {
                msg_Err( p_object, "too much MPEG-1 stuffing" );
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
                    (void) ExtractPESTimestamp( &p_header[i_skip], p_header[i_skip] >> 4, pi_pts );

                if( ( p_header[i_skip]&0x10 ) &&     /* has dts */
                    i_header >= i_skip + 10 )
                {
                    (void) ExtractPESTimestamp( &p_header[i_skip+5], 0x01, pi_dts );
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

    *pi_skip = i_skip;
    return VLC_SUCCESS;
}

#endif
