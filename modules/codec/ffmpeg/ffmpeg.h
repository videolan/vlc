/*****************************************************************************
 * ffmpeg_vdec.h: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ffmpeg.h,v 1.25 2003/10/01 22:39:43 hartman Exp $
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

#include "codecs.h"                                      /* BITMAPINFOHEADER */


#define DECODER_THREAD_COMMON \
    decoder_fifo_t      *p_fifo; \
    \
    int i_cat; /* AUDIO_ES, VIDEO_ES */ \
    int i_codec_id; \
    char *psz_namecodec; \
    \
    AVCodecContext      *p_context; \
    AVCodec             *p_codec; \
    mtime_t input_pts_previous; \
    mtime_t input_pts; \
    mtime_t pts; \
    \
    /* Private stuff for frame gathering */ \
    u8      *p_buffer;      /* buffer for gather pes */  \
    int     i_buffer_size;  /* size of allocated p_buffer */ \
    int     i_buffer;       /* bytes already present in p_buffer */


typedef struct generic_thread_s
{
    DECODER_THREAD_COMMON

} generic_thread_t;

#if LIBAVCODEC_BUILD >= 4663
#   define LIBAVCODEC_PP
#else
#   undef  LIBAVCODEC_PP
#endif

#define FREE( p ) if( p ) free( p ); p = NULL

int E_( GetPESData )( u8 *p_buf, int i_max, pes_packet_t *p_pes );

/*****************************************************************************
 * Video codec fourcc
 *****************************************************************************/

/* MPEG 1/2 video */
#define FOURCC_mpgv         VLC_FOURCC('m','p','g','v')

/* MPEG4 video */
#define FOURCC_DIVX         VLC_FOURCC('D','I','V','X')
#define FOURCC_divx         VLC_FOURCC('d','i','v','x')
#define FOURCC_DIV1         VLC_FOURCC('D','I','V','1')
#define FOURCC_div1         VLC_FOURCC('d','i','v','1')
#define FOURCC_MP4S         VLC_FOURCC('M','P','4','S')
#define FOURCC_mp4s         VLC_FOURCC('m','p','4','s')
#define FOURCC_M4S2         VLC_FOURCC('M','4','S','2')
#define FOURCC_m4s2         VLC_FOURCC('m','4','s','2')
#define FOURCC_xvid         VLC_FOURCC('x','v','i','d')
#define FOURCC_XVID         VLC_FOURCC('X','V','I','D')
#define FOURCC_XviD         VLC_FOURCC('X','v','i','D')
#define FOURCC_DX50         VLC_FOURCC('D','X','5','0')
#define FOURCC_mp4v         VLC_FOURCC('m','p','4','v')
#define FOURCC_4            VLC_FOURCC( 4,  0,  0,  0 )
#define FOURCC_m4cc         VLC_FOURCC('m','4','c','c')
#define FOURCC_M4CC         VLC_FOURCC('M','4','C','C')

/* MSMPEG4 v2 */
#define FOURCC_MPG4         VLC_FOURCC('M','P','G','4')
#define FOURCC_mpg4         VLC_FOURCC('m','p','g','4')
#define FOURCC_DIV2         VLC_FOURCC('D','I','V','2')
#define FOURCC_div2         VLC_FOURCC('d','i','v','2')
#define FOURCC_MP42         VLC_FOURCC('M','P','4','2')
#define FOURCC_mp42         VLC_FOURCC('m','p','4','2')

/* MSMPEG4 v3 / M$ mpeg4 v3 */
#define FOURCC_MPG3         VLC_FOURCC('M','P','G','3')
#define FOURCC_mpg3         VLC_FOURCC('m','p','g','3')
#define FOURCC_div3         VLC_FOURCC('d','i','v','3')
#define FOURCC_MP43         VLC_FOURCC('M','P','4','3')
#define FOURCC_mp43         VLC_FOURCC('m','p','4','3')

/* DivX 3.20 */
#define FOURCC_DIV3         VLC_FOURCC('D','I','V','3')
#define FOURCC_DIV4         VLC_FOURCC('D','I','V','4')
#define FOURCC_div4         VLC_FOURCC('d','i','v','4')
#define FOURCC_DIV5         VLC_FOURCC('D','I','V','5')
#define FOURCC_div5         VLC_FOURCC('d','i','v','5')
#define FOURCC_DIV6         VLC_FOURCC('D','I','V','6')
#define FOURCC_div6         VLC_FOURCC('d','i','v','6')

/* AngelPotion stuff */
#define FOURCC_AP41         VLC_FOURCC('A','P','4','1')

/* 3ivx doctered divx files */
#define FOURCC_3IVD         VLC_FOURCC('3','I','V','D')
#define FOURCC_3ivd         VLC_FOURCC('3','i','v','d')

/* 3ivx delta 3.5 Unsupported */ 
#define FOURCC_3IV1         VLC_FOURCC('3','I','V','1')
#define FOURCC_3iv1         VLC_FOURCC('3','i','v','1')

/* 3ivx delta 4 */
#define FOURCC_3IV2         VLC_FOURCC('3','I','V','2')
#define FOURCC_3iv2         VLC_FOURCC('3','i','v','2')

/* who knows? */
#define FOURCC_3VID         VLC_FOURCC('3','V','I','D')
#define FOURCC_3vid         VLC_FOURCC('3','v','i','d')

/* H263 and H263i */
/* H263(+) is also known as Real Video 1.0 */
#define FOURCC_H263         VLC_FOURCC('H','2','6','3')
#define FOURCC_h263         VLC_FOURCC('h','2','6','3')
#define FOURCC_U263         VLC_FOURCC('U','2','6','3')
#define FOURCC_I263         VLC_FOURCC('I','2','6','3')
#define FOURCC_i263         VLC_FOURCC('i','2','6','3')
/* Flash (H263) variant */
#define FOURCC_FLV1         VLC_FOURCC('F','L','V','1')


/* Sorenson v1/3 */
#define FOURCC_SVQ1         VLC_FOURCC('S','V','Q','1')
#define FOURCC_SVQ3         VLC_FOURCC('S','V','Q','3')

/* mjpeg */
#define FOURCC_MJPG         VLC_FOURCC( 'M', 'J', 'P', 'G' )
#define FOURCC_mjpg         VLC_FOURCC( 'm', 'j', 'p', 'g' )
    /* for mov file */
#define FOURCC_mjpa         VLC_FOURCC( 'm', 'j', 'p', 'a' )
    /* for mov file XXX: untested */
#define FOURCC_mjpb         VLC_FOURCC( 'm', 'j', 'p', 'b' )

#define FOURCC_jpeg         VLC_FOURCC( 'j', 'p', 'e', 'g' )
#define FOURCC_JPEG         VLC_FOURCC( 'J', 'P', 'E', 'G' )
#define FOURCC_JFIF         VLC_FOURCC( 'J', 'F', 'I', 'F' )
#define FOURCC_JPGL         VLC_FOURCC( 'J', 'P', 'G', 'L' )

/* Microsoft Video 1 */
#define FOURCC_MSVC         VLC_FOURCC('M','S','V','C') 
#define FOURCC_msvc         VLC_FOURCC('m','s','v','c') 
#define FOURCC_CRAM         VLC_FOURCC('C','R','A','M') 
#define FOURCC_cram         VLC_FOURCC('c','r','a','m') 
#define FOURCC_WHAM         VLC_FOURCC('W','H','A','M') 
#define FOURCC_wham         VLC_FOURCC('w','h','a','m') 

/* Windows Screen Video */
#define FOURCC_MSS1         VLC_FOURCC('M','S','S','1')

/* Microsoft RLE */
#define FOURCC_mrle         VLC_FOURCC('m','r','l','e')
#define FOURCC_1000         VLC_FOURCC(0x1,0x0,0x0,0x0)

/* Windows Media Video */
#define FOURCC_WMV1         VLC_FOURCC('W','M','V','1')
#define FOURCC_WMV2         VLC_FOURCC('W','M','V','2')

/* DV */
#define FOURCC_dvsl         VLC_FOURCC('d','v','s','l')
#define FOURCC_dvsd         VLC_FOURCC('d','v','s','d')
#define FOURCC_DVSD         VLC_FOURCC('D','V','S','D')
#define FOURCC_dvhd         VLC_FOURCC('d','v','h','d')
#define FOURCC_dvc          VLC_FOURCC('d','v','c',' ')
#define FOURCC_dvp          VLC_FOURCC('d','v','p',' ')
#define FOURCC_CDVC         VLC_FOURCC('C','D','V','C')

/* Indeo Video Codecs */
#define FOURCC_IV31         VLC_FOURCC('I','V','3','1')
#define FOURCC_iv31         VLC_FOURCC('i','v','3','1')
#define FOURCC_IV32         VLC_FOURCC('I','V','3','2')
#define FOURCC_iv32         VLC_FOURCC('i','v','3','2')

/* On2 VP3 Video Codecs */
#define FOURCC_VP31         VLC_FOURCC('V','P','3','1')
#define FOURCC_vp31         VLC_FOURCC('v','p','3','1')

/* Asus Video */
#define FOURCC_ASV1         VLC_FOURCC('A','S','V','1')
#define FOURCC_ASV2         VLC_FOURCC('A','S','V','2')

/* ATI VCR1 */
#define FOURCC_VCR1         VLC_FOURCC('V','C','R','1')

/* FFMPEG Video 1 (lossless codec) */
#define FOURCC_FFV1         VLC_FOURCC('F','F','V','1')

/* Cirrus Logic AccuPak */
#define FOURCC_CLJR         VLC_FOURCC('C','L','J','R')

/* Creative YUV */
#define FOURCC_CYUV         VLC_FOURCC('C','Y','U','V')

/* Huff YUV */
#define FOURCC_HFYU         VLC_FOURCC('H','F','Y','U')

/* Apple Video */
#define FOURCC_rpza         VLC_FOURCC('r','p','z','a')

/* Cinepak */
#define FOURCC_cvid         VLC_FOURCC('c','v','i','d')


/*****************************************************************************
 * Audio codec fourcc
 *****************************************************************************/
#define FOURCC_WMA1         VLC_FOURCC('W','M','A','1')
#define FOURCC_wma1         VLC_FOURCC('w','m','a','1')
#define FOURCC_WMA2         VLC_FOURCC('W','M','A','2')
#define FOURCC_wma2         VLC_FOURCC('w','m','a','2')
#define FOURCC_dvau         VLC_FOURCC('d','v','a','u')

#define FOURCC_MAC3         VLC_FOURCC('M','A','C','3')
#define FOURCC_MAC6         VLC_FOURCC('M','A','C','6')

#define FOURCC_RA10         VLC_FOURCC('1','4','_','4')
#define FOURCC_RA20         VLC_FOURCC('2','8','_','8')

