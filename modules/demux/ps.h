/*****************************************************************************
 * ps.h: Program Stream demuxer helper
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc_demux.h>
#include <assert.h>

/* 256-0xC0 for normal stream, 256 for 0xbd stream, 256 for 0xfd stream */
#define PS_TK_COUNT (768 - 0xc0)
#define PS_ID_TO_TK( id ) ((id) <= 0xff ? (id) - 0xc0 : \
            ((id)&0xff) + (((id)&0xff00) == 0xbd00 ? 256-0xC0 : 512-0xc0) )

typedef struct ps_psm_t ps_psm_t;
static inline int ps_id_to_type( const ps_psm_t *, int );
static inline const uint8_t *ps_id_to_lang( const ps_psm_t *, int );

typedef struct
{
    bool  b_seen;
    int         i_skip;
    int         i_id;
    es_out_id_t *es;
    es_format_t fmt;
    mtime_t     i_first_pts;
    mtime_t     i_last_pts;

} ps_track_t;

/* Init a set of track */
static inline void ps_track_init( ps_track_t tk[PS_TK_COUNT] )
{
    int i;
    for( i = 0; i < PS_TK_COUNT; i++ )
    {
        tk[i].b_seen = false;
        tk[i].i_skip = 0;
        tk[i].i_id   = 0;
        tk[i].es     = NULL;
        tk[i].i_first_pts = -1;
        tk[i].i_last_pts = -1;
        es_format_Init( &tk[i].fmt, UNKNOWN_ES, 0 );
    }
}

/* From id fill i_skip and es_format_t */
static inline int ps_track_fill( ps_track_t *tk, ps_psm_t *p_psm, int i_id )
{
    tk->i_skip = 0;
    tk->i_id = i_id;
    if( ( i_id&0xff00 ) == 0xbd00 )
    {
        if( ( i_id&0xf8 ) == 0x88 || (i_id&0xf8) == 0x98 )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('d','t','s',' ') );
            tk->i_skip = 4;
        }
        else if( ( i_id&0xf0 ) == 0x80
               ||  (i_id&0xf0) == 0xc0 ) /* AC-3, Can also be used for DD+/E-AC-3 */
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('a','5','2',' ') );
            tk->i_skip = 4;
        }
        else if( (i_id&0xf0) == 0xb0 )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('m','l','p',' ') );
            /* FIXME / untested ... no known decoder (at least not in VLC/ffmpeg) */
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
        else if( ( i_id&0xff ) == 0x10 )
        {
            es_format_Init( &tk->fmt, SPU_ES, VLC_FOURCC('t','e','l','x') );
        }
        else
        {
            es_format_Init( &tk->fmt, UNKNOWN_ES, 0 );
            return VLC_EGENERIC;
        }
    }
    else if( (i_id&0xff00) == 0xfd00 )
    {
        uint8_t i_sub_id = i_id & 0xff;
        if( i_sub_id >= 0x55 && i_sub_id <= 0x5f )
        {
            es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('W','V','C','1') );
        }
        else
        {
            es_format_Init( &tk->fmt, UNKNOWN_ES, 0 );
            return VLC_EGENERIC;
        }
    }
    else
    {
        int i_type = ps_id_to_type( p_psm , i_id );

        es_format_Init( &tk->fmt, UNKNOWN_ES, 0 );

        if( (i_id&0xf0) == 0xe0 && i_type == 0x1b )
        {
            es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('h','2','6','4') );
        }
        else if( (i_id&0xf0) == 0xe0 && i_type == 0x10 )
        {
            es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('m','p','4','v') );
        }
        else if( (i_id&0xf0) == 0xe0 && i_type == 0x02 )
        {
            es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('m','p','g','v') );
        }
        else if( ( i_id&0xe0 ) == 0xc0 && i_type == 0x0f )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('m','p','4','a') );
        }
        else if( ( i_id&0xe0 ) == 0xc0 && i_type == 0x03 )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('m','p','g','a') );
        }

        if( tk->fmt.i_cat == UNKNOWN_ES && ( i_id&0xf0 ) == 0xe0 )
        {
            es_format_Init( &tk->fmt, VIDEO_ES, VLC_FOURCC('m','p','g','v') );
        }
        else if( tk->fmt.i_cat == UNKNOWN_ES && ( i_id&0xe0 ) == 0xc0 )
        {
            es_format_Init( &tk->fmt, AUDIO_ES, VLC_FOURCC('m','p','g','a') );
        }
        else if( tk->fmt.i_cat == UNKNOWN_ES ) return VLC_EGENERIC;
    }

    /* PES packets usually contain truncated frames */
    tk->fmt.b_packetized = false;

    if( ps_id_to_lang( p_psm, i_id ) )
    {
        tk->fmt.psz_language = malloc( 4 );
        if( tk->fmt.psz_language )
        {
            memcpy( tk->fmt.psz_language, ps_id_to_lang( p_psm , i_id ), 3 );
            tk->fmt.psz_language[3] = 0;
        }
    }

    return VLC_SUCCESS;
}

/* return the id of a PES (should be valid) */
static inline int ps_pkt_id( block_t *p_pkt )
{
    if( p_pkt->p_buffer[3] == 0xbd &&
        p_pkt->i_buffer >= 9 &&
        p_pkt->i_buffer >= 9 + (size_t)p_pkt->p_buffer[8] )
    {
        /* VOB extension */
        return 0xbd00 | p_pkt->p_buffer[9+p_pkt->p_buffer[8]];
    }
    else if( p_pkt->p_buffer[3] == 0xfd &&
             p_pkt->i_buffer >= 9 &&
             (p_pkt->p_buffer[6]&0xC0) == 0x80 &&   /* mpeg2 */
             (p_pkt->p_buffer[7]&0x01) == 0x01 )    /* extension_flag */
    {
        /* ISO 13818 amendment 2 and SMPTE RP 227 */
        const uint8_t i_flags = p_pkt->p_buffer[7];
        unsigned int i_skip = 9;

        /* Find PES extension */
        if( (i_flags & 0x80 ) )
        {
            i_skip += 5;        /* pts */
            if( (i_flags & 0x40) )
                i_skip += 5;    /* dts */
        }
        if( (i_flags & 0x20 ) )
            i_skip += 6;
        if( (i_flags & 0x10 ) )
            i_skip += 3;
        if( (i_flags & 0x08 ) )
            i_skip += 1;
        if( (i_flags & 0x04 ) )
            i_skip += 1;
        if( (i_flags & 0x02 ) )
            i_skip += 2;

        if( i_skip < p_pkt->i_buffer && (p_pkt->p_buffer[i_skip]&0x01) )
        {
            const uint8_t i_flags2 = p_pkt->p_buffer[i_skip];

            /* Find PES extension 2 */
            i_skip += 1;
            if( i_flags2 & 0x80 )
                i_skip += 16;
            if( (i_flags2 & 0x40) && i_skip < p_pkt->i_buffer )
                i_skip += 1 + p_pkt->p_buffer[i_skip];
            if( i_flags2 & 0x20 )
                i_skip += 2;
            if( i_flags2 & 0x10 )
                i_skip += 2;

            if( i_skip + 1 < p_pkt->i_buffer )
            {
                const int i_extension_field_length = p_pkt->p_buffer[i_skip]&0x7f;
                if( i_extension_field_length >=1 )
                {
                    int i_stream_id_extension_flag = (p_pkt->p_buffer[i_skip+1] >> 7)&0x1;
                    if( i_stream_id_extension_flag == 0 )
                        return 0xfd00 | (p_pkt->p_buffer[i_skip+1]&0x7f);
                }
            }
        }
    }
    return p_pkt->p_buffer[3];
}

/* return the size of the next packet
 * You need to give him at least 14 bytes (and it need to start as a
 * valid packet) It does not handle less than 6 bytes */
static inline int ps_pkt_size( const uint8_t *p, int i_peek )
{
    assert( i_peek >= 6 );
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
static inline int ps_pkt_parse_system( block_t *p_pkt, ps_psm_t *p_psm,
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

        if( i_id >= 0xc0 )
        {
            int i_tk = PS_ID_TO_TK( i_id );

            if( !tk[i_tk].b_seen )
            {
                if( !ps_track_fill( &tk[i_tk], p_psm, i_id ) )
                {
                    tk[i_tk].b_seen = true;
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
    unsigned int i_skip  = 0;

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

/* Program stream map handling */
typedef struct ps_es_t
{
    int i_type;
    int i_id;

    int i_descriptor;
    uint8_t *p_descriptor;

    /* Language is iso639-2T */
    uint8_t lang[3];

} ps_es_t;

struct ps_psm_t
{
    int i_version;

    int     i_es;
    ps_es_t **es;
};

static inline int ps_id_to_type( const ps_psm_t *p_psm, int i_id )
{
    int i;
    for( i = 0; p_psm && i < p_psm->i_es; i++ )
    {
        if( p_psm->es[i]->i_id == i_id ) return p_psm->es[i]->i_type;
    }
    return 0;
}

static inline const uint8_t *ps_id_to_lang( const ps_psm_t *p_psm, int i_id )
{
    int i;
    for( i = 0; p_psm && i < p_psm->i_es; i++ )
    {
        if( p_psm->es[i]->i_id == i_id ) return p_psm->es[i]->lang;
    }
    return 0;
}

static inline void ps_psm_init( ps_psm_t *p_psm )
{
    p_psm->i_version = 0xFFFF;
    p_psm->i_es = 0;
    p_psm->es = 0;
}

static inline void ps_psm_destroy( ps_psm_t *p_psm )
{
    while( p_psm->i_es-- )
    {
        free( p_psm->es[p_psm->i_es]->p_descriptor );
        free( p_psm->es[p_psm->i_es] );
    }
    free( p_psm->es );

    p_psm->es = 0;
    p_psm->i_es = 0;
}

static inline int ps_psm_fill( ps_psm_t *p_psm, block_t *p_pkt,
                               ps_track_t tk[PS_TK_COUNT], es_out_t *out )
{
    int i_buffer = p_pkt->i_buffer;
    uint8_t *p_buffer = p_pkt->p_buffer;
    int i_length, i_version, i_info_length, i_esm_length, i_es_base, i;

    if( !p_psm || p_buffer[3] != 0xbc ) return VLC_EGENERIC;

    i_length = (uint16_t)(p_buffer[4] << 8) + p_buffer[5] + 6;
    if( i_length > i_buffer ) return VLC_EGENERIC;

    //i_current_next_indicator = (p_buffer[6] && 0x01);
    i_version = (p_buffer[6] && 0xf8);

    if( p_psm->i_version == i_version ) return VLC_EGENERIC;

    ps_psm_destroy( p_psm );

    i_info_length = (uint16_t)(p_buffer[8] << 8) + p_buffer[9];
    if( i_info_length + 10 > i_length ) return VLC_EGENERIC;

    /* Elementary stream map */
    i_esm_length = (uint16_t)(p_buffer[ 10 + i_info_length ] << 8) +
        p_buffer[ 11 + i_info_length];
    i_es_base = 12 + i_info_length;

    while( i_es_base + 4 < i_length )
    {
        ps_es_t **tmp_es;
        ps_es_t es;
        es.lang[0] = es.lang[1] = es.lang[2] = 0;

        es.i_type = p_buffer[ i_es_base  ];
        es.i_id = p_buffer[ i_es_base + 1 ];
        i_info_length = (uint16_t)(p_buffer[ i_es_base + 2 ] << 8) +
            p_buffer[ i_es_base + 3 ];

        if( i_es_base + 4 + i_info_length > i_length ) break;

        /* TODO Add support for VC-1 stream:
         *      stream_type=0xea, stream_id=0xfd AND registration
         *      descriptor 0x5 with format_identifier == 0x56432D31 (VC-1)
         *      (I need a sample that use PSM with VC-1) */

        es.p_descriptor = 0;
        es.i_descriptor = i_info_length;
        if( i_info_length > 0 )
        {
            int i = 0;

            es.p_descriptor = malloc( i_info_length );
            if( es.p_descriptor )
            {
                memcpy( es.p_descriptor, p_buffer + i_es_base + 4, i_info_length);

                while( i <= es.i_descriptor - 2 )
                {
                    /* Look for the ISO639 language descriptor */
                    if( es.p_descriptor[i] != 0x0a )
                    {
                        i += es.p_descriptor[i+1] + 2;
                        continue;
                    }

                    if( i <= es.i_descriptor - 6 )
                    {
                        es.lang[0] = es.p_descriptor[i+2];
                        es.lang[1] = es.p_descriptor[i+3];
                        es.lang[2] = es.p_descriptor[i+4];
                    }
                    break;
                }
            }
        }

        tmp_es = realloc( p_psm->es, sizeof(ps_es_t *) * (p_psm->i_es+1) );
        if( tmp_es )
        {
            p_psm->es = tmp_es;
            p_psm->es[p_psm->i_es] = malloc( sizeof(ps_es_t) );
            if( p_psm->es[p_psm->i_es] )
            {
                *p_psm->es[p_psm->i_es++] = es;
                i_es_base += 4 + i_info_length;
            }
        }
    }

    /* TODO: CRC */

    p_psm->i_version = i_version;

    /* Check/Modify our existing tracks */
    for( i = 0; i < PS_TK_COUNT; i++ )
    {
        ps_track_t tk_tmp;

        if( !tk[i].b_seen || !tk[i].es ) continue;

        if( ps_track_fill( &tk_tmp, p_psm, tk[i].i_id ) != VLC_SUCCESS )
            continue;

        if( tk_tmp.fmt.i_codec == tk[i].fmt.i_codec ) continue;

        es_out_Del( out, tk[i].es );
        tk[i] = tk_tmp;
        tk[i].b_seen = true;
        tk[i].es = es_out_Add( out, &tk[i].fmt );
    }

    return VLC_SUCCESS;
}
