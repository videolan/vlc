/*****************************************************************************
 * ffmpeg.c: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ffmpeg.c,v 1.68 2003/12/16 12:38:18 gbazin Exp $
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
    add_category_hint( N_("ffmpeg"), NULL, VLC_FALSE );
    set_capability( "decoder", 70 );
    set_callbacks( OpenDecoder, CloseDecoder );
    set_description( _("ffmpeg audio/video decoder((MS)MPEG4,SVQ1,H263,WMV,WMA)") );

    add_bool( "ffmpeg-dr", 1, NULL, DR_TEXT, DR_TEXT, VLC_TRUE );
    add_integer ( "ffmpeg-error-resilience", -1, NULL, ERROR_TEXT,
        ERROR_LONGTEXT, VLC_TRUE );
    add_integer ( "ffmpeg-workaround-bugs", 1, NULL, BUGS_TEXT, BUGS_LONGTEXT,
        VLC_FALSE );
    add_bool( "ffmpeg-hurry-up", 0, NULL, HURRYUP_TEXT, HURRYUP_LONGTEXT,
        VLC_FALSE );
    add_integer( "ffmpeg-truncated", 0, NULL, TRUNC_TEXT, TRUNC_LONGTEXT,
        VLC_FALSE );

#ifdef LIBAVCODEC_PP
    add_integer( "ffmpeg-pp-q", 0, NULL, PP_Q_TEXT, PP_Q_LONGTEXT, VLC_FALSE );
    add_string( "ffmpeg-pp-name", "default", NULL, LIBAVCODEC_PP_TEXT,
        LIBAVCODEC_PP_LONGTEXT, VLC_TRUE );
#endif

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
        free( p_sys->p_context );
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
int E_(GetFfmpegCodec)( vlc_fourcc_t i_fourcc, int *pi_cat,
                        int *pi_ffmpeg_codec, char **ppsz_name )
{
    int i_cat;
    int i_codec;
    char *psz_name;

    switch( i_fourcc )
    {

    /*
     *  Video Codecs
     */

    /* MPEG-1 Video */
    case VLC_FOURCC('m','p','1','v'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_MPEG1VIDEO;
        psz_name = "MPEG-1 Video";
        break;

    /* MPEG-2 Video */
    case VLC_FOURCC('m','p','2','v'):
    case VLC_FOURCC('m','p','g','v'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_MPEG2VIDEO;
        psz_name = "MPEG-2 Video";
        break;

    /* MPEG-4 Video */
    case VLC_FOURCC('D','I','V','X'):
    case VLC_FOURCC('d','i','v','x'):
    case VLC_FOURCC('M','P','4','S'):
    case VLC_FOURCC('m','p','4','s'):
    case VLC_FOURCC('M','4','S','2'):
    case VLC_FOURCC('m','4','s','2'):
    case VLC_FOURCC('x','v','i','d'):
    case VLC_FOURCC('X','V','I','D'):
    case VLC_FOURCC('X','v','i','D'):
    case VLC_FOURCC('D','X','5','0'):
    case VLC_FOURCC('d','x','5','0'):
    case VLC_FOURCC('m','p','4','v'):
    case VLC_FOURCC( 4,  0,  0,  0 ):
    case VLC_FOURCC('m','4','c','c'):
    case VLC_FOURCC('M','4','C','C'):
    /* 3ivx delta 3.5 Unsupported
     * putting it here gives extreme distorted images
    case VLC_FOURCC('3','I','V','1'):
    case VLC_FOURCC('3','i','v','1'): */
    /* 3ivx delta 4 */
    case VLC_FOURCC('3','I','V','2'):
    case VLC_FOURCC('3','i','v','2'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_MPEG4;
        psz_name = "MPEG-4";
        break;

    /* MSMPEG4 v1 */
    case VLC_FOURCC('D','I','V','1'):
    case VLC_FOURCC('d','i','v','1'):
    case VLC_FOURCC('M','P','G','4'):
    case VLC_FOURCC('m','p','g','4'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_MSMPEG4V1;
        psz_name = "MS MPEG-4 v1";
        break;

    /* MSMPEG4 v2 */
    case VLC_FOURCC('D','I','V','2'):
    case VLC_FOURCC('d','i','v','2'):
    case VLC_FOURCC('M','P','4','2'):
    case VLC_FOURCC('m','p','4','2'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_MSMPEG4V2;
        psz_name = "MS MPEG-4 v2";
        break;

    /* MSMPEG4 v3 / M$ mpeg4 v3 */
    case VLC_FOURCC('M','P','G','3'):
    case VLC_FOURCC('m','p','g','3'):
    case VLC_FOURCC('d','i','v','3'):
    case VLC_FOURCC('M','P','4','3'):
    case VLC_FOURCC('m','p','4','3'):
    /* DivX 3.20 */
    case VLC_FOURCC('D','I','V','3'):
    case VLC_FOURCC('D','I','V','4'):
    case VLC_FOURCC('d','i','v','4'):
    case VLC_FOURCC('D','I','V','5'):
    case VLC_FOURCC('d','i','v','5'):
    case VLC_FOURCC('D','I','V','6'):
    case VLC_FOURCC('d','i','v','6'):
    /* AngelPotion stuff */
    case VLC_FOURCC('A','P','4','1'):
    /* 3ivx doctered divx files */
    case VLC_FOURCC('3','I','V','D'):
    case VLC_FOURCC('3','i','v','d'):
    /* who knows? */
    case VLC_FOURCC('3','V','I','D'):
    case VLC_FOURCC('3','v','i','d'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_MSMPEG4V3;
        psz_name = "MS MPEG-4 v3";
        break;

    /* Sorenson v1 */
    case VLC_FOURCC('S','V','Q','1'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_SVQ1;
        psz_name = "SVQ-1 (Sorenson Video v1)";
        break;

    /* Sorenson v3 */
    case VLC_FOURCC('S','V','Q','3'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_SVQ3;
        psz_name = "SVQ-3 (Sorenson Video v3)";
        break;

    /* h264 */
    case VLC_FOURCC('h','2','6','4'):
    case VLC_FOURCC('H','2','6','4'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_H264;
        psz_name = "h264";
        break;

/* H263 and H263i */
/* H263(+) is also known as Real Video 1.0 */

/* FIXME FOURCC_H263P exist but what fourcc ? */

    /* H263 */
    case VLC_FOURCC('H','2','6','3'):
    case VLC_FOURCC('h','2','6','3'):
    case VLC_FOURCC('U','2','6','3'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_H263;
        psz_name = "H263";
        break;

    /* H263i */
    case VLC_FOURCC('I','2','6','3'):
    case VLC_FOURCC('i','2','6','3'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_H263I;
        psz_name = "I263.I";
        break;

    /* Flash (H263) variant */
    case VLC_FOURCC('F','L','V','1'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_FLV1;
        psz_name = "Flash Video";
        break;

    /* MJPEG */
    case VLC_FOURCC( 'M', 'J', 'P', 'G' ):
    case VLC_FOURCC( 'm', 'j', 'p', 'g' ):
    case VLC_FOURCC( 'm', 'j', 'p', 'a' ): /* for mov file */
    case VLC_FOURCC( 'j', 'p', 'e', 'g' ):
    case VLC_FOURCC( 'J', 'P', 'E', 'G' ):
    case VLC_FOURCC( 'J', 'F', 'I', 'F' ):
    case VLC_FOURCC( 'J', 'P', 'G', 'L' ):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_MJPEG;
        psz_name = "Motion JPEG";
        break;
    case VLC_FOURCC( 'm', 'j', 'p', 'b' ): /* for mov file */
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_MJPEGB;
        psz_name = "Motion JPEG B";
        break;

    /* DV */
    case VLC_FOURCC('d','v','s','l'):
    case VLC_FOURCC('d','v','s','d'):
    case VLC_FOURCC('D','V','S','D'):
    case VLC_FOURCC('d','v','h','d'):
    case VLC_FOURCC('d','v','c',' '):
    case VLC_FOURCC('d','v','p',' '):
    case VLC_FOURCC('C','D','V','C'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_DVVIDEO;
        psz_name = "DV video";
        break;

    /* Windows Media Video */
    case VLC_FOURCC('W','M','V','1'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_WMV1;
        psz_name ="Windows Media Video 1";
        break;
    case VLC_FOURCC('W','M','V','2'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_WMV2;
        psz_name ="Windows Media Video 2";
        break;

#if LIBAVCODEC_BUILD >= 4683
    /* Microsoft Video 1 */
    case VLC_FOURCC('M','S','V','C'):
    case VLC_FOURCC('m','s','v','c'):
    case VLC_FOURCC('C','R','A','M'):
    case VLC_FOURCC('c','r','a','m'):
    case VLC_FOURCC('W','H','A','M'):
    case VLC_FOURCC('w','h','a','m'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_MSVIDEO1;
        psz_name = "Microsoft Video 1";
        break;

    /* Microsoft RLE */
    case VLC_FOURCC('m','r','l','e'):
    case VLC_FOURCC(0x1,0x0,0x0,0x0):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_MSRLE;
        psz_name = "Microsoft RLE";
        break;
#endif

#if( !defined( WORDS_BIGENDIAN ) )
    /* Indeo Video Codecs (Quality of this decoder on ppc is not good) */
    case VLC_FOURCC('I','V','3','1'):
    case VLC_FOURCC('i','v','3','1'):
    case VLC_FOURCC('I','V','3','2'):
    case VLC_FOURCC('i','v','3','2'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_INDEO3;
        psz_name = "Indeo v3";
        break;
#endif

    /* Huff YUV */
    case VLC_FOURCC('H','F','Y','U'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_HUFFYUV;
        psz_name ="Huff YUV";
        break;

    /* Creative YUV */
    case VLC_FOURCC('C','Y','U','V'):
        i_cat = VIDEO_ES;
        i_codec = CODEC_ID_CYUV;
        psz_name ="Creative YUV";
        break;

    /* On2 VP3 Video Codecs */
    case VLC_FOURCC('V','P','3','1'):
    case VLC_FOURCC('v','p','3','1'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_VP3;
        psz_name = "On2's VP3 Video";
        break;

#if LIBAVCODEC_BUILD >= 4685
    /* Xiph.org theora */
    case VLC_FOURCC('t','h','e','o'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_THEORA;
        psz_name = "Xiph.org's Theora Video";
        break;
#endif

#if ( !defined( WORDS_BIGENDIAN ) )
    /* Asus Video (Another thing that doesn't work on PPC) */
    case VLC_FOURCC('A','S','V','1'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_ASV1;
        psz_name = "Asus V1";
        break;
    case VLC_FOURCC('A','S','V','2'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_ASV2;
        psz_name = "Asus V2";
        break;
#endif

    /* FFMPEG Video 1 (lossless codec) */
    case VLC_FOURCC('F','F','V','1'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_FFV1;
        psz_name = "FFMpeg Video 1";
        break;

    /* ATI VCR1 */
    case VLC_FOURCC('V','C','R','1'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_VCR1;
        psz_name = "ATI VCR1";
        break;

    /* Cirrus Logic AccuPak */
    case VLC_FOURCC('C','L','J','R'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_CLJR;
        psz_name = "Creative Logic AccuPak";
        break;

    case VLC_FOURCC('R','V','1','0'):
    case VLC_FOURCC('R','V','1','3'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_RV10;
        psz_name = "Real video";
        break;

#if LIBAVCODEC_BUILD >= 4683
    /* Apple Video */
    case VLC_FOURCC('r','p','z','a'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_RPZA;
        psz_name = "Apple Video";
        break;
#endif

#if LIBAVCODEC_BUILD >= 4683
    /* Cinepak */
    case VLC_FOURCC('c','v','i','d'):
        i_cat    = VIDEO_ES;
        i_codec  = CODEC_ID_CINEPAK;
        psz_name = "Cinepak";
        break;
#endif

    /*
     *  Audio Codecs
     */

    /* Windows Media Audio 1 */
    case VLC_FOURCC('W','M','A','1'):
    case VLC_FOURCC('w','m','a','1'):
        i_cat = AUDIO_ES;
        i_codec = CODEC_ID_WMAV1;
        psz_name ="Windows Media Audio 1";
        break;

    /* Windows Media Audio 2 */
    case VLC_FOURCC('W','M','A','2'):
    case VLC_FOURCC('w','m','a','2'):
        i_cat = AUDIO_ES;
        i_codec = CODEC_ID_WMAV2;
        psz_name ="Windows Media Audio 2";
        break;

    /* DV Audio */
    case VLC_FOURCC('d','v','a','u'):
        i_cat = AUDIO_ES;
        i_codec = CODEC_ID_DVAUDIO;
        psz_name = "DV audio";
        break;

    /* MACE-3 Audio */
    case VLC_FOURCC('M','A','C','3'):
        i_cat = AUDIO_ES;
        i_codec = CODEC_ID_MACE3;
        psz_name = "MACE-3 audio";
        break;

    /* MACE-6 Audio */
    case VLC_FOURCC('M','A','C','6'):
        i_cat = AUDIO_ES;
        i_codec = CODEC_ID_MACE6;
        psz_name = "MACE-6 audio";
        break;

    /* RealAudio 1.0 */
    case VLC_FOURCC('1','4','_','4'):
        i_cat    = AUDIO_ES;
        i_codec  = CODEC_ID_RA_144;
        psz_name = "RealAudio 1.0";
        break;

    /* RealAudio 2.0 */
    case VLC_FOURCC('2','8','_','8'):
        i_cat    = AUDIO_ES;
        i_codec  = CODEC_ID_RA_288;
        psz_name = "RealAudio 2.0";
        break;

    /* MPEG Audio layer 1/2/3 */
    case VLC_FOURCC('m','p','g','a'):
        i_cat    = AUDIO_ES;
        i_codec  = CODEC_ID_MP2;
        psz_name = "MPEG Audio layer 1/2";
        break;
    case VLC_FOURCC('m','p','3',' '):
        i_cat    = AUDIO_ES;
        i_codec  = CODEC_ID_MP3;
        psz_name = "MPEG Audio layer 1/2/3";
        break;

    /* A52 Audio (aka AC3) */
    case VLC_FOURCC('a','5','2',' '):
    case VLC_FOURCC('a','5','2','b'): /* VLC specific hack */
        i_cat    = AUDIO_ES;
        i_codec  = CODEC_ID_AC3;
        psz_name = "A52 Audio (aka AC3)";
        break;

    /* AAC audio */
    case VLC_FOURCC('m','p','4','a'):
        i_cat    = AUDIO_ES;
        i_codec  = CODEC_ID_AAC;
        psz_name = "MPEG AAC Audio";
        break;

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
        return VLC_TRUE;
    }

    return VLC_FALSE;
}

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
