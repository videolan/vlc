static int i_global = 0;
/*****************************************************************************
 * avi.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
/* TODO: add OpenDML write support */

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
    set_description( _("AVI muxer") );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_MUX );
    set_capability( "sout mux", 5 );
    add_shortcut( "avi" );
    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

typedef struct avi_stream_s
{
    int i_cat;

    char fcc[4];

    mtime_t i_duration;       // in µs

    int     i_frames;        // total frame count
    int64_t i_totalsize;    // total stream size

    float   f_fps;
    int     i_bitrate;

    BITMAPINFOHEADER    *p_bih;
    WAVEFORMATEX        *p_wf;

} avi_stream_t;

typedef struct avi_idx1_entry_s
{
    char     fcc[4];
    uint32_t i_flags;
    uint32_t i_pos;
    uint32_t i_length;

} avi_idx1_entry_t;

typedef struct avi_idx1_s
{
    unsigned int i_entry_count;
    unsigned int i_entry_max;

    avi_idx1_entry_t *entry;
} avi_idx1_t;

struct sout_mux_sys_t
{
    vlc_bool_t b_write_header;

    int i_streams;
    int i_stream_video;

    off_t i_movi_size;
    avi_stream_t stream[100];

    avi_idx1_t idx1;
    off_t i_idx1_size;

};

// FIXME FIXME
#define HDR_SIZE 4096

/* Flags in avih */
#define AVIF_HASINDEX       0x00000010  // Index at end of file?
#define AVIF_ISINTERLEAVED  0x00000100
#define AVIF_TRUSTCKTYPE    0x00000800  // Use CKType to find key frames?

/* Flags for index */
#define AVIIF_KEYFRAME      0x00000010L /* this frame is a key frame.*/


static block_t *avi_HeaderCreateRIFF( sout_mux_t * );
static block_t *avi_HeaderCreateidx1( sout_mux_t * );

static void SetFCC( uint8_t *p, char *fcc )
{
    memcpy( p, fcc, 4 );
}

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys = p_mux->p_sys;

    msg_Dbg( p_mux, "AVI muxer opened" );

    p_sys = malloc( sizeof( sout_mux_sys_t ) );
    p_sys->i_streams = 0;
    p_sys->i_stream_video = -1;
    p_sys->i_movi_size = 0;

    p_sys->idx1.i_entry_count = 0;
    p_sys->idx1.i_entry_max = 10000;
    p_sys->idx1.entry = calloc( p_sys->idx1.i_entry_max,
                                sizeof( avi_idx1_entry_t ) );
    p_sys->b_write_header = VLC_TRUE;


    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->p_sys        = p_sys;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_mux_t      *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys = p_mux->p_sys;

    block_t       *p_hdr, *p_idx1;
    int                 i_stream;

    msg_Dbg( p_mux, "AVI muxer closed" );

    /* first create idx1 chunk (write at the end of the stream */
    p_idx1 = avi_HeaderCreateidx1( p_mux );
    p_sys->i_idx1_size = p_idx1->i_buffer;
    sout_AccessOutWrite( p_mux->p_access, p_idx1 );

    /* calculate some value for headers creations */
    for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
    {
        avi_stream_t *p_stream;

        p_stream = &p_sys->stream[i_stream];

        p_stream->f_fps = 25;
        if( p_stream->i_duration > 0 )
        {
            p_stream->f_fps = (float)p_stream->i_frames /
                              ( (float)p_stream->i_duration /
                                (float)1000000 );
        }
        p_stream->i_bitrate = 128 * 1024;
        if( p_stream->i_duration > 0 )
        {
            p_stream->i_bitrate =
                8 * (uint64_t)1000000 *
                    (uint64_t)p_stream->i_totalsize /
                    (uint64_t)p_stream->i_duration;
        }
        msg_Info( p_mux, "stream[%d] duration:"I64Fd" totalsize:"I64Fd
                  " frames:%d fps:%f kb/s:%d",
                  i_stream,
                  (int64_t)p_stream->i_duration / (int64_t)1000000,
                  p_stream->i_totalsize,
                  p_stream->i_frames,
                  p_stream->f_fps, p_stream->i_bitrate/1024 );
    }

    p_hdr = avi_HeaderCreateRIFF( p_mux );
    sout_AccessOutSeek( p_mux->p_access, 0 );
    sout_AccessOutWrite( p_mux->p_access, p_hdr );
}

static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    vlc_bool_t *pb_bool;
    char **ppsz;

   switch( i_query )
   {
       case MUX_CAN_ADD_STREAM_WHILE_MUXING:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_FALSE;
           return VLC_SUCCESS;

       case MUX_GET_ADD_STREAM_WAIT:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_TRUE;
           return VLC_SUCCESS;

       case MUX_GET_MIME:
           ppsz = (char**)va_arg( args, char ** );
           *ppsz = strdup( "video/avi" );
           return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
   }
}

static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    avi_stream_t    *p_stream;

    if( p_sys->i_streams >= 100 )
    {
        msg_Err( p_mux, "too many streams" );
        return( -1 );
    }

    msg_Dbg( p_mux, "adding input" );
    p_input->p_sys = malloc( sizeof( int ) );

    *((int*)p_input->p_sys) = p_sys->i_streams;
    p_stream = &p_sys->stream[p_sys->i_streams];

    switch( p_input->p_fmt->i_cat )
    {
        case AUDIO_ES:
            p_stream->i_cat = AUDIO_ES;
            p_stream->fcc[0] = '0' + p_sys->i_streams / 10;
            p_stream->fcc[1] = '0' + p_sys->i_streams % 10;
            p_stream->fcc[2] = 'w';
            p_stream->fcc[3] = 'b';

            p_stream->p_bih = NULL;

            p_stream->p_wf  = malloc( sizeof( WAVEFORMATEX ) +
                                      p_input->p_fmt->i_extra );
#define p_wf p_stream->p_wf
            p_wf->cbSize = p_input->p_fmt->i_extra;
            if( p_wf->cbSize > 0 )
            {
                memcpy( &p_wf[1],
                        p_input->p_fmt->p_extra,
                        p_input->p_fmt->i_extra );
            }
            p_wf->nChannels      = p_input->p_fmt->audio.i_channels;
            p_wf->nSamplesPerSec = p_input->p_fmt->audio.i_rate;
            p_wf->nBlockAlign    = p_input->p_fmt->audio.i_blockalign;
            p_wf->nAvgBytesPerSec= p_input->p_fmt->i_bitrate / 8;
            p_wf->wBitsPerSample = 0;

            switch( p_input->p_fmt->i_codec )
            {
                case VLC_FOURCC( 'a', '5', '2', ' ' ):
                    p_wf->wFormatTag = WAVE_FORMAT_A52;
                    break;
                case VLC_FOURCC( 'm', 'p', 'g', 'a' ):
                    p_wf->wFormatTag = WAVE_FORMAT_MPEGLAYER3;
                    break;
                case VLC_FOURCC( 'w', 'm', 'a', '1' ):
                    p_wf->wFormatTag = WAVE_FORMAT_WMA1;
                    break;
                case VLC_FOURCC( 'w', 'm', 'a', ' ' ):
                case VLC_FOURCC( 'w', 'm', 'a', '2' ):
                    p_wf->wFormatTag = WAVE_FORMAT_WMA2;
                    break;
                case VLC_FOURCC( 'w', 'm', 'a', 'p' ):
                    p_wf->wFormatTag = WAVE_FORMAT_WMAP;
                    break;
                case VLC_FOURCC( 'w', 'm', 'a', 'l' ):
                    p_wf->wFormatTag = WAVE_FORMAT_WMAL;
                    break;
                    /* raw codec */
                case VLC_FOURCC( 'u', '8', ' ', ' ' ):
                    p_wf->wFormatTag = WAVE_FORMAT_PCM;
                    p_wf->nBlockAlign= p_wf->nChannels;
                    p_wf->wBitsPerSample = 8;
                    break;
                case VLC_FOURCC( 's', '1', '6', 'l' ):
                    p_wf->wFormatTag = WAVE_FORMAT_PCM;
                    p_wf->nBlockAlign= 2 * p_wf->nChannels;
                    p_wf->wBitsPerSample = 16;
                    break;
                case VLC_FOURCC( 's', '2', '4', 'l' ):
                    p_wf->wFormatTag = WAVE_FORMAT_PCM;
                    p_wf->nBlockAlign= 3 * p_wf->nChannels;
                    p_wf->wBitsPerSample = 24;
                    break;
                case VLC_FOURCC( 's', '3', '2', 'l' ):
                    p_wf->wFormatTag = WAVE_FORMAT_PCM;
                    p_wf->nBlockAlign= 4 * p_wf->nChannels;
                    p_wf->wBitsPerSample = 32;
                    break;
                default:
                    return VLC_EGENERIC;
            }
#undef p_wf
            break;
        case VIDEO_ES:
            p_stream->i_cat = VIDEO_ES;
            p_stream->fcc[0] = '0' + p_sys->i_streams / 10;
            p_stream->fcc[1] = '0' + p_sys->i_streams % 10;
            p_stream->fcc[2] = 'd';
            p_stream->fcc[3] = 'c';
            if( p_sys->i_stream_video < 0 )
            {
                p_sys->i_stream_video = p_sys->i_streams;
            }
            p_stream->p_wf  = NULL;
            p_stream->p_bih = malloc( sizeof( BITMAPINFOHEADER ) +
                                      p_input->p_fmt->i_extra );
#define p_bih p_stream->p_bih
            p_bih->biSize  = sizeof( BITMAPINFOHEADER ) +
                             p_input->p_fmt->i_extra;
            if( p_input->p_fmt->i_extra > 0 )
            {
                memcpy( &p_bih[1],
                        p_input->p_fmt->p_extra,
                        p_input->p_fmt->i_extra );
            }
            p_bih->biWidth = p_input->p_fmt->video.i_width;
            p_bih->biHeight= p_input->p_fmt->video.i_height;
            p_bih->biPlanes= 1;
            p_bih->biBitCount       = 24;
            p_bih->biSizeImage      = 0;
            p_bih->biXPelsPerMeter  = 0;
            p_bih->biYPelsPerMeter  = 0;
            p_bih->biClrUsed        = 0;
            p_bih->biClrImportant   = 0;
            switch( p_input->p_fmt->i_codec )
            {
                case VLC_FOURCC( 'm', 'p', '4', 'v' ):
                    p_bih->biCompression = VLC_FOURCC( 'X', 'V', 'I', 'D' );
                    break;
                default:
                    p_bih->biCompression = p_input->p_fmt->i_codec;
                    break;
            }
#undef p_bih
            break;
        default:
            return( VLC_EGENERIC );
    }
    p_stream->i_totalsize = 0;
    p_stream->i_frames    = 0;
    p_stream->i_duration  = 0;

    /* fixed later */
    p_stream->f_fps = 25;
    p_stream->i_bitrate = 128 * 1024;

    p_sys->i_streams++;
    return( VLC_SUCCESS );
}

static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{

    msg_Dbg( p_mux, "removing input" );

    free( p_input->p_sys );
    return( 0 );
}

static int Mux      ( sout_mux_t *p_mux )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    avi_stream_t    *p_stream;
    int i_stream;
    int i;
i_global++;

    if( p_sys->b_write_header )
    {
        block_t *p_hdr;

        msg_Dbg( p_mux, "writing header" );

        p_hdr = avi_HeaderCreateRIFF( p_mux );
        sout_AccessOutWrite( p_mux->p_access, p_hdr );

        p_sys->b_write_header = VLC_FALSE;
    }

    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        int i_count;
        block_fifo_t *p_fifo;

        i_stream = *((int*)p_mux->pp_inputs[i]->p_sys );
        p_stream = &p_sys->stream[i_stream];

        p_fifo = p_mux->pp_inputs[i]->p_fifo;
        i_count = p_fifo->i_depth;
        while( i_count > 1 )
        {
            avi_idx1_entry_t *p_idx;
            block_t *p_data;

            p_data = block_FifoGet( p_fifo );
            if( p_fifo->i_depth > 0 )
            {
                block_t *p_next = block_FifoShow( p_fifo );
                p_data->i_length = p_next->i_dts - p_data->i_dts;
            }

            p_stream->i_frames++;
            if( p_data->i_length < 0 )
            {
                msg_Warn( p_mux, "argg length < 0 l" );
                block_Release( p_data );
                i_count--;
                continue;
            }
            p_stream->i_duration  += p_data->i_length;
            p_stream->i_totalsize += p_data->i_buffer;

            /* add idx1 entry for this frame */
            p_idx = &p_sys->idx1.entry[p_sys->idx1.i_entry_count];
            memcpy( p_idx->fcc, p_stream->fcc, 4 );
            p_idx->i_flags = AVIIF_KEYFRAME;
            p_idx->i_pos   = p_sys->i_movi_size + 4;
            p_idx->i_length= p_data->i_buffer;
            p_sys->idx1.i_entry_count++;
            if( p_sys->idx1.i_entry_count >= p_sys->idx1.i_entry_max )
            {
                p_sys->idx1.i_entry_max += 10000;
                p_sys->idx1.entry = realloc( p_sys->idx1.entry,
                                             p_sys->idx1.i_entry_max * sizeof( avi_idx1_entry_t ) );
            }

            p_data = block_Realloc( p_data, 8, p_data->i_buffer );
            if( p_data )
            {
                SetFCC( p_data->p_buffer, p_stream->fcc );
                SetDWLE( p_data->p_buffer + 4, p_data->i_buffer - 8 );

                if( p_data->i_buffer & 0x01 )
                {
                    p_data = block_Realloc( p_data, 0, p_data->i_buffer + 1 );
                }
                p_sys->i_movi_size += p_data->i_buffer;
                sout_AccessOutWrite( p_mux->p_access, p_data );
            }

            i_count--;
        }

    }
    return( 0 );
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

typedef struct buffer_out_s
{
    int      i_buffer_size;
    int      i_buffer;
    uint8_t  *p_buffer;

} buffer_out_t;

static void bo_Init( buffer_out_t *p_bo, int i_size, uint8_t *p_buffer )
{
    p_bo->i_buffer_size = i_size;
    p_bo->i_buffer = 0;
    p_bo->p_buffer = p_buffer;
}
static void bo_AddByte( buffer_out_t *p_bo, uint8_t i )
{
    if( p_bo->i_buffer < p_bo->i_buffer_size )
    {
        p_bo->p_buffer[p_bo->i_buffer] = i;
    }
    p_bo->i_buffer++;
}
static void bo_AddWordLE( buffer_out_t *p_bo, uint16_t i )
{
    bo_AddByte( p_bo, i &0xff );
    bo_AddByte( p_bo, ( ( i >> 8) &0xff ) );
}
static void bo_AddWordBE( buffer_out_t *p_bo, uint16_t i )
{
    bo_AddByte( p_bo, ( ( i >> 8) &0xff ) );
    bo_AddByte( p_bo, i &0xff );
}
static void bo_AddDWordLE( buffer_out_t *p_bo, uint32_t i )
{
    bo_AddWordLE( p_bo, i &0xffff );
    bo_AddWordLE( p_bo, ( ( i >> 16) &0xffff ) );
}
static void bo_AddDWordBE( buffer_out_t *p_bo, uint32_t i )
{
    bo_AddWordBE( p_bo, ( ( i >> 16) &0xffff ) );
    bo_AddWordBE( p_bo, i &0xffff );
}
#if 0
static void bo_AddLWordLE( buffer_out_t *p_bo, uint64_t i )
{
    bo_AddDWordLE( p_bo, i &0xffffffff );
    bo_AddDWordLE( p_bo, ( ( i >> 32) &0xffffffff ) );
}
static void bo_AddLWordBE( buffer_out_t *p_bo, uint64_t i )
{
    bo_AddDWordBE( p_bo, ( ( i >> 32) &0xffffffff ) );
    bo_AddDWordBE( p_bo, i &0xffffffff );
}
#endif

static void bo_AddFCC( buffer_out_t *p_bo, char *fcc )
{
    bo_AddByte( p_bo, fcc[0] );
    bo_AddByte( p_bo, fcc[1] );
    bo_AddByte( p_bo, fcc[2] );
    bo_AddByte( p_bo, fcc[3] );
}

static void bo_AddMem( buffer_out_t *p_bo, int i_size, uint8_t *p_mem )
{
    int i;

    for( i = 0; i < i_size; i++ )
    {
        bo_AddByte( p_bo, p_mem[i] );
    }
}

/****************************************************************************
 ****************************************************************************
 **
 ** avi header generation
 **
 ****************************************************************************
 ****************************************************************************/
#define AVI_BOX_ENTER( fcc ) \
    buffer_out_t _bo_sav_; \
    bo_AddFCC( p_bo, fcc ); \
    _bo_sav_ = *p_bo; \
    bo_AddDWordLE( p_bo, 0 )

#define AVI_BOX_ENTER_LIST( fcc ) \
    AVI_BOX_ENTER( "LIST" ); \
    bo_AddFCC( p_bo, fcc )

#define AVI_BOX_EXIT( i_err ) \
    if( p_bo->i_buffer&0x01 ) bo_AddByte( p_bo, 0 ); \
    bo_AddDWordLE( &_bo_sav_, p_bo->i_buffer - _bo_sav_.i_buffer - 4 ); \
    return( i_err );

static int avi_HeaderAdd_avih( sout_mux_t *p_mux,
                               buffer_out_t *p_bo )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    avi_stream_t    *p_video = NULL;
    int         i_stream;
    uint32_t    i_microsecperframe;
    int         i_maxbytespersec;
    int         i_totalframes;
    AVI_BOX_ENTER( "avih" );

    if( p_sys->i_stream_video >= 0 )
    {
        p_video = &p_sys->stream[p_sys->i_stream_video];
        if( p_video->i_frames <= 0 )
        {
        //    p_video = NULL;
        }
    }

    if( p_video )
    {
        i_microsecperframe =
            (uint32_t)( (float)1000000 /
                        (float)p_sys->stream[p_sys->i_stream_video].f_fps );
        i_totalframes = p_sys->stream[p_sys->i_stream_video].i_frames;
    }
    else
    {
        msg_Warn( p_mux, "avi file without video track isn't a good idea..." );
        i_microsecperframe = 0;
        i_totalframes = 0;
    }

    for( i_stream = 0,i_maxbytespersec = 0; i_stream < p_sys->i_streams; i_stream++ )
    {
        if( p_sys->stream[i_stream].i_duration > 0 )
        {
            i_maxbytespersec +=
                p_sys->stream[p_sys->i_stream_video].i_totalsize /
                p_sys->stream[p_sys->i_stream_video].i_duration;
        }
    }

    bo_AddDWordLE( p_bo, i_microsecperframe );
    bo_AddDWordLE( p_bo, i_maxbytespersec );
    bo_AddDWordLE( p_bo, 0 );                   /* padding */
    bo_AddDWordLE( p_bo, AVIF_TRUSTCKTYPE |
                         AVIF_HASINDEX |
                         AVIF_ISINTERLEAVED );  /* flags */
    bo_AddDWordLE( p_bo, i_totalframes );
    bo_AddDWordLE( p_bo, 0 );                   /* initial frame */
    bo_AddDWordLE( p_bo, p_sys->i_streams );    /* streams count */
    bo_AddDWordLE( p_bo, 1024 * 1024 );         /* suggested buffer size */
    if( p_video )
    {
        bo_AddDWordLE( p_bo, p_video->p_bih->biWidth );
        bo_AddDWordLE( p_bo, p_video->p_bih->biHeight );
    }
    else
    {
        bo_AddDWordLE( p_bo, 0 );
        bo_AddDWordLE( p_bo, 0 );
    }
    bo_AddDWordLE( p_bo, 0 );                   /* ???? */
    bo_AddDWordLE( p_bo, 0 );                   /* ???? */
    bo_AddDWordLE( p_bo, 0 );                   /* ???? */
    bo_AddDWordLE( p_bo, 0 );                   /* ???? */

    AVI_BOX_EXIT( 0 );
}
static int avi_HeaderAdd_strh( sout_mux_t   *p_mux,
                               buffer_out_t *p_bo,
                               avi_stream_t *p_stream )
{
    AVI_BOX_ENTER( "strh" );

    switch( p_stream->i_cat )
    {
        case VIDEO_ES:
            {
                bo_AddFCC( p_bo, "vids" );
                bo_AddDWordBE( p_bo, p_stream->p_bih->biCompression );
                bo_AddDWordLE( p_bo, 0 );   /* flags */
                bo_AddWordLE(  p_bo, 0 );   /* priority */
                bo_AddWordLE(  p_bo, 0 );   /* langage */
                bo_AddDWordLE( p_bo, 0 );   /* initial frame */
                bo_AddDWordLE( p_bo, 1000 );/* scale */
                bo_AddDWordLE( p_bo, (uint32_t)( 1000 * p_stream->f_fps ));
                bo_AddDWordLE( p_bo, 0 );   /* start */
                bo_AddDWordLE( p_bo, p_stream->i_frames );
                bo_AddDWordLE( p_bo, 1024 * 1024 );
                bo_AddDWordLE( p_bo, -1 );  /* quality */
                bo_AddDWordLE( p_bo, 0 );   /* samplesize */
                bo_AddWordLE(  p_bo, 0 );   /* ??? */
                bo_AddWordLE(  p_bo, 0 );   /* ??? */
                bo_AddWordLE(  p_bo, p_stream->p_bih->biWidth );
                bo_AddWordLE(  p_bo, p_stream->p_bih->biHeight );
            }
            break;
        case AUDIO_ES:
            {
                int i_rate, i_scale, i_samplesize;

                i_samplesize = p_stream->p_wf->nBlockAlign;
                if( i_samplesize > 1 )
                {
                    i_scale = i_samplesize;
                    i_rate = /*i_scale **/ p_stream->i_bitrate / 8;
                }
                else
                {
                    i_samplesize = 1;
                    i_scale = 1000;
                    i_rate = 1000 * p_stream->i_bitrate / 8;
                }
                bo_AddFCC( p_bo, "auds" );
                bo_AddDWordLE( p_bo, 0 );   /* tag */
                bo_AddDWordLE( p_bo, 0 );   /* flags */
                bo_AddWordLE(  p_bo, 0 );   /* priority */
                bo_AddWordLE(  p_bo, 0 );   /* langage */
                bo_AddDWordLE( p_bo, 0 );   /* initial frame */
                bo_AddDWordLE( p_bo, i_scale );/* scale */
                bo_AddDWordLE( p_bo, i_rate );
                bo_AddDWordLE( p_bo, 0 );   /* start */
                bo_AddDWordLE( p_bo, p_stream->i_frames );
                bo_AddDWordLE( p_bo, 10 * 1024 );
                bo_AddDWordLE( p_bo, -1 );  /* quality */
                bo_AddDWordLE( p_bo, i_samplesize );
                bo_AddWordLE(  p_bo, 0 );   /* ??? */
                bo_AddWordLE(  p_bo, 0 );   /* ??? */
                bo_AddWordLE(  p_bo, 0 );
                bo_AddWordLE(  p_bo, 0 );
            }
            break;
    }

    AVI_BOX_EXIT( 0 );
}

static int avi_HeaderAdd_strf( sout_mux_t *p_mux,
                               buffer_out_t *p_bo,
                               avi_stream_t *p_stream )
{
    AVI_BOX_ENTER( "strf" );

    switch( p_stream->i_cat )
    {
        case AUDIO_ES:
            bo_AddWordLE( p_bo, p_stream->p_wf->wFormatTag );
            bo_AddWordLE( p_bo, p_stream->p_wf->nChannels );
            bo_AddDWordLE( p_bo, p_stream->p_wf->nSamplesPerSec );
            bo_AddDWordLE( p_bo, p_stream->p_wf->nAvgBytesPerSec );
            bo_AddWordLE( p_bo, p_stream->p_wf->nBlockAlign );
            bo_AddWordLE( p_bo, p_stream->p_wf->wBitsPerSample );
            bo_AddWordLE( p_bo, p_stream->p_wf->cbSize );
            bo_AddMem( p_bo, p_stream->p_wf->cbSize, (uint8_t*)&p_stream->p_wf[1] );
            break;
        case VIDEO_ES:
            bo_AddDWordLE( p_bo, p_stream->p_bih->biSize );
            bo_AddDWordLE( p_bo, p_stream->p_bih->biWidth );
            bo_AddDWordLE( p_bo, p_stream->p_bih->biHeight );
            bo_AddWordLE( p_bo, p_stream->p_bih->biPlanes );
            bo_AddWordLE( p_bo, p_stream->p_bih->biBitCount );
            if( VLC_FOURCC( 0, 0, 0, 1 ) == 0x00000001 )
            {
                bo_AddDWordBE( p_bo, p_stream->p_bih->biCompression );
            }
            else
            {
                bo_AddDWordLE( p_bo, p_stream->p_bih->biCompression );
            }
            bo_AddDWordLE( p_bo, p_stream->p_bih->biSizeImage );
            bo_AddDWordLE( p_bo, p_stream->p_bih->biXPelsPerMeter );
            bo_AddDWordLE( p_bo, p_stream->p_bih->biYPelsPerMeter );
            bo_AddDWordLE( p_bo, p_stream->p_bih->biClrUsed );
            bo_AddDWordLE( p_bo, p_stream->p_bih->biClrImportant );
//            bo_AddMem( p_bo,
//                       p_stream->p_bih->biSize - sizeof( BITMAPINFOHEADER ),
//                       (uint8_t*)&p_stream->p_bih[1] );
            break;
    }

    AVI_BOX_EXIT( 0 );
}

static int avi_HeaderAdd_strl( sout_mux_t *p_mux,
                               buffer_out_t *p_bo,
                               avi_stream_t *p_stream )
{
    AVI_BOX_ENTER_LIST( "strl" );

    avi_HeaderAdd_strh( p_mux, p_bo, p_stream );
    avi_HeaderAdd_strf( p_mux, p_bo, p_stream );

    AVI_BOX_EXIT( 0 );
}

static block_t *avi_HeaderCreateRIFF( sout_mux_t *p_mux )
{
    sout_mux_sys_t      *p_sys = p_mux->p_sys;
    block_t       *p_hdr;
    int                 i_stream;
    int                 i_maxbytespersec;
    int                 i_junk;
    buffer_out_t        bo, bo_save;

    /* Real header + LIST-movi */
    p_hdr = block_New( p_mux, HDR_SIZE + 12 );
    memset( p_hdr->p_buffer, 0, HDR_SIZE  + 12 );

    bo_Init( &bo, HDR_SIZE + 12, p_hdr->p_buffer );

    bo_AddFCC( &bo, "RIFF" );
    bo_AddDWordLE( &bo, p_sys->i_movi_size + HDR_SIZE + p_sys->i_idx1_size );
    bo_AddFCC( &bo, "AVI " );

    bo_AddFCC( &bo, "LIST" );
    memcpy( &bo_save, &bo, sizeof( buffer_out_t ) );
    bo_AddDWordLE( &bo, 0 );
    bo_AddFCC( &bo, "hdrl" );

    avi_HeaderAdd_avih( p_mux, &bo );
    for( i_stream = 0, i_maxbytespersec = 0; i_stream < p_sys->i_streams;
         i_stream++ )
    {
        avi_HeaderAdd_strl( p_mux, &bo, &p_sys->stream[i_stream] );
    }

    bo_AddDWordLE( &bo_save, bo.i_buffer - bo_save.i_buffer - 4 );

    i_junk = HDR_SIZE - bo.i_buffer - 8;
    bo_AddFCC( &bo, "JUNK" );
    bo_AddDWordLE( &bo, i_junk );

    bo.i_buffer += i_junk;
    fprintf( stderr, "Writing list-movi at %i\n", bo.i_buffer );
    bo_AddFCC( &bo, "LIST" );
    bo_AddDWordLE( &bo, p_sys->i_movi_size + 4 );
    bo_AddFCC( &bo, "movi" );

    return( p_hdr );
}

static block_t * avi_HeaderCreateidx1( sout_mux_t *p_mux )
{
    sout_mux_sys_t      *p_sys = p_mux->p_sys;
    block_t       *p_idx1;
    uint32_t            i_idx1_size;
    unsigned int        i;
    buffer_out_t        bo;

    i_idx1_size = 16 * p_sys->idx1.i_entry_count;

    p_idx1 = block_New( p_mux, i_idx1_size + 8 );
    memset( p_idx1->p_buffer, 0, i_idx1_size );

    bo_Init( &bo, i_idx1_size, p_idx1->p_buffer );
    bo_AddFCC( &bo, "idx1" );
    bo_AddDWordLE( &bo, i_idx1_size );

    for( i = 0; i < p_sys->idx1.i_entry_count; i++ )
    {
        bo_AddFCC( &bo, p_sys->idx1.entry[i].fcc );
        bo_AddDWordLE( &bo, p_sys->idx1.entry[i].i_flags );
        bo_AddDWordLE( &bo, p_sys->idx1.entry[i].i_pos );
        bo_AddDWordLE( &bo, p_sys->idx1.entry[i].i_length );
    }

    return( p_idx1 );
}

