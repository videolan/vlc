/*****************************************************************************
 * avi.c
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
/* TODO: add OpenDML write support */


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>
#include <vlc_codecs.h>
#include <vlc_boxes.h>
#include "../demux/avi/bitmapinfoheader.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-avi-"

#define CFG_ARTIST_TEXT     N_("Artist")
#define CFG_DATE_TEXT       N_("Date")
#define CFG_GENRE_TEXT      N_("Genre")
#define CFG_COPYRIGHT_TEXT  N_("Copyright")
#define CFG_COMMENT_TEXT    N_("Comment")
#define CFG_NAME_TEXT       N_("Name")
#define CFG_SUBJECT_TEXT    N_("Subject")
#define CFG_ENCODER_TEXT    N_("Encoder")
#define CFG_KEYWORDS_TEXT   N_("Keywords")

vlc_module_begin ()
    set_description( N_("AVI muxer") )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_MUX )
    set_capability( "sout mux", 5 )
    add_shortcut( "avi" )

    add_string( SOUT_CFG_PREFIX "artist", NULL,    CFG_ARTIST_TEXT, NULL, true )
    add_string( SOUT_CFG_PREFIX "date",   NULL,    CFG_DATE_TEXT, NULL, true )
    add_string( SOUT_CFG_PREFIX "genre",  NULL,    CFG_GENRE_TEXT, NULL, true )
    add_string( SOUT_CFG_PREFIX "copyright", NULL, CFG_COPYRIGHT_TEXT, NULL, true )
    add_string( SOUT_CFG_PREFIX "comment", NULL,   CFG_COMMENT_TEXT, NULL, true )
    add_string( SOUT_CFG_PREFIX "name", NULL,      CFG_NAME_TEXT, NULL, true )
    add_string( SOUT_CFG_PREFIX "subject", NULL,   CFG_SUBJECT_TEXT, NULL, true )
    add_string( SOUT_CFG_PREFIX "encoder",
                "VLC Media Player - " VERSION_MESSAGE,
                                                   CFG_ENCODER_TEXT, NULL, true )
    add_string( SOUT_CFG_PREFIX "keywords", NULL,  CFG_KEYWORDS_TEXT, NULL, true )

    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static void DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

typedef struct avi_stream_s
{
    int i_cat;

    char fcc[4];

    vlc_tick_t i_duration;       // in µs

    int     i_frames;        // total frame count
    int64_t i_totalsize;    // total stream size

    float   f_fps;
    int     i_bitrate;

    VLC_BITMAPINFOHEADER    *p_bih;
    size_t                   i_bih;
    WAVEFORMATEX            *p_wf;

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

typedef struct
{
    bool b_write_header;

    int i_streams;
    int i_stream_video;

    off_t i_movi_size;
    avi_stream_t stream[100];

    avi_idx1_t idx1;
    off_t i_idx1_size;

} sout_mux_sys_t;

#define HDR_BASE_SIZE 512 /* single video&audio ~ 400 bytes header */

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
    sout_mux_sys_t  *p_sys;

    msg_Dbg( p_mux, "AVI muxer opened" );

    p_sys = malloc( sizeof( sout_mux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->i_streams = 0;
    p_sys->i_stream_video = -1;
    p_sys->i_movi_size = 0;

    p_sys->idx1.i_entry_count = 0;
    p_sys->idx1.i_entry_max = 10000;
    p_sys->idx1.entry = calloc( p_sys->idx1.i_entry_max,
                                sizeof( avi_idx1_entry_t ) );
    if( !p_sys->idx1.entry )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_sys->b_write_header = true;


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
    if( p_idx1 )
    {
        p_sys->i_idx1_size = p_idx1->i_buffer;
        sout_AccessOutWrite( p_mux->p_access, p_idx1 );
    }
    else p_sys->i_idx1_size = 0;

    /* calculate some value for headers creations */
    for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
    {
        avi_stream_t *p_stream;

        p_stream = &p_sys->stream[i_stream];

        if( p_stream->i_duration > 0 )
        {
            p_stream->f_fps = (float)p_stream->i_frames /
                              ( (float)p_stream->i_duration /
                                (float)CLOCK_FREQ );
            p_stream->i_bitrate =
                8 * (uint64_t)1000000 *
                    (uint64_t)p_stream->i_totalsize /
                    (uint64_t)p_stream->i_duration;
        }
        else
        {
            p_stream->f_fps = 25;
            p_stream->i_bitrate = 128 * 1024;
        }
        msg_Info( p_mux, "stream[%d] duration:%"PRId64" totalsize:%"PRId64
                  " frames:%d fps:%f KiB/s:%d",
                  i_stream,
                  SEC_FROM_VLC_TICK(p_stream->i_duration),
                  p_stream->i_totalsize,
                  p_stream->i_frames,
                  p_stream->f_fps, p_stream->i_bitrate/1024 );
    }

    p_hdr = avi_HeaderCreateRIFF( p_mux );
    if ( p_hdr )
    {
        sout_AccessOutSeek( p_mux->p_access, 0 );
        sout_AccessOutWrite( p_mux->p_access, p_hdr );
    }

    for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
    {
        avi_stream_t *p_stream;
        p_stream = &p_sys->stream[i_stream];
        free( p_stream->p_bih );
        free( p_stream->p_wf );
    }
    free( p_sys->idx1.entry );
    free( p_sys );
}

static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    VLC_UNUSED(p_mux);
    bool *pb_bool;
    char **ppsz;

   switch( i_query )
   {
       case MUX_CAN_ADD_STREAM_WHILE_MUXING:
           pb_bool = va_arg( args, bool * );
           *pb_bool = false;
           return VLC_SUCCESS;

       case MUX_GET_MIME:
           ppsz = va_arg( args, char ** );
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
        return VLC_EGENERIC;
    }

    msg_Dbg( p_mux, "adding input" );
    p_input->p_sys = malloc( sizeof( int ) );
    if( !p_input->p_sys )
        return VLC_ENOMEM;

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
            p_stream->i_bih = 0;

            WAVEFORMATEX *p_wf  = malloc( sizeof( WAVEFORMATEX ) +
                                  p_input->p_fmt->i_extra );
            if( !p_wf )
            {
                free( p_input->p_sys );
                p_input->p_sys = NULL;
                return VLC_ENOMEM;
            }

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
                case VLC_CODEC_A52:
                    p_wf->wFormatTag = WAVE_FORMAT_A52;
                    p_wf->nBlockAlign= 1;
                    break;
                case VLC_CODEC_MP3:
                    p_wf->wFormatTag = WAVE_FORMAT_MPEGLAYER3;
                    p_wf->nBlockAlign= 1;
                    break;
                case VLC_CODEC_WMA1:
                    p_wf->wFormatTag = WAVE_FORMAT_WMA1;
                    break;
                case VLC_CODEC_WMA2:
                    p_wf->wFormatTag = WAVE_FORMAT_WMA2;
                    break;
                case VLC_CODEC_WMAP:
                    p_wf->wFormatTag = WAVE_FORMAT_WMAP;
                    break;
                case VLC_CODEC_WMAL:
                    p_wf->wFormatTag = WAVE_FORMAT_WMAL;
                    break;
                case VLC_CODEC_ALAW:
                    p_wf->wFormatTag = WAVE_FORMAT_ALAW;
                    break;
                case VLC_CODEC_MULAW:
                    p_wf->wFormatTag = WAVE_FORMAT_MULAW;
                    break;
                    /* raw codec */
                case VLC_CODEC_U8:
                    p_wf->wFormatTag = WAVE_FORMAT_PCM;
                    p_wf->nBlockAlign= p_wf->nChannels;
                    p_wf->wBitsPerSample = 8;
                    p_wf->nAvgBytesPerSec = (p_wf->wBitsPerSample/8) *
                                      p_wf->nSamplesPerSec * p_wf->nChannels;
                    break;
                case VLC_CODEC_S16L:
                    p_wf->wFormatTag = WAVE_FORMAT_PCM;
                    p_wf->nBlockAlign= 2 * p_wf->nChannels;
                    p_wf->wBitsPerSample = 16;
                    p_wf->nAvgBytesPerSec = (p_wf->wBitsPerSample/8) *
                                      p_wf->nSamplesPerSec * p_wf->nChannels;
                    break;
                case VLC_CODEC_S24L:
                    p_wf->wFormatTag = WAVE_FORMAT_PCM;
                    p_wf->nBlockAlign= 3 * p_wf->nChannels;
                    p_wf->wBitsPerSample = 24;
                    p_wf->nAvgBytesPerSec = (p_wf->wBitsPerSample/8) *
                                      p_wf->nSamplesPerSec * p_wf->nChannels;
                    break;
                case VLC_CODEC_S32L:
                    p_wf->wFormatTag = WAVE_FORMAT_PCM;
                    p_wf->nBlockAlign= 4 * p_wf->nChannels;
                    p_wf->wBitsPerSample = 32;
                    p_wf->nAvgBytesPerSec = (p_wf->wBitsPerSample/8) *
                                      p_wf->nSamplesPerSec * p_wf->nChannels;
                    break;
                default:
                    free( p_wf );
                    free( p_input->p_sys );
                    p_input->p_sys = NULL;
                    return VLC_EGENERIC;
            }
            p_stream->p_wf = p_wf;
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
            p_stream->p_bih = CreateBitmapInfoHeader( &p_input->fmt, &p_stream->i_bih );
            if( !p_stream->p_bih )
            {
                free( p_input->p_sys );
                p_input->p_sys = NULL;
                return VLC_ENOMEM;
            }
            break;
        default:
            free( p_input->p_sys );
            p_input->p_sys = NULL;
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

static void DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    msg_Dbg( p_mux, "removing input" );

    free( p_input->p_sys );
}

static int PrepareSamples( const avi_stream_t *p_stream,
                           const es_format_t *p_fmt,
                           block_t **pp_block )
{
    if( p_stream->i_frames == 0 && p_stream->i_cat == VIDEO_ES )
    {
       /* Add header present at the end of BITMAP info header
          to first frame in case of XVID */
       if( p_stream->p_bih->biCompression == VLC_FOURCC( 'X', 'V', 'I', 'D' ) )
       {
           size_t i_header_length =
               p_stream->p_bih->biSize - sizeof(VLC_BITMAPINFOHEADER);
           *pp_block = block_Realloc( *pp_block, i_header_length,
                                      (*pp_block)->i_buffer );
           if( !*pp_block )
               return VLC_ENOMEM;
           memcpy((*pp_block)->p_buffer,&p_stream->p_bih[1], i_header_length);
       }
    }

    /* RV24 is only BGR in AVI, and we can't use BI_BITFIELD */
    if( p_stream->i_cat == VIDEO_ES &&
        p_stream->p_bih->biCompression == BI_RGB &&
        p_stream->p_bih->biBitCount == 24 &&
        (p_fmt->video.i_bmask != 0xFF0000 ||
         p_fmt->video.i_rmask != 0x0000FF) )
    {
        unsigned rshift = ctz(p_fmt->video.i_rmask);
        unsigned gshift = ctz(p_fmt->video.i_gmask);
        unsigned bshift = ctz(p_fmt->video.i_bmask);

        uint8_t *p_data = (*pp_block)->p_buffer;
        for( size_t i=0; i<(*pp_block)->i_buffer / 3; i++ )
        {
            uint8_t *p = &p_data[i*3];
            /* reorder as BGR using shift value (done by FixRGB) */
            uint32_t v = (p[0] << 16) | (p[1] << 8) | p[2];
            p[0] = (v & p_fmt->video.i_bmask) >> bshift;
            p[1] = (v & p_fmt->video.i_gmask) >> gshift;
            p[2] = (v & p_fmt->video.i_rmask) >> rshift;
        }
    }

    return VLC_SUCCESS;
}

static int Mux      ( sout_mux_t *p_mux )
{
    sout_mux_sys_t  *p_sys = p_mux->p_sys;
    avi_stream_t    *p_stream;
    int i_stream, i;

    if( p_sys->b_write_header )
    {
        msg_Dbg( p_mux, "writing header" );

        block_t *p_hdr = avi_HeaderCreateRIFF( p_mux );
        if ( !p_hdr )
            return VLC_EGENERIC;
        sout_AccessOutWrite( p_mux->p_access, p_hdr );

        p_sys->b_write_header = false;
    }

    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        int i_count;
        block_fifo_t *p_fifo;

        if (!p_mux->pp_inputs[i]->p_sys)
            continue;

        i_stream = *((int*)p_mux->pp_inputs[i]->p_sys );
        p_stream = &p_sys->stream[i_stream];

        p_fifo = p_mux->pp_inputs[i]->p_fifo;
        i_count = block_FifoCount(  p_fifo );
        while( i_count > 1 )
        {
            avi_idx1_entry_t *p_idx;
            block_t *p_data;

            p_data = block_FifoGet( p_fifo );
            if( block_FifoCount( p_fifo ) > 0 )
            {
                block_t *p_next = block_FifoShow( p_fifo );
                p_data->i_length = p_next->i_dts - p_data->i_dts;
            }

            if( PrepareSamples( p_stream, &p_mux->pp_inputs[i]->fmt,
                                &p_data ) != VLC_SUCCESS )
            {
                i_count--;
                continue;
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
            p_idx->i_flags = 0;
            if( ( p_data->i_flags & BLOCK_FLAG_TYPE_MASK ) == 0 || ( p_data->i_flags & BLOCK_FLAG_TYPE_I ) )
                p_idx->i_flags = AVIIF_KEYFRAME;
            p_idx->i_pos   = p_sys->i_movi_size + 4;
            p_idx->i_length= p_data->i_buffer;
            p_sys->idx1.i_entry_count++;
            if( p_sys->idx1.i_entry_count >= p_sys->idx1.i_entry_max )
            {
                p_sys->idx1.i_entry_max += 10000;
                p_sys->idx1.entry = xrealloc( p_sys->idx1.entry,
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
                    if ( p_data )
                        p_data->p_buffer[ p_data->i_buffer - 1 ] = '\0';
                }

                if ( p_data )
                {
                    p_sys->i_movi_size += p_data->i_buffer;
                    sout_AccessOutWrite( p_mux->p_access, p_data );
                }
            }

            i_count--;
        }

    }
    return( 0 );
}

/****************************************************************************
 ****************************************************************************
 **
 ** avi header generation
 **
 ****************************************************************************
 ****************************************************************************/
#define AVI_BOX_ENTER( fcc ) \
    int i_datasize_offset; \
    bo_add_fourcc( p_bo, fcc ); \
    i_datasize_offset = p_bo->b->i_buffer; \
    bo_add_32le( p_bo, 0 )

#define AVI_BOX_ENTER_LIST( fcc ) \
    AVI_BOX_ENTER( "LIST" ); \
    bo_add_fourcc( p_bo, fcc )

#define AVI_BOX_EXIT( i_err ) \
    if( p_bo->b->i_buffer&0x01 ) bo_add_8( p_bo, 0 ); \
    bo_set_32le( p_bo, i_datasize_offset, p_bo->b->i_buffer - i_datasize_offset - 4 ); \
    return( i_err );

static int avi_HeaderAdd_avih( sout_mux_t *p_mux,
                               bo_t *p_bo )
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
                p_sys->stream[i_stream].i_totalsize /
                p_sys->stream[i_stream].i_duration;
        }
    }

    bo_add_32le( p_bo, i_microsecperframe );
    bo_add_32le( p_bo, i_maxbytespersec );
    bo_add_32le( p_bo, 0 );                   /* padding */
    bo_add_32le( p_bo, AVIF_TRUSTCKTYPE |
                       AVIF_HASINDEX |
                       AVIF_ISINTERLEAVED );  /* flags */
    bo_add_32le( p_bo, i_totalframes );
    bo_add_32le( p_bo, 0 );                   /* initial frame */
    bo_add_32le( p_bo, p_sys->i_streams );    /* streams count */
    bo_add_32le( p_bo, 1024 * 1024 );         /* suggested buffer size */
    if( p_video )
    {
        bo_add_32le( p_bo, p_video->p_bih->biWidth );
        bo_add_32le( p_bo, p_video->p_bih->biHeight );
    }
    else
    {
        bo_add_32le( p_bo, 0 );
        bo_add_32le( p_bo, 0 );
    }
    bo_add_32le( p_bo, 0 );                   /* ???? */
    bo_add_32le( p_bo, 0 );                   /* ???? */
    bo_add_32le( p_bo, 0 );                   /* ???? */
    bo_add_32le( p_bo, 0 );                   /* ???? */

    AVI_BOX_EXIT( 0 );
}
static int avi_HeaderAdd_strh( bo_t *p_bo, avi_stream_t *p_stream )
{
    AVI_BOX_ENTER( "strh" );

    switch( p_stream->i_cat )
    {
        case VIDEO_ES:
            {
                bo_add_fourcc( p_bo, "vids" );
                if( p_stream->p_bih->biBitCount )
                    bo_add_fourcc( p_bo, "DIB " );
                else
#ifdef WORDS_BIGENDIAN
                bo_add_32be( p_bo, p_stream->p_bih->biCompression );
#else
                bo_add_32le( p_bo, p_stream->p_bih->biCompression );
#endif
                bo_add_32le( p_bo, 0 );   /* flags */
                bo_add_16le(  p_bo, 0 );   /* priority */
                bo_add_16le(  p_bo, 0 );   /* langage */
                bo_add_32le( p_bo, 0 );   /* initial frame */
                bo_add_32le( p_bo, 1000 );/* scale */
                bo_add_32le( p_bo, (uint32_t)( 1000 * p_stream->f_fps ));
                bo_add_32le( p_bo, 0 );   /* start */
                bo_add_32le( p_bo, p_stream->i_frames );
                bo_add_32le( p_bo, 1024 * 1024 );
                bo_add_32le( p_bo, -1 );  /* quality */
                bo_add_32le( p_bo, 0 );   /* samplesize */
                bo_add_16le(  p_bo, 0 );   /* ??? */
                bo_add_16le(  p_bo, 0 );   /* ??? */
                bo_add_16le(  p_bo, p_stream->p_bih->biWidth );
                bo_add_16le(  p_bo, p_stream->p_bih->biHeight );
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
                bo_add_fourcc( p_bo, "auds" );
                bo_add_32le( p_bo, 0 );   /* tag */
                bo_add_32le( p_bo, 0 );   /* flags */
                bo_add_16le(  p_bo, 0 );   /* priority */
                bo_add_16le(  p_bo, 0 );   /* langage */
                bo_add_32le( p_bo, 0 );   /* initial frame */
                bo_add_32le( p_bo, i_scale );/* scale */
                bo_add_32le( p_bo, i_rate );
                bo_add_32le( p_bo, 0 );   /* start */
                bo_add_32le( p_bo, p_stream->i_frames );
                bo_add_32le( p_bo, 10 * 1024 );
                bo_add_32le( p_bo, -1 );  /* quality */
                bo_add_32le( p_bo, i_samplesize );
                bo_add_16le(  p_bo, 0 );   /* ??? */
                bo_add_16le(  p_bo, 0 );   /* ??? */
                bo_add_16le(  p_bo, 0 );
                bo_add_16le(  p_bo, 0 );
            }
            break;
    }

    AVI_BOX_EXIT( 0 );
}

static int avi_HeaderAdd_strf( bo_t *p_bo, avi_stream_t *p_stream )
{
    AVI_BOX_ENTER( "strf" );

    switch( p_stream->i_cat )
    {
        case AUDIO_ES:
            bo_add_16le( p_bo, p_stream->p_wf->wFormatTag );
            bo_add_16le( p_bo, p_stream->p_wf->nChannels );
            bo_add_32le( p_bo, p_stream->p_wf->nSamplesPerSec );
            bo_add_32le( p_bo, p_stream->p_wf->nAvgBytesPerSec );
            bo_add_16le( p_bo, p_stream->p_wf->nBlockAlign );
            bo_add_16le( p_bo, p_stream->p_wf->wBitsPerSample );
            bo_add_16le( p_bo, p_stream->p_wf->cbSize );
            bo_add_mem( p_bo, p_stream->p_wf->cbSize, (uint8_t*)&p_stream->p_wf[1] );
            break;
        case VIDEO_ES:
            bo_add_32le( p_bo, p_stream->p_bih->biSize );
            bo_add_32le( p_bo, p_stream->p_bih->biWidth );
            bo_add_32le( p_bo, p_stream->p_bih->biHeight );
            bo_add_16le( p_bo, p_stream->p_bih->biPlanes );
            bo_add_16le( p_bo, p_stream->p_bih->biBitCount );
#ifdef WORDS_BIGENDIAN
                bo_add_32be( p_bo, p_stream->p_bih->biCompression );
#else
                bo_add_32le( p_bo, p_stream->p_bih->biCompression );
#endif
            bo_add_32le( p_bo, p_stream->p_bih->biSizeImage );
            bo_add_32le( p_bo, p_stream->p_bih->biXPelsPerMeter );
            bo_add_32le( p_bo, p_stream->p_bih->biYPelsPerMeter );
            bo_add_32le( p_bo, p_stream->p_bih->biClrUsed );
            bo_add_32le( p_bo, p_stream->p_bih->biClrImportant );
            bo_add_mem( p_bo,
                        p_stream->i_bih - sizeof( VLC_BITMAPINFOHEADER ),
                        (uint8_t*)&p_stream->p_bih[1] );
            break;
    }

    AVI_BOX_EXIT( 0 );
}

static int avi_HeaderAdd_strl( bo_t *p_bo, avi_stream_t *p_stream )
{
    AVI_BOX_ENTER_LIST( "strl" );

    avi_HeaderAdd_strh( p_bo, p_stream );
    avi_HeaderAdd_strf( p_bo, p_stream );

    AVI_BOX_EXIT( 0 );
}

static int avi_HeaderAdd_meta( bo_t *p_bo, const char psz_meta[4],
                               const char *psz_data )
{
    if ( psz_data == NULL ) return 1;
    const char *psz = psz_data;
    AVI_BOX_ENTER( psz_meta );
    while (*psz) bo_add_8( p_bo, *psz++ );
    bo_add_8( p_bo, 0 );
    AVI_BOX_EXIT( 0 );
}

static int avi_HeaderAdd_INFO( sout_mux_t *p_mux, bo_t *p_bo )
{
    char *psz;

#define APPLY_META(var, fourcc) \
    psz = var_InheritString( p_mux, SOUT_CFG_PREFIX var );\
    if ( psz )\
    {\
        avi_HeaderAdd_meta( p_bo, fourcc, psz );\
        free( psz );\
    }

    AVI_BOX_ENTER_LIST( "INFO" );

    APPLY_META( "artist",   "IART")
    APPLY_META( "comment",  "ICMT")
    APPLY_META( "copyright","ICOP")
    APPLY_META( "date",     "ICRD")
    APPLY_META( "genre",    "IGNR")
    APPLY_META( "name",     "INAM")
    APPLY_META( "keywords", "IKEY")
    APPLY_META( "subject",  "ISBJ")
    APPLY_META( "encoder",  "ISFT")
    /* Some are missing, but are they really useful ?? */

#undef APPLY_META

    AVI_BOX_EXIT( 0 );
}

static block_t *avi_HeaderCreateRIFF( sout_mux_t *p_mux )
{
    sout_mux_sys_t      *p_sys = p_mux->p_sys;
    int                 i_stream;
    int                 i_junk;
    bo_t                bo;

    struct
    {
        int i_riffsize;
        int i_hdrllistsize;
        int i_hdrldatastart;
    } offsets;

    if (! bo_init( &bo, HDR_BASE_SIZE ) )
        return NULL;

    bo_add_fourcc( &bo, "RIFF" );
    offsets.i_riffsize = bo.b->i_buffer;
    bo_add_32le( &bo, 0xEFBEADDE );
    bo_add_fourcc( &bo, "AVI " );

    bo_add_fourcc( &bo, "LIST" );
    /* HDRL List size should exclude following data in HDR buffer
     *  -12 (RIFF, RIFF size, 'AVI ' tag),
     *  - 8 (hdr1 LIST tag and its size)
     *  - 12 (movi LIST tag, size, 'movi' listType )
     */
    offsets.i_hdrllistsize = bo.b->i_buffer;
    bo_add_32le( &bo, 0xEFBEADDE );
    bo_add_fourcc( &bo, "hdrl" );
    offsets.i_hdrldatastart = bo.b->i_buffer;

    avi_HeaderAdd_avih( p_mux, &bo );
    for( i_stream = 0; i_stream < p_sys->i_streams; i_stream++ )
    {
        avi_HeaderAdd_strl( &bo, &p_sys->stream[i_stream] );
    }

    /* align on 16 bytes */
    int i_align = ( ( bo.b->i_buffer + 12 + 0xE ) & ~ 0xF );
    i_junk = i_align - bo.b->i_buffer;
    bo_add_fourcc( &bo, "JUNK" );
    bo_add_32le( &bo, i_junk );
    for( int i=0; i< i_junk; i++ )
    {
        bo_add_8( &bo, 0 );
    }

    /* Now set hdrl size */
    bo_set_32le( &bo, offsets.i_hdrllistsize,
                 bo.b->i_buffer - offsets.i_hdrldatastart );

    avi_HeaderAdd_INFO( p_mux, &bo );

    bo_add_fourcc( &bo, "LIST" );
    bo_add_32le( &bo, p_sys->i_movi_size + 4 );
    bo_add_fourcc( &bo, "movi" );

    /* Now set RIFF size */
    bo_set_32le( &bo, offsets.i_riffsize, bo.b->i_buffer - 8
                 + p_sys->i_movi_size + p_sys->i_idx1_size );

    return( bo.b );
}

static block_t * avi_HeaderCreateidx1( sout_mux_t *p_mux )
{
    sout_mux_sys_t      *p_sys = p_mux->p_sys;
    uint32_t            i_idx1_size;
    bo_t                bo;

    i_idx1_size = 16 * p_sys->idx1.i_entry_count + 8;

    if (!i_idx1_size || !bo_init( &bo, i_idx1_size ) )
        return NULL;
    memset( bo.b->p_buffer, 0, i_idx1_size);

    bo_add_fourcc( &bo, "idx1" );
    bo_add_32le( &bo, i_idx1_size - 8);

    for( unsigned i = 0; i < p_sys->idx1.i_entry_count; i++ )
    {
        bo_add_fourcc( &bo, p_sys->idx1.entry[i].fcc );
        bo_add_32le( &bo, p_sys->idx1.entry[i].i_flags );
        bo_add_32le( &bo, p_sys->idx1.entry[i].i_pos );
        bo_add_32le( &bo, p_sys->idx1.entry[i].i_length );
    }

    return( bo.b );
}
