/*****************************************************************************
 * ps.h: Program Stream demuxer helper
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#define PS_TK_COUNT (512 - 0xc0)
#define PS_ID_TO_TK( id ) ((id) <= 0xff ? (id) - 0xc0 : ((id)&0xff) + (256 - 0xc0))

typedef struct
{
    vlc_bool_t  b_seen;
    int         i_skip;
    es_out_id_t *es;
    es_format_t fmt;
} ps_track_t;

/* Init a set of track */
static inline void ps_track_init( ps_track_t tk[PS_TK_COUNT] )
{
    int i;
    for( i = 0; i < PS_TK_COUNT; i++ )
    {
        tk[i].b_seen = VLC_FALSE;
        tk[i].i_skip = 0;
        tk[i].es     = NULL;
        es_format_Init( &tk[i].fmt, UNKNOWN_ES, 0 );
    }
}

/* From id fill i_skip and es_format_t */
static inline int ps_track_fill( ps_track_t *tk, int i_id )
{
    tk->i_skip = 0;
    if( ( i_id&0xff00 ) == 0xbd00 )
    {
        if( ( i_id&0xf8 ) == 0x88 )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('d','t','s',' ') );
            tk->i_skip = 4;
        }
        else if( ( i_id&0xf0 ) == 0x80 )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('a','5','2',' ') );
            tk->i_skip = 4;
        }
        else if( ( i_id&0xe0 ) == 0x20 )
        {
            es_format_Init( &tk->fmt, SPU_ES, VLC_FOURCC('s','p','u',' ') );
            tk->i_skip = 1;
        }
        else if( ( i_id&0xf0 ) == 0xa0 )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('l','p','c','m') );
            tk->i_skip = 1;
        }
        else if( ( i_id&0xff ) == 0x70 )
        {
            es_format_Init( &tk->fmt, SPU_ES, VLC_FOURCC('o','g','t',' ') );
        }
        else if( ( i_id&0xfc ) == 0x00 )
        {
            es_format_Init( &tk->fmt, SPU_ES, VLC_FOURCC('c','v','d',' ') );
        }
        else
        {
            es_format_Init( &tk->fmt, UNKNOWN_ES, 0 );
            return VLC_EGENERIC;
        }
    }
    else
    {
        if( ( i_id&0xf0 ) == 0xe0 )
        {
            es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('m','p','g','v') );
        }
        else if( ( i_id&0xe0 ) == 0xc0 )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('m','p','g','a') );
        }
        else
        {
            es_format_Init( &tk->fmt, UNKNOWN_ES, 0 );
            return VLC_EGENERIC;
        }
    }

    /* PES packets usually contain truncated frames */
    tk->fmt.b_packetized = VLC_FALSE;

    return VLC_SUCCESS;
}

/* return the id of a PES (should be valid) */
static inline int ps_pkt_id( block_t *p_pkt )
{
    if( p_pkt->p_buffer[3] == 0xbd &&
        p_pkt->i_buffer >= 9 &&
        p_pkt->i_buffer >= 9 + p_pkt->p_buffer[8] )
    {
        return 0xbd00 | p_pkt->p_buffer[9+p_pkt->p_buffer[8]];
    }
    return p_pkt->p_buffer[3];
}

/* return the size of the next packet
 * XXX you need to give him at least 14 bytes (and it need to start as a
 * valid packet) */
static inline int ps_pkt_size( uint8_t *p, int i_peek )
{
    if( p[3] == 0xb9 && i_peek >= 4 )
    {
        return 4;
    }
    else if( p[3] == 0xba )
    {
        if( (p[4] >> 6) == 0x01 && i_peek >= 14 )
        {
            return 14 + (p[13]&0x07);
        }
        else if( (p[4] >> 4) == 0x02 && i_peek >= 12 )
        {
            return 12;
        }
        return -1;
    }
    else if( i_peek >= 6 )
    {
        return 6 + ((p[4]<<8) | p[5] );
    }
    return -1;
}

/* parse a PACK PES */
static inline int ps_pkt_parse_pack( block_t *p_pkt, int64_t *pi_scr,
                                     int *pi_mux_rate )
{
    uint8_t *p = p_pkt->p_buffer;
    if( p_pkt->i_buffer >= 14 && (p[4] >> 6) == 0x01 )
    {
        *pi_scr =((((int64_t)p[4]&0x38) << 27 )|
                  (((int64_t)p[4]&0x03) << 28 )|
                   ((int64_t)p[5] << 20 )|
                  (((int64_t)p[6]&0xf8) << 12 )|
                  (((int64_t)p[6]&0x03) << 13 )|
                   ((int64_t)p[7] << 5 )|
                   ((int64_t)p[8] >> 3 )) * 100 / 9;

        *pi_mux_rate = ( p[10] << 14 )|( p[11] << 6 )|( p[12] >> 2);
    }
    else if( p_pkt->i_buffer >= 12 && (p[4] >> 4) == 0x02 )
    {
        *pi_scr =((((int64_t)p[4]&0x0e) << 29 )|
                   ((int64_t)p[5] << 22 )|
                  (((int64_t)p[6]&0xfe) << 14 )|
                   ((int64_t)p[7] <<  7 )|
                   ((int64_t)p[8] >> 1 )) * 100 / 9;

        *pi_mux_rate = ( ( p[9]&0x7f )<< 15 )|( p[10] << 7 )|( p[11] >> 1);
    }
    else
    {
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/* Parse a SYSTEM PES */
static inline int ps_pkt_parse_system( block_t *p_pkt,
                                       ps_track_t tk[PS_TK_COUNT] )
{
    uint8_t *p = &p_pkt->p_buffer[6 + 3 + 1 + 1 + 1];

    /* System header is not useable if it references private streams (0xBD)
     * or 'all audio streams' (0xB8) or 'all video streams' (0xB9) */
    while( p < &p_pkt->p_buffer[p_pkt->i_buffer] )
    {
        int i_id = p[0];

        /* fprintf( stderr, "   SYSTEM_START_CODEEE: id=0x%x\n", p[0] ); */
        if( p[0] >= 0xBC || p[0] == 0xB8 || p[0] == 0xB9 ) p += 2;
        p++;

        if( p[0] >= 0xc0 )
        {
            int i_tk = PS_ID_TO_TK( i_id );

            if( !tk[i_tk].b_seen )
            {
                if( !ps_track_fill( &tk[i_tk], i_id ) )
                {
                    tk[i_tk].b_seen = VLC_TRUE;
                }
            }
        }
    }
    return VLC_SUCCESS;
}

/* Parse a PES (and skip i_skip_extra in the payload) */
static inline int ps_pkt_parse_pes( block_t *p_pes, int i_skip_extra )
{
    uint8_t header[30];
    int     i_skip  = 0;

    memcpy( header, p_pes->p_buffer, __MIN( p_pes->i_buffer, 30 ) );

    switch( header[3] )
    {
        case 0xBC:  /* Program stream map */
        case 0xBE:  /* Padding */
        case 0xBF:  /* Private stream 2 */
        case 0xB0:  /* ECM */
        case 0xB1:  /* EMM */
        case 0xFF:  /* Program stream directory */
        case 0xF2:  /* DSMCC stream */
        case 0xF8:  /* ITU-T H.222.1 type E stream */
            i_skip = 6;
            break;

        default:
            if( ( header[6]&0xC0 ) == 0x80 )
            {
                /* mpeg2 PES */
                i_skip = header[8] + 9;

                if( header[7]&0x80 )    /* has pts */
                {
                    p_pes->i_pts = ((mtime_t)(header[ 9]&0x0e ) << 29)|
                                    (mtime_t)(header[10] << 22)|
                                   ((mtime_t)(header[11]&0xfe) << 14)|
                                    (mtime_t)(header[12] << 7)|
                                    (mtime_t)(header[13] >> 1);

                    if( header[7]&0x40 )    /* has dts */
                    {
                         p_pes->i_dts = ((mtime_t)(header[14]&0x0e ) << 29)|
                                         (mtime_t)(header[15] << 22)|
                                        ((mtime_t)(header[16]&0xfe) << 14)|
                                         (mtime_t)(header[17] << 7)|
                                         (mtime_t)(header[18] >> 1);
                    }
                }
            }
            else
            {
                i_skip = 6;
                while( i_skip < 23 && header[i_skip] == 0xff )
                {
                    i_skip++;
                }
                if( i_skip == 23 )
                {
                    /* msg_Err( p_demux, "too much MPEG-1 stuffing" ); */
                    return VLC_EGENERIC;
                }
                if( ( header[i_skip] & 0xC0 ) == 0x40 )
                {
                    i_skip += 2;
                }

                if(  header[i_skip]&0x20 )
                {
                     p_pes->i_pts = ((mtime_t)(header[i_skip]&0x0e ) << 29)|
                                     (mtime_t)(header[i_skip+1] << 22)|
                                    ((mtime_t)(header[i_skip+2]&0xfe) << 14)|
                                     (mtime_t)(header[i_skip+3] << 7)|
                                     (mtime_t)(header[i_skip+4] >> 1);

                    if( header[i_skip]&0x10 )    /* has dts */
                    {
                         p_pes->i_dts = ((mtime_t)(header[i_skip+5]&0x0e ) << 29)|
                                         (mtime_t)(header[i_skip+6] << 22)|
                                        ((mtime_t)(header[i_skip+7]&0xfe) << 14)|
                                         (mtime_t)(header[i_skip+8] << 7)|
                                         (mtime_t)(header[i_skip+9] >> 1);
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
    }

    i_skip += i_skip_extra;

    if( p_pes->i_buffer <= i_skip )
    {
        return VLC_EGENERIC;
    }

    p_pes->p_buffer += i_skip;
    p_pes->i_buffer -= i_skip;

    p_pes->i_dts = 100 * p_pes->i_dts / 9;
    p_pes->i_pts = 100 * p_pes->i_pts / 9;

    return VLC_SUCCESS;
}
