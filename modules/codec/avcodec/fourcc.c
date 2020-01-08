/*****************************************************************************
 * fourcc.c: libavcodec <-> libvlc conversion routines
 *****************************************************************************
 * Copyright (C) 1999-2009 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_codec.h>

#include <libavcodec/avcodec.h>
#include "avcodec.h"

/*****************************************************************************
 * Codec fourcc -> libavcodec Codec_id mapping
 * Sorted by AVCodecID enumeration order
 *****************************************************************************/
struct vlc_avcodec_fourcc
{
    vlc_fourcc_t i_fourcc;
    unsigned i_codec;
};

/*
 * Video Codecs
 */
static const struct vlc_avcodec_fourcc video_codecs[] =
{
    { VLC_CODEC_MP1V, AV_CODEC_ID_MPEG1VIDEO },
    { VLC_CODEC_MP2V, AV_CODEC_ID_MPEG2VIDEO }, /* prefer MPEG2 over MPEG1 */
    { VLC_CODEC_MPGV, AV_CODEC_ID_MPEG2VIDEO }, /* prefer MPEG2 over MPEG1 */
    /* AV_CODEC_ID_MPEG2VIDEO_XVMC */
    { VLC_CODEC_H261, AV_CODEC_ID_H261 },
    { VLC_CODEC_H263, AV_CODEC_ID_H263 },
    { VLC_CODEC_RV10, AV_CODEC_ID_RV10 },
    { VLC_CODEC_RV13, AV_CODEC_ID_RV10 },
    { VLC_CODEC_RV20, AV_CODEC_ID_RV20 },
    { VLC_CODEC_MJPG, AV_CODEC_ID_MJPEG },
    { VLC_CODEC_MJPGB, AV_CODEC_ID_MJPEGB },
    { VLC_CODEC_LJPG, AV_CODEC_ID_LJPEG },
    { VLC_CODEC_SP5X, AV_CODEC_ID_SP5X },
    { VLC_CODEC_JPEGLS, AV_CODEC_ID_JPEGLS },
    { VLC_CODEC_MP4V, AV_CODEC_ID_MPEG4 },
    { VLC_CODEC_RGB32, AV_CODEC_ID_RAWVIDEO },
    { VLC_CODEC_RGB24, AV_CODEC_ID_RAWVIDEO },
    { VLC_CODEC_RGB16, AV_CODEC_ID_RAWVIDEO },
    { VLC_CODEC_RGB8, AV_CODEC_ID_RAWVIDEO },
    { VLC_CODEC_RGBA, AV_CODEC_ID_RAWVIDEO },
    { VLC_CODEC_ARGB, AV_CODEC_ID_RAWVIDEO },
    { VLC_CODEC_DIV1, AV_CODEC_ID_MSMPEG4V1 },
    { VLC_CODEC_DIV2, AV_CODEC_ID_MSMPEG4V2 },
    { VLC_CODEC_DIV3, AV_CODEC_ID_MSMPEG4V3 },
    { VLC_CODEC_WMV1, AV_CODEC_ID_WMV1 },
    { VLC_CODEC_WMV2, AV_CODEC_ID_WMV2 },
    { VLC_CODEC_H263P, AV_CODEC_ID_H263P },
    { VLC_CODEC_H263I, AV_CODEC_ID_H263I },
    { VLC_CODEC_FLV1, AV_CODEC_ID_FLV1 },
    { VLC_CODEC_SVQ1, AV_CODEC_ID_SVQ1 },
    { VLC_CODEC_SVQ3, AV_CODEC_ID_SVQ3 },
    { VLC_CODEC_DV, AV_CODEC_ID_DVVIDEO },
    { VLC_CODEC_HUFFYUV, AV_CODEC_ID_HUFFYUV },
    { VLC_CODEC_CYUV, AV_CODEC_ID_CYUV },
    { VLC_CODEC_H264, AV_CODEC_ID_H264 },
    { VLC_CODEC_INDEO3, AV_CODEC_ID_INDEO3 },
    { VLC_CODEC_VP3, AV_CODEC_ID_VP3 },
    { VLC_CODEC_THEORA, AV_CODEC_ID_THEORA },
#if ( !defined( WORDS_BIGENDIAN ) )
    /* Asus Video (Another thing that doesn't work on PPC) */
    { VLC_CODEC_ASV1, AV_CODEC_ID_ASV1 },
    { VLC_CODEC_ASV2, AV_CODEC_ID_ASV2 },
#endif
    { VLC_CODEC_FFV1, AV_CODEC_ID_FFV1 },
    { VLC_CODEC_4XM, AV_CODEC_ID_4XM },
    { VLC_CODEC_VCR1, AV_CODEC_ID_VCR1 },
    { VLC_CODEC_CLJR, AV_CODEC_ID_CLJR },
    { VLC_CODEC_MDEC, AV_CODEC_ID_MDEC },
    { VLC_CODEC_ROQ, AV_CODEC_ID_ROQ },
    { VLC_CODEC_INTERPLAY, AV_CODEC_ID_INTERPLAY_VIDEO },
    { VLC_CODEC_XAN_WC3, AV_CODEC_ID_XAN_WC3 },
    { VLC_CODEC_XAN_WC4, AV_CODEC_ID_XAN_WC4 },
    { VLC_CODEC_RPZA, AV_CODEC_ID_RPZA },
    { VLC_CODEC_CINEPAK, AV_CODEC_ID_CINEPAK },
    { VLC_CODEC_WS_VQA, AV_CODEC_ID_WS_VQA },
    { VLC_CODEC_MSRLE, AV_CODEC_ID_MSRLE },
    { VLC_CODEC_MSVIDEO1, AV_CODEC_ID_MSVIDEO1 },
    { VLC_CODEC_IDCIN, AV_CODEC_ID_IDCIN },
    { VLC_CODEC_8BPS, AV_CODEC_ID_8BPS },
    { VLC_CODEC_SMC, AV_CODEC_ID_SMC },
    { VLC_CODEC_FLIC, AV_CODEC_ID_FLIC },
    { VLC_CODEC_TRUEMOTION1, AV_CODEC_ID_TRUEMOTION1 },
    { VLC_CODEC_VMDVIDEO, AV_CODEC_ID_VMDVIDEO },
    { VLC_CODEC_LCL_MSZH, AV_CODEC_ID_MSZH },
    { VLC_CODEC_LCL_ZLIB, AV_CODEC_ID_ZLIB },
    { VLC_CODEC_QTRLE, AV_CODEC_ID_QTRLE },
    { VLC_CODEC_TSCC, AV_CODEC_ID_TSCC },
    { VLC_CODEC_ULTI, AV_CODEC_ID_ULTI },
    { VLC_CODEC_QDRAW, AV_CODEC_ID_QDRAW },
    { VLC_CODEC_VIXL, AV_CODEC_ID_VIXL },
    { VLC_CODEC_QPEG, AV_CODEC_ID_QPEG },
    { VLC_CODEC_PNG, AV_CODEC_ID_PNG },
    { VLC_CODEC_PPM, AV_CODEC_ID_PPM },
    /* AV_CODEC_ID_PBM */
    { VLC_CODEC_PGM, AV_CODEC_ID_PGM },
    { VLC_CODEC_PGMYUV, AV_CODEC_ID_PGMYUV },
    { VLC_CODEC_PAM, AV_CODEC_ID_PAM },
    { VLC_CODEC_FFVHUFF, AV_CODEC_ID_FFVHUFF },
    { VLC_CODEC_RV30, AV_CODEC_ID_RV30 },
    { VLC_CODEC_RV40, AV_CODEC_ID_RV40 },
    { VLC_CODEC_VC1,  AV_CODEC_ID_VC1 },
    { VLC_CODEC_WMVA, AV_CODEC_ID_VC1 },
    { VLC_CODEC_WMV3, AV_CODEC_ID_WMV3 },
    { VLC_CODEC_WMVP, AV_CODEC_ID_WMV3 },
    { VLC_CODEC_LOCO, AV_CODEC_ID_LOCO },
    { VLC_CODEC_WNV1, AV_CODEC_ID_WNV1 },
    { VLC_CODEC_AASC, AV_CODEC_ID_AASC },
    { VLC_CODEC_INDEO2, AV_CODEC_ID_INDEO2 },
    { VLC_CODEC_FRAPS, AV_CODEC_ID_FRAPS },
    { VLC_CODEC_TRUEMOTION2, AV_CODEC_ID_TRUEMOTION2 },
    { VLC_CODEC_BMP, AV_CODEC_ID_BMP },
    { VLC_CODEC_CSCD, AV_CODEC_ID_CSCD },
    { VLC_CODEC_MMVIDEO, AV_CODEC_ID_MMVIDEO },
    { VLC_CODEC_ZMBV, AV_CODEC_ID_ZMBV },
    { VLC_CODEC_AVS, AV_CODEC_ID_AVS },
    { VLC_CODEC_SMACKVIDEO, AV_CODEC_ID_SMACKVIDEO },
    { VLC_CODEC_NUV, AV_CODEC_ID_NUV },
    { VLC_CODEC_KMVC, AV_CODEC_ID_KMVC },
    { VLC_CODEC_FLASHSV, AV_CODEC_ID_FLASHSV },
    { VLC_CODEC_CAVS, AV_CODEC_ID_CAVS },
    { VLC_CODEC_JPEG2000, AV_CODEC_ID_JPEG2000 },
    { VLC_CODEC_VMNC, AV_CODEC_ID_VMNC },
    { VLC_CODEC_VP5, AV_CODEC_ID_VP5 },
    { VLC_CODEC_VP6, AV_CODEC_ID_VP6 },
    { VLC_CODEC_VP6F, AV_CODEC_ID_VP6F },
    { VLC_CODEC_TARGA, AV_CODEC_ID_TARGA },
    { VLC_CODEC_DSICINVIDEO, AV_CODEC_ID_DSICINVIDEO },
    { VLC_CODEC_TIERTEXSEQVIDEO, AV_CODEC_ID_TIERTEXSEQVIDEO },
    { VLC_CODEC_TIFF, AV_CODEC_ID_TIFF },
    { VLC_CODEC_GIF, AV_CODEC_ID_GIF },
    { VLC_CODEC_DXA, AV_CODEC_ID_DXA },
    { VLC_CODEC_DNXHD, AV_CODEC_ID_DNXHD },
    { VLC_CODEC_THP, AV_CODEC_ID_THP },
    { VLC_CODEC_SGI, AV_CODEC_ID_SGI },
    { VLC_CODEC_C93, AV_CODEC_ID_C93 },
    { VLC_CODEC_BETHSOFTVID, AV_CODEC_ID_BETHSOFTVID },
    /* AV_CODEC_ID_PTX */
    { VLC_CODEC_TXD, AV_CODEC_ID_TXD },
    { VLC_CODEC_VP6A, AV_CODEC_ID_VP6A },
    { VLC_CODEC_AMV, AV_CODEC_ID_AMV },
    { VLC_CODEC_VB, AV_CODEC_ID_VB },
    { VLC_CODEC_PCX, AV_CODEC_ID_PCX },
    /* AV_CODEC_ID_SUNRAST */
    { VLC_CODEC_INDEO4, AV_CODEC_ID_INDEO4 },
    { VLC_CODEC_INDEO5, AV_CODEC_ID_INDEO5 },
    { VLC_CODEC_MIMIC, AV_CODEC_ID_MIMIC },
    { VLC_CODEC_RL2, AV_CODEC_ID_RL2 },
    { VLC_CODEC_ESCAPE124, AV_CODEC_ID_ESCAPE124 },
    { VLC_CODEC_DIRAC, AV_CODEC_ID_DIRAC },
    { VLC_CODEC_BFI, AV_CODEC_ID_BFI },
    { VLC_CODEC_CMV, AV_CODEC_ID_CMV },
    { VLC_CODEC_MOTIONPIXELS, AV_CODEC_ID_MOTIONPIXELS },
    { VLC_CODEC_TGV, AV_CODEC_ID_TGV },
    { VLC_CODEC_TGQ, AV_CODEC_ID_TGQ },
    { VLC_CODEC_TQI, AV_CODEC_ID_TQI },
    { VLC_CODEC_AURA, AV_CODEC_ID_AURA },
    /* AV_CODEC_ID_AURA2 */
    /* AV_CODEC_ID_V210X */
    { VLC_CODEC_TMV, AV_CODEC_ID_TMV },
    { VLC_CODEC_V210, AV_CODEC_ID_V210 },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 54, 50, 100 ) && LIBAVCODEC_VERSION_MICRO >= 100
    { VLC_CODEC_VUYA, AV_CODEC_ID_AYUV },
#endif
    /* AV_CODEC_ID_DPX */
    { VLC_CODEC_MAD, AV_CODEC_ID_MAD },
    { VLC_CODEC_FRWU, AV_CODEC_ID_FRWU },
    { VLC_CODEC_FLASHSV2, AV_CODEC_ID_FLASHSV2 },
    /* AV_CODEC_ID_CDGRAPHICS */
    /* AV_CODEC_ID_R210 */
    { VLC_CODEC_ANM, AV_CODEC_ID_ANM },
    { VLC_CODEC_BINKVIDEO, AV_CODEC_ID_BINKVIDEO },
    /* AV_CODEC_ID_IFF_ILBM */
    /* AV_CODEC_ID_IFF_BYTERUN1 */
    { VLC_CODEC_KGV1, AV_CODEC_ID_KGV1 },
    { VLC_CODEC_YOP, AV_CODEC_ID_YOP },
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 58, 53, 100 ) && LIBAVCODEC_VERSION_MICRO >= 100
    { VLC_CODEC_VP4, AV_CODEC_ID_VP4 },
#endif
    { VLC_CODEC_VP8, AV_CODEC_ID_VP8 },
    /* AV_CODEC_ID_PICTOR */
    /* AV_CODEC_ID_ANSI */
    /* AV_CODEC_ID_A64_MULTI */
    /* AV_CODEC_ID_A64_MULTI5 */
    /* AV_CODEC_ID_R10K */
    { VLC_CODEC_MXPEG, AV_CODEC_ID_MXPEG },
    { VLC_CODEC_LAGARITH, AV_CODEC_ID_LAGARITH },
    { VLC_CODEC_PRORES, AV_CODEC_ID_PRORES },
    { VLC_CODEC_JV, AV_CODEC_ID_JV },
    { VLC_CODEC_DFA, AV_CODEC_ID_DFA },
    { VLC_CODEC_WMVP, AV_CODEC_ID_WMV3IMAGE },
    { VLC_CODEC_WMVP2, AV_CODEC_ID_VC1IMAGE },
    { VLC_CODEC_UTVIDEO, AV_CODEC_ID_UTVIDEO },
    { VLC_CODEC_BMVVIDEO, AV_CODEC_ID_BMV_VIDEO },
    { VLC_CODEC_VBLE, AV_CODEC_ID_VBLE },
    { VLC_CODEC_DXTORY, AV_CODEC_ID_DXTORY },
    { VLC_CODEC_V410, AV_CODEC_ID_V410 },
    /* AV_CODEC_ID_XWD */
    { VLC_CODEC_CDXL, AV_CODEC_ID_CDXL },
    /* AV_CODEC_ID_XBM */
    /* AV_CODEC_ID_ZEROCODEC */
    { VLC_CODEC_MSS1, AV_CODEC_ID_MSS1 },
    { VLC_CODEC_MSA1, AV_CODEC_ID_MSA1 },
    { VLC_CODEC_TSC2, AV_CODEC_ID_TSCC2 },
    { VLC_CODEC_MTS2, AV_CODEC_ID_MTS2 },
    { VLC_CODEC_CLLC, AV_CODEC_ID_CLLC },
    { VLC_CODEC_MSS2, AV_CODEC_ID_MSS2 },
    { VLC_CODEC_VP9, AV_CODEC_ID_VP9 },
#if LIBAVCODEC_VERSION_CHECK( 57, 26, 0, 83, 101 )
    { VLC_CODEC_AV1, AV_CODEC_ID_AV1 },
#endif
    { VLC_CODEC_ICOD, AV_CODEC_ID_AIC },
    /* AV_CODEC_ID_ESCAPE130 */
    { VLC_CODEC_G2M4, AV_CODEC_ID_G2M },
    { VLC_CODEC_G2M2, AV_CODEC_ID_G2M },
    { VLC_CODEC_G2M3, AV_CODEC_ID_G2M },
    /* AV_CODEC_ID_WEBP */
    { VLC_CODEC_HNM4_VIDEO, AV_CODEC_ID_HNM4_VIDEO },
    { VLC_CODEC_HEVC, AV_CODEC_ID_HEVC },

    { VLC_CODEC_FIC , AV_CODEC_ID_FIC },
    /* AV_CODEC_ID_ALIAS_PIX */
    /* AV_CODEC_ID_BRENDER_PIX */
    /* AV_CODEC_ID_PAF_VIDEO */
    /* AV_CODEC_ID_EXR */

    { VLC_CODEC_VP7 , AV_CODEC_ID_VP7 },
    /* AV_CODEC_ID_SANM */
    /* AV_CODEC_ID_SGIRLE */
    /* AV_CODEC_ID_MVC1 */
    /* AV_CODEC_ID_MVC2 */
    { VLC_CODEC_HQX, AV_CODEC_ID_HQX },

    { VLC_CODEC_TDSC, AV_CODEC_ID_TDSC },

    { VLC_CODEC_HQ_HQA, AV_CODEC_ID_HQ_HQA },

    { VLC_CODEC_HAP, AV_CODEC_ID_HAP },
    /* AV_CODEC_ID_DDS */

    { VLC_CODEC_DXV, AV_CODEC_ID_DXV },

    /* ffmpeg only: AV_CODEC_ID_BRENDER_PIX */
    /* ffmpeg only: AV_CODEC_ID_Y41P */
    /* ffmpeg only: AV_CODEC_ID_EXR */
    /* ffmpeg only: AV_CODEC_ID_AVRP */
    /* ffmpeg only: AV_CODEC_ID_012V */
    /* ffmpeg only: AV_CODEC_ID_AVUI */
    /* ffmpeg only: AV_CODEC_ID_TARGA_Y216 */
#if LIBAVCODEC_VERSION_MICRO >= 100
    { VLC_CODEC_V308, AV_CODEC_ID_V308 },
    { VLC_CODEC_V408, AV_CODEC_ID_V408 },
    { VLC_CODEC_YUV4, AV_CODEC_ID_YUV4 },
#endif
    /* ffmpeg only: AV_CODEC_ID_SANM */
    /* ffmpeg only: AV_CODEC_ID_PAF_VIDEO */
    /* ffmpeg only: AV_CODEC_ID_AVRN */
    /* ffmpeg only: AV_CODEC_ID_CPIA */
    /* ffmpeg only: AV_CODEC_ID_XFACE */
    /* ffmpeg only: AV_CODEC_ID_SGIRLE */
    /* ffmpeg only: AV_CODEC_ID_MVC1 */
    /* ffmpeg only: AV_CODEC_ID_MVC2 */
    /* ffmpeg only: AV_CODEC_ID_SNOW */
    /* ffmpeg only: AV_CODEC_ID_SMVJPEG */

#if LIBAVCODEC_VERSION_CHECK( 58, 0, 0, 1, 100 )
    { VLC_CODEC_MAGICYUV, AV_CODEC_ID_MAGICYUV },
#endif

#if LIBAVCODEC_VERSION_MICRO >= 100 /* FFmpeg only */
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 57, 24, 102 )
    { VLC_CODEC_CINEFORM, AV_CODEC_ID_CFHD },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 57, 70, 100 )
    { VLC_CODEC_PIXLET, AV_CODEC_ID_PIXLET },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 57, 71, 101 )
    { VLC_CODEC_SPEEDHQ, AV_CODEC_ID_SPEEDHQ },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 57, 79, 100 )
    { VLC_CODEC_FMVC, AV_CODEC_ID_FMVC },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 58, 24, 100 )
    { VLC_CODEC_IMM4, AV_CODEC_ID_IMM4 },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 58, 49, 100 )
    { VLC_CODEC_AGM, AV_CODEC_ID_AGM },
#endif

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT( 58, 56, 100 )
    { VLC_CODEC_IMM5, AV_CODEC_ID_IMM5 },
#endif
#endif /* FFmpeg only */
};

/*
 *  Audio Codecs
 */
static const struct vlc_avcodec_fourcc audio_codecs[] =
{
    /* PCM */
    { VLC_CODEC_S16L, AV_CODEC_ID_PCM_S16LE },
    { VLC_CODEC_S16B, AV_CODEC_ID_PCM_S16BE },
    { VLC_CODEC_U16L, AV_CODEC_ID_PCM_U16LE },
    { VLC_CODEC_U16B, AV_CODEC_ID_PCM_U16BE },
    { VLC_CODEC_S8, AV_CODEC_ID_PCM_S8 },
    { VLC_CODEC_U8, AV_CODEC_ID_PCM_U8 },
    { VLC_CODEC_MULAW, AV_CODEC_ID_PCM_MULAW },
    { VLC_CODEC_ALAW, AV_CODEC_ID_PCM_ALAW },
    { VLC_CODEC_S32L, AV_CODEC_ID_PCM_S32LE },
    { VLC_CODEC_S32B, AV_CODEC_ID_PCM_S32BE },
    { VLC_CODEC_U32L, AV_CODEC_ID_PCM_U32LE },
    { VLC_CODEC_U32B, AV_CODEC_ID_PCM_U32BE },
    { VLC_CODEC_S24L, AV_CODEC_ID_PCM_S24LE },
    { VLC_CODEC_S24B, AV_CODEC_ID_PCM_S24BE },
    { VLC_CODEC_U24L, AV_CODEC_ID_PCM_U24LE },
    { VLC_CODEC_U24B, AV_CODEC_ID_PCM_U24BE },
    { VLC_CODEC_S24DAUD, AV_CODEC_ID_PCM_S24DAUD },
    /* AV_CODEC_ID_PCM_ZORK */
    { VLC_CODEC_S16L_PLANAR, AV_CODEC_ID_PCM_S16LE_PLANAR },
    /* AV_CODEC_ID_PCM_DVD */
    { VLC_CODEC_F32B, AV_CODEC_ID_PCM_F32BE },
    { VLC_CODEC_F32L, AV_CODEC_ID_PCM_F32LE },
    { VLC_CODEC_F64B, AV_CODEC_ID_PCM_F64BE },
    { VLC_CODEC_F64L, AV_CODEC_ID_PCM_F64LE },
    { VLC_CODEC_BD_LPCM, AV_CODEC_ID_PCM_BLURAY },
    /* AV_CODEC_ID_PCM_LXF */
    /* AV_CODEC_ID_S302M */
    /* AV_CODEC_ID_PCM_S8_PLANAR */
    /* AV_CODEC_ID_PCM_S24LE_PLANAR */
    /* AV_CODEC_ID_PCM_S32LE_PLANAR */
    /* ffmpeg only: AV_CODEC_ID_PCM_S16BE_PLANAR */

    /* ADPCM */
    { VLC_CODEC_ADPCM_IMA_QT, AV_CODEC_ID_ADPCM_IMA_QT },
    { VLC_CODEC_ADPCM_IMA_WAV, AV_CODEC_ID_ADPCM_IMA_WAV },
    /* AV_CODEC_ID_ADPCM_IMA_DK3 */
    /* AV_CODEC_ID_ADPCM_IMA_DK4 */
    { VLC_CODEC_ADPCM_IMA_WS, AV_CODEC_ID_ADPCM_IMA_WS },
    /* AV_CODEC_ID_ADPCM_IMA_SMJPEG */
    { VLC_CODEC_ADPCM_MS, AV_CODEC_ID_ADPCM_MS },
    { VLC_CODEC_ADPCM_4XM, AV_CODEC_ID_ADPCM_4XM },
    { VLC_CODEC_ADPCM_XA, AV_CODEC_ID_ADPCM_XA },
    { VLC_CODEC_ADPCM_ADX, AV_CODEC_ID_ADPCM_ADX },
    { VLC_CODEC_ADPCM_EA, AV_CODEC_ID_ADPCM_EA },
    { VLC_CODEC_ADPCM_G726, AV_CODEC_ID_ADPCM_G726 },
    { VLC_CODEC_ADPCM_CREATIVE, AV_CODEC_ID_ADPCM_CT },
    { VLC_CODEC_ADPCM_SWF, AV_CODEC_ID_ADPCM_SWF },
    { VLC_CODEC_ADPCM_YAMAHA, AV_CODEC_ID_ADPCM_YAMAHA },
    { VLC_CODEC_ADPCM_SBPRO_4, AV_CODEC_ID_ADPCM_SBPRO_4 },
    { VLC_CODEC_ADPCM_SBPRO_3, AV_CODEC_ID_ADPCM_SBPRO_3 },
    { VLC_CODEC_ADPCM_SBPRO_2, AV_CODEC_ID_ADPCM_SBPRO_2 },
    { VLC_CODEC_ADPCM_THP, AV_CODEC_ID_ADPCM_THP },
    { VLC_CODEC_ADPCM_IMA_AMV, AV_CODEC_ID_ADPCM_IMA_AMV },
    { VLC_CODEC_ADPCM_EA_R1, AV_CODEC_ID_ADPCM_EA_R1 },
    /* AV_CODEC_ID_ADPCM_EA_R3 */
    /* AV_CODEC_ID_ADPCM_EA_R2 */
    { VLC_CODEC_ADPCM_IMA_EA_SEAD, AV_CODEC_ID_ADPCM_IMA_EA_SEAD },
    /* AV_CODEC_ID_ADPCM_IMA_EA_EACS */
    /* AV_CODEC_ID_ADPCM_EA_XAS */
    /* AV_CODEC_ID_ADPCM_EA_MAXIS_XA */
    /* AV_CODEC_ID_ADPCM_IMA_ISS */
    { VLC_CODEC_ADPCM_G722, AV_CODEC_ID_ADPCM_G722 },
    { VLC_CODEC_ADPCM_IMA_APC, AV_CODEC_ID_ADPCM_IMA_APC },
    /* ffmpeg only: AV_CODEC_ID_VIMA */
    /* ffmpeg only: AV_CODEC_ID_ADPCM_AFC */
    /* ffmpeg only: AV_CODEC_ID_ADPCM_IMA_OKI */
    /* ffmpeg only: AV_CODEC_ID_ADPCM_DTK */
    /* ffmpeg only: AV_CODEC_ID_ADPCM_IMA_RAD */
    /* ffmpeg only: AV_CODEC_ID_ADPCM_G726LE */

    /* AMR */
    { VLC_CODEC_AMR_NB, AV_CODEC_ID_AMR_NB },
    { VLC_CODEC_AMR_WB, AV_CODEC_ID_AMR_WB },

    /* RealAudio */
    { VLC_CODEC_RA_144, AV_CODEC_ID_RA_144 },
    { VLC_CODEC_RA_288, AV_CODEC_ID_RA_288 },

    /* DPCM */
    { VLC_CODEC_ROQ_DPCM, AV_CODEC_ID_ROQ_DPCM },
    { VLC_CODEC_INTERPLAY_DPCM, AV_CODEC_ID_INTERPLAY_DPCM },
    /* AV_CODEC_ID_XAN_DPCM */
    /* AV_CODEC_ID_SOL_DPCM */

    /* audio codecs */
    { VLC_CODEC_MPGA, AV_CODEC_ID_MP2 },
    { VLC_CODEC_MP2, AV_CODEC_ID_MP2 },
    { VLC_CODEC_MP3, AV_CODEC_ID_MP3 },
    { VLC_CODEC_MP4A, AV_CODEC_ID_AAC },
    { VLC_CODEC_A52, AV_CODEC_ID_AC3 },
    { VLC_CODEC_DTS, AV_CODEC_ID_DTS },
    { VLC_CODEC_VORBIS, AV_CODEC_ID_VORBIS },
    { VLC_CODEC_DVAUDIO, AV_CODEC_ID_DVAUDIO },
    { VLC_CODEC_WMA1, AV_CODEC_ID_WMAV1 },
    { VLC_CODEC_WMA2, AV_CODEC_ID_WMAV2 },
    { VLC_CODEC_MACE3, AV_CODEC_ID_MACE3 },
    { VLC_CODEC_MACE6, AV_CODEC_ID_MACE6 },
    { VLC_CODEC_VMDAUDIO, AV_CODEC_ID_VMDAUDIO },
    { VLC_CODEC_FLAC, AV_CODEC_ID_FLAC },
    /* AV_CODEC_ID_MP3ADU */
    /* AV_CODEC_ID_MP3ON4 */
    { VLC_CODEC_SHORTEN, AV_CODEC_ID_SHORTEN },
    { VLC_CODEC_ALAC, AV_CODEC_ID_ALAC },
    /* AV_CODEC_ID_WESTWOOD_SND1 */
    { VLC_CODEC_GSM, AV_CODEC_ID_GSM },
    { VLC_CODEC_QDM2, AV_CODEC_ID_QDM2 },
#if (LIBAVCODEC_VERSION_MICRO >= 100) && (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 71, 100))
    { VLC_CODEC_QDMC, AV_CODEC_ID_QDMC },
#endif
    { VLC_CODEC_COOK, AV_CODEC_ID_COOK },
    { VLC_CODEC_TRUESPEECH, AV_CODEC_ID_TRUESPEECH },
    { VLC_CODEC_TTA, AV_CODEC_ID_TTA },
    { VLC_CODEC_SMACKAUDIO, AV_CODEC_ID_SMACKAUDIO },
    { VLC_CODEC_QCELP, AV_CODEC_ID_QCELP },
    { VLC_CODEC_WAVPACK, AV_CODEC_ID_WAVPACK },
    { VLC_CODEC_DSICINAUDIO, AV_CODEC_ID_DSICINAUDIO },
    { VLC_CODEC_IMC, AV_CODEC_ID_IMC },
    { VLC_CODEC_MUSEPACK7, AV_CODEC_ID_MUSEPACK7 },
    { VLC_CODEC_MLP, AV_CODEC_ID_MLP },
    { VLC_CODEC_GSM_MS, AV_CODEC_ID_GSM_MS },
    { VLC_CODEC_ATRAC3, AV_CODEC_ID_ATRAC3 },
    { VLC_CODEC_APE, AV_CODEC_ID_APE },
    { VLC_CODEC_NELLYMOSER, AV_CODEC_ID_NELLYMOSER },
    { VLC_CODEC_MUSEPACK8, AV_CODEC_ID_MUSEPACK8 },
    { VLC_CODEC_SPEEX, AV_CODEC_ID_SPEEX },
    { VLC_CODEC_WMAS, AV_CODEC_ID_WMAVOICE },
    { VLC_CODEC_WMAP, AV_CODEC_ID_WMAPRO },
    { VLC_CODEC_WMAL, AV_CODEC_ID_WMALOSSLESS },
    { VLC_CODEC_ATRAC3P, AV_CODEC_ID_ATRAC3P },
    { VLC_CODEC_EAC3, AV_CODEC_ID_EAC3 },
    { VLC_CODEC_SIPR, AV_CODEC_ID_SIPR },
    /* AV_CODEC_ID_MP1 */
    { VLC_CODEC_TWINVQ, AV_CODEC_ID_TWINVQ },
    { VLC_CODEC_TRUEHD, AV_CODEC_ID_TRUEHD },
    { VLC_CODEC_ALS, AV_CODEC_ID_MP4ALS },
    { VLC_CODEC_ATRAC1, AV_CODEC_ID_ATRAC1 },
    { VLC_CODEC_BINKAUDIO_RDFT, AV_CODEC_ID_BINKAUDIO_RDFT },
    { VLC_CODEC_BINKAUDIO_DCT, AV_CODEC_ID_BINKAUDIO_DCT },
    { VLC_CODEC_MP4A, AV_CODEC_ID_AAC_LATM },
    /* AV_CODEC_ID_QDMC */
    /* AV_CODEC_ID_CELT */
    { VLC_CODEC_G723_1, AV_CODEC_ID_G723_1 },
    /* AV_CODEC_ID_G729 */
    /* AV_CODEC_ID_8SVX_EXP */
    /* AV_CODEC_ID_8SVX_FIB */
    { VLC_CODEC_BMVAUDIO, AV_CODEC_ID_BMV_AUDIO },
    { VLC_CODEC_RALF, AV_CODEC_ID_RALF },
    { VLC_CODEC_INDEO_AUDIO, AV_CODEC_ID_IAC },
    /* AV_CODEC_ID_ILBC */
    { VLC_CODEC_OPUS, AV_CODEC_ID_OPUS },
    /* AV_CODEC_ID_COMFORT_NOISE */
    { VLC_CODEC_TAK, AV_CODEC_ID_TAK },
    { VLC_CODEC_METASOUND, AV_CODEC_ID_METASOUND },
    /* AV_CODEC_ID_PAF_AUDIO */
    { VLC_CODEC_ON2AVC, AV_CODEC_ID_ON2AVC },

    /* DSD (FFmpeg only) */
#if LIBAVCODEC_VERSION_MICRO >= 100
    { VLC_CODEC_DSD_LSBF, AV_CODEC_ID_DSD_LSBF },
    { VLC_CODEC_DSD_MSBF, AV_CODEC_ID_DSD_MSBF },
    { VLC_CODEC_DSD_LSBF_PLANAR, AV_CODEC_ID_DSD_LSBF_PLANAR },
    { VLC_CODEC_DSD_MSBF_PLANAR, AV_CODEC_ID_DSD_MSBF_PLANAR },
#endif

    /* ffmpeg only: AV_CODEC_ID_FFWAVESYNTH */
    /* ffmpeg only: AV_CODEC_ID_SONIC */
    /* ffmpeg only: AV_CODEC_ID_SONIC_LS */
    /* ffmpeg only: AV_CODEC_ID_PAF_AUDIO */
    /* ffmpeg only: AV_CODEC_ID_EVRC */
    /* ffmpeg only: AV_CODEC_ID_SMV */
};

/* Subtitle streams */
static const struct vlc_avcodec_fourcc spu_codecs[] =
{
    { VLC_CODEC_SPU, AV_CODEC_ID_DVD_SUBTITLE },
    { VLC_CODEC_DVBS, AV_CODEC_ID_DVB_SUBTITLE },
    { VLC_CODEC_SUBT, AV_CODEC_ID_TEXT },
    { VLC_CODEC_XSUB, AV_CODEC_ID_XSUB },
    { VLC_CODEC_SSA, AV_CODEC_ID_SSA },
    { VLC_CODEC_TX3G, AV_CODEC_ID_MOV_TEXT },
    { VLC_CODEC_BD_PG, AV_CODEC_ID_HDMV_PGS_SUBTITLE },
#if (LIBAVCODEC_VERSION_MICRO >= 100) && (LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(57, 71, 100))
    { VLC_CODEC_BD_TEXT, AV_CODEC_ID_HDMV_TEXT_SUBTITLE },
#endif
    { VLC_CODEC_TELETEXT, AV_CODEC_ID_DVB_TELETEXT },
    /* AV_CODEC_ID_SRT */
    /* ffmpeg only: AV_CODEC_ID_MICRODVD */
    /* ffmpeg only: AV_CODEC_ID_EIA_608 */
    /* ffmpeg only: AV_CODEC_ID_JACOSUB */
    /* ffmpeg only: AV_CODEC_ID_SAMI */
    /* ffmpeg only: AV_CODEC_ID_REALTEXT */
    /* ffmpeg only: AV_CODEC_ID_SUBVIEWER1 */
    /* ffmpeg only: AV_CODEC_ID_SUBVIEWER */
    /* ffmpeg only: AV_CODEC_ID_SUBRIP */
    /* ffmpeg only: AV_CODEC_ID_WEBVTT */
    /* ffmpeg only: AV_CODEC_ID_MPL2 */
    /* ffmpeg only: AV_CODEC_ID_VPLAYER */
    /* ffmpeg only: AV_CODEC_ID_PJS */
    /* ffmpeg only: AV_CODEC_ID_ASS */
};

bool GetFfmpegCodec( enum es_format_category_e cat, vlc_fourcc_t i_fourcc,
                     unsigned *pi_ffmpeg_codec, const char **ppsz_name )
{
    const struct vlc_avcodec_fourcc *base;
    size_t count;

    switch( cat )
    {
        case VIDEO_ES:
            base = video_codecs;
            count = ARRAY_SIZE(video_codecs);
            break;
        case AUDIO_ES:
            base = audio_codecs;
            count = ARRAY_SIZE(audio_codecs);
            break;
        case SPU_ES:
            base = spu_codecs;
            count = ARRAY_SIZE(spu_codecs);
            break;
        default:
            base = NULL;
            count = 0;
    }

    i_fourcc = vlc_fourcc_GetCodec( cat, i_fourcc );

    for( size_t i = 0; i < count; i++ )
    {
        if( base[i].i_fourcc == i_fourcc )
        {
            if( pi_ffmpeg_codec != NULL )
                *pi_ffmpeg_codec = base[i].i_codec;
            if( ppsz_name )
                *ppsz_name = vlc_fourcc_GetDescription( cat, i_fourcc );
            return true;
        }
    }
    return false;
}

vlc_fourcc_t GetVlcFourcc( unsigned i_ffmpeg_codec )
{
    for( size_t i = 0; i < ARRAY_SIZE(video_codecs); i++ )
    {
        if( i_ffmpeg_codec == AV_CODEC_ID_RAWVIDEO )
            return VLC_CODEC_UNKNOWN;
        if( video_codecs[i].i_codec == i_ffmpeg_codec )
            return video_codecs[i].i_fourcc;
    }
    for( size_t i = 0; i < ARRAY_SIZE(audio_codecs); i++ )
    {
        if( audio_codecs[i].i_codec == i_ffmpeg_codec )
            return audio_codecs[i].i_fourcc;
    }
    for( size_t i = 0; i < ARRAY_SIZE(spu_codecs); i++ )
    {
        if( spu_codecs[i].i_codec == i_ffmpeg_codec )
            return spu_codecs[i].i_fourcc;
    }
    return VLC_CODEC_UNKNOWN;
}
