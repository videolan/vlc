/*****************************************************************************
 * ffmpeg.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ffmpeg.c,v 1.36 2003/05/09 23:23:45 fenrir Exp $
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc/decoder.h>
#include <vlc/input.h>

#include <string.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

/* ffmpeg header */
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#if LIBAVCODEC_BUILD < 4655
#   error You must have a libavcodec >= 4655 (get CVS)
#endif


#include "ffmpeg.h"

#ifdef LIBAVCODEC_PP
#   ifdef HAVE_POSTPROC_POSTPROCESS_H
#       include <postproc/postprocess.h>
#   else
#       include <libpostproc/postprocess.h>
#   endif
#else
#   include "postprocessing/postprocessing.h"
#endif

#include "video.h" // video ffmpeg specific
#include "audio.h" // audio ffmpeg specific

/*
 * Local prototypes
 */
int             E_(OpenChroma)  ( vlc_object_t * );
static int      OpenDecoder     ( vlc_object_t * );
static int      RunDecoder      ( decoder_fifo_t * );

static int      InitThread      ( generic_thread_t * );
static void     EndThread       ( generic_thread_t * );


static int      b_ffmpeginit = 0;

static int ffmpeg_GetFfmpegCodec( vlc_fourcc_t, int *, int *, char ** );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define ERROR_RESILIENCE_LONGTEXT \
    "ffmpeg can make errors resiliences.          \n" \
    "Nevertheless, with buggy encoder (like ISO MPEG-4 encoder from M$) " \
    "this will produce a lot of errors.\n" \
    "Valid range is -1 to 99 (-1 disable all errors resiliences)."

#define HURRY_UP_LONGTEXT \
    "Allow the decoder to partially decode or skip frame(s) " \
    "when there not enough time.\n It's usefull with low CPU power " \
    "but it could produce broken pictures."

#define POSTPROCESSING_Q_LONGTEXT \
    "Quality of post processing\n"\
    "Valid range is 0 to 6"

#define POSTPROCESSING_AQ_LONGTEXT \
    "Post processing quality is selected upon time left " \
    "but no more than requested quality\n" \
    "Not yet implemented !"

#define WORKAROUND_BUGS_LONGTEXT \
    "Try to fix some bugs\n" \
    "1  autodetect\n" \
    "2  old msmpeg4\n" \
    "4  xvid interlaced\n" \
    "8  ump4 \n" \
    "16 no padding\n" \
    "32 ac vlc" \
    "64 Qpel chroma"

/* FIXME (cut/past from ffmpeg */
#define LIBAVCODEC_PP_LONGHELP \
"<filterName>[:<option>[:<option>...]][[,|/][-]<filterName>[:<option>...]]...\n" \
"long form example:\n" \
"vdeblock:autoq/hdeblock:autoq/linblenddeint    default,-vdeblock\n" \
"short form example:\n" \
"vb:a/hb:a/lb                                   de,-vb\n" \
"more examples:\n" \
"tn:64:128:256\n" \
"Filters                        Options\n" \
"short  long name       short   long option     Description\n" \
"*      *               a       autoq           cpu power dependant enabler\n" \
"                       c       chrom           chrominance filtring enabled\n" \
"                       y       nochrom         chrominance filtring disabled\n" \
"hb     hdeblock        (2 Threshold)           horizontal deblocking filter\n" \
"       1. difference factor: default=64, higher -> more deblocking\n" \
"       2. flatness threshold: default=40, lower -> more deblocking\n" \
"                       the h & v deblocking filters share these\n" \
"                       so u cant set different thresholds for h / v\n" \
"vb     vdeblock        (2 Threshold)           vertical deblocking filter\n" \
"h1     x1hdeblock                              Experimental h deblock filter 1\n" \
"v1     x1vdeblock                              Experimental v deblock filter 1\n" \
"dr     dering                                  Deringing filter\n" \
"al     autolevels                              automatic brightness / contrast\n" \
"                       f       fullyrange      stretch luminance to (0..255)\n" \
"lb     linblenddeint                           linear blend deinterlacer\n" \
"li     linipoldeint                            linear interpolating deinterlace\n" \
"ci     cubicipoldeint                          cubic interpolating deinterlacer\n" \
"md     mediandeint                             median deinterlacer\n" \
"fd     ffmpegdeint                             ffmpeg deinterlacer\n" \
"de     default                                 hb:a,vb:a,dr:a,al\n" \
"fa     fast                                    h1:a,v1:a,dr:a,al\n" \
"tn     tmpnoise        (3 Thresholds)          Temporal Noise Reducer\n" \
"                       1. <= 2. <= 3.          larger -> stronger filtering\n" \
"fq     forceQuant      <quantizer>             Force quantizer\n"

vlc_module_begin();
    add_category_hint( N_("ffmpeg"), NULL, VLC_FALSE );
    set_capability( "decoder", 70 );
    set_callbacks( OpenDecoder, NULL );
    set_description( _("ffmpeg audio/video decoder((MS)MPEG4,SVQ1,H263,WMV,WMA)") );

    add_bool( "ffmpeg-dr", 0, NULL,
              "direct rendering",
              "direct rendering", VLC_TRUE );
    add_integer ( "ffmpeg-error-resilience", -1, NULL,
                  "error resilience", ERROR_RESILIENCE_LONGTEXT, VLC_TRUE );
    add_integer ( "ffmpeg-workaround-bugs", 1, NULL,
                  "workaround bugs", WORKAROUND_BUGS_LONGTEXT, VLC_FALSE );
    add_bool( "ffmpeg-hurry-up", 0, NULL, "hurry up", HURRY_UP_LONGTEXT, VLC_FALSE );
    add_integer( "ffmpeg-truncated", -1, NULL, "truncated stream", "truncated stream -1:auto,0:disable,:1:enable", VLC_FALSE );
    add_category_hint( N_("Post processing"), NULL, VLC_FALSE );

    add_integer( "ffmpeg-pp-q", 0, NULL,
                 "post processing quality", POSTPROCESSING_Q_LONGTEXT, VLC_FALSE );
#ifdef LIBAVCODEC_PP
    add_string( "ffmpeg-pp-name", "default", NULL,
                "ffmpeg postproc filter chains", LIBAVCODEC_PP_LONGHELP, VLC_TRUE );
#else
    add_module( "ffmpeg-pp", "postprocessing",NULL, NULL,
                N_( "ffmpeg postprocessing module" ), NULL, VLC_FALSE );
    add_bool( "ffmpeg-pp-auto", 0, NULL,
              "auto-level Post processing quality", POSTPROCESSING_AQ_LONGTEXT, VLC_FALSE );
    add_bool( "ffmpeg-db-yv", 0, NULL,
              "force vertical luminance deblocking",
              "force vertical luminance deblocking (override other settings)", VLC_TRUE );
    add_bool( "ffmpeg-db-yh", 0, NULL,
              "force horizontal luminance deblocking",
              "force horizontal luminance deblocking (override other settings)", VLC_TRUE );
    add_bool( "ffmpeg-db-cv", 0, NULL,
              "force vertical chrominance deblocking",
              "force vertical chrominance deblocking (override other settings)", VLC_TRUE );
    add_bool( "ffmpeg-db-ch", 0, NULL,
              "force horizontal chrominance deblocking",
              "force horizontal chrominance deblocking (override other settings) ", VLC_TRUE );
    add_bool( "ffmpeg-dr-y", 0, NULL,
              "force luminance deringing",
              "force luminance deringing (override other settings)", VLC_TRUE );
    add_bool( "ffmpeg-dr-c", 0, NULL,
              "force chrominance deringing",
              "force chrominance deringing (override other settings)", VLC_TRUE );
#endif

    /* chroma conversion submodule */
    add_submodule();
    set_capability( "chroma", 50 );
    set_callbacks( E_(OpenChroma), NULL );
    set_description( _("ffmpeg chroma conversion") );

vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_fifo_t *p_fifo = (decoder_fifo_t*) p_this;

    if( ffmpeg_GetFfmpegCodec( p_fifo->i_fourcc, NULL, NULL, NULL ) )
    {
        p_fifo->pf_run = RunDecoder;
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}

typedef union decoder_thread_u
{
    generic_thread_t gen;
    adec_thread_t    audio;
    vdec_thread_t    video;

} decoder_thread_t;


/*****************************************************************************
 * RunDecoder: this function is called just after the thread is created
 *****************************************************************************/
static int RunDecoder( decoder_fifo_t *p_fifo )
{
    generic_thread_t *p_decoder;
    int b_error;

    if ( !(p_decoder = malloc( sizeof( decoder_thread_t ) ) ) )
    {
        msg_Err( p_fifo, "out of memory" );
        DecoderError( p_fifo );
        return( -1 );
    }
    memset( p_decoder, 0, sizeof( decoder_thread_t ) );

    p_decoder->p_fifo = p_fifo;

    if( InitThread( p_decoder ) != 0 )
    {
        msg_Err( p_fifo, "initialization failed" );
        DecoderError( p_fifo );
        return( -1 );
    }

    while( (!p_decoder->p_fifo->b_die) && (!p_decoder->p_fifo->b_error) )
    {
        switch( p_decoder->i_cat )
        {
            case VIDEO_ES:
                E_( DecodeThread_Video )( (vdec_thread_t*)p_decoder );
                break;
            case AUDIO_ES:
                E_( DecodeThread_Audio )( (adec_thread_t*)p_decoder );
                break;
        }
    }

    if( ( b_error = p_decoder->p_fifo->b_error ) )
    {
        DecoderError( p_decoder->p_fifo );
    }

    EndThread( p_decoder );

    if( b_error )
    {
        return( -1 );
    }

    return( 0 );
}

/*****************************************************************************
 *
 * Functions that initialize, decode and end the decoding process
 *
 *****************************************************************************/

/*****************************************************************************
 * InitThread: initialize vdec output thread
 *****************************************************************************
 * This function is called from decoder_Run and performs the second step
 * of the initialization. It returns 0 on success. Note that the thread's
 * flag are not modified inside this function.
 *
 * ffmpeg codec will be open, some memory allocated. But Vout is not yet
 *   open (done after the first decoded frame)
 *****************************************************************************/

static int InitThread( generic_thread_t *p_decoder )
{
    int i_result;

     /* *** init ffmpeg library (libavcodec) *** */
    if( !b_ffmpeginit )
    {
        avcodec_init();
        avcodec_register_all();
        b_ffmpeginit = 1;

        msg_Dbg( p_decoder->p_fifo, "libavcodec initialized (interface "
                                    LIBAVCODEC_BUILD_STR ")" );
    }
    else
    {
        msg_Dbg( p_decoder->p_fifo, "libavcodec already initialized" );
    }

    /* *** determine codec type *** */
    ffmpeg_GetFfmpegCodec( p_decoder->p_fifo->i_fourcc,
                           &p_decoder->i_cat,
                           &p_decoder->i_codec_id,
                           &p_decoder->psz_namecodec );

    /* *** ask ffmpeg for a decoder *** */
    if( !( p_decoder->p_codec =
                avcodec_find_decoder( p_decoder->i_codec_id ) ) )
    {
        msg_Err( p_decoder->p_fifo,
                 "codec not found (%s)",
                 p_decoder->psz_namecodec );
        return( -1 );
    }

     /* *** Get a p_context *** */
    p_decoder->p_context = avcodec_alloc_context();

    switch( p_decoder->i_cat )
    {
        case VIDEO_ES:
            i_result = E_( InitThread_Video )( (vdec_thread_t*)p_decoder );
            break;
        case AUDIO_ES:
            i_result = E_( InitThread_Audio )( (adec_thread_t*)p_decoder );
            break;
        default:
            i_result = -1;
    }

    p_decoder->pts = 0;
    p_decoder->p_buffer = NULL;
    p_decoder->i_buffer = 0;
    p_decoder->i_buffer_size = 0;

    return( i_result );
}

/*****************************************************************************
 * EndThread: thread destruction
 *****************************************************************************
 * This function is called when the thread ends after a sucessful
 * initialization.
 *****************************************************************************/
static void EndThread( generic_thread_t *p_decoder )
{

    if( !p_decoder )
    {
        return;
    }

    if( p_decoder->p_context != NULL)
    {
        FREE( p_decoder->p_context->extradata );
        avcodec_close( p_decoder->p_context );
        msg_Dbg( p_decoder->p_fifo,
                 "ffmpeg codec (%s) stopped",
                 p_decoder->psz_namecodec );
        free( p_decoder->p_context );
    }

    FREE( p_decoder->p_buffer );

    switch( p_decoder->i_cat )
    {
        case AUDIO_ES:
            E_( EndThread_Audio )( (adec_thread_t*)p_decoder );
            break;
        case VIDEO_ES:
            E_( EndThread_Video )( (vdec_thread_t*)p_decoder );
            break;
    }

    free( p_decoder );
}

/*****************************************************************************
 * locales Functions
 *****************************************************************************/

int E_( GetPESData )( u8 *p_buf, int i_max, pes_packet_t *p_pes )
{
    int i_copy;
    int i_count;

    data_packet_t   *p_data;

    i_count = 0;
    p_data = p_pes->p_first;
    while( p_data != NULL && i_count < i_max )
    {

        i_copy = __MIN( p_data->p_payload_end - p_data->p_payload_start,
                        i_max - i_count );

        if( i_copy > 0 )
        {
            memcpy( p_buf,
                    p_data->p_payload_start,
                    i_copy );
        }

        p_data = p_data->p_next;
        i_count += i_copy;
        p_buf   += i_copy;
    }

    if( i_count < i_max )
    {
        memset( p_buf, 0, i_max - i_count );
    }
    return( i_count );
}


static int ffmpeg_GetFfmpegCodec( vlc_fourcc_t i_fourcc,
                                  int *pi_cat,
                                  int *pi_ffmpeg_codec,
                                  char **ppsz_name )
{
    int i_cat;
    int i_codec;
    char *psz_name;

    switch( i_fourcc )
    {
#if 0
        /* XXX don't use it */
        case FOURCC_mpgv:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_MPEG1VIDEO;
            psz_name = "MPEG-1/2 Video";
            break;
#endif
        case FOURCC_DIV1:
        case FOURCC_div1:
        case FOURCC_MPG4:
        case FOURCC_mpg4:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_MSMPEG4V1;
            psz_name = "MS MPEG-4 v1";
            break;

        case FOURCC_DIV2:
        case FOURCC_div2:
        case FOURCC_MP42:
        case FOURCC_mp42:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_MSMPEG4V2;
            psz_name = "MS MPEG-4 v2";
            break;

        case FOURCC_MPG3:
        case FOURCC_mpg3:
        case FOURCC_div3:
        case FOURCC_MP43:
        case FOURCC_mp43:
        case FOURCC_DIV3:
        case FOURCC_DIV4:
        case FOURCC_div4:
        case FOURCC_DIV5:
        case FOURCC_div5:
        case FOURCC_DIV6:
        case FOURCC_div6:
        case FOURCC_AP41:
        case FOURCC_3VID:
        case FOURCC_3vid:
        case FOURCC_3IVD:
        case FOURCC_3ivd:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_MSMPEG4V3;
            psz_name = "MS MPEG-4 v3";
            break;

        case FOURCC_SVQ1:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_SVQ1;
            psz_name = "SVQ-1 (Sorenson Video v1)";
            break;
#if LIBAVCODEC_BUILD >= 4666
        case FOURCC_SVQ3:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_SVQ3;
            psz_name = "SVQ-3 (Sorenson Video v3)";
            break;
#endif

        case FOURCC_DIVX:
        case FOURCC_divx:
        case FOURCC_MP4S:
        case FOURCC_mp4s:
        case FOURCC_M4S2:
        case FOURCC_m4s2:
        case FOURCC_xvid:
        case FOURCC_XVID:
        case FOURCC_XviD:
        case FOURCC_DX50:
        case FOURCC_mp4v:
        case FOURCC_4:
        /* 3iv1 is unsupported by ffmpeg
           putting it here gives extreme distorted images
        case FOURCC_3IV1:
        case FOURCC_3iv1:
        */
        case FOURCC_3IV2:
        case FOURCC_3iv2:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_MPEG4;
            psz_name = "MPEG-4";
            break;
/* FIXME FOURCC_H263P exist but what fourcc ? */
        case FOURCC_H263:
        case FOURCC_h263:
        case FOURCC_U263:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_H263;
            psz_name = "H263";
            break;

        case FOURCC_I263:
        case FOURCC_i263:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_H263I;
            psz_name = "I263.I";
            break;
        case FOURCC_WMV1:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_WMV1;
            psz_name ="Windows Media Video 1";
            break;
        case FOURCC_WMV2:
        case FOURCC_MSS1:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_WMV2;
            psz_name ="Windows Media Video 2";
            break;
        case FOURCC_MJPG:
        case FOURCC_mjpg:
        case FOURCC_mjpa:
        case FOURCC_jpeg:
        case FOURCC_JPEG:
        case FOURCC_JFIF:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_MJPEG;
            psz_name = "Motion JPEG";
            break;
        case FOURCC_mjpb:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_MJPEGB;
            psz_name = "Motion JPEG B";
            break;
        case FOURCC_dvsl:
        case FOURCC_dvsd:
        case FOURCC_DVSD:
        case FOURCC_dvhd:
        case FOURCC_dvc:
        case FOURCC_dvp:
            i_cat = VIDEO_ES;
            i_codec = CODEC_ID_DVVIDEO;
            psz_name = "DV video";
            break;

        case FOURCC_MAC3:
            i_cat = AUDIO_ES;
            i_codec = CODEC_ID_MACE3;
            psz_name = "MACE-3 audio";
            break;
        case FOURCC_MAC6:
            i_cat = AUDIO_ES;
            i_codec = CODEC_ID_MACE6;
            psz_name = "MACE-6 audio";
            break;
        case FOURCC_dvau:
            i_cat = AUDIO_ES;
            i_codec = CODEC_ID_DVAUDIO;
            psz_name = "DV audio";
            break;

        case FOURCC_WMA1:
        case FOURCC_wma1:
            i_cat = AUDIO_ES;
            i_codec = CODEC_ID_WMAV1;
            psz_name ="Windows Media Audio 1";
            break;
        case FOURCC_WMA2:
        case FOURCC_wma2:
            i_cat = AUDIO_ES;
            i_codec = CODEC_ID_WMAV2;
            psz_name ="Windows Media Audio 2";
            break;

#if LIBAVCODEC_BUILD >= 4663
        case FOURCC_IV31:
        case FOURCC_iv31:
        case FOURCC_IV32:
        case FOURCC_iv32:
            i_cat    = VIDEO_ES;
            i_codec  = CODEC_ID_INDEO3;
            psz_name = "Indeo v3";
            break;
#endif
        default:
            i_cat = UNKNOWN_ES;
            i_codec = CODEC_ID_NONE;
            psz_name = NULL;
            break;
    }

    if( i_codec != CODEC_ID_NONE )
    {
        if( pi_cat ) *pi_cat = i_cat;
        if( pi_ffmpeg_codec ) *pi_ffmpeg_codec = i_codec;
        if( ppsz_name ) *ppsz_name = psz_name;
        return( VLC_TRUE );
    }

    return( VLC_FALSE );
}



