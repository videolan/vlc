/*****************************************************************************
 * asf.c
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: asf.c,v 1.9 2003/12/22 02:24:53 sam Exp $
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("ASF muxer") );
    set_capability( "sout mux", 5 );
    add_shortcut( "asf" );
    add_shortcut( "asfh" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Locales prototypes
 *****************************************************************************/
static int  Capability(sout_mux_t *, int, void *, void * );
static int  AddStream( sout_mux_t *, sout_input_t * );
static int  DelStream( sout_mux_t *, sout_input_t * );
static int  Mux      ( sout_mux_t * );

typedef struct
{
    uint32_t v1; /* le */
    uint16_t v2; /* le */
    uint16_t v3; /* le */
    uint8_t  v4[8];
} guid_t;

typedef struct
{
    int          i_id;
    int          i_cat;

    /* codec informations */
    uint16_t     i_tag;     /* for audio */
    vlc_fourcc_t i_fourcc;  /* for video */
    char         *psz_name; /* codec name */

    int          i_sequence;

    int          i_extra;
    uint8_t     *p_extra;
} asf_track_t;

struct sout_mux_sys_t
{
    guid_t          fid;    /* file id */
    int             i_packet_size;
    int64_t         i_packet_count;
    mtime_t         i_dts_first;
    mtime_t         i_dts_last;
    int64_t         i_bitrate;

    int             i_track;
    asf_track_t     track[128];

    vlc_bool_t      b_write_header;

    sout_buffer_t   *pk;
    int             i_pk_used;
    int             i_pk_frame;
    mtime_t         i_pk_dts;

    vlc_bool_t      b_asf_http;
    int             i_seq;

    /* meta data */
    char            *psz_title;
    char            *psz_author;
    char            *psz_copyright;
    char            *psz_comment;
    char            *psz_rating;
};

static int MuxGetStream( sout_mux_t *, int *pi_stream, mtime_t *pi_dts );

static sout_buffer_t *asf_header_create( sout_mux_t *, vlc_bool_t b_broadcast );
static sout_buffer_t *asf_packet_create( sout_mux_t *,
                                         asf_track_t *, sout_buffer_t * );
static sout_buffer_t *asf_stream_end_create( sout_mux_t *);

typedef struct
{
    int      i_buffer_size;
    int      i_buffer;
    uint8_t  *p_buffer;
} bo_t;

static void bo_init     ( bo_t *, uint8_t *, int  );
static void bo_add_u8   ( bo_t *, uint8_t  );
static void bo_addle_u16( bo_t *, uint16_t );
static void bo_addle_u32( bo_t *, uint32_t );
static void bo_addle_u64( bo_t *, uint64_t );
static void bo_add_mem  ( bo_t *, uint8_t *, int );

static void bo_addle_str16( bo_t *, char * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t     *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys;
    int i;

    msg_Dbg( p_mux, "Asf muxer opened" );

    p_mux->pf_capacity  = Capability;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    p_mux->p_sys = p_sys = malloc( sizeof( sout_mux_sys_t ) );
    p_sys->b_asf_http = p_mux->psz_mux && !strcmp( p_mux->psz_mux, "asfh" );
    if( p_sys->b_asf_http )
    {
        msg_Dbg( p_mux, "creating asf stream to be used with mmsh" );
    }
    p_sys->pk = NULL;
    p_sys->i_pk_used    = 0;
    p_sys->i_pk_frame   = 0;
    p_sys->i_dts_first  = -1;
    p_sys->i_dts_last   = 0;
    p_sys->i_bitrate    = 0;
    p_sys->i_seq        = 0;

    p_sys->b_write_header = VLC_TRUE;
    p_sys->i_track = 1;
    p_sys->i_packet_size = 4096;
    p_sys->i_packet_count= 0;
    /* generate a random fid */
    srand( mdate() & 0xffffffff );
    p_sys->fid.v1 = 0xbabac001;
    p_sys->fid.v2 = ( (uint64_t)rand() << 16 ) / RAND_MAX;
    p_sys->fid.v3 = ( (uint64_t)rand() << 16 ) / RAND_MAX;
    for( i = 0; i < 8; i++ )
    {
        p_sys->fid.v4[i] = ( (uint64_t)rand() << 8 ) / RAND_MAX;
    }
    /* meta data */
    p_sys->psz_title    = sout_cfg_find_value( p_mux->p_cfg, "title" );
    p_sys->psz_author   = sout_cfg_find_value( p_mux->p_cfg, "author" );
    p_sys->psz_copyright= sout_cfg_find_value( p_mux->p_cfg, "copyright" );
    p_sys->psz_comment  = sout_cfg_find_value( p_mux->p_cfg, "comment" );
    p_sys->psz_rating   = sout_cfg_find_value( p_mux->p_cfg, "rating" );
    if( p_sys->psz_title == NULL )
    {
        p_sys->psz_title = "";
    }
    if( p_sys->psz_author == NULL )
    {
        p_sys->psz_author = "";
    }
    if( p_sys->psz_copyright == NULL )
    {
        p_sys->psz_copyright = "";
    }
    if( p_sys->psz_comment == NULL )
    {
        p_sys->psz_comment = "";
    }
    if( p_sys->psz_rating == NULL )
    {
        p_sys->psz_rating = "";
    }
    msg_Dbg( p_mux,
             "meta data: title='%s' author='%s' copyright='%s' comment='%s' rating='%s'",
             p_sys->psz_title, p_sys->psz_author, p_sys->psz_copyright,
             p_sys->psz_comment, p_sys->psz_rating );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t     *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    sout_buffer_t  *out;
    int i;

    msg_Dbg( p_mux, "Asf muxer closed" );

    if( ( out = asf_stream_end_create( p_mux ) ) )
    {
        sout_AccessOutWrite( p_mux->p_access, out );
    }

    /* rewrite header */
    if( !sout_AccessOutSeek( p_mux->p_access, 0 ) )
    {
        out = asf_header_create( p_mux, VLC_FALSE );
        sout_AccessOutWrite( p_mux->p_access, out );
    }

    for( i = 1; i < p_sys->i_track; i++ )
    {
        free( p_sys->track[i].p_extra );
    }
    free( p_sys );
}

/*****************************************************************************
 * Capability:
 *****************************************************************************/
static int Capability( sout_mux_t *p_mux, int i_query,
                       void *p_args, void *p_answer )
{
   switch( i_query )
   {
        case SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME:
            *(vlc_bool_t*)p_answer = VLC_FALSE;
            return( SOUT_MUX_CAP_ERR_OK );
        default:
            return( SOUT_MUX_CAP_ERR_UNIMPLEMENTED );
   }
}

/*****************************************************************************
 * AddStream:
 *****************************************************************************/
static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t   *p_sys = p_mux->p_sys;
    asf_track_t      *tk;
    bo_t             bo;

    msg_Dbg( p_mux, "adding input" );
    if( p_sys->i_track > 127 )
    {
        msg_Dbg( p_mux, "cannot add this track (too much track)" );
        return VLC_EGENERIC;
    }

    tk = p_input->p_sys = &p_sys->track[p_sys->i_track];
    tk->i_id  = p_sys->i_track;
    tk->i_cat = p_input->p_fmt->i_cat;
    tk->i_sequence = 0;

    switch( tk->i_cat )
    {
        case AUDIO_ES:
        {
            int      i_blockalign = p_input->p_fmt->audio.i_blockalign;
            int      i_bitspersample = 0;
            int      i_extra = 0;

            switch( p_input->p_fmt->i_codec )
            {
                case VLC_FOURCC( 'a', '5', '2', ' ' ):
                    tk->i_tag = WAVE_FORMAT_A52;
                    tk->psz_name = "A/52";
                    break;
                case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
#if 1
                    tk->psz_name = "MPEG Audio Layer 3";
                    tk->i_tag = WAVE_FORMAT_MPEGLAYER3;
                    i_blockalign = 1;
                    i_extra = 12;
                    break;
#else
                    tk->psz_name = "MPEG Audio Layer 1/2";
                    tk->i_tag = WAVE_FORMAT_MPEG;
                    i_blockalign = 1;
                    i_extra = 22;
                    break;
#endif
                case VLC_FOURCC( 'w', 'm', 'a', '1' ):
                    tk->psz_name = "Windows Media Audio 1";
                    tk->i_tag = WAVE_FORMAT_WMA1;
                    break;
                case VLC_FOURCC( 'w', 'm', 'a', '2' ):
                    tk->psz_name = "Windows Media Audio 2";
                    tk->i_tag = WAVE_FORMAT_WMA2;
                    break;
                case VLC_FOURCC( 'w', 'm', 'a', '3' ):
                    tk->psz_name = "Windows Media Audio 3";
                    tk->i_tag = WAVE_FORMAT_WMA3;
                    break;
                    /* raw codec */
                case VLC_FOURCC( 'u', '8', ' ', ' ' ):
                    tk->psz_name = "Raw audio 8bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 8;
                    break;
                case VLC_FOURCC( 's', '1', '6', 'l' ):
                    tk->psz_name = "Raw audio 16bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= 2 * p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 16;
                    break;
                case VLC_FOURCC( 's', '2', '4', 'l' ):
                    tk->psz_name = "Raw audio 24bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= 3 * p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 24;
                    break;
                case VLC_FOURCC( 's', '3', '2', 'l' ):
                    tk->psz_name = "Raw audio 32bits";
                    tk->i_tag = WAVE_FORMAT_PCM;
                    i_blockalign= 4 * p_input->p_fmt->audio.i_channels;
                    i_bitspersample = 32;
                    break;
                default:
                    return VLC_EGENERIC;
            }


            tk->i_extra = sizeof( WAVEFORMATEX ) +
                          p_input->p_fmt->i_extra + i_extra;
            tk->p_extra = malloc( tk->i_extra );
            bo_init( &bo, tk->p_extra, tk->i_extra );
            bo_addle_u16( &bo, tk->i_tag );
            bo_addle_u16( &bo, p_input->p_fmt->audio.i_channels );
            bo_addle_u32( &bo, p_input->p_fmt->audio.i_rate );
            bo_addle_u32( &bo, p_input->p_fmt->i_bitrate / 8 );
            bo_addle_u16( &bo, i_blockalign );
            bo_addle_u16( &bo, i_bitspersample );
            if( p_input->p_fmt->i_extra > 0 )
            {
                bo_addle_u16( &bo, p_input->p_fmt->i_extra );
                bo_add_mem  ( &bo, p_input->p_fmt->p_extra,
                              p_input->p_fmt->i_extra );
            }
            else
            {
                bo_addle_u16( &bo, i_extra );
                if( tk->i_tag == WAVE_FORMAT_MPEGLAYER3 )
                {
                    msg_Dbg( p_mux, "adding mp3 header" );
                    bo_addle_u16( &bo, 1 );     /* wId */
                    bo_addle_u32( &bo, 2 );     /* fdwFlags */
                    bo_addle_u16( &bo, 1152 );  /* nBlockSize */
                    bo_addle_u16( &bo, 1 );     /* nFramesPerBlock */
                    bo_addle_u16( &bo, 1393 );  /* nCodecDelay */
                }
                else if( tk->i_tag == WAVE_FORMAT_MPEG )
                {
                    msg_Dbg( p_mux, "adding mp2 header" );
                    bo_addle_u16( &bo, 2 );     /* fwHeadLayer */
                    bo_addle_u32( &bo, p_input->p_fmt->i_bitrate );
                    bo_addle_u16( &bo, p_input->p_fmt->audio.i_channels == 2 ?1:8 );
                    bo_addle_u16( &bo, 0 );     /* fwHeadModeExt */
                    bo_addle_u16( &bo, 1 );     /* wHeadEmphasis */
                    bo_addle_u16( &bo, 16 );    /* fwHeadFlags */
                    bo_addle_u32( &bo, 0 );     /* dwPTSLow */
                    bo_addle_u32( &bo, 0 );     /* dwPTSHigh */
                }
            }

            if( p_input->p_fmt->i_bitrate > 24000 )
            {
                p_sys->i_bitrate += p_input->p_fmt->i_bitrate;
            }
            else
            {
                p_sys->i_bitrate += 512000;
            }
            break;
        }
        case VIDEO_ES:
        {
            tk->i_extra = 11 + sizeof( BITMAPINFOHEADER ) +
                          p_input->p_fmt->i_extra;
            tk->p_extra = malloc( tk->i_extra );
            bo_init( &bo, tk->p_extra, tk->i_extra );
            bo_addle_u32( &bo, p_input->p_fmt->video.i_width );
            bo_addle_u32( &bo, p_input->p_fmt->video.i_height );
            bo_add_u8   ( &bo, 0x02 );  /* flags */
            bo_addle_u16( &bo, sizeof( BITMAPINFOHEADER ) +
                               p_input->p_fmt->i_extra );
            bo_addle_u32( &bo, sizeof( BITMAPINFOHEADER ) +
                               p_input->p_fmt->i_extra );
            bo_addle_u32( &bo, p_input->p_fmt->video.i_width );
            bo_addle_u32( &bo, p_input->p_fmt->video.i_height );
            bo_addle_u16( &bo, 1 );
            bo_addle_u16( &bo, 24 );
            if( p_input->p_fmt->i_codec == VLC_FOURCC('m','p','4','v') )
            {
                tk->psz_name = "MPEG-4 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', '4', 'S' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('D','I','V','3') )
            {
                tk->psz_name = "MSMPEG-4 V3 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', '4', '3' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('D','I','V','2') )
            {
                tk->psz_name = "MSMPEG-4 V2 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', '4', '2' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('D','I','V','1') )
            {
                tk->psz_name = "MSMPEG-4 V1 Video";
                tk->i_fourcc = VLC_FOURCC( 'M', 'P', 'G', '4' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('W','M','V','1') )
            {
                tk->psz_name = "Windows Media Video 1";
                tk->i_fourcc = VLC_FOURCC( 'W', 'M', 'V', '1' );
            }
            else if( p_input->p_fmt->i_codec == VLC_FOURCC('W','M','V','2') )
            {
                tk->psz_name = "Windows Media Video 2";
                tk->i_fourcc = VLC_FOURCC( 'W', 'M', 'V', '2' );
            }
            else
            {
                tk->psz_name = "Unknow Video";
                tk->i_fourcc = p_input->p_fmt->i_codec;
            }
            bo_add_mem( &bo, (uint8_t*)&tk->i_fourcc, 4 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            bo_addle_u32( &bo, 0 );
            if( p_input->p_fmt->i_extra > 0 )
            {
                bo_add_mem  ( &bo, p_input->p_fmt->p_extra,
                              p_input->p_fmt->i_extra );
            }

            if( p_input->p_fmt->i_bitrate > 50000 )
            {
                p_sys->i_bitrate += p_input->p_fmt->i_bitrate;
            }
            else
            {
                p_sys->i_bitrate += 1000000;
            }
            break;
        }
        default:
            msg_Err(p_mux, "unhandled track type" );
            return VLC_EGENERIC;
    }

    p_sys->i_track++;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DelStream:
 *****************************************************************************/
static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    msg_Dbg( p_mux, "removing input" );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Mux:
 *****************************************************************************/
static int Mux      ( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    if( p_sys->b_write_header )
    {
        sout_buffer_t *out = asf_header_create( p_mux, VLC_TRUE );

        out->i_flags |= SOUT_BUFFER_FLAGS_HEADER;
        sout_AccessOutWrite( p_mux->p_access, out );

        p_sys->b_write_header = VLC_FALSE;
    }

    for( ;; )
    {
        sout_input_t  *p_input;
        asf_track_t   *tk;
        int           i_stream;
        mtime_t       i_dts;
        sout_buffer_t *data;
        sout_buffer_t *pk;

        if( MuxGetStream( p_mux, &i_stream, &i_dts ) )
        {
            /* not enough data */
            return VLC_SUCCESS;
        }

        if( p_sys->i_dts_first < 0 )
        {
            p_sys->i_dts_first = i_dts;
        }
        if( p_sys->i_dts_last < i_dts )
        {
            p_sys->i_dts_last = i_dts;
        }

        p_input = p_mux->pp_inputs[i_stream];
        tk      = (asf_track_t*)p_input->p_sys;

        data = sout_FifoGet( p_input->p_fifo );

        if( ( pk = asf_packet_create( p_mux, tk, data ) ) )
        {
            sout_AccessOutWrite( p_mux->p_access, pk );
        }
    }

    return VLC_SUCCESS;
}


static int MuxGetStream( sout_mux_t *p_mux,
                         int        *pi_stream,
                         mtime_t    *pi_dts )
{
    mtime_t i_dts;
    int     i_stream;
    int     i;

    for( i = 0, i_dts = 0, i_stream = -1; i < p_mux->i_nb_inputs; i++ )
    {
        sout_input_t  *p_input = p_mux->pp_inputs[i];
        sout_buffer_t *p_data;

        if( p_input->p_fifo->i_depth <= 0 )
        {
            if( p_input->p_fmt->i_cat == AUDIO_ES ||
                p_input->p_fmt->i_cat == VIDEO_ES )
            {
                /* We need that audio+video fifo contain at least 1 packet */
                return VLC_EGENERIC;
            }
            /* SPU */
            continue;
        }

        p_data = sout_FifoShow( p_input->p_fifo );
        if( i_stream == -1 ||
            p_data->i_dts < i_dts )
        {
            i_stream = i;
            i_dts    = p_data->i_dts;
        }
    }

    *pi_stream = i_stream;
    *pi_dts = i_dts;

    return VLC_SUCCESS;
}

/****************************************************************************
 * Asf header construction
 ****************************************************************************/

/****************************************************************************
 * Buffer out
 ****************************************************************************/
static void bo_init( bo_t *p_bo, uint8_t *p_buffer, int i_size )
{
    p_bo->i_buffer_size = i_size;
    p_bo->i_buffer = 0;
    p_bo->p_buffer = p_buffer;
}
static void bo_add_u8( bo_t *p_bo, uint8_t i )
{
    if( p_bo->i_buffer < p_bo->i_buffer_size )
    {
        p_bo->p_buffer[p_bo->i_buffer] = i;
    }
    p_bo->i_buffer++;
}
static void bo_addle_u16( bo_t *p_bo, uint16_t i )
{
    bo_add_u8( p_bo, i &0xff );
    bo_add_u8( p_bo, ( ( i >> 8) &0xff ) );
}
static void bo_addle_u32( bo_t *p_bo, uint32_t i )
{
    bo_addle_u16( p_bo, i &0xffff );
    bo_addle_u16( p_bo, ( ( i >> 16) &0xffff ) );
}
static void bo_addle_u64( bo_t *p_bo, uint64_t i )
{
    bo_addle_u32( p_bo, i &0xffffffff );
    bo_addle_u32( p_bo, ( ( i >> 32) &0xffffffff ) );
}

static void bo_add_mem( bo_t *p_bo, uint8_t *p_mem, int i_size )
{
    int i_copy = __MIN( i_size, p_bo->i_buffer_size - p_bo->i_buffer );

    if( i_copy > 0 )
    {
        memcpy( &p_bo->p_buffer[p_bo->i_buffer],
                p_mem,
                i_copy );
    }
    p_bo->i_buffer += i_size;
}

static void bo_addle_str16( bo_t *bo, char *str )
{
    bo_addle_u16( bo, strlen( str ) + 1 );
    for( ;; )
    {
        uint16_t c;

        c = (uint8_t)*str++;
        bo_addle_u16( bo, c );
        if( c == '\0' )
        {
            break;
        }
    }
}

static void bo_addle_str16_nosize( bo_t *bo, char *str )
{
    for( ;; )
    {
        uint16_t c;

        c = (uint8_t)*str++;
        bo_addle_u16( bo, c );
        if( c == '\0' )
        {
            break;
        }
    }
}

/****************************************************************************
 * guid
 ****************************************************************************/
static void bo_add_guid( bo_t *p_bo, const guid_t *id )
{
    int i;
    bo_addle_u32( p_bo, id->v1 );
    bo_addle_u16( p_bo, id->v2 );
    bo_addle_u16( p_bo, id->v3 );
    for( i = 0; i < 8; i++ )
    {
        bo_add_u8( p_bo, id->v4[i] );
    }
}

static const guid_t asf_object_header_guid =
{
    0x75B22630,
    0x668E,
    0x11CF,
    { 0xA6,0xD9, 0x00,0xAA,0x00,0x62,0xCE,0x6C }
};
static const guid_t asf_object_data_guid =
{
    0x75B22636,
    0x668E,
    0x11CF,
    { 0xA6,0xD9, 0x00,0xAA,0x00,0x62,0xCE,0x6C }
};

static const guid_t asf_object_file_properties_guid =
{
    0x8cabdca1,
    0xa947,
    0x11cf,
    { 0x8e,0xe4, 0x00,0xC0,0x0C,0x20,0x53,0x65 }

};
static const guid_t asf_object_stream_properties_guid =
{
    0xB7DC0791,
    0xA9B7,
    0x11CF,
    { 0x8E,0xE6, 0x00,0xC0,0x0C,0x20,0x53,0x65 }

};
static const guid_t asf_object_header_extention_guid =
{
   0x5FBF03B5,
   0xA92E,
   0x11CF,
   { 0x8E,0xE3, 0x00,0xC0,0x0C,0x20,0x53,0x65 }
};

static const guid_t asf_object_stream_type_audio =
{
    0xF8699E40,
    0x5B4D,
    0x11CF,
    { 0xA8,0xFD, 0x00,0x80,0x5F,0x5C,0x44,0x2B }
};

static const guid_t asf_object_stream_type_video =
{
    0xbc19efc0,
    0x5B4D,
    0x11CF,
    { 0xA8,0xFD, 0x00,0x80,0x5F,0x5C,0x44,0x2B }
};

static const guid_t asf_guid_audio_conceal_none =
{
    0x20FB5700,
    0x5B55,
    0x11CF,
    { 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B }
};
static const guid_t asf_guid_video_conceal_none =
{
    0x20FB5700,
    0x5B55,
    0x11CF,
    { 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B }
};
static const guid_t asf_guid_reserved_1 =
{
    0xABD3D211,
    0xA9BA,
    0x11cf,
    { 0x8E, 0xE6,0x00, 0xC0, 0x0C ,0x20, 0x53, 0x65 }
};
static const guid_t asf_object_codec_comment_guid =
{
    0x86D15240,
    0x311D,
    0x11D0,
    { 0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6 }
};
static const guid_t asf_object_codec_comment_reserved_guid =
{
    0x86D15241,
    0x311D,
    0x11D0,
    { 0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6 }
};
static const guid_t asf_object_content_description_guid =
{
    0x75B22633,
    0x668E,
    0x11CF,
    { 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c }
};

static void asf_chunk_add( bo_t *bo,
                           int i_type, int i_len, int i_flags, int i_seq )
{
    bo_addle_u16( bo, i_type );
    bo_addle_u16( bo, i_len + 8 );
    bo_addle_u32( bo, i_seq );
    bo_addle_u16( bo, i_flags );
    bo_addle_u16( bo, i_len + 8 );
}

static sout_buffer_t *asf_header_create( sout_mux_t *p_mux,
                                         vlc_bool_t b_broadcast )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    asf_track_t    *tk;

    mtime_t        i_duration = 0;
    int i_size;
    int i_ci_size;
    int i_cd_size = 0;
    sout_buffer_t *out;
    bo_t          bo;
    int           i;

    if( p_sys->i_dts_first > 0 )
    {
        i_duration = p_sys->i_dts_last - p_sys->i_dts_first;
        if( i_duration < 0 )
        {
            i_duration = 0;
        }
    }

    /* calculate header size */
    i_size = 30 + 104 + 46;
    i_ci_size = 44;
    for( i = 1; i < p_sys->i_track; i++ )
    {
        i_size += 78 + p_sys->track[i].i_extra;
        i_ci_size += 8 + 2 * strlen( p_sys->track[i].psz_name );
        if( p_sys->track[i].i_cat == AUDIO_ES )
        {
            i_ci_size += 4;
        }
        else if( p_sys->track[i].i_cat == VIDEO_ES )
        {
            i_ci_size += 6;
        }
    }
    if( *p_sys->psz_title || *p_sys->psz_author || *p_sys->psz_copyright ||
        *p_sys->psz_comment || *p_sys->psz_rating )
    {
        i_cd_size = 34 + 2 * ( strlen( p_sys->psz_title ) + 1 +
                             strlen( p_sys->psz_author ) + 1 +
                             strlen( p_sys->psz_copyright ) + 1 +
                             strlen( p_sys->psz_comment ) + 1 +
                             strlen( p_sys->psz_rating ) + 1 );
    }

    i_size += i_ci_size + i_cd_size;

    if( p_sys->b_asf_http )
    {
        out = sout_BufferNew( p_mux->p_sout, i_size + 50 + 12 );
        bo_init( &bo, out->p_buffer, i_size + 50 + 12 );
        asf_chunk_add( &bo, 0x4824, i_size + 50, 0xc00, p_sys->i_seq++ );
    }
    else
    {
        out = sout_BufferNew( p_mux->p_sout, i_size + 50 );
        bo_init( &bo, out->p_buffer, i_size + 50 );
    }
    /* header object */
    bo_add_guid ( &bo, &asf_object_header_guid );
    bo_addle_u64( &bo, i_size );
    bo_addle_u32( &bo, 2 + p_sys->i_track - 1 );
    bo_add_u8   ( &bo, 1 );
    bo_add_u8   ( &bo, 2 );

    /* sub object */

    /* file properties */
    bo_add_guid ( &bo, &asf_object_file_properties_guid );
    bo_addle_u64( &bo, 104 );
    bo_add_guid ( &bo, &p_sys->fid );
    bo_addle_u64( &bo, i_size + 50 + p_sys->i_packet_count *
                                p_sys->i_packet_size ); /* file size */
    bo_addle_u64( &bo, 0 );                 /* creation date */
    bo_addle_u64( &bo, b_broadcast ? 0xffffffffLL : p_sys->i_packet_count );
    bo_addle_u64( &bo, i_duration * 10 );   /* play duration (100ns) */
    bo_addle_u64( &bo, i_duration * 10 );   /* send duration (100ns) */
    bo_addle_u64( &bo, 3000 );              /* preroll duration (ms) */
    bo_addle_u32( &bo, b_broadcast ? 0x01 : 0x00);      /* flags */
    bo_addle_u32( &bo, p_sys->i_packet_size );  /* packet size min */
    bo_addle_u32( &bo, p_sys->i_packet_size );  /* packet size max */
    bo_addle_u32( &bo, p_sys->i_bitrate );      /* maxbitrate */

    /* header extention */
    bo_add_guid ( &bo, &asf_object_header_extention_guid );
    bo_addle_u64( &bo, 46 );
    bo_add_guid ( &bo, &asf_guid_reserved_1 );
    bo_addle_u16( &bo, 6 );
    bo_addle_u32( &bo, 0 );

    /* content description header */
    if( i_cd_size > 0 )
    {
        bo_add_guid ( &bo, &asf_object_content_description_guid );
        bo_addle_u64( &bo, i_cd_size );
        bo_addle_u16( &bo, 2 * strlen( p_sys->psz_title ) + 2 );
        bo_addle_u16( &bo, 2 * strlen( p_sys->psz_author ) + 2 );
        bo_addle_u16( &bo, 2 * strlen( p_sys->psz_copyright ) + 2 );
        bo_addle_u16( &bo, 2 * strlen( p_sys->psz_comment ) + 2 );
        bo_addle_u16( &bo, 2 * strlen( p_sys->psz_rating ) + 2 );

        bo_addle_str16_nosize( &bo, p_sys->psz_title );
        bo_addle_str16_nosize( &bo, p_sys->psz_author );
        bo_addle_str16_nosize( &bo, p_sys->psz_copyright );
        bo_addle_str16_nosize( &bo, p_sys->psz_comment );
        bo_addle_str16_nosize( &bo, p_sys->psz_rating );
    }

    /* stream properties */
    for( i = 1; i < p_sys->i_track; i++ )
    {
        tk = &p_sys->track[i];

        bo_add_guid ( &bo, &asf_object_stream_properties_guid );
        bo_addle_u64( &bo, 78 + tk->i_extra );
        if( tk->i_cat == AUDIO_ES )
        {
            bo_add_guid( &bo, &asf_object_stream_type_audio );
            bo_add_guid( &bo, &asf_guid_audio_conceal_none );
        }
        else if( tk->i_cat == VIDEO_ES )
        {
            bo_add_guid( &bo, &asf_object_stream_type_video );
            bo_add_guid( &bo, &asf_guid_video_conceal_none );
        }
        bo_addle_u64( &bo, 0 );         /* time offset */
        bo_addle_u32( &bo, tk->i_extra );
        bo_addle_u32( &bo, 0 );         /* 0 */
        bo_addle_u16( &bo, tk->i_id );  /* stream number */
        bo_addle_u32( &bo, 0 );
        bo_add_mem  ( &bo, tk->p_extra, tk->i_extra );
    }

    /* Codec Infos */
    bo_add_guid ( &bo, &asf_object_codec_comment_guid );
    bo_addle_u64( &bo, i_ci_size );
    bo_add_guid ( &bo, &asf_object_codec_comment_reserved_guid );
    bo_addle_u32( &bo, p_sys->i_track - 1 );
    for( i = 1; i < p_sys->i_track; i++ )
    {
        tk = &p_sys->track[i];

        bo_addle_u16( &bo, tk->i_id );
        bo_addle_str16( &bo, tk->psz_name );
        bo_addle_u16( &bo, 0 );
        if( tk->i_cat == AUDIO_ES )
        {
            bo_addle_u16( &bo, 2 );
            bo_addle_u16( &bo, tk->i_tag );
        }
        else if( tk->i_cat == VIDEO_ES )
        {
            bo_addle_u16( &bo, 4 );
            bo_add_mem  ( &bo, (uint8_t*)&tk->i_fourcc, 4 );

        }
    }

    /* data object */
    bo_add_guid ( &bo, &asf_object_data_guid );
    bo_addle_u64( &bo, 50 + p_sys->i_packet_count * p_sys->i_packet_size );
    bo_add_guid ( &bo, &p_sys->fid );
    bo_addle_u64( &bo, p_sys->i_packet_count );
    bo_addle_u16( &bo, 0x101 );

    return out;
}

/****************************************************************************
 *
 ****************************************************************************/
static sout_buffer_t *asf_packet_create( sout_mux_t *p_mux,
                                         asf_track_t *tk, sout_buffer_t *data )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    int     i_data = data->i_size;
    int     i_pos  = 0;
    uint8_t *p_data= data->p_buffer;
    sout_buffer_t *first = NULL, **last = &first;
    int     i_preheader = p_sys->b_asf_http ? 12 : 0;

    while( i_pos < i_data )
    {
        bo_t          bo;
        int           i_payload;

        if( p_sys->pk == NULL )
        {
            p_sys->pk = sout_BufferNew( p_mux->p_sout,
                                        p_sys->i_packet_size + i_preheader);
            /* reserve 14 bytes for the packet header */
            p_sys->i_pk_used = 14 + i_preheader;
            p_sys->i_pk_frame = 0;
            p_sys->i_pk_dts = data->i_dts;
        }


        bo_init( &bo, &p_sys->pk->p_buffer[p_sys->i_pk_used],
                      p_sys->i_packet_size - p_sys->i_pk_used );

        /* add payload (header size = 17) */
        i_payload = __MIN( i_data - i_pos,
                           p_sys->i_packet_size - p_sys->i_pk_used - 17 );
        bo_add_u8   ( &bo, 0x80 | tk->i_id );
        bo_add_u8   ( &bo, tk->i_sequence );
        bo_addle_u32( &bo, i_pos );
        bo_add_u8   ( &bo, 0x08 );  /* flags */
        bo_addle_u32( &bo, i_data );
        bo_addle_u32( &bo, ( data->i_dts - p_sys->i_dts_first )/ 1000 );
        bo_addle_u16( &bo, i_payload );
        bo_add_mem  ( &bo, &p_data[i_pos], i_payload );
        i_pos += i_payload;
        p_sys->i_pk_used += 17 + i_payload;

        p_sys->i_pk_frame++;

        if( p_sys->i_pk_used + 17 >= p_sys->i_packet_size )
        {
            /* not enough data for another payload, flush the packet */
            int i_pad = p_sys->i_packet_size - p_sys->i_pk_used;

            bo_init( &bo, p_sys->pk->p_buffer, 14 + i_preheader );

            if( p_sys->b_asf_http )
            {
                asf_chunk_add( &bo, 0x4424,
                               p_sys->i_packet_size, 0x00, p_sys->i_seq++);
            }
            bo_add_u8   ( &bo, 0x82 );
            bo_addle_u16( &bo, 0 );
            bo_add_u8( &bo, 0x11 );
            bo_add_u8( &bo, 0x5d );
            bo_addle_u16( &bo, i_pad );
            bo_addle_u32( &bo, ( p_sys->i_pk_dts - p_sys->i_dts_first )/ 1000 );
            bo_addle_u16( &bo, 0 * data->i_length / 1000 );
            bo_add_u8( &bo, 0x80 | p_sys->i_pk_frame );

            /* append the packet */
            *last = p_sys->pk;
            last  = &p_sys->pk->p_next;

            p_sys->pk = NULL;

            p_sys->i_packet_count++;
        }
    }

    tk->i_sequence++;
    sout_BufferDelete( p_mux->p_sout, data );

    return first;
}

static sout_buffer_t *asf_stream_end_create( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    sout_buffer_t *out = NULL;
    bo_t          bo;

    if( p_sys->b_asf_http )
    {
        out = sout_BufferNew( p_mux->p_sout, 12 );
        bo_init( &bo, out->p_buffer, 12 );
        asf_chunk_add( &bo, 0x4524, 0, 0x00, p_sys->i_seq++ );
    }
    return out;
}
