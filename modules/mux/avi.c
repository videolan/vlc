/*****************************************************************************
 * avi.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: avi.c,v 1.8 2003/02/24 14:14:43 fenrir Exp $
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
/* TODO: add OpenDML write support */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#include "codecs.h"


#define AVIF_HASINDEX       0x00000010  // Index at end of file?
#define AVIF_MUSTUSEINDEX   0x00000020
#define AVIF_ISINTERLEAVED  0x00000100
#define AVIF_TRUSTCKTYPE    0x00000800  // Use CKType to find key frames?
#define AVIF_WASCAPTUREFILE 0x00010000
#define AVIF_COPYRIGHTED    0x00020000

/* Flags for index */
#define AVIIF_LIST          0x00000001L /* chunk is a 'LIST' */
#define AVIIF_KEYFRAME      0x00000010L /* this frame is a key frame.*/
#define AVIIF_NOTIME        0x00000100L /* this frame doesn't take any time */
#define AVIIF_COMPUSE       0x0FFF0000L /* these bits are for compressor use */

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static int Capability(sout_instance_t *, int , void *, void * );
static int AddStream( sout_instance_t *, sout_input_t * );
static int DelStream( sout_instance_t *, sout_input_t * );
static int Mux      ( sout_instance_t * );

static sout_buffer_t *avi_HeaderCreateRIFF( sout_instance_t *p_sout );
static sout_buffer_t *avi_HeaderCreateidx1( sout_instance_t *p_sout );

static void SetFCC( uint8_t *p, char *fcc )
{
    p[0] = fcc[0];
    p[1] = fcc[1];
    p[2] = fcc[2];
    p[3] = fcc[3];
}

static void SetDWLE( uint8_t *p, uint32_t i_dw )
{
    p[3] = ( i_dw >> 24 )&0xff;
    p[2] = ( i_dw >> 16 )&0xff;
    p[1] = ( i_dw >>  8 )&0xff;
    p[0] = ( i_dw       )&0xff;
}

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Avi muxer") );
    set_capability( "sout mux", 5 );
    add_shortcut( "avi" );
    set_callbacks( Open, Close );
vlc_module_end();

// FIXME FIXME
#define HDR_SIZE 10240

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

typedef struct sout_mux_s
{
    int i_streams;
    int i_stream_video;

    off_t i_movi_size;
    avi_stream_t stream[100];

    avi_idx1_t idx1;
    off_t i_idx1_size;

} sout_mux_t;

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_mux_t          *p_mux;
    sout_buffer_t       *p_hdr;

    p_mux = malloc( sizeof( sout_mux_t ) );
    p_mux->i_streams = 0;
    p_mux->i_stream_video = -1;
    p_mux->i_movi_size = 0;

    p_mux->idx1.i_entry_count = 0;
    p_mux->idx1.i_entry_max = 10000;
    p_mux->idx1.entry = calloc( p_mux->idx1.i_entry_max, sizeof( avi_idx1_entry_t ) );

    msg_Info( p_sout, "Open" );

    p_sout->pf_mux_capacity  = Capability;
    p_sout->pf_mux_addstream = AddStream;
    p_sout->pf_mux_delstream = DelStream;
    p_sout->pf_mux           = Mux;
    p_sout->p_mux_data = (void*)p_mux;
    p_sout->i_mux_preheader  = 8; // (fourcc,length) header

    /* room to add header at the end */
    p_hdr = sout_BufferNew( p_sout, HDR_SIZE );
    memset( p_hdr->p_buffer, 0, HDR_SIZE );
    sout_AccessOutWrite( p_sout->p_access, p_hdr );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    sout_buffer_t       *p_hdr, *p_idx1;
    int                 i_stream;

    msg_Info( p_sout, "Close" );

    /* first create idx1 chunk (write at the end of the stream */
    p_idx1 = avi_HeaderCreateidx1( p_sout );
    p_mux->i_idx1_size = p_idx1->i_size;
    sout_AccessOutWrite( p_sout->p_access, p_idx1 );

    /* calculate some value for headers creations */
    for( i_stream = 0; i_stream < p_mux->i_streams; i_stream++ )
    {
        avi_stream_t *p_stream;

        p_stream = &p_mux->stream[i_stream];

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
        msg_Err( p_sout,"stream[%d] duration:%lld totalsize:%lld frames:%d fps:%f kb/s:%d",
                i_stream,
                p_stream->i_duration/1000000, p_stream->i_totalsize,
                p_stream->i_frames,
                p_stream->f_fps, p_stream->i_bitrate/1024 );
    }

    p_hdr = avi_HeaderCreateRIFF( p_sout );
    sout_AccessOutSeek( p_sout->p_access, 0 );
    sout_AccessOutWrite( p_sout->p_access, p_hdr );
}

static int Capability( sout_instance_t *p_sout, int i_query, void *p_args, void *p_answer )
{
   switch( i_query )
   {
        case SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME:
            *(vlc_bool_t*)p_answer = VLC_TRUE;
            return( SOUT_MUX_CAP_ERR_OK );
        default:
            return( SOUT_MUX_CAP_ERR_UNIMPLEMENTED );
   }
}

static int AddStream( sout_instance_t *p_sout, sout_input_t *p_input )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    avi_stream_t        *p_stream;

    if( p_mux->i_streams >= 100 )
    {
        msg_Err( p_sout, "too many streams" );
        return( -1 );
    }
    if( p_input->input_format.p_format == NULL )
    {
        msg_Err( p_sout, "stream descriptor missing" );
        return( -1 );
    }

    msg_Dbg( p_sout, "adding input" );
    p_input->p_mux_data = malloc( sizeof( int ) );

    *((int*)p_input->p_mux_data) = p_mux->i_streams;
    p_stream = &p_mux->stream[p_mux->i_streams];

    switch( p_input->input_format.i_cat )
    {
        case AUDIO_ES:
            {
                WAVEFORMATEX *p_wf =
                    (WAVEFORMATEX*)p_input->input_format.p_format;

                p_stream->i_cat = AUDIO_ES;
                p_stream->fcc[0] = '0' + p_mux->i_streams / 10;
                p_stream->fcc[1] = '0' + p_mux->i_streams % 10;
                p_stream->fcc[2] = 'w';
                p_stream->fcc[3] = 'b';

                p_stream->p_bih = NULL;
                p_stream->p_wf  = malloc( sizeof( WAVEFORMATEX ) + p_wf->cbSize );
                memcpy( p_stream->p_wf,
                        p_wf,
                        sizeof( WAVEFORMATEX ) + p_wf->cbSize);
            }
            break;
        case VIDEO_ES:
            {
                BITMAPINFOHEADER *p_bih =
                    (BITMAPINFOHEADER*)p_input->input_format.p_format;;

                p_stream->i_cat = VIDEO_ES;
                p_stream->fcc[0] = '0' + p_mux->i_streams / 10;
                p_stream->fcc[1] = '0' + p_mux->i_streams % 10;
                p_stream->fcc[2] = 'd';
                p_stream->fcc[3] = 'c';
                if( p_mux->i_stream_video < 0 )
                {
                    p_mux->i_stream_video = p_mux->i_streams;
                }
                p_stream->p_wf  = NULL;
                p_stream->p_bih = malloc( p_bih->biSize );
                memcpy( p_stream->p_bih,
                        p_bih,
                        p_bih->biSize );
            }
            break;
        default:
            return( -1 );
    }
    p_stream->i_totalsize = 0;
    p_stream->i_frames     = 0;
    p_stream->i_duration  = 0;

    p_mux->i_streams++;
    return( 0 );
}

static int DelStream( sout_instance_t *p_sout, sout_input_t *p_input )
{
//    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;

    msg_Dbg( p_sout, "removing input" );

    free( p_input->p_mux_data ); p_input->p_mux_data = NULL;

    return( 0 );
}

static int Mux      ( sout_instance_t *p_sout )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    avi_stream_t        *p_stream;
    int i_stream;
    int i;

    for( i = 0; i < p_sout->i_nb_inputs; i++ )
    {
        int i_count;
        sout_fifo_t *p_fifo;

        i_stream = *((int*)p_sout->pp_inputs[i]->p_mux_data );
        p_stream = &p_mux->stream[i_stream];

        p_fifo = p_sout->pp_inputs[i]->p_fifo;
        i_count = p_fifo->i_depth;
        while( i_count > 0 )
        {
            avi_idx1_entry_t *p_idx;
            sout_buffer_t *p_data;

            p_data = sout_FifoGet( p_fifo );

            p_stream->i_frames++;
            p_stream->i_duration  += p_data->i_length;
            p_stream->i_totalsize += p_data->i_size;

            /* add idx1 entry for this frame */
            p_idx = &p_mux->idx1.entry[p_mux->idx1.i_entry_count];
            memcpy( p_idx->fcc, p_stream->fcc, 4 );
            p_idx->i_flags = AVIIF_KEYFRAME;
            p_idx->i_pos   = p_mux->i_movi_size + 4;
            p_idx->i_length= p_data->i_size;
            p_mux->idx1.i_entry_count++;
            if( p_mux->idx1.i_entry_count >= p_mux->idx1.i_entry_max )
            {
                p_mux->idx1.i_entry_max += 10000;
                p_mux->idx1.entry = realloc( p_mux->idx1.entry,
                                             p_mux->idx1.i_entry_max * sizeof( avi_idx1_entry_t ) );
            }


            if( sout_BufferReallocFromPreHeader( p_sout, p_data, 8 ) )
            {
                /* there isn't enough data in preheader */
                sout_buffer_t *p_hdr;

                p_hdr = sout_BufferNew( p_sout, 8 );
                SetFCC( p_hdr->p_buffer, p_stream->fcc );
                SetDWLE( p_hdr->p_buffer + 4, p_data->i_size );

                sout_AccessOutWrite( p_sout->p_access, p_hdr );
                p_mux->i_movi_size += p_hdr->i_size;

            }
            else
            {
                SetFCC( p_data->p_buffer, p_stream->fcc );
                SetDWLE( p_data->p_buffer + 4, p_data->i_size - 8 );
            }

            if( p_data->i_size & 0x01 )
            {
                sout_BufferRealloc( p_sout, p_data, p_data->i_size + 1 );
                p_data->i_size += 1;
            }

            sout_AccessOutWrite( p_sout->p_access, p_data );
            p_mux->i_movi_size += p_data->i_size;

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

static int avi_HeaderAdd_avih( sout_instance_t *p_sout,
                               buffer_out_t *p_bo )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    avi_stream_t *p_video = NULL;
    int         i_stream;
    uint32_t    i_microsecperframe;
    int         i_maxbytespersec;
    int         i_totalframes;
    AVI_BOX_ENTER( "avih" );

    if( p_mux->i_stream_video >= 0 )
    {
        p_video = &p_mux->stream[p_mux->i_stream_video];
        if( p_video->i_frames <= 0 )
        {
            p_video = NULL;
        }
    }

    if( p_video )
    {
        i_microsecperframe =
            (uint32_t)( (float)1000000 /
                        (float)p_mux->stream[p_mux->i_stream_video].f_fps );
        i_totalframes = p_mux->stream[p_mux->i_stream_video].i_frames;
    }
    else
    {
        msg_Warn( p_sout, "avi file without audio video track isn't a good idea..." );
        i_microsecperframe = 0;
        i_totalframes = 0;
    }

    for( i_stream = 0,i_maxbytespersec = 0; i_stream < p_mux->i_streams; i_stream++ )
    {
        if( p_mux->stream[p_mux->i_stream_video].i_duration > 0 )
        {
            i_maxbytespersec +=
                p_mux->stream[p_mux->i_stream_video].i_totalsize /
                p_mux->stream[p_mux->i_stream_video].i_duration;
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
    bo_AddDWordLE( p_bo, p_mux->i_streams );    /* streams count */
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
static int avi_HeaderAdd_strh( sout_instance_t *p_sout,
                               buffer_out_t *p_bo,
                               avi_stream_t *p_stream )
{
//    sout_mux_t *p_mux = (sout_mux_t*)p_sout->p_mux_data;
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
                    i_rate = i_scale * p_stream->i_bitrate / 8;
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

static int avi_HeaderAdd_strf( sout_instance_t *p_sout,
                               buffer_out_t *p_bo,
                               avi_stream_t *p_stream )
{
//    sout_mux_t *p_mux = (sout_mux_t*)p_sout->p_mux_data;
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
            bo_AddMem( p_bo,
                       p_stream->p_bih->biSize - sizeof( BITMAPINFOHEADER ),
                       (uint8_t*)&p_stream->p_bih[1] );
            break;
    }

    AVI_BOX_EXIT( 0 );
}

static int avi_HeaderAdd_strl( sout_instance_t *p_sout,
                               buffer_out_t *p_bo,
                               avi_stream_t *p_stream )
{
//    sout_mux_t *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    AVI_BOX_ENTER_LIST( "strl" );

    avi_HeaderAdd_strh( p_sout, p_bo, p_stream );
    avi_HeaderAdd_strf( p_sout, p_bo, p_stream );

    AVI_BOX_EXIT( 0 );
}

static sout_buffer_t *avi_HeaderCreateRIFF( sout_instance_t *p_sout )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    sout_buffer_t       *p_hdr;
    int                 i_stream;
    int                 i_maxbytespersec;
    int                 i_junk;
    buffer_out_t        bo;

    p_hdr = sout_BufferNew( p_sout, HDR_SIZE );
    memset( p_hdr->p_buffer, 0, HDR_SIZE );

    bo_Init( &bo, HDR_SIZE, p_hdr->p_buffer );

    bo_AddFCC( &bo, "RIFF" );
    bo_AddDWordLE( &bo, p_mux->i_movi_size + HDR_SIZE - 8 + p_mux->i_idx1_size );
    bo_AddFCC( &bo, "AVI " );

    bo_AddFCC( &bo, "LIST" );
    bo_AddDWordLE( &bo, HDR_SIZE - 8);
    bo_AddFCC( &bo, "hdrl" );

    avi_HeaderAdd_avih( p_sout, &bo );
    for( i_stream = 0,i_maxbytespersec = 0; i_stream < p_mux->i_streams; i_stream++ )
    {
        avi_HeaderAdd_strl( p_sout, &bo, &p_mux->stream[i_stream] );
    }

    i_junk = HDR_SIZE - bo.i_buffer - 8 - 12;
    bo_AddFCC( &bo, "JUNK" );
    bo_AddDWordLE( &bo, i_junk );

    bo.i_buffer += i_junk;
    bo_AddFCC( &bo, "LIST" );
    bo_AddDWordLE( &bo, p_mux->i_movi_size + 4 );
    bo_AddFCC( &bo, "movi" );

    return( p_hdr );
}

static sout_buffer_t * avi_HeaderCreateidx1( sout_instance_t *p_sout )
{
    sout_mux_t          *p_mux = (sout_mux_t*)p_sout->p_mux_data;
    sout_buffer_t       *p_idx1;
    uint32_t            i_idx1_size;
    unsigned int        i;
    buffer_out_t        bo;

    i_idx1_size = 16 * p_mux->idx1.i_entry_count;

    p_idx1 = sout_BufferNew( p_sout, i_idx1_size + 8 );
    memset( p_idx1->p_buffer, 0, i_idx1_size );

    bo_Init( &bo, i_idx1_size, p_idx1->p_buffer );
    bo_AddFCC( &bo, "idx1" );
    bo_AddDWordLE( &bo, i_idx1_size );

    for( i = 0; i < p_mux->idx1.i_entry_count; i++ )
    {
        bo_AddFCC( &bo, p_mux->idx1.entry[i].fcc );
        bo_AddDWordLE( &bo, p_mux->idx1.entry[i].i_flags );
        bo_AddDWordLE( &bo, p_mux->idx1.entry[i].i_pos );
        bo_AddDWordLE( &bo, p_mux->idx1.entry[i].i_length );
    }

    return( p_idx1 );
}


