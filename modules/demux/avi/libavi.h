/*****************************************************************************
 * libavi.h : LibAVI library 
 ******************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: libavi.h,v 1.2 2002/11/05 10:07:56 gbazin Exp $
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

/* flags for use in <dwFlags> in AVIFileHdr */
#define AVIF_HASINDEX       0x00000010  /* Index at end of file? */
#define AVIF_MUSTUSEINDEX   0x00000020
#define AVIF_ISINTERLEAVED  0x00000100
#define AVIF_TRUSTCKTYPE    0x00000800  /* Use CKType to find key frames? */
#define AVIF_WASCAPTUREFILE 0x00010000
#define AVIF_COPYRIGHTED    0x00020000

/* Flags for index */
#define AVIIF_LIST          0x00000001L /* chunk is a 'LIST' */
#define AVIIF_KEYFRAME      0x00000010L /* this frame is a key frame.*/
#define AVIIF_NOTIME        0x00000100L /* this frame doesn't take any time */
#define AVIIF_COMPUSE       0x0FFF0000L /* these bits are for compressor use */

#define AVIIF_FIXKEYFRAME   0x00001000L /* invented; used to say that 
                                           the keyframe flag isn't a true flag
                                           but have to be verified */
#define MKFOURCC( a, b, c, d ) \
    ( ((u32)a) | ( ((u32)b) << 8 ) | ( ((u32)c) << 16 ) | ( ((u32)d) << 24 ) )
#define MKTWOCC( a, b ) \
    ( (u16)(a) | ( (u16)(b) << 8 ) )

    /* *** avi stuff *** */

#define AVIFOURCC_RIFF         MKFOURCC('R','I','F','F')
#define AVIFOURCC_LIST         MKFOURCC('L','I','S','T')
#define AVIFOURCC_JUNK         MKFOURCC('J','U','N','K')
#define AVIFOURCC_AVI          MKFOURCC('A','V','I',' ')
#define AVIFOURCC_WAVE         MKFOURCC('W','A','V','E')
#define AVIFOURCC_INFO         MKFOURCC('I','N','F','O')

#define AVIFOURCC_avih         MKFOURCC('a','v','i','h')
#define AVIFOURCC_hdrl         MKFOURCC('h','d','r','l')
#define AVIFOURCC_movi         MKFOURCC('m','o','v','i')
#define AVIFOURCC_idx1         MKFOURCC('i','d','x','1')

#define AVIFOURCC_strl         MKFOURCC('s','t','r','l')
#define AVIFOURCC_strh         MKFOURCC('s','t','r','h')
#define AVIFOURCC_strf         MKFOURCC('s','t','r','f')
#define AVIFOURCC_strd         MKFOURCC('s','t','r','d')

#define AVIFOURCC_rec          MKFOURCC('r','e','c',' ')
#define AVIFOURCC_auds         MKFOURCC('a','u','d','s')
#define AVIFOURCC_vids         MKFOURCC('v','i','d','s')

#define AVIFOURCC_IARL         MKFOURCC('I','A','R','L')
#define AVIFOURCC_IART         MKFOURCC('I','A','R','T')
#define AVIFOURCC_ICMS         MKFOURCC('I','C','M','S')
#define AVIFOURCC_ICMT         MKFOURCC('I','C','M','T')
#define AVIFOURCC_ICOP         MKFOURCC('I','C','O','P')
#define AVIFOURCC_ICRD         MKFOURCC('I','C','R','D')
#define AVIFOURCC_ICRP         MKFOURCC('I','C','R','P')
#define AVIFOURCC_IDIM         MKFOURCC('I','D','I','M')
#define AVIFOURCC_IDPI         MKFOURCC('I','D','P','I')
#define AVIFOURCC_IENG         MKFOURCC('I','E','N','G')
#define AVIFOURCC_IGNR         MKFOURCC('I','G','N','R')
#define AVIFOURCC_IKEY         MKFOURCC('I','K','E','Y')
#define AVIFOURCC_ILGT         MKFOURCC('I','L','G','T')
#define AVIFOURCC_IMED         MKFOURCC('I','M','E','D')
#define AVIFOURCC_INAM         MKFOURCC('I','N','A','M')
#define AVIFOURCC_IPLT         MKFOURCC('I','P','L','T')
#define AVIFOURCC_IPRD         MKFOURCC('I','P','R','D')
#define AVIFOURCC_ISBJ         MKFOURCC('I','S','B','J')
#define AVIFOURCC_ISFT         MKFOURCC('I','S','F','T')
#define AVIFOURCC_ISHP         MKFOURCC('I','S','H','P')
#define AVIFOURCC_ISRC         MKFOURCC('I','S','R','C')
#define AVIFOURCC_ISRF         MKFOURCC('I','S','R','F')
#define AVIFOURCC_ITCH         MKFOURCC('I','T','C','H')
#define AVIFOURCC_ISMP         MKFOURCC('I','S','M','P')
#define AVIFOURCC_IDIT         MKFOURCC('I','D','I','T')

    
#define AVITWOCC_wb            MKTWOCC('w','b')
#define AVITWOCC_db            MKTWOCC('d','b')
#define AVITWOCC_dc            MKTWOCC('d','c')
#define AVITWOCC_pc            MKTWOCC('p','c')
    /* *** codex stuff ***  */

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

    /* ?? */
#define FOURCC_3IV1         VLC_FOURCC('3','I','V','1')
    /* H263 and H263i */
#define FOURCC_H263         VLC_FOURCC('H','2','6','3')
#define FOURCC_h263         VLC_FOURCC('h','2','6','3')
#define FOURCC_U263         VLC_FOURCC('U','2','6','3')
#define FOURCC_I263         VLC_FOURCC('I','2','6','3')
#define FOURCC_i263         VLC_FOURCC('i','2','6','3')

/* Sound formats */
#define WAVE_FORMAT_UNKNOWN         0x0000
#define WAVE_FORMAT_PCM             0x0001
#define WAVE_FORMAT_MPEG            0x0050
#define WAVE_FORMAT_MPEGLAYER3      0x0055
#define WAVE_FORMAT_A52             0x2000
#define WAVE_FORMAT_WMA1            0x0160
#define WAVE_FORMAT_WMA2            0x0161


#define AVI_CHUNK_COMMON            \
    u32     i_chunk_fourcc;         \
    u64     i_chunk_size;           \
    u64     i_chunk_pos;            \
    union  avi_chunk_u *p_next;    \
    union  avi_chunk_u *p_father;  \
    union  avi_chunk_u *p_first;   \
    union  avi_chunk_u *p_last;

#define AVI_CHUNK( p_chk ) (avi_chunk_t*)(p_chk)

typedef struct idx1_entry_s
{
    vlc_fourcc_t i_fourcc;
    u32 i_flags;
    u32 i_pos;
    u32 i_length;

} idx1_entry_t;
typedef struct avi_chunk_common_s
{
    AVI_CHUNK_COMMON
} avi_chunk_common_t;

typedef struct avi_chunk_list_s
{
    AVI_CHUNK_COMMON
    u32 i_type;
} avi_chunk_list_t;

typedef struct avi_chunk_idx1_s
{
    AVI_CHUNK_COMMON
    int i_entry_count;
    int i_entry_max;
    idx1_entry_t *entry;

} avi_chunk_idx1_t;

typedef struct avi_chunk_avih_s
{
    AVI_CHUNK_COMMON
    u32 i_microsecperframe;
    u32 i_maxbytespersec;
    u32 i_reserved1; /* dwPaddingGranularity;    pad to multiples of this
                         size; normally 2K */
    u32 i_flags;
    u32 i_totalframes;
    u32 i_initialframes;
    u32 i_streams;
    u32 i_suggestedbuffersize;
    u32 i_width;
    u32 i_height;
    u32 i_scale;
    u32 i_rate;
    u32 i_start;
    u32 i_length;
} avi_chunk_avih_t;

typedef struct avi_chunk_strh_s
{
    AVI_CHUNK_COMMON
    u32 i_type;
    u32 i_handler;
    u32 i_flags;
    u32 i_reserved1;    /* wPriority wLanguage */
    u32 i_initialframes;
    u32 i_scale;
    u32 i_rate;
    u32 i_start;
    u32 i_length;       /* In units above... */
    u32 i_suggestedbuffersize;
    u32 i_quality;
    u32 i_samplesize;
} avi_chunk_strh_t;

typedef struct avi_chunk_strf_auds_s
{
    AVI_CHUNK_COMMON
    void *p_wfx;    //  waveformatex_t loaded from file

    u16 i_formattag;        // + 0x00
    u16 i_channels;         // + 0x02
    u32 i_samplespersec;    // + 0x04
    u32 i_avgbytespersec;   // + 0x08
    u16 i_blockalign;       // + 0x0c
    u16 i_bitspersample;    // + 0x0e
    u16 i_size; /* the extra size in bytes */
    u8  *p_data;
} avi_chunk_strf_auds_t;

typedef struct avi_chunk_strf_vids_s
{
    AVI_CHUNK_COMMON
    BITMAPINFOHEADER *p_bih;
} avi_chunk_strf_vids_t;

typedef union avi_chunk_strf_u
{
    avi_chunk_strf_auds_t   auds;
    avi_chunk_strf_vids_t   vids;
} avi_chunk_strf_t;

typedef struct avi_chunk_strd_s
{
    AVI_CHUNK_COMMON
    u8  *p_data;
} avi_chunk_strd_t;

typedef struct avi_chunk_STRING_s
{
    AVI_CHUNK_COMMON
    char *p_type;
    char *p_str;
} avi_chunk_STRING_t;

typedef union avi_chunk_u
{
    avi_chunk_common_t  common;
    avi_chunk_list_t    list;
    avi_chunk_idx1_t    idx1;
    avi_chunk_avih_t    avih;
    avi_chunk_strh_t    strh;
    avi_chunk_strf_t    strf;
    avi_chunk_strd_t    strd;
    avi_chunk_STRING_t  strz;
} avi_chunk_t;

/****************************************************************************
 * AVI_TestFile : test file header to see if it's an avi file
 ****************************************************************************/
int     AVI_TestFile( input_thread_t *p_input );

/****************************************************************************
 * Stream(input) acces function
 ****************************************************************************/
off_t   AVI_TellAbsolute( input_thread_t *p_input );
int     AVI_SeekAbsolute( input_thread_t *p_input, off_t i_pos);
int     AVI_ReadData( input_thread_t *p_input, u8 *p_buff, int i_size );
int     AVI_SkipBytes( input_thread_t *p_input, int i_count );

int     _AVI_ChunkRead( input_thread_t *p_input,
                        avi_chunk_t *p_chk,
                        avi_chunk_t *p_father,
                        int b_seekable );
void    _AVI_ChunkFree( input_thread_t *p_input,
                         avi_chunk_t *p_chk );
int     _AVI_ChunkGoto( input_thread_t *p_input,
                        avi_chunk_t *p_chk );
void    _AVI_ChunkDumpDebug( input_thread_t *p_input,
                             avi_chunk_t  *p_chk );

int     _AVI_ChunkCount( avi_chunk_t *p_chk, u32 i_fourcc );
avi_chunk_t *_AVI_ChunkFind( avi_chunk_t *p_chk, u32 i_fourcc, int i_number );

int     AVI_ChunkReadRoot( input_thread_t *p_input,
                           avi_chunk_t *p_root,
                           int b_seekable );
void    AVI_ChunkFreeRoot( input_thread_t *p_input,
                           avi_chunk_t  *p_chk );
#define AVI_ChunkRead( p_input, p_chk, p_father, b_seekable ) \
    _AVI_ChunkRead( p_input, \
                    (avi_chunk_t*)p_chk, \
                    (avi_chunk_t*)p_father, \
                    b_seekable )
#define AVI_ChunkFree( p_input, p_chk ) \
    _AVI_ChunkFree( p_input, (avi_chunk_t*)p_chk )
    
#define AVI_ChunkGoto( p_input, p_chk ) \
    _AVI_ChunkGoto( p_input, (avi_chunk_t*)p_chk )
    
#define AVI_ChunkDumpDebug( p_input, p_chk ) \
    _AVI_ChunkDumpDebug( p_input, (avi_chunk_t*)p_chk )


#define AVI_ChunkCount( p_chk, i_fourcc ) \
    _AVI_ChunkCount( (avi_chunk_t*)p_chk, i_fourcc )
#define AVI_ChunkFind( p_chk, i_fourcc, i_number ) \
    _AVI_ChunkFind( (avi_chunk_t*)p_chk, i_fourcc, i_number )



