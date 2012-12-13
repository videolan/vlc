/*****************************************************************************
 * real.c: Real demuxer.
 *****************************************************************************
 * Copyright (C) 2004, 2006-2007 the VideoLAN team
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

/**
 * Status of this demuxer:
 * Real Media format
 * -----------------
 *
 * version v3 w/ 14_4/lpcJ is ok.
 * version v4/5: - atrac3 is ok.
 *               - cook is ok.
 *               - raac, racp are ok.
 *               - dnet is twisted "The byte order of the data is reversed
 *                                  from standard AC3" but ok
 *               - 28_8 is ok.
 *               - sipr is ok.
 *               - ralf is unsupported, but hardly any sample exist.
 *               - mp3 is unsupported, one sample exists...
 *
 * Real Audio Only
 * ---------------
 * v3 and v4/5 headers are parsed.
 * Doesn't work yet...
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_demux.h>
#include <vlc_charset.h>
#include <vlc_meta.h>

#include <assert.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Real demuxer" ) )
    set_capability( "demux", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_callbacks( Open, Close )
    add_shortcut( "real", "rm" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    int         i_id;
    es_format_t fmt;

    es_out_id_t *p_es;

    unsigned    i_frame_size;

    int         i_frame_num;
    unsigned    i_frame_pos;
    int         i_frame_slice;
    int         i_frame_slice_count;
    block_t     *p_frame;

    int         i_subpacket_h;
    int         i_subpacket_size;
    int         i_coded_frame_size;

    int         i_subpacket;
    int         i_subpackets;
    block_t     **p_subpackets;
    mtime_t     *p_subpackets_timecode;
    int         i_out_subpacket;

    block_t     *p_sipr_packet;
    int         i_sipr_subpacket_count;
    mtime_t     i_last_dts;
} real_track_t;

typedef struct
{
    uint32_t i_file_offset;
    uint32_t i_time_offset;
    uint32_t i_frame_index;
} real_index_t;

struct demux_sys_t
{
    int64_t  i_data_offset;
    int64_t  i_data_size;
    uint32_t i_data_packets_count;
    uint32_t i_data_packets;
    int64_t  i_data_offset_next;

    bool     b_real_audio;

    int64_t i_our_duration;

    char* psz_title;
    char* psz_artist;
    char* psz_copyright;
    char* psz_description;

    int          i_track;
    real_track_t **track;

    size_t     i_buffer;
    uint8_t buffer[65536];

    int64_t     i_pcr;

    int64_t     i_index_offset;
    bool        b_seek;
    real_index_t *p_index;
};

static const unsigned char i_subpacket_size_sipr[4] = { 29, 19, 37, 20 };

static int Demux( demux_t * );
static int Control( demux_t *, int i_query, va_list args );


static void DemuxVideo( demux_t *, real_track_t *tk, mtime_t i_dts, unsigned i_flags );
static void DemuxAudio( demux_t *, real_track_t *tk, mtime_t i_pts, unsigned i_flags );

static int ControlSeekByte( demux_t *, int64_t i_bytes );
static int ControlSeekTime( demux_t *, mtime_t i_time );

static int HeaderRead( demux_t *p_demux );
static int CodecParse( demux_t *p_demux, int i_len, int i_num );

static void     RVoid( const uint8_t **pp_data, int *pi_data, int i_size );
static int      RLength( const uint8_t **pp_data, int *pi_data );
static uint8_t  R8( const uint8_t **pp_data, int *pi_data );
static uint16_t R16( const uint8_t **pp_data, int *pi_data );
static uint32_t R32( const uint8_t **pp_data, int *pi_data );
static void     SiprPacketReorder(uint8_t *buf, int sub_packet_h, int framesize);

/*****************************************************************************
 * Open
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;

    const uint8_t *p_peek;
    bool           b_real_audio = false;

    if( stream_Peek( p_demux->s, &p_peek, 10 ) < 10 )
        return VLC_EGENERIC;

    /* Real Audio */
    if( !memcmp( p_peek, ".ra", 3 ) )
    {
        msg_Err( p_demux, ".ra files unsuported" );
        b_real_audio = true;
    }
    /* Real Media Format */
    else if( memcmp( p_peek, ".RMF", 4 ) )
    {
        return VLC_EGENERIC;
    }

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    p_demux->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    p_sys->i_data_offset = 0;
    p_sys->i_track = 0;
    p_sys->track   = NULL;
    p_sys->i_pcr   = VLC_TS_INVALID;

    p_sys->b_seek  = false;
    p_sys->b_real_audio = b_real_audio;

    /* Parse the headers */
    /* Real Audio files */
    if( b_real_audio )
    {
        CodecParse( p_demux, 32, 0 ); /* At least 32 */
        return VLC_EGENERIC;                     /* We don't know how to read
                                                    correctly the data yet */
    }
    /* RMF files */
    else if( HeaderRead( p_demux ) )
    {
        msg_Err( p_demux, "invalid header" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    for( int i = 0; i < p_sys->i_track; i++ )
    {
        real_track_t *tk = p_sys->track[i];

        es_format_Clean( &tk->fmt );

        if( tk->p_frame )
            block_Release( tk->p_frame );

        for( int j = 0; j < tk->i_subpackets; j++ )
        {
            if( tk->p_subpackets[ j ] )
                block_Release( tk->p_subpackets[ j ] );
        }
        free( tk->p_subpackets );
        free( tk->p_subpackets_timecode );
        if( tk->p_sipr_packet )
            block_Release( tk->p_sipr_packet );
        free( tk );
    }
    if( p_sys->i_track > 0 )
        free( p_sys->track );

    free( p_sys->psz_title );
    free( p_sys->psz_artist );
    free( p_sys->psz_copyright );
    free( p_sys->psz_description );
    free( p_sys->p_index );

    free( p_sys );
}


/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t     header[18];

    if( p_sys->i_data_packets >= p_sys->i_data_packets_count &&
        p_sys->i_data_packets_count )
    {
        if( stream_Read( p_demux->s, header, 18 ) < 18 )
            return 0;

        if( memcmp( header, "DATA", 4 ) )
            return 0;

        p_sys->i_data_offset = stream_Tell( p_demux->s ) - 18;
        p_sys->i_data_size   = GetDWBE( &header[4] );
        p_sys->i_data_packets_count = GetDWBE( &header[10] );
        p_sys->i_data_packets = 0;
        p_sys->i_data_offset_next = GetDWBE( &header[14] );

        msg_Dbg( p_demux, "entering new DATA packets=%d next=%u",
                 p_sys->i_data_packets_count,
                 (unsigned int)p_sys->i_data_offset_next );
    }

    /* Read Packet Header */
    if( stream_Read( p_demux->s, header, 12 ) < 12 )
        return 0;
    //const int i_version = GetWBE( &header[0] );
    const size_t  i_size = GetWBE( &header[2] ) - 12;
    const int     i_id   = GetWBE( &header[4] );
    const int64_t i_pts  = VLC_TS_0 + 1000 * GetDWBE( &header[6] );
    const int     i_flags= header[11]; /* flags 0x02 -> keyframe */

    p_sys->i_data_packets++;
    if( i_size > sizeof(p_sys->buffer) )
    {
        msg_Err( p_demux, "Got a NUKK size to read. (Invalid format?)" );
        return 1;
    }

    p_sys->i_buffer = stream_Read( p_demux->s, p_sys->buffer, i_size );
    if( p_sys->i_buffer < i_size )
        return 0;

    real_track_t *tk = NULL;
    for( int i = 0; i < p_sys->i_track; i++ )
    {
        if( p_sys->track[i]->i_id == i_id )
            tk = p_sys->track[i];
    }

    if( !tk )
    {
        msg_Warn( p_demux, "unknown track id(0x%x)", i_id );
        return 1;
    }

    if( tk->fmt.i_cat == VIDEO_ES )
    {
        DemuxVideo( p_demux, tk, i_pts, i_flags );
    }
    else
    {
        assert( tk->fmt.i_cat == AUDIO_ES );
        DemuxAudio( p_demux, tk, i_pts, i_flags );
    }

    /* Update PCR */
    mtime_t i_pcr = VLC_TS_INVALID;
    for( int i = 0; i < p_sys->i_track; i++ )
    {
        real_track_t *tk = p_sys->track[i];

        if( i_pcr <= VLC_TS_INVALID || ( tk->i_last_dts > VLC_TS_INVALID && tk->i_last_dts < i_pcr ) )
            i_pcr = tk->i_last_dts;
    }
    if( i_pcr > VLC_TS_INVALID && i_pcr != p_sys->i_pcr )
    {
        p_sys->i_pcr = i_pcr;
        es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_pcr );
    }
    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    int64_t i64;
    int64_t *pi64;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*) va_arg( args, double* );

            /* read stream size maybe failed in rtsp streaming, 
               so use duration to determin the position at first  */
            if( p_sys->i_our_duration > 0 )
            {
                if( p_sys->i_pcr > VLC_TS_INVALID )
                    *pf = (double)p_sys->i_pcr / 1000.0 / p_sys->i_our_duration;
                else
                    *pf = 0.0;
                return VLC_SUCCESS;
            }

            i64 = stream_Size( p_demux->s );
            if( i64 > 0 )
                *pf = (double)1.0*stream_Tell( p_demux->s ) / (double)i64;
            else
                *pf = 0.0;
            return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );

            if( p_sys->i_our_duration > 0 )
            {
                *pi64 = p_sys->i_pcr > VLC_TS_INVALID ? p_sys->i_pcr : 0;
                return VLC_SUCCESS;
            }

            /* same as GET_POSTION */
            i64 = stream_Size( p_demux->s );
            if( p_sys->i_our_duration > 0 && i64 > 0 )
            {
                *pi64 = (int64_t)( 1000.0 * p_sys->i_our_duration * stream_Tell( p_demux->s ) / i64 );
                return VLC_SUCCESS;
            }

            *pi64 = 0;
            return VLC_EGENERIC;

        case DEMUX_SET_POSITION:
            f = (double) va_arg( args, double );
            i64 = (int64_t) ( stream_Size( p_demux->s ) * f );

            if( !p_sys->p_index && i64 != 0 )
            {
                /* TODO seek */
                msg_Err( p_demux,"Seek No Index Real File failed!" );
                return VLC_EGENERIC; // no index!
            }
            else if( i64 == 0 )
            {
                /* it is a rtsp stream , it is specials in access/rtsp/... */
                msg_Dbg(p_demux, "Seek in real rtsp stream!");
                p_sys->i_pcr = VLC_TS_0 + INT64_C(1000) * ( p_sys->i_our_duration * f  );
                p_sys->b_seek = true;
                return stream_Seek( p_demux->s, p_sys->i_pcr - VLC_TS_0 );
            }
            return ControlSeekByte( p_demux, i64 );

        case DEMUX_SET_TIME:
            if( !p_sys->p_index )
                return VLC_EGENERIC;

            i64 = (int64_t) va_arg( args, int64_t );
            return ControlSeekTime( p_demux, i64 );

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
 
            if( p_sys->i_our_duration <= 0 )
            {
                *pi64 = 0;
                return VLC_EGENERIC;
            }

            /* our stored duration is in ms, so... */
            *pi64 = INT64_C(1000) * p_sys->i_our_duration;
            return VLC_SUCCESS;

        case DEMUX_GET_META:
        {
            vlc_meta_t *p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t* );

            /* the core will crash if we provide NULL strings, so check
             * every string first */
            if( p_sys->psz_title )
                vlc_meta_SetTitle( p_meta, p_sys->psz_title );
            if( p_sys->psz_artist )
                vlc_meta_SetArtist( p_meta, p_sys->psz_artist );
            if( p_sys->psz_copyright )
                vlc_meta_SetCopyright( p_meta, p_sys->psz_copyright );
            if( p_sys->psz_description )
                vlc_meta_SetDescription( p_meta, p_sys->psz_description );
            return VLC_SUCCESS;
        }

        case DEMUX_GET_FPS:
        default:
            return VLC_EGENERIC;
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Helpers: demux
 *****************************************************************************/
static void CheckPcr( demux_t *p_demux, real_track_t *tk, mtime_t i_dts )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_dts > VLC_TS_INVALID )
        tk->i_last_dts = i_dts;

    if( p_sys->i_pcr > VLC_TS_INVALID || i_dts <= VLC_TS_INVALID )
        return;

    p_sys->i_pcr = i_dts;
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_pcr );
}

static void DemuxVideo( demux_t *p_demux, real_track_t *tk, mtime_t i_dts, unsigned i_flags )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    const uint8_t *p_data = p_sys->buffer;
    int     i_data = p_sys->i_buffer;

    while( i_data > 1 )
    {
        uint8_t i_hdr = R8( &p_data, &i_data );
        uint8_t i_type = i_hdr >> 6;

        uint8_t i_seq;
        int i_len;
        int i_pos;
        int i_frame_num;

        if( i_type == 1 )
        {
            R8( &p_data, &i_data );
            i_len = i_data;
            i_pos = 0;
            i_frame_num = -1;
            i_seq = 1;
            i_hdr &= ~0x3f;
        }
        else if( i_type == 3 )
        {
            i_len = RLength( &p_data, &i_data );
            i_pos = RLength( &p_data, &i_data );
            i_frame_num = R8( &p_data, &i_data );
            i_seq = 1;
            i_hdr &= ~0x3f;
        }
        else
        {
            assert( i_type == 0 || i_type == 2 );
            i_seq = R8( &p_data, &i_data );
            i_len = RLength( &p_data, &i_data );

            i_pos = RLength( &p_data, &i_data );
            i_frame_num = R8( &p_data, &i_data );
        }

        if( (i_seq & 0x7f) == 1 || tk->i_frame_num != i_frame_num )
        {
            tk->i_frame_slice = 0;
            tk->i_frame_slice_count = 2 * (i_hdr & 0x3f) + 1;
            tk->i_frame_pos = 2*4 * tk->i_frame_slice_count + 1;
            tk->i_frame_size = i_len + 2*4 * tk->i_frame_slice_count + 1;
            tk->i_frame_num = i_frame_num;

            if( tk->p_frame )
                block_Release( tk->p_frame );

            tk->p_frame = block_Alloc( tk->i_frame_size );
            if( !tk->p_frame )
            {
                tk->i_frame_size = 0;
                return;
            }

            tk->p_frame->i_dts = i_dts;
            tk->p_frame->i_pts = VLC_TS_INVALID;
            if( i_flags & 0x02 )
                tk->p_frame->i_flags |= BLOCK_FLAG_TYPE_I;

            i_dts = VLC_TS_INVALID;
        }

        int i_frame_data;
        if( i_type == 3 )
        {
            i_frame_data = i_len;
        }
        else
        {
            i_frame_data = i_data;
            if( i_type == 2 && i_frame_data > i_pos )
                i_frame_data = i_pos;
        }
        if( i_frame_data > i_data )
            break;

        /* */
        tk->i_frame_slice++;
        if( tk->i_frame_slice > tk->i_frame_slice_count || !tk->p_frame )
            break;

        /* */
        SetDWLE( &tk->p_frame->p_buffer[2*4*(tk->i_frame_slice-1) + 1 + 0], 1 );
        SetDWLE( &tk->p_frame->p_buffer[2*4*(tk->i_frame_slice-1) + 1 + 4], tk->i_frame_pos - (2*4 * tk->i_frame_slice_count + 1) );

        if( tk->i_frame_pos + i_frame_data > tk->i_frame_size )
            break;

        memcpy( &tk->p_frame->p_buffer[tk->i_frame_pos], p_data, i_frame_data );
        RVoid( &p_data, &i_data, i_frame_data );
        tk->i_frame_pos += i_frame_data;

        if( i_type != 0 || tk->i_frame_pos >= tk->i_frame_size )
        {
            /* Fix the buffer once the real number of slice is known */
            tk->p_frame->p_buffer[0] = tk->i_frame_slice - 1;
            tk->p_frame->i_buffer = tk->i_frame_pos - 2*4*( tk->i_frame_slice_count - tk->i_frame_slice );

            memmove( &tk->p_frame->p_buffer[1+2*4*tk->i_frame_slice      ],
                     &tk->p_frame->p_buffer[1+2*4*tk->i_frame_slice_count],
                     tk->i_frame_pos - (2*4*tk->i_frame_slice_count + 1) );

            /* Send it */
            CheckPcr( p_demux, tk, tk->p_frame->i_dts );
            es_out_Send( p_demux->out, tk->p_es, tk->p_frame );

            tk->i_frame_size = 0;
            tk->p_frame = NULL;
        }
    }
}

static void DemuxAudioMethod1( demux_t *p_demux, real_track_t *tk, mtime_t i_pts, unsigned int i_flags )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t *p_buf = p_sys->buffer;

    /* Sanity check */
    if( (i_flags & 2) || p_sys->b_seek )
    {
        tk->i_subpacket = 0;
        tk->i_out_subpacket = 0;
        p_sys->b_seek = false;
    }

    if( tk->fmt.i_codec == VLC_CODEC_COOK ||
        tk->fmt.i_codec == VLC_CODEC_ATRAC3 )
    {
        const int i_num = tk->i_frame_size / tk->i_subpacket_size;
        const int y = tk->i_subpacket / ( tk->i_frame_size / tk->i_subpacket_size );

        for( int i = 0; i < i_num; i++ )
        {
            int i_index = tk->i_subpacket_h * i +
                          ((tk->i_subpacket_h + 1) / 2) * (y&1) + (y>>1);
            if( i_index >= tk->i_subpackets )
                return;

            block_t *p_block = block_Alloc( tk->i_subpacket_size );
            if( !p_block )
                return;
            if( &p_buf[tk->i_subpacket_size] > &p_sys->buffer[p_sys->i_buffer] )
                return;

            memcpy( p_block->p_buffer, p_buf, tk->i_subpacket_size );
            p_block->i_dts =
            p_block->i_pts = VLC_TS_INVALID;

            p_buf += tk->i_subpacket_size;

            if( tk->p_subpackets[i_index] != NULL )
            {
                msg_Dbg(p_demux, "p_subpackets[ %d ] not null!",  i_index );
                block_Release( tk->p_subpackets[i_index] );
            }

            tk->p_subpackets[i_index] = p_block;
            if( tk->i_subpacket == 0 )
                tk->p_subpackets_timecode[0] = i_pts;
            tk->i_subpacket++;
        }
    }
    else
    {
        const int y = tk->i_subpacket / (tk->i_subpacket_h / 2);
        assert( tk->fmt.i_codec == VLC_CODEC_RA_288 );

        for( int i = 0; i < tk->i_subpacket_h / 2; i++ )
        {
            int i_index = (i * 2 * tk->i_frame_size / tk->i_coded_frame_size) + y;
            if( i_index >= tk->i_subpackets )
                return;

            block_t *p_block = block_Alloc( tk->i_coded_frame_size);
            if( !p_block )
                return;
            if( &p_buf[tk->i_coded_frame_size] > &p_sys->buffer[p_sys->i_buffer] )
                return;

            memcpy( p_block->p_buffer, p_buf, tk->i_coded_frame_size );
            p_block->i_dts =
            p_block->i_pts = i_index == 0 ? i_pts : VLC_TS_INVALID;

            p_buf += tk->i_coded_frame_size;

            if( tk->p_subpackets[i_index] != NULL )
            {
                msg_Dbg(p_demux, "p_subpackets[ %d ] not null!",  i_index );
                block_Release( tk->p_subpackets[i_index] );
            }

            tk->p_subpackets[i_index] = p_block;
            tk->i_subpacket++;
        }
    }

    while( tk->i_out_subpacket != tk->i_subpackets &&
           tk->p_subpackets[tk->i_out_subpacket] )
    {
        block_t *p_block = tk->p_subpackets[tk->i_out_subpacket];
        tk->p_subpackets[tk->i_out_subpacket] = NULL;

        if( tk->p_subpackets_timecode[tk->i_out_subpacket] )
        {
            p_block->i_dts =
            p_block->i_pts = tk->p_subpackets_timecode[tk->i_out_subpacket];

            tk->p_subpackets_timecode[tk->i_out_subpacket] = 0;
        }
        tk->i_out_subpacket++;

        CheckPcr( p_demux, tk, p_block->i_pts );
        es_out_Send( p_demux->out, tk->p_es, p_block );
    }

    if( tk->i_subpacket == tk->i_subpackets &&
        tk->i_out_subpacket != tk->i_subpackets )
    {
        msg_Warn( p_demux, "i_subpacket != i_out_subpacket, "
                  "this shouldn't happen" );
    }

    if( tk->i_subpacket == tk->i_subpackets )
    {
        tk->i_subpacket = 0;
        tk->i_out_subpacket = 0;
    }
}

static void DemuxAudioMethod2( demux_t *p_demux, real_track_t *tk, mtime_t i_pts )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->i_buffer < 2 )
        return;

    unsigned i_sub = (p_sys->buffer[1] >> 4)&0x0f;
    if( p_sys->i_buffer < 2+2*i_sub )
        return;

    uint8_t *p_sub = &p_sys->buffer[2+2*i_sub];

    for( unsigned i = 0; i < i_sub; i++ )
    {
        const int i_sub_size = GetWBE( &p_sys->buffer[2+i*2] );
        block_t *p_block = block_Alloc( i_sub_size );
        if( !p_block )
            break;

        if( &p_sub[i_sub_size] > &p_sys->buffer[p_sys->i_buffer] )
            break;

        memcpy( p_block->p_buffer, p_sub, i_sub_size );
        p_sub += i_sub_size;

        p_block->i_dts =
        p_block->i_pts = i == 0 ? i_pts : VLC_TS_INVALID;

        CheckPcr( p_demux, tk, p_block->i_pts );
        es_out_Send( p_demux->out, tk->p_es, p_block );
    }
}
static void DemuxAudioMethod3( demux_t *p_demux, real_track_t *tk, mtime_t i_pts )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->i_buffer <= 0 )
        return;

    block_t *p_block = block_Alloc( p_sys->i_buffer );
    if( !p_block )
        return;

    if( tk->fmt.i_codec == VLC_CODEC_A52 )
    {
        uint8_t *p_src = p_sys->buffer;
        uint8_t *p_dst = p_block->p_buffer;

        /* byte swap data */
        while( p_dst < &p_block->p_buffer[p_sys->i_buffer - 1])
        {
            *p_dst++ = p_src[1];
            *p_dst++ = p_src[0];

            p_src += 2;
        }
    }
    else
    {
        memcpy( p_block->p_buffer, p_sys->buffer, p_sys->i_buffer );
    }
    p_block->i_dts =
    p_block->i_pts = i_pts;

    CheckPcr( p_demux, tk, p_block->i_pts );
    es_out_Send( p_demux->out, tk->p_es, p_block );
}

// Sipr packet re-ordering code and index table borrowed from
// the MPlayer Realmedia demuxer.
static const uint8_t sipr_swap_index_table[38][2] = {
    {  0, 63 }, {  1, 22 }, {  2, 44 }, {  3, 90 },
    {  5, 81 }, {  7, 31 }, {  8, 86 }, {  9, 58 },
    { 10, 36 }, { 12, 68 }, { 13, 39 }, { 14, 73 },
    { 15, 53 }, { 16, 69 }, { 17, 57 }, { 19, 88 },
    { 20, 34 }, { 21, 71 }, { 24, 46 }, { 25, 94 },
    { 26, 54 }, { 28, 75 }, { 29, 50 }, { 32, 70 },
    { 33, 92 }, { 35, 74 }, { 38, 85 }, { 40, 56 },
    { 42, 87 }, { 43, 65 }, { 45, 59 }, { 48, 79 },
    { 49, 93 }, { 51, 89 }, { 55, 95 }, { 61, 76 },
    { 67, 83 }, { 77, 80 }
};

static void SiprPacketReorder(uint8_t *buf, int sub_packet_h, int framesize)
{
    int n, bs = sub_packet_h * framesize * 2 / 96; // nibbles per subpacket

    for (n = 0; n < 38; n++) {
        int j;
        int i = bs * sipr_swap_index_table[n][0];
        int o = bs * sipr_swap_index_table[n][1];

        /* swap 4 bit-nibbles of block 'i' with 'o' */
        for (j = 0; j < bs; j++, i++, o++) {
           int x = (buf[i >> 1] >> (4 * (i & 1))) & 0xF,
                y = (buf[o >> 1] >> (4 * (o & 1))) & 0xF;

            buf[o >> 1] = (x << (4 * (o & 1))) |
                (buf[o >> 1] & (0xF << (4 * !(o & 1))));
            buf[i >> 1] = (y << (4 * (i & 1))) |
                (buf[i >> 1] & (0xF << (4 * !(i & 1))));
        }
    }
}

static void DemuxAudioSipr( demux_t *p_demux, real_track_t *tk, mtime_t i_pts )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block = tk->p_sipr_packet;

    if( p_sys->i_buffer < tk->i_frame_size
     || tk->i_sipr_subpacket_count >= tk->i_subpacket_h )
        return;

    if( !p_block )
    {
        p_block = block_Alloc( tk->i_frame_size * tk->i_subpacket_h );
        if( !p_block )
            return;
        tk->p_sipr_packet = p_block;
    }
    memcpy( p_block->p_buffer + tk->i_sipr_subpacket_count * tk->i_frame_size,
            p_sys->buffer, tk->i_frame_size );
    if (!tk->i_sipr_subpacket_count)
    {
        p_block->i_dts =
        p_block->i_pts = i_pts;
    }

    if( ++tk->i_sipr_subpacket_count < tk->i_subpacket_h )
        return;

    SiprPacketReorder(p_block->p_buffer, tk->i_subpacket_h, tk->i_frame_size);
    CheckPcr( p_demux, tk, p_block->i_pts );
    es_out_Send( p_demux->out, tk->p_es, p_block );
    tk->i_sipr_subpacket_count = 0;
    tk->p_sipr_packet = NULL;
}

static void DemuxAudio( demux_t *p_demux, real_track_t *tk, mtime_t i_pts, unsigned i_flags )
{
    switch( tk->fmt.i_codec )
    {
    case VLC_CODEC_COOK:
    case VLC_CODEC_ATRAC3:
    case VLC_CODEC_RA_288:
        DemuxAudioMethod1( p_demux, tk, i_pts, i_flags );
        break;
    case VLC_CODEC_MP4A:
        DemuxAudioMethod2( p_demux, tk, i_pts );
        break;
    case VLC_CODEC_SIPR:
        DemuxAudioSipr( p_demux, tk, i_pts );
        break;
    default:
        DemuxAudioMethod3( p_demux, tk, i_pts );
        break;
    }
}

/*****************************************************************************
 * Helpers: seek/control
 *****************************************************************************/
static int ControlGoToIndex( demux_t *p_demux, real_index_t *p_index )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->b_seek = true;
    p_sys->i_pcr = INT64_C(1000) * p_index->i_time_offset;
    for( int i = 0; i < p_sys->i_track; i++ )
        p_sys->track[i]->i_last_dts = 0;
    return stream_Seek( p_demux->s, p_index->i_file_offset );
}
static int ControlSeekTime( demux_t *p_demux, mtime_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    real_index_t *p_index = p_sys->p_index;

    while( p_index->i_file_offset != 0 )
    {
        if( p_index->i_time_offset * INT64_C(1000) > i_time )
        {
            if( p_index != p_sys->p_index )
                p_index--;
            break;
        }
        p_index++;
    }
    if( p_index->i_file_offset == 0 )
        return VLC_EGENERIC;
    return ControlGoToIndex( p_demux, p_index );
}
static int ControlSeekByte( demux_t *p_demux, int64_t i_bytes )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    real_index_t *p_index = p_sys->p_index;

    while( p_index->i_file_offset != 0 )
    {
        if( p_index->i_file_offset > i_bytes )
        {
            if( p_index != p_sys->p_index )
                p_index--;
            break;
        }
        p_index++;
    }
    if( p_index->i_file_offset == 0 )
        return VLC_EGENERIC;
    return ControlGoToIndex( p_demux, p_index );
}

/*****************************************************************************
 * Helpers: header reading
 *****************************************************************************/

/**
 * This function will read a pascal string with size stored in 2 bytes from
 * a stream_t.
 *
 * FIXME what is the right charset ?
 */
static char *StreamReadString2( stream_t *s )
{
    uint8_t p_tmp[2];

    if( stream_Read( s, p_tmp, 2 ) < 2 )
        return NULL;

    const int i_length = GetWBE( p_tmp );
    if( i_length <= 0 )
        return NULL;

    char *psz_string = xcalloc( 1, i_length + 1 );

    stream_Read( s, psz_string, i_length ); /* Valid even if !psz_string */

    if( psz_string )
        EnsureUTF8( psz_string );
    return psz_string;
}

/**
 * This function will read a pascal string with size stored in 1 byte from a
 * memory buffer.
 *
 * FIXME what is the right charset ?
 */
static char *MemoryReadString1( const uint8_t **pp_data, int *pi_data )
{
    const uint8_t *p_data = *pp_data;
    int           i_data = *pi_data;

    char *psz_string = NULL;

    if( i_data < 1 )
        goto exit;

    int i_length = *p_data++; i_data--;
    if( i_length > i_data )
        i_length = i_data;

    if( i_length > 0 )
    {
        psz_string = strndup( (const char*)p_data, i_length );
        if( psz_string )
            EnsureUTF8( psz_string );

        p_data += i_length;
        i_data -= i_length;
    }

exit:
    *pp_data = p_data;
    *pi_data = i_data;
    return psz_string;
}

/**
 * This function parses(skip) the .RMF identification chunk.
 */
static int HeaderRMF( demux_t *p_demux )
{
    uint8_t p_buffer[8];

    if( stream_Read( p_demux->s, p_buffer, 8 ) < 8 )
        return VLC_EGENERIC;

    msg_Dbg( p_demux, "    - file version=0x%x num headers=%d",
             GetDWBE( &p_buffer[0] ), GetDWBE( &p_buffer[4] ) );
    return VLC_SUCCESS;
}
/**
 * This function parses the PROP properties chunk.
 */
static int HeaderPROP( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    uint8_t p_buffer[40];
    int i_flags;

    if( stream_Read( p_demux->s, p_buffer, 40 ) < 40 )
        return VLC_EGENERIC;

    msg_Dbg( p_demux, "    - max bitrate=%d avg bitrate=%d",
             GetDWBE(&p_buffer[0]), GetDWBE(&p_buffer[4]) );
    msg_Dbg( p_demux, "    - max packet size=%d avg bitrate=%d",
             GetDWBE(&p_buffer[8]), GetDWBE(&p_buffer[12]) );
    msg_Dbg( p_demux, "    - packets count=%d", GetDWBE(&p_buffer[16]) );
    msg_Dbg( p_demux, "    - duration=%d ms", GetDWBE(&p_buffer[20]) );
    msg_Dbg( p_demux, "    - preroll=%d ms", GetDWBE(&p_buffer[24]) );
    msg_Dbg( p_demux, "    - index offset=%d", GetDWBE(&p_buffer[28]) );
    msg_Dbg( p_demux, "    - data offset=%d", GetDWBE(&p_buffer[32]) );
    msg_Dbg( p_demux, "    - num streams=%d", GetWBE(&p_buffer[36]) );

    /* set the duration for export in control */
    p_sys->i_our_duration = GetDWBE(&p_buffer[20]);

    p_sys->i_index_offset = GetDWBE(&p_buffer[28]);

    i_flags = GetWBE(&p_buffer[38]);
    msg_Dbg( p_demux, "    - flags=0x%x %s%s%s",
             i_flags,
             i_flags&0x0001 ? "PN_SAVE_ENABLED " : "",
             i_flags&0x0002 ? "PN_PERFECT_PLAY_ENABLED " : "",
             i_flags&0x0004 ? "PN_LIVE_BROADCAST" : "" );

    return VLC_SUCCESS;
}
/**
 * This functions parses the CONT commentairs chunk.
 */
static int HeaderCONT( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* */
    p_sys->psz_title = StreamReadString2( p_demux->s );
    if( p_sys->psz_title )
        msg_Dbg( p_demux, "    - title=`%s'", p_sys->psz_title );

    /* */
    p_sys->psz_artist = StreamReadString2( p_demux->s );
    if( p_sys->psz_artist )
        msg_Dbg( p_demux, "    - artist=`%s'", p_sys->psz_artist );

    /* */
    p_sys->psz_copyright = StreamReadString2( p_demux->s );
    if( p_sys->psz_copyright )
        msg_Dbg( p_demux, "    - copyright=`%s'", p_sys->psz_copyright );

    /* */
    p_sys->psz_description = StreamReadString2( p_demux->s );
    if( p_sys->psz_description )
        msg_Dbg( p_demux, "    - comment=`%s'", p_sys->psz_description );

    return VLC_SUCCESS;
}
/**
 * This function parses the MDPR (Media properties) chunk.
 */
static int HeaderMDPR( demux_t *p_demux )
{
    uint8_t p_buffer[30];

    if( stream_Read( p_demux->s, p_buffer, 30 ) < 30 )
        return VLC_EGENERIC;

    const int i_num = GetWBE( &p_buffer[0] );
    msg_Dbg( p_demux, "    - id=0x%x", i_num );
    msg_Dbg( p_demux, "    - max bitrate=%d avg bitrate=%d",
             GetDWBE(&p_buffer[2]), GetDWBE(&p_buffer[6]) );
    msg_Dbg( p_demux, "    - max packet size=%d avg packet size=%d",
             GetDWBE(&p_buffer[10]), GetDWBE(&p_buffer[14]) );
    msg_Dbg( p_demux, "    - start time=%d", GetDWBE(&p_buffer[18]) );
    msg_Dbg( p_demux, "    - preroll=%d", GetDWBE(&p_buffer[22]) );
    msg_Dbg( p_demux, "    - duration=%d", GetDWBE(&p_buffer[26]) );

    /* */
    const uint8_t *p_peek;
    int i_peek_org = stream_Peek( p_demux->s, &p_peek, 2 * 256 );
    int i_peek = i_peek_org;
    if( i_peek <= 0 )
        return VLC_EGENERIC;

    char *psz_name = MemoryReadString1( &p_peek, &i_peek );
    if( psz_name )
    {
        msg_Dbg( p_demux, "    - name=`%s'", psz_name );
        free( psz_name );
    }
    char *psz_mime = MemoryReadString1( &p_peek, &i_peek );
    if( psz_mime )
    {
        msg_Dbg( p_demux, "    - mime=`%s'", psz_mime );
        free( psz_mime );
    }
    const int i_skip = i_peek_org - i_peek;
    if( i_skip > 0 && stream_Read( p_demux->s, NULL, i_skip ) < i_skip )
        return VLC_EGENERIC;

    /* */
    if( stream_Read( p_demux->s, p_buffer, 4 ) < 4 )
        return VLC_EGENERIC;

    const uint32_t i_size = GetDWBE( p_buffer );
    if( i_size > 0 )
    {
        CodecParse( p_demux, i_size, i_num );
        unsigned size = stream_Read( p_demux->s, NULL, i_size );
        if( size < i_size )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
/**
 * This function parses DATA chunk (it contains the actual movie data).
 */
static int HeaderDATA( demux_t *p_demux, uint32_t i_size )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t p_buffer[8];

    if( stream_Read( p_demux->s, p_buffer, 8 ) < 8 )
        return VLC_EGENERIC;

    p_sys->i_data_offset    = stream_Tell( p_demux->s ) - 10;
    p_sys->i_data_size      = i_size;
    p_sys->i_data_packets_count = GetDWBE( p_buffer );
    p_sys->i_data_packets   = 0;
    p_sys->i_data_offset_next = GetDWBE( &p_buffer[4] );

    msg_Dbg( p_demux, "    - packets count=%d next=%u",
             p_sys->i_data_packets_count,
             (unsigned int)p_sys->i_data_offset_next );
    return VLC_SUCCESS;
}
/**
 * This function parses the INDX (movie index chunk).
 * It is optional but seeking without it is ... hard.
 */
static void HeaderINDX( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t       buffer[20];

    uint32_t      i_index_count;

    if( p_sys->i_index_offset == 0 )
        return;

    stream_Seek( p_demux->s, p_sys->i_index_offset );

    if( stream_Read( p_demux->s, buffer, 20 ) < 20 )
        return ;

    const uint32_t i_id = VLC_FOURCC( buffer[0], buffer[1], buffer[2], buffer[3] );
    const uint32_t i_size      = GetDWBE( &buffer[4] );
    int i_version   = GetWBE( &buffer[8] );

    msg_Dbg( p_demux, "Real index %4.4s size=%d version=%d",
                 (char*)&i_id, i_size, i_version );

    if( (i_size < 20) && (i_id != VLC_FOURCC('I','N','D','X')) )
        return;

    i_index_count = GetDWBE( &buffer[10] );

    msg_Dbg( p_demux, "Real Index : num : %d ", i_index_count );

    if( i_index_count >= ( 0xffffffff / sizeof(*p_sys->p_index) ) )
        return;

    if( GetDWBE( &buffer[16] ) > 0 )
        msg_Dbg( p_demux, "Real Index: Does next index exist? %d ",
                        GetDWBE( &buffer[16] )  );

    /* One extra entry is allocated (that MUST be set to 0) to identify the
     * end of the index.
     * TODO add a clean entry count (easier to build index on the fly) */
    p_sys->p_index = calloc( i_index_count + 1, sizeof(*p_sys->p_index) );
    if( !p_sys->p_index )
        return;

    for( unsigned int i = 0; i < i_index_count; i++ )
    {
        uint8_t p_entry[14];

        if( stream_Read( p_demux->s, p_entry, 14 ) < 14 )
            return ;

        if( GetWBE( &p_entry[0] ) != 0 )
        {
            msg_Dbg( p_demux, "Real Index: invaild version of index entry %d ",
                              GetWBE( &p_entry[0] ) );
            return;
        }

        real_index_t *p_idx = &p_sys->p_index[i];
        
        p_idx->i_time_offset = GetDWBE( &p_entry[2] );
        p_idx->i_file_offset = GetDWBE( &p_entry[6] );
        p_idx->i_frame_index = GetDWBE( &p_entry[10] );

#if 0
        msg_Dbg( p_demux,
                 "Real Index: time %"PRIu32" file %"PRIu32" frame %"PRIu32,
                 p_idx->i_time_offset,
                 p_idx->i_file_offset,
                 p_idx->i_frame_index );
#endif
    }
}


/**
 * This function parses the complete RM headers and move the
 * stream pointer to the data to be read.
 */
static int HeaderRead( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for( ;; )
    {
        const int64_t i_stream_position = stream_Tell( p_demux->s );
        uint8_t header[100];    /* FIXME */

        /* Read the header */
        if( stream_Read( p_demux->s, header, 10 ) < 10 )
            return VLC_EGENERIC;

        const uint32_t i_id = VLC_FOURCC( header[0], header[1],
                                          header[2], header[3] );
        const uint32_t i_size = GetDWBE( &header[4] );
        const int i_version = GetWBE( &header[8] );

        msg_Dbg( p_demux, "object %4.4s size=%d version=%d",
                 (char*)&i_id, i_size, i_version );

        /* */
        if( i_size < 10 && i_id != VLC_FOURCC('D','A','T','A') )
        {
            msg_Dbg( p_demux, "invalid size for object %4.4s", (char*)&i_id );
            return VLC_EGENERIC;
        }

        int i_ret;
        switch( i_id )
        {
        case VLC_FOURCC('.','R','M','F'):
            i_ret = HeaderRMF( p_demux );
            break;
        case VLC_FOURCC('P','R','O','P'):
            i_ret = HeaderPROP( p_demux );
            break;
        case VLC_FOURCC('C','O','N','T'):
            i_ret = HeaderCONT( p_demux );
            break;
        case VLC_FOURCC('M','D','P','R'):
            i_ret = HeaderMDPR( p_demux );
            break;
        case VLC_FOURCC('D','A','T','A'):
            i_ret = HeaderDATA( p_demux, i_size );
            break;
        default:
            /* unknow header */
            msg_Dbg( p_demux, "unknown chunk" );
            i_ret = VLC_SUCCESS;
            break;
        }
        if( i_ret )
            return i_ret;

        if( i_id == VLC_FOURCC('D','A','T','A') ) /* In this case, parsing is finished */
            break;

        /* Skip unread data */
        const int64_t i_stream_current = stream_Tell( p_demux->s );
        const int64_t i_stream_skip = (i_stream_position + i_size) - i_stream_current;

        if( i_stream_skip > 0 )
        {
            if( stream_Read( p_demux->s, NULL, i_stream_skip ) != i_stream_skip )
                return VLC_EGENERIC;
        }
        else if( i_stream_skip < 0 )
        {
            return VLC_EGENERIC;
        }
    }

    /* read index if possible */
    if( p_sys->i_index_offset > 0 )
    {
        const int64_t i_position = stream_Tell( p_demux->s );

        HeaderINDX( p_demux );

        if( stream_Seek( p_demux->s, i_position ) )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void CodecMetaRead( demux_t *p_demux, const uint8_t **pp_data, int *pi_data )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Title */
    p_sys->psz_title = MemoryReadString1( pp_data, pi_data );
    if( p_sys->psz_title )
        msg_Dbg( p_demux, "    - title=`%s'", p_sys->psz_title );

    /* Authors */
    p_sys->psz_artist = MemoryReadString1( pp_data, pi_data );
    if( p_sys->psz_artist )
        msg_Dbg( p_demux, "    - artist=`%s'", p_sys->psz_artist );

    /* Copyright */
    p_sys->psz_copyright = MemoryReadString1( pp_data, pi_data );
    if( p_sys->psz_copyright )
        msg_Dbg( p_demux, "    - copyright=`%s'", p_sys->psz_copyright );

    /* Comment */
    p_sys->psz_description = MemoryReadString1( pp_data, pi_data );
    if( p_sys->psz_description )
        msg_Dbg( p_demux, "    - Comment=`%s'", p_sys->psz_description );
}

static int CodecVideoParse( demux_t *p_demux, int i_tk_id, const uint8_t *p_data, int i_data )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_data < 34 )
        return VLC_EGENERIC;

    /* */
    es_format_t fmt;
    es_format_Init( &fmt, VIDEO_ES,
                    VLC_FOURCC( p_data[8], p_data[9], p_data[10], p_data[11] ) );
    fmt.video.i_width = GetWBE( &p_data[12] );
    fmt.video.i_height= GetWBE( &p_data[14] );
    fmt.video.i_frame_rate = (GetWBE( &p_data[22] ) << 16) | GetWBE( &p_data[24] );
    fmt.video.i_frame_rate_base = 1 << 16;

    fmt.i_extra = i_data - 26;
    fmt.p_extra = malloc( fmt.i_extra );
    if( !fmt.p_extra )
        return VLC_ENOMEM;

    memcpy( fmt.p_extra, &p_data[26], fmt.i_extra );

    //msg_Dbg( p_demux, "    - video 0x%08x 0x%08x", dw0, dw1 );

    /* */
    switch( GetDWBE( &p_data[30] ) )
    {
    case 0x10003000:
    case 0x10003001:
        fmt.i_codec = VLC_CODEC_RV13;
        break;
    case 0x20001000:
    case 0x20100001:
    case 0x20200002:
    case 0x20201002:
        fmt.i_codec = VLC_CODEC_RV20;
        break;
    case 0x30202002:
        fmt.i_codec = VLC_CODEC_RV30;
        break;
    case 0x40000000:
        fmt.i_codec = VLC_CODEC_RV40;
        break;
    }
    msg_Dbg( p_demux, "    - video %4.4s %dx%d - %8.8x",
             (char*)&fmt.i_codec, fmt.video.i_width, fmt.video.i_height, GetDWBE( &p_data[30] ) );

    real_track_t *tk = malloc( sizeof( *tk ) );
    if( !tk )
    {
        es_format_Clean( &fmt );
        return VLC_ENOMEM;
    }
    tk->i_out_subpacket = 0;
    tk->i_subpacket = 0;
    tk->i_subpackets = 0;
    tk->p_subpackets = NULL;
    tk->p_subpackets_timecode = NULL;
    tk->i_id = i_tk_id;
    tk->fmt = fmt;
    tk->i_frame_num = -1;
    tk->i_frame_size = 0;
    tk->p_frame = NULL;
    tk->i_last_dts = 0;
    tk->p_sipr_packet = NULL;
    tk->p_es = es_out_Add( p_demux->out, &fmt );

    TAB_APPEND( p_sys->i_track, p_sys->track, tk );
    return VLC_SUCCESS;
}
static int CodecAudioParse( demux_t *p_demux, int i_tk_id, const uint8_t *p_data, int i_data )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    es_format_t fmt;

    if( i_data < 6 )
        return VLC_EGENERIC;

    int i_flavor = 0;
    int i_coded_frame_size = 0;
    int i_subpacket_h = 0;
    int i_frame_size = 0;
    int i_subpacket_size = 0;
    char p_genr[4];
    int i_version = GetWBE( &p_data[4] );
    int i_extra_codec = 0;

    msg_Dbg( p_demux, "    - audio version=%d", i_version );

    es_format_Init( &fmt, AUDIO_ES, 0 );

    RVoid( &p_data, &i_data, 6 );                         /* 4 + version */
    if( i_version == 3 ) /* RMF version 3 or .ra version 3 */
    {
        RVoid( &p_data, &i_data, 2 + 10 + 4 );

        /* Meta Datas */
        CodecMetaRead( p_demux, &p_data, &i_data );

        RVoid( &p_data, &i_data, 1 + 1 );
        if( i_data >= 4 )
            memcpy( &fmt.i_codec, p_data, 4 );
        RVoid( &p_data, &i_data, 4 );

        fmt.audio.i_channels = 1;      /* This is always the case in rm3 */
        fmt.audio.i_rate = 8000;

        msg_Dbg( p_demux, "    - audio codec=%4.4s channels=%d rate=%dHz",
             (char*)&fmt.i_codec, fmt.audio.i_channels, fmt.audio.i_rate );
    }
    else  /* RMF version 4/5 or .ra version 4 */
    {
        RVoid( &p_data, &i_data, 2 + 4 + 4 + 2 + 4 );
        i_flavor = R16( &p_data, &i_data );
        i_coded_frame_size = R32( &p_data, &i_data );
        RVoid( &p_data, &i_data, 4 + 4 + 4 );
        i_subpacket_h = R16( &p_data, &i_data );
        i_frame_size = R16( &p_data, &i_data );
        i_subpacket_size = R16( &p_data, &i_data );
        if( !i_frame_size || !i_coded_frame_size )
        {
            es_format_Clean( &fmt );
            return VLC_EGENERIC;
        }

        RVoid( &p_data, &i_data, 2 + (i_version == 5 ? 6 : 0 ) );

        fmt.audio.i_rate = R16( &p_data, &i_data );
        RVoid( &p_data, &i_data, 2 );
        fmt.audio.i_bitspersample = R16( &p_data, &i_data );
        fmt.audio.i_channels = R16( &p_data, &i_data );
        fmt.audio.i_blockalign = i_frame_size;

        if( i_version == 5 )
        {
            if( i_data >= 8 )
            {
                memcpy( p_genr,       &p_data[0], 4 );
                memcpy( &fmt.i_codec, &p_data[4], 4 );
            }
            RVoid( &p_data, &i_data, 8 );
        }
        else /* version 4 */
        {
            if( i_data > 0 )
                RVoid( &p_data, &i_data, 1 + *p_data );
            if( i_data >= 1 + 4 )
                memcpy( &fmt.i_codec, &p_data[1], 4 );
            if( i_data > 0 )
                RVoid( &p_data, &i_data, 1 + *p_data );
        }

        msg_Dbg( p_demux, "    - audio codec=%4.4s channels=%d rate=%dHz",
             (char*)&fmt.i_codec, fmt.audio.i_channels, fmt.audio.i_rate );

        RVoid( &p_data, &i_data, 3 );

        if( p_sys->b_real_audio )
        {
            CodecMetaRead( p_demux, &p_data, &i_data );
        }
        else
        {
            if( i_version == 5 )
                RVoid( &p_data, &i_data, 1 );
            i_extra_codec = R32( &p_data, &i_data );
        }
    }

    switch( fmt.i_codec )
    {
    case VLC_FOURCC('l','p','c','J'):
    case VLC_FOURCC('1','4','_','4'):
        fmt.i_codec = VLC_CODEC_RA_144;
        fmt.audio.i_blockalign = 0x14 ;
        break;

    case VLC_FOURCC('2','8','_','8'):
        if( i_coded_frame_size <= 0 )
        {
            es_format_Clean( &fmt );
            return VLC_EGENERIC;
        }
        fmt.i_codec = VLC_CODEC_RA_288;
        fmt.audio.i_blockalign = i_coded_frame_size;
        break;

    case VLC_FOURCC( 'a','5','2',' ' ):
    case VLC_FOURCC( 'd','n','e','t' ):
        fmt.i_codec = VLC_CODEC_A52;
        break;

    case VLC_FOURCC( 'r','a','a','c' ):
    case VLC_FOURCC( 'r','a','c','p' ):
        fmt.i_codec = VLC_CODEC_MP4A;

        if( i_extra_codec > 0 )
        {
            i_extra_codec--;
            RVoid( &p_data, &i_data, 1 );
        }
        if( i_extra_codec > 0 )
        {
            fmt.p_extra = malloc( i_extra_codec );
            if( !fmt.p_extra || i_extra_codec > i_data )
            {
                free( fmt.p_extra );
                return VLC_ENOMEM;
            }

            fmt.i_extra = i_extra_codec;
            memcpy( fmt.p_extra, p_data, fmt.i_extra );
        }
        break;

    case VLC_CODEC_SIPR:
        fmt.i_codec = VLC_CODEC_SIPR;
        if( i_flavor > 3 )
        {
            msg_Dbg( p_demux, "    - unsupported sipr flavorc=%i", i_flavor );
            es_format_Clean( &fmt );
            return VLC_EGENERIC;
        }

        i_subpacket_size = i_subpacket_size_sipr[i_flavor];
        // The libavcodec sipr decoder requires stream bitrate
        // to be set during initialization so that the correct mode
        // can be selected.
        fmt.i_bitrate = fmt.audio.i_rate;
        msg_Dbg( p_demux, "    - sipr flavor=%i", i_flavor );

    case VLC_CODEC_COOK:
    case VLC_CODEC_ATRAC3:
        if( i_subpacket_size <= 0 || i_frame_size / i_subpacket_size <= 0 )
        {
            es_format_Clean( &fmt );
            return VLC_EGENERIC;
        }
        if( !memcmp( p_genr, "genr", 4 ) )
            fmt.audio.i_blockalign = i_subpacket_size;
        else
            fmt.audio.i_blockalign = i_coded_frame_size;

        if( i_extra_codec > 0 )
        {
            fmt.p_extra = malloc( i_extra_codec );
            if( !fmt.p_extra || i_extra_codec > i_data )
            {
                free( fmt.p_extra );
                return VLC_ENOMEM;
            }

            fmt.i_extra = i_extra_codec;
            memcpy( fmt.p_extra, p_data, fmt.i_extra );
        }

        break;

    case VLC_FOURCC('r','a','l','f'):
    default:
        msg_Dbg( p_demux, "    - unknown audio codec=%4.4s",
                (char*)&fmt.i_codec );
        es_format_Clean( &fmt );
        return VLC_EGENERIC;
        break;
    }
    msg_Dbg( p_demux, "    - extra data=%d", fmt.i_extra );

    /* */
    real_track_t *tk = malloc( sizeof( *tk ) );
    if( !tk )
    {
        es_format_Clean( &fmt );
        return VLC_ENOMEM;
    }
    tk->i_id = i_tk_id;
    tk->fmt = fmt;
    tk->i_frame_size = 0;
    tk->p_frame = NULL;

    tk->i_subpacket_h = i_subpacket_h;
    tk->i_subpacket_size = i_subpacket_size;
    tk->i_coded_frame_size = i_coded_frame_size;
    tk->i_frame_size = i_frame_size;

    tk->i_out_subpacket = 0;
    tk->i_subpacket = 0;
    tk->i_subpackets = 0;
    tk->p_subpackets = NULL;
    tk->p_subpackets_timecode = NULL;

    tk->p_sipr_packet          = NULL;
    tk->i_sipr_subpacket_count = 0;

    if( fmt.i_codec == VLC_CODEC_COOK ||
        fmt.i_codec == VLC_CODEC_ATRAC3 )
    {
        tk->i_subpackets =
            i_subpacket_h * i_frame_size / tk->i_subpacket_size;
        tk->p_subpackets =
            xcalloc( tk->i_subpackets, sizeof(block_t *) );
        tk->p_subpackets_timecode =
            xcalloc( tk->i_subpackets , sizeof( int64_t ) );
    }
    else if( fmt.i_codec == VLC_CODEC_RA_288 )
    {
        tk->i_subpackets =
            i_subpacket_h * i_frame_size / tk->i_coded_frame_size;
        tk->p_subpackets =
            xcalloc( tk->i_subpackets, sizeof(block_t *) );
        tk->p_subpackets_timecode =
            xcalloc( tk->i_subpackets , sizeof( int64_t ) );
    }

    /* Check if the calloc went correctly */
    if( tk->i_subpacket > 0 && ( !tk->p_subpackets || !tk->p_subpackets_timecode ) )
    {
        free( tk->p_subpackets_timecode );
        free( tk->p_subpackets );
        free( tk );
        msg_Err( p_demux, "Can't alloc subpacket" );
        return VLC_EGENERIC;
    }

    tk->i_last_dts = 0;
    tk->p_es = es_out_Add( p_demux->out, &fmt );

    TAB_APPEND( p_sys->i_track, p_sys->track, tk );

    return VLC_SUCCESS;
}


static int CodecParse( demux_t *p_demux, int i_len, int i_num )
{
    const uint8_t *p_peek;

    msg_Dbg( p_demux, "    - specific data len=%d", i_len );
    if( stream_Peek( p_demux->s, &p_peek, i_len ) < i_len )
        return VLC_EGENERIC;

    if( i_len >= 8 && !memcmp( &p_peek[4], "VIDO", 4 ) )
    {
        return CodecVideoParse( p_demux, i_num, p_peek, i_len );
    }
    else if( i_len >= 4 && !memcmp( &p_peek[0], ".ra\xfd", 4 ) )
    {
        return CodecAudioParse( p_demux, i_num, p_peek, i_len );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Helpers: memory buffer fct.
 *****************************************************************************/
static void RVoid( const uint8_t **pp_data, int *pi_data, int i_size )
{
    if( i_size > *pi_data )
        i_size = *pi_data;

    *pp_data += i_size;
    *pi_data -= i_size;
}
#define RX(name, type, size, code ) \
static type name( const uint8_t **pp_data, int *pi_data ) { \
    if( *pi_data < (size) )          \
        return 0;                    \
    type v = code;                   \
    RVoid( pp_data, pi_data, size ); \
    return v;                        \
}
RX(R8,  uint8_t, 1, **pp_data )
RX(R16, uint16_t, 2, GetWBE( *pp_data ) )
RX(R32, uint32_t, 4, GetDWBE( *pp_data ) )
static int RLength( const uint8_t **pp_data, int *pi_data )
{
    const int v0 = R16( pp_data, pi_data ) & 0x7FFF;
    if( v0 >= 0x4000 )
        return v0 - 0x4000;
    return (v0 << 16) | R16( pp_data, pi_data );
}


