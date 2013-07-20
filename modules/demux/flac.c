/*****************************************************************************
 * flac.c : FLAC demux module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
#include <vlc_demux.h>
#include <vlc_meta.h>                 /* vlc_meta_* */
#include <vlc_input.h>                /* vlc_input_attachment, vlc_seekpoint */
#include <vlc_codec.h>                /* decoder_t */
#include <vlc_charset.h>              /* EnsureUTF8 */

#include <assert.h>
#include "xiph_metadata.h"            /* vorbis comments */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("FLAC demuxer") )
    set_capability( "demux", 155 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_callbacks( Open, Close )
    add_shortcut( "flac" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

static int  ReadMeta( demux_t *, uint8_t **pp_streaminfo, int *pi_streaminfo );

struct demux_sys_t
{
    bool  b_start;
    es_out_id_t *p_es;

    /* Packetizer */
    decoder_t *p_packetizer;

    vlc_meta_t *p_meta;

    int64_t i_time_offset;
    int64_t i_pts;
    int64_t i_pts_start;

    int64_t i_length; /* Length from stream info */
    int64_t i_data_pos;

    /* */
    int         i_seekpoint;
    seekpoint_t **seekpoint;

    /* */
    int                i_attachments;
    input_attachment_t **attachments;
    int                i_cover_idx;
    int                i_cover_score;
};

#define STREAMINFO_SIZE 34
#define FLAC_PACKET_SIZE 16384

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    const uint8_t *p_peek;
    uint8_t     *p_streaminfo;
    int         i_streaminfo;
    es_format_t fmt;

    /* Have a peep at the show. */
    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 ) return VLC_EGENERIC;

    if( p_peek[0]!='f' || p_peek[1]!='L' || p_peek[2]!='a' || p_peek[3]!='C' )
    {
        if( !p_demux->b_force ) return VLC_EGENERIC;

        /* User forced */
        msg_Err( p_demux, "this doesn't look like a flac stream, "
                 "continuing anyway" );
    }

    p_sys = malloc( sizeof( demux_sys_t ) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    p_demux->pf_demux   = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys      = p_sys;
    p_sys->b_start = true;
    p_sys->p_meta = NULL;
    p_sys->i_length = 0;
    p_sys->i_time_offset = 0;
    p_sys->i_pts = 0;
    p_sys->i_pts_start = 0;
    p_sys->p_es = NULL;
    TAB_INIT( p_sys->i_seekpoint, p_sys->seekpoint );
    TAB_INIT( p_sys->i_attachments, p_sys->attachments);
    p_sys->i_cover_idx = 0;
    p_sys->i_cover_score = 0;

    /* We need to read and store the STREAMINFO metadata */
    if( ReadMeta( p_demux, &p_streaminfo, &i_streaminfo ) )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Load the FLAC packetizer */
    /* Store STREAMINFO for the decoder and packetizer */
    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_FLAC );
    fmt.i_extra = i_streaminfo;
    fmt.p_extra = p_streaminfo;

    p_sys->p_packetizer = demux_PacketizerNew( p_demux, &fmt, "flac" );
    if( !p_sys->p_packetizer )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( p_sys->i_cover_idx < p_sys->i_attachments )
    {
        char psz_url[128];
        if( !p_sys->p_meta )
            p_sys->p_meta = vlc_meta_New();
        snprintf( psz_url, sizeof(psz_url), "attachment://%s",
                  p_sys->attachments[p_sys->i_cover_idx]->psz_name );
        vlc_meta_Set( p_sys->p_meta, vlc_meta_ArtworkURL, psz_url );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    for( int i = 0; i < p_sys->i_seekpoint; i++ )
        vlc_seekpoint_Delete(p_sys->seekpoint[i]);
    TAB_CLEAN( p_sys->i_seekpoint, p_sys->seekpoint );

    for( int i = 0; i < p_sys->i_attachments; i++ )
        vlc_input_attachment_Delete( p_sys->attachments[i] );
    TAB_CLEAN( p_sys->i_attachments, p_sys->attachments);

    /* Delete the decoder */
    demux_PacketizerDestroy( p_sys->p_packetizer );

    if( p_sys->p_meta )
        vlc_meta_Delete( p_sys->p_meta );
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block_in, *p_block_out;

    if( !( p_block_in = stream_Block( p_demux->s, FLAC_PACKET_SIZE ) ) )
        return 0;

    p_block_in->i_pts = p_block_in->i_dts = p_sys->b_start ? VLC_TS_0 : VLC_TS_INVALID;
    p_sys->b_start = false;

    while( (p_block_out = p_sys->p_packetizer->pf_packetize(
                p_sys->p_packetizer, &p_block_in )) )
    {
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            p_block_out->p_next = NULL;

            if( p_sys->p_es == NULL )
            {
                p_sys->p_packetizer->fmt_out.b_packetized = true;
                p_sys->p_es = es_out_Add( p_demux->out, &p_sys->p_packetizer->fmt_out);
            }

            p_sys->i_pts = p_block_out->i_dts - VLC_TS_0;

            /* Correct timestamp */
            p_block_out->i_pts += p_sys->i_time_offset;
            p_block_out->i_dts += p_sys->i_time_offset;

            /* set PCR */
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block_out->i_dts );

            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

            p_block_out = p_next;
        }
    }
    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int64_t ControlGetLength( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const int64_t i_size = stream_Size(p_demux->s) - p_sys->i_data_pos;
    int64_t i_length = p_sys->i_length;
    int i;

    /* Try to fix length using seekpoint and current size for truncated file */
    for( i = p_sys->i_seekpoint-1; i >= 0; i-- )
    {
        seekpoint_t *s = p_sys->seekpoint[i];
        if( s->i_byte_offset <= i_size )
        {
            if( i+1 < p_sys->i_seekpoint )
            {
                /* Broken file */
                seekpoint_t *n = p_sys->seekpoint[i+1];
                assert( n->i_byte_offset != s->i_byte_offset); /* Should be ensured by ParseSeekTable */
                i_length = s->i_time_offset + (n->i_time_offset-s->i_time_offset) * (i_size-s->i_byte_offset) / (n->i_byte_offset-s->i_byte_offset);
            }
            break;
        }
    }
    return i_length;
}

static int64_t ControlGetTime( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    return __MAX(p_sys->i_pts, p_sys->i_pts_start) + p_sys->i_time_offset;
}

static int ControlSetTime( demux_t *p_demux, int64_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t i_delta_time;
    bool b_seekable;
    int i;

    /* */
    stream_Control( p_demux->s, STREAM_CAN_SEEK, &b_seekable );
    if( !b_seekable )
        return VLC_EGENERIC;

    /* */
    assert( p_sys->i_seekpoint > 0 );   /* ReadMeta ensure at least (0,0) */
    for( i = p_sys->i_seekpoint-1; i >= 0; i-- )
    {
        if( p_sys->seekpoint[i]->i_time_offset <= i_time )
            break;
    }
    i_delta_time = i_time - p_sys->seekpoint[i]->i_time_offset;

    /* XXX We do exact seek if it's not too far away(45s) */
    if( i_delta_time < 45*INT64_C(1000000) )
    {
        if( stream_Seek( p_demux->s, p_sys->seekpoint[i]->i_byte_offset+p_sys->i_data_pos ) )
            return VLC_EGENERIC;

        p_sys->i_time_offset = p_sys->seekpoint[i]->i_time_offset - p_sys->i_pts;
        p_sys->i_pts_start = p_sys->i_pts+i_delta_time;
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, p_sys->i_pts_start + p_sys->i_time_offset );
    }
    else
    {
        int64_t i_delta_offset;
        int64_t i_next_time;
        int64_t i_next_offset;

        if( i+1 < p_sys->i_seekpoint )
        {
            i_next_time   = p_sys->seekpoint[i+1]->i_time_offset;
            i_next_offset = p_sys->seekpoint[i+1]->i_byte_offset;
        }
        else
        {
            i_next_time   = p_sys->i_length;
            i_next_offset = stream_Size(p_demux->s)-p_sys->i_data_pos;
        }

        i_delta_offset = 0;
        if( i_next_time-p_sys->seekpoint[i]->i_time_offset > 0 )
            i_delta_offset = (i_next_offset - p_sys->seekpoint[i]->i_byte_offset) * i_delta_time /
                             (i_next_time-p_sys->seekpoint[i]->i_time_offset);

        if( stream_Seek( p_demux->s, p_sys->seekpoint[i]->i_byte_offset+p_sys->i_data_pos + i_delta_offset ) )
            return VLC_EGENERIC;

        p_sys->i_pts_start = p_sys->i_pts;
        p_sys->i_time_offset = (p_sys->seekpoint[i]->i_time_offset+i_delta_time) - p_sys->i_pts;
    }
    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_query == DEMUX_GET_META )
    {
        vlc_meta_t *p_meta = (vlc_meta_t *)va_arg( args, vlc_meta_t* );
        if( p_demux->p_sys->p_meta )
            vlc_meta_Merge( p_meta, p_demux->p_sys->p_meta );
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_HAS_UNSUPPORTED_META )
    {
        bool *pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = true;
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_GET_LENGTH )
    {
        int64_t *pi64 = (int64_t*)va_arg( args, int64_t * );
        *pi64 = ControlGetLength( p_demux );
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_SET_TIME )
    {
        int64_t i_time = (int64_t)va_arg( args, int64_t );
        return ControlSetTime( p_demux, i_time );
    }
    else if( i_query == DEMUX_SET_POSITION )
    {
        const double f = (double)va_arg( args, double );
        int64_t i_time = f * ControlGetLength( p_demux );
        return ControlSetTime( p_demux, i_time );
    }
    else if( i_query == DEMUX_GET_TIME )
    {
        int64_t *pi64 = (int64_t*)va_arg( args, int64_t * );
        *pi64 = ControlGetTime( p_demux );
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_GET_POSITION )
    {
        double *pf = (double*)va_arg( args, double * );
        const int64_t i_length = ControlGetLength(p_demux);
        if( i_length > 0 )
        {
            double current = ControlGetTime(p_demux);
            *pf = current / (double)i_length;
        }
        else
            *pf= 0.0;
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_GET_ATTACHMENTS )
    {
        input_attachment_t ***ppp_attach =
            (input_attachment_t***)va_arg( args, input_attachment_t*** );
        int *pi_int = (int*)va_arg( args, int * );

        if( p_sys->i_attachments <= 0 )
            return VLC_EGENERIC;

        *pi_int = p_sys->i_attachments;
        *ppp_attach = xmalloc( sizeof(input_attachment_t*) * p_sys->i_attachments );
        for( int i = 0; i < p_sys->i_attachments; i++ )
            (*ppp_attach)[i] = vlc_input_attachment_Duplicate( p_sys->attachments[i] );
        return VLC_SUCCESS;
    }

    return demux_vaControlHelper( p_demux->s, p_sys->i_data_pos, -1,
                                   8*0, 1, i_query, args );
}

enum
{
    META_STREAMINFO = 0,
    META_SEEKTABLE = 3,
    META_COMMENT = 4,
    META_PICTURE = 6,
};

static inline int Get24bBE( const uint8_t *p )
{
    return (p[0] << 16)|(p[1] << 8)|(p[2]);
}

static void ParseStreamInfo( int *pi_rate, int64_t *pi_count, uint8_t *p_data );
static void ParseSeekTable( demux_t *p_demux, const uint8_t *p_data, int i_data,
                            int i_sample_rate );
static void ParseComment( demux_t *, const uint8_t *p_data, int i_data );
static void ParsePicture( demux_t *, const uint8_t *p_data, int i_data );

static int  ReadMeta( demux_t *p_demux, uint8_t **pp_streaminfo, int *pi_streaminfo )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int     i_peek;
    const uint8_t *p_peek;
    bool b_last;
    int i_sample_rate = 0;
    int64_t i_sample_count;

    *pp_streaminfo = NULL;

    /* Be sure we have seekpoint 0 */
    seekpoint_t *s = vlc_seekpoint_New();
    s->i_time_offset = 0;
    s->i_byte_offset = 0;
    TAB_APPEND( p_sys->i_seekpoint, p_sys->seekpoint, s );

    uint8_t header[4];
    if( stream_Read( p_demux->s, header, 4) < 4)
        return VLC_EGENERIC;

    if (memcmp(header, "fLaC", 4))
        return VLC_EGENERIC;

    b_last = 0;
    while( !b_last )
    {
        int i_len;
        int i_type;

        i_peek = stream_Peek( p_demux->s, &p_peek, 4 );
        if( i_peek < 4 )
            break;
        b_last = p_peek[0]&0x80;
        i_type = p_peek[0]&0x7f;
        i_len  = Get24bBE( &p_peek[1] );

        if( i_type == META_STREAMINFO && !*pp_streaminfo )
        {
            if( i_len != STREAMINFO_SIZE ) {
                msg_Err( p_demux, "invalid size %d for a STREAMINFO metadata block", i_len );
                return VLC_EGENERIC;
            }

            *pi_streaminfo = STREAMINFO_SIZE;
            *pp_streaminfo = malloc( STREAMINFO_SIZE);
            if( *pp_streaminfo == NULL )
                return VLC_EGENERIC;

            if( stream_Read( p_demux->s, NULL, 4) < 4)
                return VLC_EGENERIC;
            if( stream_Read( p_demux->s, *pp_streaminfo, STREAMINFO_SIZE ) != STREAMINFO_SIZE )
            {
                msg_Err( p_demux, "failed to read STREAMINFO metadata block" );
                free( *pp_streaminfo );
                return VLC_EGENERIC;
            }

            /* */
            ParseStreamInfo( &i_sample_rate, &i_sample_count, *pp_streaminfo );
            if( i_sample_rate > 0 )
                p_sys->i_length = i_sample_count * INT64_C(1000000)/i_sample_rate;
            continue;
        }
        else if( i_type == META_SEEKTABLE )
        {
            i_peek = stream_Peek( p_demux->s, &p_peek, 4+i_len );
            if( i_peek == 4+i_len )
                ParseSeekTable( p_demux, p_peek, i_peek, i_sample_rate );
        }
        else if( i_type == META_COMMENT )
        {
            i_peek = stream_Peek( p_demux->s, &p_peek, 4+i_len );
            if( i_peek == 4+i_len )
                ParseComment( p_demux, p_peek, i_peek );
        }
        else if( i_type == META_PICTURE )
        {
            i_peek = stream_Peek( p_demux->s, &p_peek, 4+i_len );
            if( i_peek == 4+i_len )
                ParsePicture( p_demux, p_peek, i_peek );
        }

        if( stream_Read( p_demux->s, NULL, 4+i_len ) < 4+i_len )
            break;
    }

    /* */
    p_sys->i_data_pos = stream_Tell( p_demux->s );

    if (!*pp_streaminfo)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}
static void ParseStreamInfo( int *pi_rate, int64_t *pi_count, uint8_t *p_data )
{
    *pi_rate = GetDWBE(&p_data[4+6]) >> 12;
    *pi_count = GetQWBE(&p_data[4+6]) &  ((INT64_C(1)<<36)-1);
}

static void ParseSeekTable( demux_t *p_demux, const uint8_t *p_data, int i_data,
                            int i_sample_rate )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    seekpoint_t *s;
    int i;

    if( i_sample_rate <= 0 )
        return;

    /* */
    for( i = 0; i < (i_data-4)/18; i++ )
    {
        const int64_t i_sample = GetQWBE( &p_data[4+18*i+0] );
        int j;

        if( i_sample < 0 || i_sample >= INT64_MAX )
            continue;

        s = vlc_seekpoint_New();
        s->i_time_offset = i_sample * INT64_C(1000000)/i_sample_rate;
        s->i_byte_offset = GetQWBE( &p_data[4+18*i+8] );

        /* Check for duplicate entry */
        for( j = 0; j < p_sys->i_seekpoint; j++ )
        {
            if( p_sys->seekpoint[j]->i_time_offset == s->i_time_offset ||
                p_sys->seekpoint[j]->i_byte_offset == s->i_byte_offset )
            {
                vlc_seekpoint_Delete( s );
                s = NULL;
                break;
            }
        }
        if( s )
        {
            TAB_APPEND( p_sys->i_seekpoint, p_sys->seekpoint, s );
        }
    }
    /* TODO sort it by size and remove wrong seek entry (time not increasing) */
}

static void ParseComment( demux_t *p_demux, const uint8_t *p_data, int i_data )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_data < 4 )
        return;

    vorbis_ParseComment( &p_sys->p_meta, &p_data[4], i_data - 4,
        &p_sys->i_attachments, &p_sys->attachments,
        &p_sys->i_cover_score, &p_sys->i_cover_idx, NULL, NULL );
}

static void ParsePicture( demux_t *p_demux, const uint8_t *p_data, int i_data )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    i_data -= 4; p_data += 4;

    input_attachment_t *p_attachment = ParseFlacPicture( p_data, i_data,
        p_sys->i_attachments, &p_sys->i_cover_score, &p_sys->i_cover_idx );
    if( p_attachment == NULL )
        return;

    TAB_APPEND( p_sys->i_attachments, p_sys->attachments, p_attachment );
}

