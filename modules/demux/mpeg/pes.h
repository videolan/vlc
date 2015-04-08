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

#define FROM_SCALE_NZ(x) ((x) * 100 / 9)
#define TO_SCALE_NZ(x)   ((x) * 9 / 100)

#define FROM_SCALE(x) (VLC_TS_0 + FROM_SCALE_NZ(x))
#define TO_SCALE(x)   TO_SCALE_NZ((x) - VLC_TS_0)

static inline mtime_t ExtractPESTimestamp( const uint8_t *p_data )
{
    return ((mtime_t)(p_data[ 0]&0x0e ) << 29)|
             (mtime_t)(p_data[1] << 22)|
            ((mtime_t)(p_data[2]&0xfe) << 14)|
             (mtime_t)(p_data[3] << 7)|
             (mtime_t)(p_data[4] >> 1);
}

static inline mtime_t ExtractMPEG1PESTimestamp( const uint8_t *p_data )
{
    return ((mtime_t)(p_data[ 0]&0x38 ) << 27)|
            ((mtime_t)(p_data[0]&0x03 ) << 28)|
             (mtime_t)(p_data[1] << 20)|
            ((mtime_t)(p_data[2]&0xf8 ) << 12)|
            ((mtime_t)(p_data[2]&0x03 ) << 13)|
             (mtime_t)(p_data[3] << 5) |
             (mtime_t)(p_data[4] >> 3);
}

static int ParsePESHeader( vlc_object_t *p_object, const uint8_t *p_header, size_t i_header,
                           unsigned *pi_skip, mtime_t *pi_dts, mtime_t *pi_pts,
                           uint8_t *pi_stream_id )
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
        break;
    default:
        if( ( p_header[6]&0xC0 ) == 0x80 )
        {
            /* mpeg2 PES */
            i_skip = p_header[8] + 9;

            if( p_header[7]&0x80 )    /* has pts */
            {
                if( i_header < 9 + 5 )
                    return VLC_EGENERIC;
                *pi_pts = ExtractPESTimestamp( &p_header[9] );

                if( p_header[7]&0x40 )    /* has dts */
                {
                    if( i_header < 14 + 5 )
                        return VLC_EGENERIC;
                    *pi_dts = ExtractPESTimestamp( &p_header[14] );
                }
            }
        }
        else
        {
            i_skip = 6;
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
            if( ( p_header[i_skip] & 0xC0 ) == 0x40 )
            {
                i_skip += 2;
            }

            if( i_header < i_skip + 1 )
                return VLC_EGENERIC;

            if(  p_header[i_skip]&0x20 )
            {
                if( i_header < i_skip + 5 )
                    return VLC_EGENERIC;
                *pi_pts = ExtractPESTimestamp( &p_header[i_skip] );

                if( p_header[i_skip]&0x10 )    /* has dts */
                {
                    if( i_header < i_skip + 10 )
                        return VLC_EGENERIC;
                    *pi_dts = ExtractPESTimestamp( &p_header[i_skip+5] );
                    i_skip += 10;
                }
                else
                {
                    i_skip += 5;
                }
            }
            else
            {
                i_skip += 1;
            }
        }
        break;
    }

    *pi_skip = i_skip;
    return VLC_SUCCESS;
}
