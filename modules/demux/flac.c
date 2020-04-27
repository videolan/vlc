/*****************************************************************************
 * flac.c : FLAC demux module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2008 VLC authors and VideoLAN
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
#include <limits.h>
#include "xiph_metadata.h"            /* vorbis comments */
#include "../packetizer/flac.h"

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

static int  ParseHeaders( demux_t *, es_format_t * );

typedef struct
{
    vlc_tick_t  i_time_offset;
    uint64_t i_byte_offset;
} flac_seekpoint_t;

typedef struct
{
    bool  b_start;
    int   i_next_block_flags;
    es_out_id_t *p_es;
    block_t *p_current_block;

    /* Packetizer */
    decoder_t *p_packetizer;

    vlc_meta_t *p_meta;

    vlc_tick_t i_pts;
    struct flac_stream_info stream_info;
    bool b_stream_info;

    vlc_tick_t i_length; /* Length from stream info */
    uint64_t i_data_pos;

    /* */
    int         i_seekpoint;
    flac_seekpoint_t **seekpoint;

    /* title/chapters seekpoints */
    int           i_title_seekpoints;
    seekpoint_t **pp_title_seekpoints;

    /* */
    int                i_attachments;
    input_attachment_t **attachments;
    int                i_cover_idx;
    int                i_cover_score;
} demux_sys_t;

#define FLAC_PACKET_SIZE 16384
#define FLAC_MAX_PREROLL      VLC_TICK_FROM_SEC(4)
#define FLAC_MAX_SLOW_PREROLL VLC_TICK_FROM_SEC(45)

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    const uint8_t *p_peek;
    es_format_t fmt;

    /* Have a peep at the show. */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 4 ) < 4 ) return VLC_EGENERIC;

    if( p_peek[0]!='f' || p_peek[1]!='L' || p_peek[2]!='a' || p_peek[3]!='C' )
    {
        if( !p_demux->obj.force
         && !demux_IsContentType( p_demux, "audio/flac" ) )
            return VLC_EGENERIC;

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
    p_sys->i_next_block_flags = 0;
    p_sys->p_packetizer = NULL;
    p_sys->p_meta = NULL;
    p_sys->i_length = 0;
    p_sys->i_pts = VLC_TICK_INVALID;
    p_sys->b_stream_info = false;
    p_sys->p_es = NULL;
    p_sys->p_current_block = NULL;
    TAB_INIT( p_sys->i_seekpoint, p_sys->seekpoint );
    TAB_INIT( p_sys->i_attachments, p_sys->attachments);
    TAB_INIT( p_sys->i_title_seekpoints, p_sys->pp_title_seekpoints );
    p_sys->i_cover_idx = 0;
    p_sys->i_cover_score = 0;

    es_format_Init( &fmt, AUDIO_ES, VLC_CODEC_FLAC );

    /* We need to read and store the STREAMINFO metadata into fmt extra */
    if( ParseHeaders( p_demux, &fmt ) )
        goto error;

    /* Load the FLAC packetizer */
    p_sys->p_packetizer = demux_PacketizerNew( p_demux, &fmt, "flac" );
    if( !p_sys->p_packetizer )
        goto error;

    if( p_sys->i_cover_idx < p_sys->i_attachments )
    {
        char psz_url[128];
        if( !p_sys->p_meta )
            p_sys->p_meta = vlc_meta_New();
        snprintf( psz_url, sizeof(psz_url), "attachment://%s",
                  p_sys->attachments[p_sys->i_cover_idx]->psz_name );
        vlc_meta_Set( p_sys->p_meta, vlc_meta_ArtworkURL, psz_url );
    }

    p_sys->p_packetizer->fmt_in.i_id = 0;
    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->p_packetizer->fmt_in );
    if( !p_sys->p_es )
        goto error;

    return VLC_SUCCESS;
error:
    Close( p_this );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->p_current_block )
        block_Release( p_sys->p_current_block );

    for( int i = 0; i < p_sys->i_seekpoint; i++ )
        free(p_sys->seekpoint[i]);
    TAB_CLEAN( p_sys->i_seekpoint, p_sys->seekpoint );

    for( int i = 0; i < p_sys->i_attachments; i++ )
        vlc_input_attachment_Delete( p_sys->attachments[i] );
    TAB_CLEAN( p_sys->i_attachments, p_sys->attachments);

    for( int i = 0; i < p_sys->i_title_seekpoints; i++ )
        vlc_seekpoint_Delete( p_sys->pp_title_seekpoints[i] );
    TAB_CLEAN( p_sys->i_title_seekpoints, p_sys->pp_title_seekpoints );

    /* Delete the decoder */
    if( p_sys->p_packetizer )
        demux_PacketizerDestroy( p_sys->p_packetizer );

    if( p_sys->p_meta )
        vlc_meta_Delete( p_sys->p_meta );
    free( p_sys );
}

static block_t *GetPacketizedBlock( decoder_t *p_packetizer,
                                    const struct flac_stream_info *streaminfo,
                                    block_t **pp_current_block )
{
    block_t *p_block = p_packetizer->pf_packetize( p_packetizer, pp_current_block );
    if( p_block )
    {
        if( p_block->i_buffer >= FLAC_HEADER_SIZE_MIN && p_block->i_buffer < INT_MAX )
        {
            struct flac_header_info headerinfo = { .i_pts = VLC_TICK_INVALID };
            int i_ret = FLAC_ParseSyncInfo( p_block->p_buffer, p_block->i_buffer,
                                            streaminfo, NULL, &headerinfo );
            assert( i_ret != 0 ); /* Same as packetizer */
            /* Use Frame PTS, not the interpolated one */
            p_block->i_dts = p_block->i_pts = headerinfo.i_pts;
        }
    }
    return p_block;
}

static void FlushPacketizer( decoder_t *p_packetizer )
{
    if( p_packetizer->pf_flush )
        p_packetizer->pf_flush( p_packetizer );
    else
    {
        block_t *p_block_out;
        while( (p_block_out = p_packetizer->pf_packetize( p_packetizer, NULL )) )
            block_Release( p_block_out );
    }
}

static void Reset( demux_sys_t *p_sys )
{
    p_sys->i_pts = VLC_TICK_INVALID;

    FlushPacketizer( p_sys->p_packetizer );
    if( p_sys->p_current_block )
    {
        block_Release( p_sys->p_current_block );
        p_sys->p_current_block = NULL;
    }
}

static int RefineSeek( demux_t *p_demux, vlc_tick_t i_time, double i_bytemicrorate,
                       uint64_t i_lowpos, uint64_t i_highpos )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool b_found = false;
    block_t *p_block_out;
    block_t *p_block_in;

    unsigned i_frame_size = FLAC_FRAME_SIZE_MIN;

    bool b_canfastseek = false;
    vlc_stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &b_canfastseek );

    uint64_t i_start_pos = vlc_stream_Tell( p_demux->s );

    while( !b_found )
    {
        FlushPacketizer( p_sys->p_packetizer );

        p_block_out = NULL;
        p_block_in = NULL;

        while( !p_block_out )
        {
            if( !p_block_in )
            {
                if( !(p_block_in = vlc_stream_Block( p_demux->s, i_frame_size )) )
                    break;
            }

            p_block_out = GetPacketizedBlock( p_sys->p_packetizer,
                                              p_sys->b_stream_info ? &p_sys->stream_info : NULL,
                                             &p_block_in );
        }

        if( !p_block_out )
        {
            if( p_block_in )
                block_Release( p_block_in );
            break;
        }

        if( p_block_out->i_buffer > i_frame_size )
            i_frame_size = p_block_out->i_buffer;

        /* If we are further than wanted block */
        if( p_block_out->i_dts >= i_time )
        {
            vlc_tick_t i_diff = p_block_out->i_dts - i_time;
            /* Not in acceptable approximation range */
            if( i_diff > VLC_TICK_FROM_MS(100) && i_diff / i_bytemicrorate > i_frame_size )
            {
                i_highpos = i_start_pos;
                i_start_pos -= ( i_diff / i_bytemicrorate );
                i_start_pos = __MAX(i_start_pos, i_lowpos + i_frame_size);
            }
            else b_found = true;
        }
        else
        {
            vlc_tick_t i_diff = i_time - p_block_out->i_dts;
            /* Not in acceptable NEXT_TIME demux range */
            if( i_diff >= ((b_canfastseek) ? FLAC_MAX_PREROLL : FLAC_MAX_SLOW_PREROLL) &&
                i_diff / i_bytemicrorate > i_frame_size )
            {
                i_lowpos = i_start_pos;
                i_start_pos += ( i_diff / i_bytemicrorate );
                i_start_pos = __MIN(i_start_pos, i_highpos - i_frame_size);
            }
            else b_found = true;
        }

        if( p_block_out )
            block_Release( p_block_out );
        if( p_block_in )
            block_Release( p_block_in );

        if( !b_found )
        {
            if( i_highpos < i_lowpos || i_highpos - i_lowpos < i_frame_size )
                break;

            if( VLC_SUCCESS != vlc_stream_Seek( p_demux->s, i_start_pos ) )
                break;
        }
    }

    return b_found ? VLC_SUCCESS : VLC_EGENERIC;
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block_out;

    bool b_eof = false;
    if( p_sys->p_current_block == NULL )
    {
        p_sys->p_current_block = vlc_stream_Block( p_demux->s, FLAC_PACKET_SIZE );
        b_eof = (p_sys->p_current_block == NULL);
    }

    if ( p_sys->p_current_block )
    {
        p_sys->p_current_block->i_flags = p_sys->i_next_block_flags;
        p_sys->i_next_block_flags = 0;
        p_sys->p_current_block->i_pts =
        p_sys->p_current_block->i_dts = p_sys->b_start ? VLC_TICK_0 : VLC_TICK_INVALID;
    }

    while( (p_block_out = GetPacketizedBlock( p_sys->p_packetizer,
                            p_sys->b_stream_info ? &p_sys->stream_info : NULL,
                            p_sys->p_current_block ? &p_sys->p_current_block : NULL ) ) )
    {
        /* Only clear on output when packet is accepted as sync #17111 */
        p_sys->b_start = false;
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            p_block_out->p_next = NULL;

            /* set PCR */
            if( unlikely(p_sys->i_pts == VLC_TICK_INVALID) )
                es_out_SetPCR( p_demux->out, __MAX(p_block_out->i_dts - 1, VLC_TICK_0) );

            if(p_block_out->i_dts != VLC_TICK_INVALID)
                p_sys->i_pts = p_block_out->i_dts;

            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

            es_out_SetPCR( p_demux->out, p_sys->i_pts );

            p_block_out = p_next;
        }
        break;
    }

    return b_eof ? VLC_DEMUXER_EOF : VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static vlc_tick_t ControlGetLength( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    const uint64_t i_size = stream_Size(p_demux->s) - p_sys->i_data_pos;
    vlc_tick_t i_length = p_sys->i_length;
    int i;

    /* Try to fix length using seekpoint and current size for truncated file */
    for( i = p_sys->i_seekpoint-1; i >= 0; i-- )
    {
        flac_seekpoint_t *s = p_sys->seekpoint[i];
        if( s->i_byte_offset <= i_size )
        {
            if( i+1 < p_sys->i_seekpoint )
            {
                /* Broken file */
                flac_seekpoint_t *n = p_sys->seekpoint[i+1];
                assert( n->i_byte_offset != s->i_byte_offset); /* Should be ensured by ParseSeekTable */
                i_length = s->i_time_offset + (n->i_time_offset-s->i_time_offset) * (i_size-s->i_byte_offset) / (n->i_byte_offset-s->i_byte_offset);
            }
            break;
        }
    }
    return i_length;
}

static vlc_tick_t ControlGetTime( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    return p_sys->i_pts;
}

static int ControlSetTime( demux_t *p_demux, vlc_tick_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    bool b_seekable;
    int i;

    /* */
    vlc_stream_Control( p_demux->s, STREAM_CAN_SEEK, &b_seekable );
    if( !b_seekable )
        return VLC_EGENERIC;

    const vlc_tick_t i_length = ControlGetLength( p_demux );
    if( i_length <= 0 )
        return VLC_EGENERIC;

    const uint64_t i_stream_size = stream_Size( p_demux->s );
    if( i_stream_size <= p_sys->i_data_pos )
        return VLC_EGENERIC;

    const double i_bytemicrorate = (double) i_length / (i_stream_size - p_sys->i_data_pos);
    if( i_bytemicrorate == 0 )
        return VLC_EGENERIC;

    uint64_t i_lower = p_sys->i_data_pos;
    uint64_t i_upper = i_stream_size;
    uint64_t i_start_pos;

    assert( p_sys->i_seekpoint > 0 );   /* ReadMeta ensure at least (0,0) */
    if( p_sys->i_seekpoint > 1 )
    {
        /* lookup base offset */
        for( i = p_sys->i_seekpoint-1; i >= 0; i-- )
        {
            if( p_sys->seekpoint[i]->i_time_offset <= i_time )
                break;
        }

        i_lower = p_sys->seekpoint[0]->i_byte_offset + p_sys->i_data_pos;
        if( i+1 < p_sys->i_seekpoint )
            i_upper = p_sys->seekpoint[i+1]->i_byte_offset + p_sys->i_data_pos;

        i_start_pos = i_lower;
    }
    else
    {
        i_start_pos = i_time / i_bytemicrorate;
    }

    if( VLC_SUCCESS != vlc_stream_Seek( p_demux->s, i_start_pos ) )
        return VLC_EGENERIC;

    int i_ret = RefineSeek( p_demux, i_time, i_bytemicrorate, i_lower, i_upper );
    if( i_ret == VLC_SUCCESS )
    {
        p_sys->i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;
        Reset( p_sys );
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_time );
    }

    return i_ret;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_query == DEMUX_GET_META )
    {
        vlc_meta_t *p_meta = va_arg( args, vlc_meta_t * );
        if( p_sys->p_meta )
            vlc_meta_Merge( p_meta, p_sys->p_meta );
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_HAS_UNSUPPORTED_META )
    {
        bool *pb_bool = va_arg( args, bool* );
        *pb_bool = true;
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_GET_LENGTH )
    {
        *va_arg( args, vlc_tick_t * ) = ControlGetLength( p_demux );
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_SET_TIME )
    {
        return ControlSetTime( p_demux, va_arg( args, vlc_tick_t ) );
    }
    else if( i_query == DEMUX_SET_POSITION )
    {
        const double f = va_arg( args, double );
        vlc_tick_t i_length = ControlGetLength( p_demux );
        int i_ret;
        if( i_length > 0 )
        {
            i_ret = ControlSetTime( p_demux, i_length * f );
            if( i_ret == VLC_SUCCESS )
                return i_ret;
        }
        /* just byte pos seek */
        i_ret = vlc_stream_Seek( p_demux->s, (int64_t) (f * stream_Size( p_demux->s )) );
        if( i_ret == VLC_SUCCESS )
        {
            p_sys->i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;
            Reset( p_sys );
        }
        return i_ret;
    }
    else if( i_query == DEMUX_GET_TIME )
    {
        *va_arg( args, vlc_tick_t * ) = ControlGetTime( p_demux );
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_GET_POSITION )
    {
        const vlc_tick_t i_length = ControlGetLength(p_demux);
        if( i_length > 0 )
        {
            vlc_tick_t current = ControlGetTime(p_demux);
            if( current <= i_length )
            {
                *(va_arg( args, double * )) = (double)current / (double)i_length;
                return VLC_SUCCESS;
            }
        }
        /* Else fallback on byte position */
    }
    else if( i_query == DEMUX_GET_ATTACHMENTS )
    {
        input_attachment_t ***ppp_attach =
            va_arg( args, input_attachment_t *** );
        int *pi_int = va_arg( args, int * );

        if( p_sys->i_attachments <= 0 )
            return VLC_EGENERIC;

        *ppp_attach = vlc_alloc( p_sys->i_attachments, sizeof(input_attachment_t*) );
        if( !*ppp_attach )
            return VLC_EGENERIC;
        *pi_int = p_sys->i_attachments;
        for( int i = 0; i < p_sys->i_attachments; i++ )
            (*ppp_attach)[i] = vlc_input_attachment_Duplicate( p_sys->attachments[i] );
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_GET_TITLE_INFO )
    {
        input_title_t ***ppp_title = va_arg( args, input_title_t *** );
        int *pi_int = va_arg( args, int * );
        int *pi_title_offset = va_arg( args, int * );
        int *pi_seekpoint_offset = va_arg( args, int * );

        if( !p_sys->i_title_seekpoints )
            return VLC_EGENERIC;

        *pi_int = 1;
        *ppp_title = malloc( sizeof(input_title_t*) );
        if(!*ppp_title)
            return VLC_EGENERIC;

        input_title_t *p_title = (*ppp_title)[0] = vlc_input_title_New();
        if(!p_title)
        {
            free(*ppp_title);
            return VLC_EGENERIC;
        }

        p_title->seekpoint = vlc_alloc( p_sys->i_title_seekpoints, sizeof(seekpoint_t*) );
        if(!p_title->seekpoint)
        {
            vlc_input_title_Delete(p_title);
            free(*ppp_title);
            return VLC_EGENERIC;
        }

        p_title->i_seekpoint = p_sys->i_title_seekpoints;
        for( int i = 0; i < p_title->i_seekpoint; i++ )
            p_title->seekpoint[i] = vlc_seekpoint_Duplicate( p_sys->pp_title_seekpoints[i] );

        *pi_title_offset = 0;
        *pi_seekpoint_offset = 0;

        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_SET_TITLE )
    {
        const int i_title = va_arg( args, int );
        if( i_title != 0 )
            return VLC_EGENERIC;
        return VLC_SUCCESS;
    }
    else if( i_query == DEMUX_SET_SEEKPOINT )
    {
        const int i_seekpoint = va_arg( args, int );
        if( !p_sys->i_title_seekpoints || i_seekpoint >= p_sys->i_title_seekpoints )
            return VLC_EGENERIC;
        return ControlSetTime( p_demux, p_sys->pp_title_seekpoints[i_seekpoint]->i_time_offset );
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

static void ParseSeekTable( demux_t *p_demux, const uint8_t *p_data, size_t i_data,
                            unsigned i_sample_rate );
static void ParseComment( demux_t *, const uint8_t *p_data, size_t i_data );
static void ParsePicture( demux_t *, const uint8_t *p_data, size_t i_data );

static int  ParseHeaders( demux_t *p_demux, es_format_t *p_fmt )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ssize_t i_peek;
    const uint8_t *p_peek;
    bool b_last;

    /* Be sure we have seekpoint 0 */
    flac_seekpoint_t *s = xmalloc( sizeof (*s) );
    s->i_time_offset = 0;
    s->i_byte_offset = 0;
    TAB_APPEND( p_sys->i_seekpoint, p_sys->seekpoint, s );

    uint8_t header[4];
    if( vlc_stream_Read( p_demux->s, header, 4) < 4)
        return VLC_EGENERIC;

    if (memcmp(header, "fLaC", 4))
        return VLC_EGENERIC;

    b_last = 0;
    while( !b_last )
    {
        int i_len;
        int i_type;

        i_peek = vlc_stream_Peek( p_demux->s, &p_peek, 4 );
        if( i_peek < 4 )
            break;
        b_last = p_peek[0]&0x80;
        i_type = p_peek[0]&0x7f;
        i_len  = Get24bBE( &p_peek[1] );

        if( i_type == META_STREAMINFO && p_fmt->p_extra == NULL )
        {
            if( i_len != FLAC_STREAMINFO_SIZE ) {
                msg_Err( p_demux, "invalid size %d for a STREAMINFO metadata block", i_len );
                return VLC_EGENERIC;
            }

            p_fmt->p_extra = malloc( FLAC_STREAMINFO_SIZE );
            if( p_fmt->p_extra == NULL )
                return VLC_EGENERIC;

            if( vlc_stream_Read( p_demux->s, NULL, 4) < 4)
            {
                FREENULL( p_fmt->p_extra );
                return VLC_EGENERIC;
            }
            if( vlc_stream_Read( p_demux->s, p_fmt->p_extra,
                                 FLAC_STREAMINFO_SIZE ) != FLAC_STREAMINFO_SIZE )
            {
                msg_Err( p_demux, "failed to read STREAMINFO metadata block" );
                FREENULL( p_fmt->p_extra );
                return VLC_EGENERIC;
            }
            p_fmt->i_extra = FLAC_STREAMINFO_SIZE;

            /* */
            p_sys->b_stream_info = true;
            FLAC_ParseStreamInfo( (uint8_t *) p_fmt->p_extra, &p_sys->stream_info );

            p_fmt->audio.i_rate = p_sys->stream_info.sample_rate;
            p_fmt->audio.i_channels = p_sys->stream_info.channels;
            p_fmt->audio.i_bitspersample = p_sys->stream_info.bits_per_sample;
            if( p_sys->stream_info.sample_rate > 0 )
                p_sys->i_length = vlc_tick_from_samples(p_sys->stream_info.total_samples,
                                  p_sys->stream_info.sample_rate);

            continue;
        }
        else if( i_type == META_SEEKTABLE )
        {
            i_peek = vlc_stream_Peek( p_demux->s, &p_peek, 4+i_len );
            if( i_peek == 4+i_len )
                ParseSeekTable( p_demux, p_peek, i_peek, p_fmt->audio.i_rate );
        }
        else if( i_type == META_COMMENT )
        {
            i_peek = vlc_stream_Peek( p_demux->s, &p_peek, 4+i_len );
            if( i_peek == 4+i_len )
                ParseComment( p_demux, p_peek, i_peek );
        }
        else if( i_type == META_PICTURE )
        {
            i_peek = vlc_stream_Peek( p_demux->s, &p_peek, 4+i_len );
            if( i_peek == 4+i_len )
                ParsePicture( p_demux, p_peek, i_peek );
        }

        if( vlc_stream_Read( p_demux->s, NULL, 4+i_len ) < 4+i_len )
            break;
    }

    /* */
    p_sys->i_data_pos = vlc_stream_Tell( p_demux->s );

    if ( p_fmt->p_extra == NULL )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static void ParseSeekTable( demux_t *p_demux, const uint8_t *p_data, size_t i_data,
                            unsigned i_sample_rate )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    flac_seekpoint_t *s;
    size_t i;

    if( i_sample_rate == 0 )
        return;

    /* */
    for( i = 0; i < (i_data-4)/18; i++ )
    {
        const int64_t i_sample = GetQWBE( &p_data[4+18*i+0] );
        int j;

        if( i_sample < 0 || i_sample >= INT64_MAX ||
            GetQWBE( &p_data[4+18*i+8] ) < FLAC_STREAMINFO_SIZE )
            break;

        s = xmalloc( sizeof (*s) );
        s->i_time_offset = vlc_tick_from_samples(i_sample, i_sample_rate);
        s->i_byte_offset = GetQWBE( &p_data[4+18*i+8] );

        /* Check for duplicate entry */
        for( j = 0; j < p_sys->i_seekpoint; j++ )
        {
            if( p_sys->seekpoint[j]->i_time_offset == s->i_time_offset ||
                p_sys->seekpoint[j]->i_byte_offset == s->i_byte_offset )
            {
                free( s );
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

static void ParseComment( demux_t *p_demux, const uint8_t *p_data, size_t i_data )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_data < 4 )
        return;

    vorbis_ParseComment( NULL, &p_sys->p_meta, &p_data[4], i_data - 4,
        &p_sys->i_attachments, &p_sys->attachments,
        &p_sys->i_cover_score, &p_sys->i_cover_idx,
        &p_sys->i_title_seekpoints, &p_sys->pp_title_seekpoints, NULL, NULL );
}

static void ParsePicture( demux_t *p_demux, const uint8_t *p_data, size_t i_data )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    i_data -= 4; p_data += 4;

    input_attachment_t *p_attachment = ParseFlacPicture( p_data, i_data,
        p_sys->i_attachments, &p_sys->i_cover_score, &p_sys->i_cover_idx );
    if( p_attachment == NULL )
        return;

    TAB_APPEND( p_sys->i_attachments, p_sys->attachments, p_attachment );
}
