/*****************************************************************************
 * ffmpeg_vdec.h: video decoder using ffmpeg library
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: ffmpeg.h,v 1.17 2003/04/17 10:58:30 fenrir Exp $
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

#define GetWLE( p ) \
    ( *(u8*)(p) + ( *((u8*)(p)+1) << 8 ) )

#define GetDWLE( p ) \
    (  *(u8*)(p) + ( *((u8*)(p)+1) << 8 ) + \
     ( *((u8*)(p)+2) << 16 ) + ( *((u8*)(p)+3) << 24 ) )

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
#define FOURCC_H263         VLC_FOURCC('H','2','6','3')
#define FOURCC_h263         VLC_FOURCC('h','2','6','3')
#define FOURCC_U263         VLC_FOURCC('U','2','6','3')
#define FOURCC_I263         VLC_FOURCC('I','2','6','3')
#define FOURCC_i263         VLC_FOURCC('i','2','6','3')

/* Sorenson v1 */
#define FOURCC_SVQ1 VLC_FOURCC( 'S', 'V', 'Q', '1' )

/* mjpeg */
#define FOURCC_MJPG VLC_FOURCC( 'M', 'J', 'P', 'G' )
#define FOURCC_mjpg VLC_FOURCC( 'm', 'j', 'p', 'g' )
    /* for mov file */
#define FOURCC_mjpa VLC_FOURCC( 'm', 'j', 'p', 'a' )
    /* for mov file XXX: untested */
#define FOURCC_mjpb VLC_FOURCC( 'm', 'j', 'p', 'b' )

#define FOURCC_jpeg VLC_FOURCC( 'j', 'p', 'e', 'g' )
#define FOURCC_JPEG VLC_FOURCC( 'J', 'P', 'E', 'G' )
#define FOURCC_JFIF VLC_FOURCC( 'J', 'F', 'I', 'F' )

/* wmv */
#define FOURCC_WMV1         VLC_FOURCC('W','M','V','1')
#define FOURCC_WMV2         VLC_FOURCC('W','M','V','2')
#define FOURCC_MSS1         VLC_FOURCC('M','S','S','1')

#define FOURCC_dvsl         VLC_FOURCC('d','v','s','l')
#define FOURCC_dvsd         VLC_FOURCC('d','v','s','d')
#define FOURCC_DVSD         VLC_FOURCC('D','V','S','D')
#define FOURCC_dvhd         VLC_FOURCC('d','v','h','d')
#define FOURCC_dvc          VLC_FOURCC('d','v','c',' ')
#define FOURCC_dvp          VLC_FOURCC('d','v','p',' ')

#define FOURCC_IV31         VLC_FOURCC('I','V','3','1')
#define FOURCC_iv31         VLC_FOURCC('i','v','3','1')
#define FOURCC_IV32         VLC_FOURCC('I','V','3','2')
#define FOURCC_iv32         VLC_FOURCC('i','v','3','2')

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

