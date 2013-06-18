/*****************************************************************************
 * video.c: video decoder using the libavcodec library
 *****************************************************************************
 * Copyright (C) 1999-2001 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_codec.h>
#include <vlc_avcodec.h>
#include <vlc_cpu.h>
#include <vlc_modules.h>
#include <assert.h>

#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#include <libavutil/pixdesc.h>

#include "avcodec.h"
#include "va.h"

/*****************************************************************************
 * decoder_sys_t : decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    AVCODEC_COMMON_MEMBERS

    /* Video decoder specific part */
    mtime_t i_pts;

    AVFrame          *p_ff_pic;

    /* for frame skipping algo */
    bool b_hurry_up;
    enum AVDiscard i_skip_frame;
    enum AVDiscard i_skip_idct;

    /* how many decoded frames are late */
    int     i_late_frames;
    mtime_t i_late_frames_start;

    /* for direct rendering */
    bool b_direct_rendering;
    int  i_direct_rendering_used;

    bool b_has_b_frames;

    /* Hack to force display of still pictures */
    bool b_first_frame;


    /* */
#if LIBAVCODEC_VERSION_MAJOR < 54
    AVPaletteControl palette;
#else
# warning FIXME
#endif

    /* */
    bool b_flush;

    /* VA API */
    vlc_va_t *p_va;

    vlc_sem_t sem_mt;
};

#ifdef HAVE_AVCODEC_MT
#   define wait_mt(s) vlc_sem_wait( &s->sem_mt )
#   define post_mt(s) vlc_sem_post( &s->sem_mt )
#else
#   define wait_mt(s)
#   define post_mt(s)
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void ffmpeg_InitCodec      ( decoder_t * );
static void ffmpeg_CopyPicture    ( decoder_t *, picture_t *, AVFrame * );
static int  ffmpeg_GetFrameBuf    ( struct AVCodecContext *, AVFrame * );
static void ffmpeg_ReleaseFrameBuf( struct AVCodecContext *, AVFrame * );
static enum PixelFormat ffmpeg_GetFormat( AVCodecContext *,
                                          const enum PixelFormat * );
static void vlc_va_Delete( vlc_va_t * );

static uint32_t ffmpeg_CodecTag( vlc_fourcc_t fcc )
{
    uint8_t *p = (uint8_t*)&fcc;
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

/*****************************************************************************
 * Local Functions
 *****************************************************************************/

/* Returns a new picture buffer */
static inline picture_t *ffmpeg_NewPictBuf( decoder_t *p_dec,
                                            AVCodecContext *p_context )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    p_dec->fmt_out.video.i_width = p_context->width;
    p_dec->fmt_out.video.i_height = p_context->height;

    if( !p_context->width || !p_context->height )
    {
        return NULL; /* invalid display size */
    }

    if( !p_sys->p_va && GetVlcChroma( &p_dec->fmt_out.video, p_context->pix_fmt ) )
    {
        /* we are doomed, but not really, because most codecs set their pix_fmt
         * much later
         * FIXME does it make sense here ? */
        p_dec->fmt_out.video.i_chroma = VLC_CODEC_I420;
    }
    p_dec->fmt_out.i_codec = p_dec->fmt_out.video.i_chroma;

    /* If an aspect-ratio was specified in the input format then force it */
    if( p_dec->fmt_in.video.i_sar_num > 0 && p_dec->fmt_in.video.i_sar_den > 0 )
    {
        p_dec->fmt_out.video.i_sar_num = p_dec->fmt_in.video.i_sar_num;
        p_dec->fmt_out.video.i_sar_den = p_dec->fmt_in.video.i_sar_den;
    }
    else
    {
        p_dec->fmt_out.video.i_sar_num = p_context->sample_aspect_ratio.num;
        p_dec->fmt_out.video.i_sar_den = p_context->sample_aspect_ratio.den;

        if( !p_dec->fmt_out.video.i_sar_num || !p_dec->fmt_out.video.i_sar_den )
        {
            p_dec->fmt_out.video.i_sar_num = 1;
            p_dec->fmt_out.video.i_sar_den = 1;
        }
    }

    if( p_dec->fmt_in.video.i_frame_rate > 0 &&
        p_dec->fmt_in.video.i_frame_rate_base > 0 )
    {
        p_dec->fmt_out.video.i_frame_rate =
            p_dec->fmt_in.video.i_frame_rate;
        p_dec->fmt_out.video.i_frame_rate_base =
            p_dec->fmt_in.video.i_frame_rate_base;
    }
    else if( p_context->time_base.num > 0 && p_context->time_base.den > 0 )
    {
        p_dec->fmt_out.video.i_frame_rate = p_context->time_base.den;
        p_dec->fmt_out.video.i_frame_rate_base = p_context->time_base.num * __MAX( p_context->ticks_per_frame, 1 );
    }

    return decoder_NewPicture( p_dec );
}

/*****************************************************************************
 * InitVideo: initialize the video decoder
 *****************************************************************************
 * the ffmpeg codec will be opened, some memory allocated. The vout is not yet
 * opened (done after the first decoded frame).
 *****************************************************************************/
int InitVideoDec( decoder_t *p_dec, AVCodecContext *p_context,
                      AVCodec *p_codec, int i_codec_id, const char *psz_namecodec )
{
    decoder_sys_t *p_sys;
    int i_val;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = calloc( 1, sizeof(decoder_sys_t) ) ) == NULL )
        return VLC_ENOMEM;

    p_codec->type = AVMEDIA_TYPE_VIDEO;
    p_context->codec_type = AVMEDIA_TYPE_VIDEO;
    p_context->codec_id = i_codec_id;
    p_sys->p_context = p_context;
    p_sys->p_codec = p_codec;
    p_sys->i_codec_id = i_codec_id;
    p_sys->psz_namecodec = psz_namecodec;
    p_sys->p_ff_pic = avcodec_alloc_frame();
    p_sys->b_delayed_open = true;
    p_sys->p_va = NULL;
    vlc_sem_init( &p_sys->sem_mt, 0 );

    /* ***** Fill p_context with init values ***** */
    p_sys->p_context->codec_tag = ffmpeg_CodecTag( p_dec->fmt_in.i_original_fourcc ?: p_dec->fmt_in.i_codec );

    /*  ***** Get configuration of ffmpeg plugin ***** */
    p_sys->p_context->workaround_bugs =
        var_InheritInteger( p_dec, "avcodec-workaround-bugs" );
    p_sys->p_context->err_recognition =
        var_InheritInteger( p_dec, "avcodec-error-resilience" );

    if( var_CreateGetBool( p_dec, "grayscale" ) )
        p_sys->p_context->flags |= CODEC_FLAG_GRAY;

    i_val = var_CreateGetInteger( p_dec, "avcodec-vismv" );
    if( i_val ) p_sys->p_context->debug_mv = i_val;

    i_val = var_CreateGetInteger( p_dec, "avcodec-skiploopfilter" );
    if( i_val >= 4 ) p_sys->p_context->skip_loop_filter = AVDISCARD_ALL;
    else if( i_val == 3 ) p_sys->p_context->skip_loop_filter = AVDISCARD_NONKEY;
    else if( i_val == 2 ) p_sys->p_context->skip_loop_filter = AVDISCARD_BIDIR;
    else if( i_val == 1 ) p_sys->p_context->skip_loop_filter = AVDISCARD_NONREF;

    if( var_CreateGetBool( p_dec, "avcodec-fast" ) )
        p_sys->p_context->flags2 |= CODEC_FLAG2_FAST;

#if LIBAVCODEC_VERSION_CHECK( 54, 41, 0, 91, 100 )
    if( var_InheritBool( p_dec, "avcodec-ignorecrop" ) )
        p_sys->p_context->flags2 |= CODEC_FLAG2_IGNORE_CROP;
#endif

    /* ***** libavcodec frame skipping ***** */
    p_sys->b_hurry_up = var_CreateGetBool( p_dec, "avcodec-hurry-up" );

    switch( var_CreateGetInteger( p_dec, "avcodec-skip-frame" ) )
    {
        case -1:
            p_sys->p_context->skip_frame = AVDISCARD_NONE;
            break;
        case 0:
            p_sys->p_context->skip_frame = AVDISCARD_DEFAULT;
            break;
        case 1:
            p_sys->p_context->skip_frame = AVDISCARD_NONREF;
            break;
        case 2:
            p_sys->p_context->skip_frame = AVDISCARD_NONKEY;
            break;
        case 3:
            p_sys->p_context->skip_frame = AVDISCARD_ALL;
            break;
        default:
            p_sys->p_context->skip_frame = AVDISCARD_NONE;
            break;
    }
    p_sys->i_skip_frame = p_sys->p_context->skip_frame;

    switch( var_CreateGetInteger( p_dec, "avcodec-skip-idct" ) )
    {
        case -1:
            p_sys->p_context->skip_idct = AVDISCARD_NONE;
            break;
        case 0:
            p_sys->p_context->skip_idct = AVDISCARD_DEFAULT;
            break;
        case 1:
            p_sys->p_context->skip_idct = AVDISCARD_NONREF;
            break;
        case 2:
            p_sys->p_context->skip_idct = AVDISCARD_NONKEY;
            break;
        case 3:
            p_sys->p_context->skip_idct = AVDISCARD_ALL;
            break;
        default:
            p_sys->p_context->skip_idct = AVDISCARD_NONE;
            break;
    }
    p_sys->i_skip_idct = p_sys->p_context->skip_idct;

    /* ***** libavcodec direct rendering ***** */
    p_sys->b_direct_rendering = false;
    p_sys->i_direct_rendering_used = -1;
    if( var_CreateGetBool( p_dec, "avcodec-dr" ) &&
       (p_sys->p_codec->capabilities & CODEC_CAP_DR1) &&
        /* No idea why ... but this fixes flickering on some TSCC streams */
        p_sys->i_codec_id != AV_CODEC_ID_TSCC && p_sys->i_codec_id != AV_CODEC_ID_CSCD &&
        p_sys->i_codec_id != AV_CODEC_ID_CINEPAK &&
        !p_sys->p_context->debug_mv )
    {
        /* Some codecs set pix_fmt only after the 1st frame has been decoded,
         * so we need to do another check in ffmpeg_GetFrameBuf() */
        p_sys->b_direct_rendering = true;
    }

    /* libavcodec doesn't properly release old pictures when frames are skipped */
    //if( p_sys->b_hurry_up ) p_sys->b_direct_rendering = false;
    if( p_sys->b_direct_rendering )
    {
        msg_Dbg( p_dec, "trying to use direct rendering" );
        p_sys->p_context->flags |= CODEC_FLAG_EMU_EDGE;
    }
    else
    {
        msg_Dbg( p_dec, "direct rendering is disabled" );
    }

    /* Always use our get_buffer wrapper so we can calculate the
     * PTS correctly */
    p_sys->p_context->get_buffer = ffmpeg_GetFrameBuf;
    p_sys->p_context->reget_buffer = avcodec_default_reget_buffer;
    p_sys->p_context->release_buffer = ffmpeg_ReleaseFrameBuf;
    p_sys->p_context->opaque = p_dec;

#ifdef HAVE_AVCODEC_MT
    int i_thread_count = var_InheritInteger( p_dec, "avcodec-threads" );
    if( i_thread_count <= 0 )
    {
        i_thread_count = vlc_GetCPUCount();
        if( i_thread_count > 1 )
            i_thread_count++;

        //FIXME: take in count the decoding time
        i_thread_count = __MIN( i_thread_count, 4 );
    }
    i_thread_count = __MIN( i_thread_count, 16 );
    msg_Dbg( p_dec, "allowing %d thread(s) for decoding", i_thread_count );
    p_sys->p_context->thread_count = i_thread_count;

    if( i_codec_id == AV_CODEC_ID_MPEG4 )
        p_sys->p_context->thread_count = 1;

#endif

    char *hw = var_CreateGetString( p_dec, "avcodec-hw" ); /* FIXME */
    if( (hw == NULL || strcasecmp( hw, "none" )) &&
        (i_codec_id == AV_CODEC_ID_MPEG1VIDEO || i_codec_id == AV_CODEC_ID_MPEG2VIDEO ||
         i_codec_id == AV_CODEC_ID_MPEG4 || i_codec_id == AV_CODEC_ID_H263 ||
         i_codec_id == AV_CODEC_ID_H264 ||
         i_codec_id == AV_CODEC_ID_VC1 || i_codec_id == AV_CODEC_ID_WMV3) )
    {
#if defined(HAVE_AVCODEC_MT) && LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 1, 0)
        if( p_sys->p_context->thread_type & FF_THREAD_FRAME )
        {
            msg_Warn( p_dec, "threaded frame decoding is not compatible with avcodec-hw, disabled" );
            p_sys->p_context->thread_type &= ~FF_THREAD_FRAME;
        }
        if( ( p_sys->p_context->thread_type & FF_THREAD_SLICE ) &&
            ( i_codec_id == AV_CODEC_ID_MPEG1VIDEO || i_codec_id == AV_CODEC_ID_MPEG2VIDEO ) )
        {
            msg_Warn( p_dec, "threaded slice decoding is not compatible with avcodec-hw, disabled" );
            p_sys->p_context->thread_type &= ~FF_THREAD_SLICE;
        }
#endif
        p_sys->p_context->get_format = ffmpeg_GetFormat;
    }
    free( hw );
#ifdef HAVE_AVCODEC_MT
    if( p_sys->p_context->thread_type & FF_THREAD_FRAME )
        p_dec->i_extra_picture_buffers = 2 * p_sys->p_context->thread_count;
#endif


    /* ***** misc init ***** */
    p_sys->i_pts = VLC_TS_INVALID;
    p_sys->b_has_b_frames = false;
    p_sys->b_first_frame = true;
    p_sys->b_flush = false;
    p_sys->i_late_frames = 0;

    /* Set output properties */
    p_dec->fmt_out.i_cat = VIDEO_ES;
    if( GetVlcChroma( &p_dec->fmt_out.video, p_context->pix_fmt ) != VLC_SUCCESS )
    {
        /* we are doomed. but not really, because most codecs set their pix_fmt later on */
        p_dec->fmt_out.i_codec = VLC_CODEC_I420;
    }
    p_dec->fmt_out.i_codec = p_dec->fmt_out.video.i_chroma;

#if LIBAVCODEC_VERSION_MAJOR < 54
    /* Setup palette */
    memset( &p_sys->palette, 0, sizeof(p_sys->palette) );
    if( p_dec->fmt_in.video.p_palette )
    {
        p_sys->palette.palette_changed = 1;

        for( int i = 0; i < __MIN( AVPALETTE_COUNT, p_dec->fmt_in.video.p_palette->i_entries ); i++ )
        {
            union {
                uint32_t u;
                uint8_t a[4];
            } c;
            c.a[0] = p_dec->fmt_in.video.p_palette->palette[i][0];
            c.a[1] = p_dec->fmt_in.video.p_palette->palette[i][1];
            c.a[2] = p_dec->fmt_in.video.p_palette->palette[i][2];
            c.a[3] = p_dec->fmt_in.video.p_palette->palette[i][3];

            p_sys->palette.palette[i] = c.u;
        }
        p_sys->p_context->palctrl = &p_sys->palette;

        p_dec->fmt_out.video.p_palette = malloc( sizeof(video_palette_t) );
        if( p_dec->fmt_out.video.p_palette )
            *p_dec->fmt_out.video.p_palette = *p_dec->fmt_in.video.p_palette;
    }
    else if( p_sys->i_codec_id != CODEC_ID_MSVIDEO1 && p_sys->i_codec_id != CODEC_ID_CINEPAK )
    {
        p_sys->p_context->palctrl = &p_sys->palette;
    }
#else
# warning FIXME
#endif

    /* ***** init this codec with special data ***** */
    ffmpeg_InitCodec( p_dec );

    /* ***** Open the codec ***** */
    if( ffmpeg_OpenCodec( p_dec ) < 0 )
    {
        msg_Err( p_dec, "cannot open codec (%s)", p_sys->psz_namecodec );
        av_free( p_sys->p_ff_pic );
        vlc_sem_destroy( &p_sys->sem_mt );
        free( p_sys );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DecodeVideo: Called to decode one or more frames
 *****************************************************************************/
picture_t *DecodeVideo( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    AVCodecContext *p_context = p_sys->p_context;
    int b_drawpicture;
    int b_null_size = false;
    block_t *p_block;

    if( !pp_block || !*pp_block )
        return NULL;

    if( !p_context->extradata_size && p_dec->fmt_in.i_extra )
    {
        ffmpeg_InitCodec( p_dec );
        if( p_sys->b_delayed_open )
        {
            if( ffmpeg_OpenCodec( p_dec ) )
                msg_Err( p_dec, "cannot open codec (%s)", p_sys->psz_namecodec );
        }
    }

    p_block = *pp_block;
    if( p_sys->b_delayed_open )
    {
        block_Release( p_block );
        return NULL;
    }

    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        p_sys->i_pts = VLC_TS_INVALID; /* To make sure we recover properly */

        p_sys->i_late_frames = 0;

        if( p_block->i_flags & BLOCK_FLAG_DISCONTINUITY )
            avcodec_flush_buffers( p_context );

        block_Release( p_block );
        return NULL;
    }

    if( p_block->i_flags & BLOCK_FLAG_PREROLL )
    {
        /* Do not care about late frames when prerolling
         * TODO avoid decoding of non reference frame
         * (ie all B except for H264 where it depends only on nal_ref_idc) */
        p_sys->i_late_frames = 0;
    }

    if( !p_dec->b_pace_control && (p_sys->i_late_frames > 0) &&
        (mdate() - p_sys->i_late_frames_start > INT64_C(5000000)) )
    {
        if( p_sys->i_pts > VLC_TS_INVALID )
        {
            msg_Err( p_dec, "more than 5 seconds of late video -> "
                     "dropping frame (computer too slow ?)" );
            p_sys->i_pts = VLC_TS_INVALID; /* To make sure we recover properly */
        }
        block_Release( p_block );
        p_sys->i_late_frames--;
        return NULL;
    }

    /* A good idea could be to decode all I pictures and see for the other */
    if( !p_dec->b_pace_control &&
        p_sys->b_hurry_up &&
        (p_sys->i_late_frames > 4) )
    {
        b_drawpicture = 0;
        if( p_sys->i_late_frames < 12 )
        {
            p_context->skip_frame =
                    (p_sys->i_skip_frame <= AVDISCARD_NONREF) ?
                    AVDISCARD_NONREF : p_sys->i_skip_frame;
        }
        else
        {
            /* picture too late, won't decode
             * but break picture until a new I, and for mpeg4 ...*/
            p_sys->i_late_frames--; /* needed else it will never be decrease */
            block_Release( p_block );
            return NULL;
        }
    }
    else
    {
        if( p_sys->b_hurry_up )
            p_context->skip_frame = p_sys->i_skip_frame;
        if( !(p_block->i_flags & BLOCK_FLAG_PREROLL) )
            b_drawpicture = 1;
        else
            b_drawpicture = 0;
    }

    if( p_context->width <= 0 || p_context->height <= 0 )
    {
        if( p_sys->b_hurry_up )
            p_context->skip_frame = p_sys->i_skip_frame;
        b_null_size = true;
    }
    else if( !b_drawpicture )
    {
        /* It creates broken picture
         * FIXME either our parser or ffmpeg is broken */
#if 0
        if( p_sys->b_hurry_up )
            p_context->skip_frame = __MAX( p_context->skip_frame,
                                                  AVDISCARD_NONREF );
#endif
    }

    /*
     * Do the actual decoding now */

    /* Don't forget that libavcodec requires a little more bytes
     * that the real frame size */
    if( p_block->i_buffer > 0 )
    {
        p_sys->b_flush = ( p_block->i_flags & BLOCK_FLAG_END_OF_SEQUENCE ) != 0;

        p_block = block_Realloc( p_block, 0,
                            p_block->i_buffer + FF_INPUT_BUFFER_PADDING_SIZE );
        if( !p_block )
            return NULL;
        p_block->i_buffer -= FF_INPUT_BUFFER_PADDING_SIZE;
        *pp_block = p_block;
        memset( p_block->p_buffer + p_block->i_buffer, 0,
                FF_INPUT_BUFFER_PADDING_SIZE );
    }

    while( p_block->i_buffer > 0 || p_sys->b_flush )
    {
        int i_used, b_gotpicture;
        picture_t *p_pic;
        AVPacket pkt;

        post_mt( p_sys );

        av_init_packet( &pkt );
        pkt.data = p_block->p_buffer;
        pkt.size = p_block->i_buffer;
        pkt.pts = p_block->i_pts;
        pkt.dts = p_block->i_dts;

        /* Make sure we don't reuse the same timestamps twice */
        p_block->i_pts =
        p_block->i_dts = VLC_TS_INVALID;

        i_used = avcodec_decode_video2( p_context, p_sys->p_ff_pic,
                                       &b_gotpicture, &pkt );

        if( b_null_size && !p_sys->b_flush &&
            p_context->width > 0 && p_context->height > 0 )
        {
            /* Reparse it to not drop the I frame */
            b_null_size = false;
            if( p_sys->b_hurry_up )
                p_context->skip_frame = p_sys->i_skip_frame;
            i_used = avcodec_decode_video2( p_context, p_sys->p_ff_pic,
                                           &b_gotpicture, &pkt );
        }
        wait_mt( p_sys );

        if( p_sys->b_flush )
            p_sys->b_first_frame = true;

        if( p_block->i_buffer <= 0 )
            p_sys->b_flush = false;

        if( i_used < 0 )
        {
            if( b_drawpicture )
                msg_Warn( p_dec, "cannot decode one frame (%zu bytes)",
                          p_block->i_buffer );
            block_Release( p_block );
            return NULL;
        }
        else if( i_used > p_block->i_buffer ||
                 p_context->thread_count > 1 )
        {
            i_used = p_block->i_buffer;
        }

        /* Consumed bytes */
        p_block->i_buffer -= i_used;
        p_block->p_buffer += i_used;

        /* Nothing to display */
        if( !b_gotpicture )
        {
            if( i_used == 0 ) break;
            continue;
        }

        /* Sanity check (seems to be needed for some streams) */
        if( p_sys->p_ff_pic->pict_type == AV_PICTURE_TYPE_B)
        {
            p_sys->b_has_b_frames = true;
        }

        /* Compute the PTS */
        mtime_t i_pts =
                    p_sys->p_ff_pic->pkt_pts;
        if (i_pts <= VLC_TS_INVALID)
            i_pts = p_sys->p_ff_pic->pkt_dts;

        if( i_pts <= VLC_TS_INVALID )
            i_pts = p_sys->i_pts;

        /* Interpolate the next PTS */
        if( i_pts > VLC_TS_INVALID )
            p_sys->i_pts = i_pts;
        if( p_sys->i_pts > VLC_TS_INVALID )
        {
            /* interpolate the next PTS */
            if( p_dec->fmt_in.video.i_frame_rate > 0 &&
                p_dec->fmt_in.video.i_frame_rate_base > 0 )
            {
                p_sys->i_pts += INT64_C(1000000) *
                    (2 + p_sys->p_ff_pic->repeat_pict) *
                    p_dec->fmt_in.video.i_frame_rate_base /
                    (2 * p_dec->fmt_in.video.i_frame_rate);
            }
            else if( p_context->time_base.den > 0 )
            {
                int i_tick = p_context->ticks_per_frame;
                if( i_tick <= 0 )
                    i_tick = 1;

                p_sys->i_pts += INT64_C(1000000) *
                    (2 + p_sys->p_ff_pic->repeat_pict) *
                    i_tick * p_context->time_base.num /
                    (2 * p_context->time_base.den);
            }
        }

        /* Update frame late count (except when doing preroll) */
        mtime_t i_display_date = 0;
        if( !(p_block->i_flags & BLOCK_FLAG_PREROLL) )
            i_display_date = decoder_GetDisplayDate( p_dec, i_pts );

        if( i_display_date > 0 && i_display_date <= mdate() )
        {
            p_sys->i_late_frames++;
            if( p_sys->i_late_frames == 1 )
                p_sys->i_late_frames_start = mdate();
        }
        else
        {
            p_sys->i_late_frames = 0;
        }

        if( !b_drawpicture || ( !p_sys->p_va && !p_sys->p_ff_pic->linesize[0] ) )
            continue;

        if( p_sys->p_va != NULL || p_sys->p_ff_pic->opaque == NULL )
        {
            /* Get a new picture */
            p_pic = ffmpeg_NewPictBuf( p_dec, p_context );
            if( !p_pic )
            {
                block_Release( p_block );
                return NULL;
            }

            /* Fill p_picture_t from AVVideoFrame and do chroma conversion
             * if needed */
            ffmpeg_CopyPicture( p_dec, p_pic, p_sys->p_ff_pic );
        }
        else
        {
            p_pic = (picture_t *)p_sys->p_ff_pic->opaque;
            decoder_LinkPicture( p_dec, p_pic );
        }

        if( !p_dec->fmt_in.video.i_sar_num || !p_dec->fmt_in.video.i_sar_den )
        {
            /* Fetch again the aspect ratio in case it changed */
            p_dec->fmt_out.video.i_sar_num
                = p_context->sample_aspect_ratio.num;
            p_dec->fmt_out.video.i_sar_den
                = p_context->sample_aspect_ratio.den;

            if( !p_dec->fmt_out.video.i_sar_num || !p_dec->fmt_out.video.i_sar_den )
            {
                p_dec->fmt_out.video.i_sar_num = 1;
                p_dec->fmt_out.video.i_sar_den = 1;
            }
        }

        /* Send decoded frame to vout */
        if( i_pts > VLC_TS_INVALID)
        {
            p_pic->date = i_pts;

            if( p_sys->b_first_frame )
            {
                /* Hack to force display of still pictures */
                p_sys->b_first_frame = false;
                p_pic->b_force = true;
            }

            p_pic->i_nb_fields = 2 + p_sys->p_ff_pic->repeat_pict;
            p_pic->b_progressive = !p_sys->p_ff_pic->interlaced_frame;
            p_pic->b_top_field_first = p_sys->p_ff_pic->top_field_first;

            return p_pic;
        }
        else
        {
            decoder_DeletePicture( p_dec, p_pic );
        }
    }

    block_Release( p_block );
    return NULL;
}

/*****************************************************************************
 * EndVideo: decoder destruction
 *****************************************************************************
 * This function is called when the thread ends after a successful
 * initialization.
 *****************************************************************************/
void EndVideoDec( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    post_mt( p_sys );

    /* do not flush buffers if codec hasn't been opened (theora/vorbis/VC1) */
    if( p_sys->p_context->codec )
        avcodec_flush_buffers( p_sys->p_context );

    wait_mt( p_sys );

    if( p_sys->p_ff_pic )
        av_free( p_sys->p_ff_pic );

    if( p_sys->p_va )
        vlc_va_Delete( p_sys->p_va );

    vlc_sem_destroy( &p_sys->sem_mt );
}

/*****************************************************************************
 * ffmpeg_InitCodec: setup codec extra initialization data for ffmpeg
 *****************************************************************************/
static void ffmpeg_InitCodec( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_size = p_dec->fmt_in.i_extra;

    if( !i_size ) return;

    if( p_sys->i_codec_id == AV_CODEC_ID_SVQ3 )
    {
        uint8_t *p;

        p_sys->p_context->extradata_size = i_size + 12;
        p = p_sys->p_context->extradata  =
            malloc( p_sys->p_context->extradata_size );
        if( !p )
            return;

        memcpy( &p[0],  "SVQ3", 4 );
        memset( &p[4], 0, 8 );
        memcpy( &p[12], p_dec->fmt_in.p_extra, i_size );

        /* Now remove all atoms before the SMI one */
        if( p_sys->p_context->extradata_size > 0x5a &&
            strncmp( (char*)&p[0x56], "SMI ", 4 ) )
        {
            uint8_t *psz = &p[0x52];

            while( psz < &p[p_sys->p_context->extradata_size - 8] )
            {
                int i_size = GetDWBE( psz );
                if( i_size <= 1 )
                {
                    /* FIXME handle 1 as long size */
                    break;
                }
                if( !strncmp( (char*)&psz[4], "SMI ", 4 ) )
                {
                    memmove( &p[0x52], psz,
                             &p[p_sys->p_context->extradata_size] - psz );
                    break;
                }

                psz += i_size;
            }
        }
    }
    else
    {
        p_sys->p_context->extradata_size = i_size;
        p_sys->p_context->extradata =
            malloc( i_size + FF_INPUT_BUFFER_PADDING_SIZE );
        if( p_sys->p_context->extradata )
        {
            memcpy( p_sys->p_context->extradata,
                    p_dec->fmt_in.p_extra, i_size );
            memset( &((uint8_t*)p_sys->p_context->extradata)[i_size],
                    0, FF_INPUT_BUFFER_PADDING_SIZE );
        }
    }
}

/*****************************************************************************
 * ffmpeg_CopyPicture: copy a picture from ffmpeg internal buffers to a
 *                     picture_t structure (when not in direct rendering mode).
 *****************************************************************************/
static void ffmpeg_CopyPicture( decoder_t *p_dec,
                                picture_t *p_pic, AVFrame *p_ff_pic )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_va )
    {
        vlc_va_Extract( p_sys->p_va, p_pic, p_ff_pic );
    }
    else if( FindVlcChroma( p_sys->p_context->pix_fmt ) )
    {
        int i_plane, i_size, i_line;
        uint8_t *p_dst, *p_src;
        int i_src_stride, i_dst_stride;

        for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
        {
            p_src  = p_ff_pic->data[i_plane];
            p_dst = p_pic->p[i_plane].p_pixels;
            i_src_stride = p_ff_pic->linesize[i_plane];
            i_dst_stride = p_pic->p[i_plane].i_pitch;

            i_size = __MIN( i_src_stride, i_dst_stride );
            for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines;
                 i_line++ )
            {
                memcpy( p_dst, p_src, i_size );
                p_src += i_src_stride;
                p_dst += i_dst_stride;
            }
        }
    }
    else
    {
        const char *name = av_get_pix_fmt_name( p_sys->p_context->pix_fmt );
        msg_Err( p_dec, "Unsupported decoded output format %d (%s)",
                 p_sys->p_context->pix_fmt, name ? name : "unknown" );
        p_dec->b_error = 1;
    }
}

static int ffmpeg_va_GetFrameBuf( struct AVCodecContext *p_context, AVFrame *p_ff_pic )
{
    decoder_t *p_dec = (decoder_t *)p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;
    vlc_va_t *p_va = p_sys->p_va;

    /* hwaccel_context is not present in old ffmpeg version */
    if( vlc_va_Setup( p_va,
                &p_context->hwaccel_context, &p_dec->fmt_out.video.i_chroma,
                p_context->width, p_context->height ) )
    {
        msg_Err( p_dec, "vlc_va_Setup failed" );
        return -1;
    }

    if( vlc_va_Get( p_va, p_ff_pic ) )
    {
        msg_Err( p_dec, "VaGrabSurface failed" );
        return -1;
    }

    p_ff_pic->type = FF_BUFFER_TYPE_USER;
    return 0;
}

static picture_t *ffmpeg_dr_GetFrameBuf(struct AVCodecContext *p_context)
{
    decoder_t *p_dec = (decoder_t *)p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;

    int i_width = p_context->width;
    int i_height = p_context->height;
    avcodec_align_dimensions( p_context, &i_width, &i_height );

    picture_t *p_pic = NULL;
    if (GetVlcChroma(&p_dec->fmt_out.video, p_context->pix_fmt) != VLC_SUCCESS)
        goto no_dr;

    if (p_context->pix_fmt == PIX_FMT_PAL8)
        goto no_dr;

    p_dec->fmt_out.i_codec = p_dec->fmt_out.video.i_chroma;

    p_pic = ffmpeg_NewPictBuf( p_dec, p_context );
    if( !p_pic )
        goto no_dr;

    if( p_pic->p[0].i_pitch / p_pic->p[0].i_pixel_pitch < i_width ||
        p_pic->p[0].i_lines < i_height )
        goto no_dr;

    for( int i = 0; i < p_pic->i_planes; i++ )
    {
        unsigned i_align;
        switch( p_sys->i_codec_id )
        {
        case AV_CODEC_ID_SVQ1:
        case AV_CODEC_ID_VP5:
        case AV_CODEC_ID_VP6:
        case AV_CODEC_ID_VP6F:
        case AV_CODEC_ID_VP6A:
            i_align = 16;
            break;
        default:
            i_align = i == 0 ? 16 : 8;
            break;
        }
        if( p_pic->p[i].i_pitch % i_align )
            goto no_dr;
        if( (intptr_t)p_pic->p[i].p_pixels % i_align )
            goto no_dr;
    }

    if( p_context->pix_fmt == PIX_FMT_YUV422P )
    {
        if( 2 * p_pic->p[1].i_pitch != p_pic->p[0].i_pitch ||
            2 * p_pic->p[2].i_pitch != p_pic->p[0].i_pitch )
            goto no_dr;
    }

    return p_pic;

no_dr:
    if (p_pic)
        decoder_DeletePicture( p_dec, p_pic );

    return NULL;
}

/*****************************************************************************
 * ffmpeg_GetFrameBuf: callback used by ffmpeg to get a frame buffer.
 *****************************************************************************
 * It is used for direct rendering as well as to get the right PTS for each
 * decoded picture (even in indirect rendering mode).
 *****************************************************************************/
static int ffmpeg_GetFrameBuf( struct AVCodecContext *p_context,
                               AVFrame *p_ff_pic )
{
    decoder_t *p_dec = (decoder_t *)p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;

    /* */
    p_ff_pic->opaque = NULL;
#if ! LIBAVCODEC_VERSION_CHECK(54, 34, 0, 79, 101)
    p_ff_pic->pkt_pts = p_context->pkt ? p_context->pkt->pts : AV_NOPTS_VALUE;
#endif
#if LIBAVCODEC_VERSION_MAJOR < 54
    p_ff_pic->age = 256*256*256*64;
#endif

    if( p_sys->p_va )
        return ffmpeg_va_GetFrameBuf(p_context, p_ff_pic);

    if( !p_sys->b_direct_rendering )
        return avcodec_default_get_buffer( p_context, p_ff_pic );

    wait_mt( p_sys );
    /* Some codecs set pix_fmt only after the 1st frame has been decoded,
     * so we need to check for direct rendering again. */

    picture_t *p_pic = ffmpeg_dr_GetFrameBuf(p_context);
    if (!p_pic) {
        if( p_sys->i_direct_rendering_used != 0 )
        {
            msg_Warn( p_dec, "disabling direct rendering" );
            p_sys->i_direct_rendering_used = 0;
        }

        post_mt( p_sys );
        return avcodec_default_get_buffer( p_context, p_ff_pic );
    }

    if( p_sys->i_direct_rendering_used != 1 ) {
        msg_Dbg( p_dec, "using direct rendering" );
        p_sys->i_direct_rendering_used = 1;
    }

    p_context->draw_horiz_band = NULL;
    post_mt( p_sys );

    p_ff_pic->opaque = (void*)p_pic;
    p_ff_pic->type = FF_BUFFER_TYPE_USER;
    p_ff_pic->data[0] = p_pic->p[0].p_pixels;
    p_ff_pic->data[1] = p_pic->p[1].p_pixels;
    p_ff_pic->data[2] = p_pic->p[2].p_pixels;
    p_ff_pic->data[3] = NULL; /* alpha channel but I'm not sure */

    p_ff_pic->linesize[0] = p_pic->p[0].i_pitch;
    p_ff_pic->linesize[1] = p_pic->p[1].i_pitch;
    p_ff_pic->linesize[2] = p_pic->p[2].i_pitch;
    p_ff_pic->linesize[3] = 0;

    return 0;
}

static void ffmpeg_ReleaseFrameBuf( struct AVCodecContext *p_context,
                                    AVFrame *p_ff_pic )
{
    decoder_t *p_dec = (decoder_t *)p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->p_va )
        vlc_va_Release( p_sys->p_va, p_ff_pic );
    else if( p_ff_pic->opaque )
        decoder_UnlinkPicture( p_dec, (picture_t*)p_ff_pic->opaque);
    else if( p_ff_pic->type == FF_BUFFER_TYPE_INTERNAL )
        /* We can end up here without the AVFrame being allocated by
         * avcodec_default_get_buffer() if VA is used and the frame is
         * released when the decoder is closed
         */
        avcodec_default_release_buffer( p_context, p_ff_pic );

    for( int i = 0; i < 4; i++ )
        p_ff_pic->data[i] = NULL;
}

static int ffmpeg_va_Start( void *func, va_list ap )
{
    vlc_va_t *va = va_arg( ap, vlc_va_t * );
    int codec = va_arg( ap, int );
    const es_format_t *fmt = va_arg( ap, const es_format_t * );
    int (*open)( vlc_va_t *, int, const es_format_t * ) = func;

    return open( va, codec, fmt );
}

static vlc_va_t *vlc_va_New( vlc_object_t *parent, int codec_id,
                             const es_format_t *fmt )
{
    vlc_va_t *p_va = vlc_object_create( parent, sizeof( *p_va ) );
    if( unlikely(p_va == NULL) )
        return NULL;

    p_va->module = vlc_module_load( p_va, "hw decoder", "$avcodec-hw",
                                    true, ffmpeg_va_Start, p_va,
                                    codec_id, fmt );
    if( p_va->module == NULL )
    {
        vlc_object_release( p_va );
        p_va = NULL;
    }
    return p_va;
}

static void ffmpeg_va_Stop( void *func, va_list ap )
{
    vlc_va_t *va = va_arg( ap, vlc_va_t * );
    void (*close)( vlc_va_t * ) = func;

    close( va );
}

static void vlc_va_Delete( vlc_va_t *va )
{
    vlc_module_unload( va->module, ffmpeg_va_Stop, va );
    vlc_object_release( va );
}

static enum PixelFormat ffmpeg_GetFormat( AVCodecContext *p_context,
                                          const enum PixelFormat *pi_fmt )
{
    decoder_t *p_dec = p_context->opaque;
    decoder_sys_t *p_sys = p_dec->p_sys;
    vlc_va_t *p_va = p_sys->p_va;

    if( p_va != NULL )
        vlc_va_Delete( p_va );

    /* Profile and level informations are needed now.
     * TODO: avoid code duplication with avcodec.c */
    if( p_context->profile != FF_PROFILE_UNKNOWN)
        p_dec->fmt_in.i_profile = p_context->profile;
    if( p_context->level != FF_LEVEL_UNKNOWN)
        p_dec->fmt_in.i_level = p_context->level;

    p_va = vlc_va_New( VLC_OBJECT(p_dec), p_sys->i_codec_id, &p_dec->fmt_in );
    if( p_va == NULL )
        goto end;

    /* Try too look for a supported hw acceleration */
    for( size_t i = 0; pi_fmt[i] != PIX_FMT_NONE; i++ )
    {
        const char *name = av_get_pix_fmt_name(pi_fmt[i]);
        msg_Dbg( p_dec, "Available decoder output format %d (%s)",
                 pi_fmt[i], name ? name : "unknown" );
        if( p_va->pix_fmt != pi_fmt[i] )
            continue;

        /* We try to call vlc_va_Setup when possible to detect errors when
         * possible (later is too late) */
        if( p_context->width > 0 && p_context->height > 0
         && vlc_va_Setup( p_va, &p_context->hwaccel_context,
                          &p_dec->fmt_out.video.i_chroma,
                          p_context->width, p_context->height ) )
        {
            msg_Err( p_dec, "acceleration setup failure" );
            break;
        }

        if( p_va->description )
            msg_Info( p_dec, "Using %s for hardware decoding.",
                      p_va->description );

        /* FIXME this will disable direct rendering
         * even if a new pixel format is renegotiated
         */
        p_sys->b_direct_rendering = false;
        p_sys->p_va = p_va;
        p_context->draw_horiz_band = NULL;
        return pi_fmt[i];
    }

    msg_Err( p_dec, "acceleration not available" );
    vlc_va_Delete( p_va );

    p_sys->p_va = NULL;

end:
    /* Fallback to default behaviour */
    return avcodec_default_get_format( p_context, pi_fmt );
}
