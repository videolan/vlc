/*****************************************************************************
 * ffmpeg.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <vlc/vlc.h>
#include <vlc/decoder.h>

/* ffmpeg header */
#define HAVE_MMX
#ifdef HAVE_FFMPEG_AVCODEC_H
#   include <ffmpeg/avcodec.h>
#else
#   include <avcodec.h>
#endif

#if LIBAVCODEC_BUILD < 4680
#   error You must have a libavcodec >= 4680 (get CVS)
#endif

#include "ffmpeg.h"

#ifdef LIBAVCODEC_PP
#   ifdef HAVE_POSTPROC_POSTPROCESS_H
#       include <postproc/postprocess.h>
#   else
#       include <libpostproc/postprocess.h>
#   endif
#endif

/*****************************************************************************
 * decoder_sys_t: decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Common part between video and audio decoder */
    int i_cat;
    int i_codec_id;
    char *psz_namecodec;

    AVCodecContext *p_context;
    AVCodec        *p_codec;
};

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int OpenDecoder( vlc_object_t * );
static void CloseDecoder( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();

    /* decoder main module */
    set_description( _("ffmpeg audio/video decoder((MS)MPEG4,SVQ1,H263,WMV,WMA)") );
    set_capability( "decoder", 70 );
    set_callbacks( OpenDecoder, CloseDecoder );

    add_bool( "ffmpeg-dr", 1, NULL, DR_TEXT, DR_TEXT, VLC_TRUE );
    add_integer ( "ffmpeg-error-resilience", -1, NULL, ERROR_TEXT,
        ERROR_LONGTEXT, VLC_TRUE );
    add_integer ( "ffmpeg-workaround-bugs", 1, NULL, BUGS_TEXT, BUGS_LONGTEXT,
        VLC_FALSE );
    add_bool( "ffmpeg-hurry-up", 0, NULL, HURRYUP_TEXT, HURRYUP_LONGTEXT,
        VLC_FALSE );

#ifdef LIBAVCODEC_PP
    add_integer( "ffmpeg-pp-q", 0, NULL, PP_Q_TEXT, PP_Q_LONGTEXT, VLC_FALSE );
    add_string( "ffmpeg-pp-name", "default", NULL, LIBAVCODEC_PP_TEXT,
        LIBAVCODEC_PP_LONGTEXT, VLC_TRUE );
#endif
    add_integer( "ffmpeg-debug", 0, NULL, DEBUG_TEST, DEBUG_LONGTEST, VLC_TRUE );

    /* chroma conversion submodule */
    add_submodule();
    set_capability( "chroma", 50 );
    set_callbacks( E_(OpenChroma), E_(CloseChroma) );
    set_description( _("ffmpeg chroma conversion") );

    /* encoder submodule */
    add_submodule();
    set_description( _("ffmpeg audio/video encoder") );
    set_capability( "encoder", 100 );
    set_callbacks( E_(OpenEncoder), E_(CloseEncoder) );

    /* demux submodule */
    add_submodule();
    set_description( _("ffmpeg demuxer" ) );
    set_capability( "demux2", 1 );
    set_callbacks( E_(OpenDemux), E_(CloseDemux) );

    var_Create( p_module->p_libvlc, "avcodec", VLC_VAR_MUTEX );
vlc_module_end();

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*) p_this;
    int i_cat, i_codec_id, i_result;
    char *psz_namecodec;

    AVCodecContext *p_context;
    AVCodec        *p_codec;

    /* *** determine codec type *** */
    if( !E_(GetFfmpegCodec)( p_dec->fmt_in.i_codec, &i_cat, &i_codec_id,
                             &psz_namecodec ) )
    {
        return VLC_EGENERIC;
    }

    /* Bail out if buggy decoder */
    if( i_codec_id == CODEC_ID_AAC )
    {
        msg_Dbg( p_dec, "refusing to use ffmpeg's (%s) decoder which is buggy",
                 psz_namecodec );
        return VLC_EGENERIC;
    }

    /* Initialization must be done before avcodec_find_decoder() */
    E_(InitLibavcodec)(p_this);

    /* *** ask ffmpeg for a decoder *** */
    if( !( p_codec = avcodec_find_decoder( i_codec_id ) ) )
    {
        msg_Dbg( p_dec, "codec not found (%s)", psz_namecodec );
        return VLC_EGENERIC;
    }

    /* *** get a p_context *** */
    p_context = avcodec_alloc_context();
    p_context->debug = config_GetInt( p_dec, "ffmpeg-debug" );

    /* Set CPU capabilities */
    p_context->dsp_mask = 0;
    if( !(p_dec->p_libvlc->i_cpu & CPU_CAPABILITY_MMX) )
    {
        p_context->dsp_mask |= FF_MM_MMX;
    }
    if( !(p_dec->p_libvlc->i_cpu & CPU_CAPABILITY_MMXEXT) )
    {
        p_context->dsp_mask |= FF_MM_MMXEXT;
    }
    if( !(p_dec->p_libvlc->i_cpu & CPU_CAPABILITY_3DNOW) )
    {
        p_context->dsp_mask |= FF_MM_3DNOW;
    }
    if( !(p_dec->p_libvlc->i_cpu & CPU_CAPABILITY_SSE) )
    {
        p_context->dsp_mask |= FF_MM_SSE;
        p_context->dsp_mask |= FF_MM_SSE2;
    }

    switch( i_cat )
    {
    case VIDEO_ES:
        p_dec->b_need_packetized = VLC_TRUE;
        p_dec->pf_decode_video = E_(DecodeVideo);
        i_result = E_( InitVideoDec )( p_dec, p_context, p_codec,
                                       i_codec_id, psz_namecodec );
        break;
    case AUDIO_ES:
        p_dec->pf_decode_audio = E_(DecodeAudio);
        i_result = E_( InitAudioDec )( p_dec, p_context, p_codec,
                                       i_codec_id, psz_namecodec );
        break;
    default:
        i_result = VLC_EGENERIC;
    }

    p_dec->p_sys->i_cat = i_cat;

    return i_result;
}

/*****************************************************************************
 * CloseDecoder: decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_sys->p_context )
    {
        if( p_sys->p_context->extradata )
            free( p_sys->p_context->extradata );

        avcodec_close( p_sys->p_context );
        msg_Dbg( p_dec, "ffmpeg codec (%s) stopped", p_sys->psz_namecodec );
        av_free( p_sys->p_context );
    }

    switch( p_sys->i_cat )
    {
    case AUDIO_ES:
        E_( EndAudioDec )( p_dec );
        break;
    case VIDEO_ES:
        E_( EndVideoDec )( p_dec );
        break;
    }

    free( p_sys );
}
  
/*****************************************************************************
 * local Functions
 *****************************************************************************/
int E_(GetFfmpegChroma)( vlc_fourcc_t i_chroma )
{
    switch( i_chroma )
    {
    case VLC_FOURCC( 'I', '4', '2', '0' ):
        return PIX_FMT_YUV420P;
    case VLC_FOURCC( 'I', '4', '2', '2' ):
        return PIX_FMT_YUV422P;
    case VLC_FOURCC( 'I', '4', '4', '4' ):
        return PIX_FMT_YUV444P;
    case VLC_FOURCC( 'R', 'V', '1', '5' ):
        return PIX_FMT_RGB555;
    case VLC_FOURCC( 'R', 'V', '1', '6' ):
        return PIX_FMT_RGB565;
    case VLC_FOURCC( 'R', 'V', '2', '4' ):
        return PIX_FMT_RGB24;
    case VLC_FOURCC( 'R', 'V', '3', '2' ):
        return PIX_FMT_RGBA32;
    case VLC_FOURCC( 'G', 'R', 'E', 'Y' ):
        return PIX_FMT_GRAY8;
    case VLC_FOURCC( 'Y', 'U', 'Y', '2' ):
        return PIX_FMT_YUV422;
    default:
        return -1;
    }
}

void E_(InitLibavcodec)( vlc_object_t *p_object )
{
    static int b_ffmpeginit = 0;
    vlc_value_t lockval;

    var_Get( p_object->p_libvlc, "avcodec", &lockval );
    vlc_mutex_lock( lockval.p_address );

    /* *** init ffmpeg library (libavcodec) *** */
    if( !b_ffmpeginit )
    {
        avcodec_init();
        avcodec_register_all();
        b_ffmpeginit = 1;

        msg_Dbg( p_object, "libavcodec initialized (interface %d )",
                 LIBAVCODEC_BUILD );
    }
    else
    {
        msg_Dbg( p_object, "libavcodec already initialized" );
    }

    vlc_mutex_unlock( lockval.p_address );
}


static struct
{
    vlc_fourcc_t  i_fourcc;
    int  i_codec;
    int  i_cat;
    char *psz_name;

} codecs_table[] =
{
    /* MPEG-1 Video */
    { VLC_FOURCC('m','p','1','v'), CODEC_ID_MPEG1VIDEO,
      VIDEO_ES, "MPEG-1 Video" },

    /* MPEG-2 Video */
    { VLC_FOURCC('m','p','2','v'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG-2 Video" },
    { VLC_FOURCC('m','p','g','v'), CODEC_ID_MPEG2VIDEO,
      VIDEO_ES, "MPEG-2 Video" },

    /* MPEG-4 Video */
    { VLC_FOURCC('D','I','V','X'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('d','i','v','x'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('M','P','4','S'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('m','p','4','s'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('M','4','S','2'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('m','4','s','2'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('x','v','i','d'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('X','V','I','D'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('X','v','i','D'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('D','X','5','0'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('d','x','5','0'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('m','p','4','v'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC( 4,  0,  0,  0 ), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('m','4','c','c'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('M','4','C','C'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    /* 3ivx delta 3.5 Unsupported
     * putting it here gives extreme distorted images
    { VLC_FOURCC('3','I','V','1'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('3','i','v','1'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" }, */
    /* 3ivx delta 4 */
    { VLC_FOURCC('3','I','V','2'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },
    { VLC_FOURCC('3','i','v','2'), CODEC_ID_MPEG4,
      VIDEO_ES, "MPEG-4 Video" },

    /* MSMPEG4 v1 */
    { VLC_FOURCC('D','I','V','1'), CODEC_ID_MSMPEG4V1,
      VIDEO_ES, "MS MPEG-4 Video v1" },
    { VLC_FOURCC('d','i','v','1'), CODEC_ID_MSMPEG4V1,
      VIDEO_ES, "MS MPEG-4 Video v1" },
    { VLC_FOURCC('M','P','G','4'), CODEC_ID_MSMPEG4V1,
      VIDEO_ES, "MS MPEG-4 Video v1" },
    { VLC_FOURCC('m','p','g','4'), CODEC_ID_MSMPEG4V1,
      VIDEO_ES, "MS MPEG-4 Video v1" },

    /* MSMPEG4 v2 */
    { VLC_FOURCC('D','I','V','2'), CODEC_ID_MSMPEG4V2,
      VIDEO_ES, "MS MPEG-4 Video v2" },
    { VLC_FOURCC('d','i','v','2'), CODEC_ID_MSMPEG4V2,
      VIDEO_ES, "MS MPEG-4 Video v2" },
    { VLC_FOURCC('M','P','4','2'), CODEC_ID_MSMPEG4V2,
      VIDEO_ES, "MS MPEG-4 Video v2" },
    { VLC_FOURCC('m','p','4','2'), CODEC_ID_MSMPEG4V2,
      VIDEO_ES, "MS MPEG-4 Video v2" },

    /* MSMPEG4 v3 / M$ mpeg4 v3 */
    { VLC_FOURCC('M','P','G','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('m','p','g','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('d','i','v','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('M','P','4','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('m','p','4','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    /* DivX 3.20 */
    { VLC_FOURCC('D','I','V','3'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('D','I','V','4'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('d','i','v','4'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('D','I','V','5'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('d','i','v','5'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('D','I','V','6'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('d','i','v','6'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    /* AngelPotion stuff */
    { VLC_FOURCC('A','P','4','1'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    /* 3ivx doctered divx files */
    { VLC_FOURCC('3','I','V','D'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('3','i','v','d'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    /* who knows? */
    { VLC_FOURCC('3','V','I','D'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },
    { VLC_FOURCC('3','v','i','d'), CODEC_ID_MSMPEG4V3,
      VIDEO_ES, "MS MPEG-4 Video v3" },

    /* Sorenson v1 */
    { VLC_FOURCC('S','V','Q','1'), CODEC_ID_SVQ1,
      VIDEO_ES, "SVQ-1 (Sorenson Video v1)" },

    /* Sorenson v3 */
    { VLC_FOURCC('S','V','Q','3'), CODEC_ID_SVQ3,
      VIDEO_ES, "SVQ-3 (Sorenson Video v3)" },

    /* h264 */
    { VLC_FOURCC('h','2','6','4'), CODEC_ID_H264,
      VIDEO_ES, "h264" },
    { VLC_FOURCC('H','2','6','4'), CODEC_ID_H264,
      VIDEO_ES, "h264" },

/* H263 and H263i */
/* H263(+) is also known as Real Video 1.0 */

/* FIXME FOURCC_H263P exist but what fourcc ? */

    /* H263 */
    { VLC_FOURCC('H','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "H263" },
    { VLC_FOURCC('h','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "H263" },
    { VLC_FOURCC('U','2','6','3'), CODEC_ID_H263,
      VIDEO_ES, "H263" },

    /* H263i */
    { VLC_FOURCC('I','2','6','3'), CODEC_ID_H263I,
      VIDEO_ES, "I263.I" },
    { VLC_FOURCC('i','2','6','3'), CODEC_ID_H263I,
      VIDEO_ES, "I263.I" },

    /* Flash (H263) variant */
    { VLC_FOURCC('F','L','V','1'), CODEC_ID_FLV1,
      VIDEO_ES, "Flash Video" },

#if LIBAVCODEC_BUILD > 4680
    { VLC_FOURCC('F','L','I','C'), CODEC_ID_FLIC,
      VIDEO_ES, "Flic Video" },
#endif

    /* MJPEG */
    { VLC_FOURCC( 'M', 'J', 'P', 'G' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'm', 'j', 'p', 'g' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'm', 'j', 'p', 'a' ), CODEC_ID_MJPEG, /* for mov file */
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'j', 'p', 'e', 'g' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'J', 'P', 'E', 'G' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'J', 'F', 'I', 'F' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },
    { VLC_FOURCC( 'J', 'P', 'G', 'L' ), CODEC_ID_MJPEG,
      VIDEO_ES, "Motion JPEG Video" },

    { VLC_FOURCC( 'm', 'j', 'p', 'b' ), CODEC_ID_MJPEGB, /* for mov file */
      VIDEO_ES, "Motion JPEG B Video" },

#if LIBAVCODEC_BUILD > 4680
    { VLC_FOURCC( 'S', 'P', '5', 'X' ), CODEC_ID_SP5X,
      VIDEO_ES, "Sunplus Motion JPEG Video" },
#endif

    /* DV */
    { VLC_FOURCC('d','v','s','l'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('d','v','s','d'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('D','V','S','D'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('d','v','h','d'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('d','v','c',' '), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('d','v','p',' '), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },
    { VLC_FOURCC('C','D','V','C'), CODEC_ID_DVVIDEO,
      VIDEO_ES, "DV Video" },

    /* Windows Media Video */
    { VLC_FOURCC('W','M','V','1'), CODEC_ID_WMV1,
      VIDEO_ES, "Windows Media Video 1" },
    { VLC_FOURCC('W','M','V','2'), CODEC_ID_WMV2,
      VIDEO_ES, "Windows Media Video 2" },

#if LIBAVCODEC_BUILD >= 4683
    /* Microsoft Video 1 */
    { VLC_FOURCC('M','S','V','C'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },
    { VLC_FOURCC('m','s','v','c'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },
    { VLC_FOURCC('C','R','A','M'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },
    { VLC_FOURCC('c','r','a','m'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },
    { VLC_FOURCC('W','H','A','M'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },
    { VLC_FOURCC('w','h','a','m'), CODEC_ID_MSVIDEO1,
      VIDEO_ES, "Microsoft Video 1" },

    /* Microsoft RLE */
    { VLC_FOURCC('m','r','l','e'), CODEC_ID_MSRLE,
      VIDEO_ES, "Microsoft RLE Video" },
    { VLC_FOURCC(0x1,0x0,0x0,0x0), CODEC_ID_MSRLE,
      VIDEO_ES, "Microsoft RLE Video" },
#endif

#if( !defined( WORDS_BIGENDIAN ) )
    /* Indeo Video Codecs (Quality of this decoder on ppc is not good) */
    { VLC_FOURCC('I','V','3','1'), CODEC_ID_INDEO3,
      VIDEO_ES, "Indeo Video v3" },
    { VLC_FOURCC('i','v','3','1'), CODEC_ID_INDEO3,
      VIDEO_ES, "Indeo Video v3" },
    { VLC_FOURCC('I','V','3','2'), CODEC_ID_INDEO3,
      VIDEO_ES, "Indeo Video v3" },
    { VLC_FOURCC('i','v','3','2'), CODEC_ID_INDEO3,
      VIDEO_ES, "Indeo Video v3" },
#endif

    /* Huff YUV */
    { VLC_FOURCC('H','F','Y','U'), CODEC_ID_HUFFYUV,
      VIDEO_ES, "Huff YUV Video" },

    /* Creative YUV */
    { VLC_FOURCC('C','Y','U','V'), CODEC_ID_CYUV,
      VIDEO_ES, "Creative YUV Video" },

    /* On2 VP3 Video Codecs */
    { VLC_FOURCC('V','P','3','1'), CODEC_ID_VP3,
      VIDEO_ES, "On2's VP3 Video" },
    { VLC_FOURCC('v','p','3','1'), CODEC_ID_VP3,
      VIDEO_ES, "On2's VP3 Video" },

#if LIBAVCODEC_BUILD >= 4685
    /* Xiph.org theora */
    { VLC_FOURCC('t','h','e','o'), CODEC_ID_THEORA,
      VIDEO_ES, "Xiph.org's Theora Video" },
#endif

#if ( !defined( WORDS_BIGENDIAN ) )
    /* Asus Video (Another thing that doesn't work on PPC) */
    { VLC_FOURCC('A','S','V','1'), CODEC_ID_ASV1,
      VIDEO_ES, "Asus V1 Video" },
    { VLC_FOURCC('A','S','V','2'), CODEC_ID_ASV2,
      VIDEO_ES, "Asus V2 Video" },
#endif

    /* FFMPEG Video 1 (lossless codec) */
    { VLC_FOURCC('F','F','V','1'), CODEC_ID_FFV1,
      VIDEO_ES, "FFMpeg Video 1" },

    /* ATI VCR1 */
    { VLC_FOURCC('V','C','R','1'), CODEC_ID_VCR1,
      VIDEO_ES, "ATI VCR1 Video" },

    /* Cirrus Logic AccuPak */
    { VLC_FOURCC('C','L','J','R'), CODEC_ID_CLJR,
      VIDEO_ES, "Creative Logic AccuPak" },

    /* Real Video */
    { VLC_FOURCC('R','V','1','0'), CODEC_ID_RV10,
      VIDEO_ES, "Real Video 10" },
    { VLC_FOURCC('R','V','1','3'), CODEC_ID_RV10,
      VIDEO_ES, "Real Video 13" },

#if LIBAVCODEC_BUILD >= 4684
    /* Apple Video */
    { VLC_FOURCC('r','p','z','a'), CODEC_ID_RPZA,
      VIDEO_ES, "Apple Video" },

    { VLC_FOURCC('s','m','c',' '), CODEC_ID_SMC,
      VIDEO_ES, "Apple graphics" },

    /* Cinepak */
    { VLC_FOURCC('c','v','i','d'), CODEC_ID_CINEPAK,
      VIDEO_ES, "Cinepak Video" },

    /* Id Quake II CIN */
    { VLC_FOURCC('I','D','C','I'), CODEC_ID_IDCIN,
      VIDEO_ES, "Id Quake II CIN Video" },
#endif

    /* 4X Technologies */
    { VLC_FOURCC('4','x','m','v'), CODEC_ID_4XM,
      VIDEO_ES, "4X Technologies Video" },

#if LIBAVCODEC_BUILD >= 4694
    /* Duck TrueMotion */
    { VLC_FOURCC('D','U','C','K'), CODEC_ID_TRUEMOTION1,
      VIDEO_ES, "Duck TrueMotion v1 Video" },
#endif

    /* Interplay MVE */
    { VLC_FOURCC('i','m','v','e'), CODEC_ID_INTERPLAY_VIDEO,
      VIDEO_ES, "Interplay MVE Video" },

    /* Id RoQ */
    { VLC_FOURCC('R','o','Q','v'), CODEC_ID_ROQ,
      VIDEO_ES, "Id RoQ Video" },

    /* Sony Playstation MDEC */
    { VLC_FOURCC('M','D','E','C'), CODEC_ID_MDEC,
      VIDEO_ES, "PSX MDEC Video" },

#if LIBAVCODEC_BUILD >= 4699
    /* Sierra VMD */
    { VLC_FOURCC('v','m','d','v'), CODEC_ID_VMDVIDEO,
      VIDEO_ES, "Sierra VMD Video" },
#endif

    /*
     *  Audio Codecs
     */

    /* Windows Media Audio 1 */
    { VLC_FOURCC('W','M','A','1'), CODEC_ID_WMAV1,
      AUDIO_ES, "Windows Media Audio 1" },
    { VLC_FOURCC('w','m','a','1'), CODEC_ID_WMAV1,
      AUDIO_ES, "Windows Media Audio 1" },

    /* Windows Media Audio 2 */
    { VLC_FOURCC('W','M','A','2'), CODEC_ID_WMAV2,
      AUDIO_ES, "Windows Media Audio 2" },
    { VLC_FOURCC('w','m','a','2'), CODEC_ID_WMAV2,
      AUDIO_ES, "Windows Media Audio 2" },

    /* DV Audio */
    { VLC_FOURCC('d','v','a','u'), CODEC_ID_DVAUDIO,
      AUDIO_ES, "DV Audio" },

    /* MACE-3 Audio */
    { VLC_FOURCC('M','A','C','3'), CODEC_ID_MACE3,
      AUDIO_ES, "MACE-3 Audio" },

    /* MACE-6 Audio */
    { VLC_FOURCC('M','A','C','6'), CODEC_ID_MACE6,
      AUDIO_ES, "MACE-6 Audio" },

    /* RealAudio 1.0 */
    { VLC_FOURCC('1','4','_','4'), CODEC_ID_RA_144,
      AUDIO_ES, "RealAudio 1.0" },

    /* RealAudio 2.0 */
    { VLC_FOURCC('2','8','_','8'), CODEC_ID_RA_288,
      AUDIO_ES, "RealAudio 2.0" },

    /* MPEG Audio layer 1/2/3 */
    { VLC_FOURCC('m','p','g','a'), CODEC_ID_MP2,
      AUDIO_ES, "MPEG Audio layer 1/2" },
    { VLC_FOURCC('m','p','3',' '), CODEC_ID_MP3,
      AUDIO_ES, "MPEG Audio layer 1/2/3" },

    /* A52 Audio (aka AC3) */
    { VLC_FOURCC('a','5','2',' '), CODEC_ID_AC3,
      AUDIO_ES, "A52 Audio (aka AC3)" },
    { VLC_FOURCC('a','5','2','b'), CODEC_ID_AC3, /* VLC specific hack */
      AUDIO_ES, "A52 Audio (aka AC3)" },

    /* AAC audio */
    { VLC_FOURCC('m','p','4','a'), CODEC_ID_AAC,
      AUDIO_ES, "MPEG AAC Audio" },

    /* 4X Technologies */
    { VLC_FOURCC('4','x','m','a'), CODEC_ID_ADPCM_4XM,
      AUDIO_ES, "4X Technologies Audio" },

    /* Interplay DPCM */
    { VLC_FOURCC('i','d','p','c'), CODEC_ID_INTERPLAY_DPCM,
      AUDIO_ES, "Interplay DPCM Audio" },

    /* Id RoQ */
    { VLC_FOURCC('R','o','Q','a'), CODEC_ID_ROQ_DPCM,
      AUDIO_ES, "Id RoQ DPCM Audio" },

#if LIBAVCODEC_BUILD >= 4685
    /* Sony Playstation XA ADPCM */
    { VLC_FOURCC('x','a',' ',' '), CODEC_ID_ADPCM_XA,
      AUDIO_ES, "PSX XA ADPCM Audio" },

    /* ADX ADPCM */
    { VLC_FOURCC('a','d','x',' '), CODEC_ID_ADPCM_ADX,
      AUDIO_ES, "ADX ADPCM Audio" },
#endif

#if LIBAVCODEC_BUILD >= 4699
    /* Sierra VMD */
    { VLC_FOURCC('v','m','d','a'), CODEC_ID_VMDAUDIO,
      AUDIO_ES, "Sierra VMD Audio" },
#endif

    /* PCM */
    { VLC_FOURCC('s','8',' ',' '), CODEC_ID_PCM_S8,
      AUDIO_ES, "PCM S8" },
    { VLC_FOURCC('u','8',' ',' '), CODEC_ID_PCM_U8,
      AUDIO_ES, "PCM U8" },
    { VLC_FOURCC('s','1','6','l'), CODEC_ID_PCM_S16LE,
      AUDIO_ES, "PCM S16 LE" },
    { VLC_FOURCC('s','1','6','b'), CODEC_ID_PCM_S16BE,
      AUDIO_ES, "PCM S16 BE" },
    { VLC_FOURCC('u','1','6','l'), CODEC_ID_PCM_U16LE,
      AUDIO_ES, "PCM U16 LE" },
    { VLC_FOURCC('u','1','6','b'), CODEC_ID_PCM_U16BE,
      AUDIO_ES, "PCM U16 BE" },
    { VLC_FOURCC('a','l','a','w'), CODEC_ID_PCM_ALAW,
      AUDIO_ES, "PCM ALAW" },
    { VLC_FOURCC('u','l','a','w'), CODEC_ID_PCM_MULAW,
      AUDIO_ES, "PCM ULAW" },

    {0}
};

int E_(GetFfmpegCodec)( vlc_fourcc_t i_fourcc, int *pi_cat,
                        int *pi_ffmpeg_codec, char **ppsz_name )
{
    int i;

    for( i = 0; codecs_table[i].i_fourcc != 0; i++ )
    {
        if( codecs_table[i].i_fourcc == i_fourcc )
        {
            if( pi_cat ) *pi_cat = codecs_table[i].i_cat;
            if( pi_ffmpeg_codec ) *pi_ffmpeg_codec = codecs_table[i].i_codec;
            if( ppsz_name ) *ppsz_name = codecs_table[i].psz_name;

            return VLC_TRUE;
        }
    }
    return VLC_FALSE;
}

int E_(GetVlcFourcc)( int i_ffmpeg_codec, int *pi_cat,
                      vlc_fourcc_t *pi_fourcc, char **ppsz_name )
{
    int i;

    for( i = 0; codecs_table[i].i_codec != 0; i++ )
    {
        if( codecs_table[i].i_codec == i_ffmpeg_codec )
        {
            if( pi_cat ) *pi_cat = codecs_table[i].i_cat;
            if( pi_fourcc ) *pi_fourcc = codecs_table[i].i_fourcc;
            if( ppsz_name ) *ppsz_name = codecs_table[i].psz_name;

            return VLC_TRUE;
        }
    }
    return VLC_FALSE;
}
