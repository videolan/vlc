/*****************************************************************************
 * libavi.h : LibAVI library
 ******************************************************************************
 * Copyright (C) 2001-2003 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

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

#define AVIIF_FIXKEYFRAME   0x00001000L /* invented; used to say that */
                                        /* the keyframe flag isn't a true flag */
                                        /* but have to be verified */

#define AVI_CHUNK_COMMON            \
    vlc_fourcc_t i_chunk_fourcc;    \
    uint64_t i_chunk_size;          \
    uint64_t i_chunk_pos;           \
    union  avi_chunk_u *p_next;    \
    union  avi_chunk_u *p_father;  \
    union  avi_chunk_u *p_first;   \
    union  avi_chunk_u *p_last;

#define AVI_CHUNK( p_chk ) (avi_chunk_t*)(p_chk)

typedef struct idx1_entry_s
{
    vlc_fourcc_t i_fourcc;
    uint32_t i_flags;
    uint32_t i_pos;
    uint32_t i_length;

} idx1_entry_t;
typedef struct avi_chunk_common_s
{
    AVI_CHUNK_COMMON
} avi_chunk_common_t;

typedef struct avi_chunk_list_s
{
    AVI_CHUNK_COMMON
    vlc_fourcc_t i_type;
} avi_chunk_list_t;

typedef struct avi_chunk_idx1_s
{
    AVI_CHUNK_COMMON
    unsigned int i_entry_count;
    unsigned int i_entry_max;
    idx1_entry_t *entry;

} avi_chunk_idx1_t;

typedef struct avi_chunk_avih_s
{
    AVI_CHUNK_COMMON
    uint32_t i_microsecperframe;
    uint32_t i_maxbytespersec;
    uint32_t i_reserved1; /* dwPaddingGranularity;    pad to multiples of this
                             size; normally 2K */
    uint32_t i_flags;
    uint32_t i_totalframes;
    uint32_t i_initialframes;
    uint32_t i_streams;
    uint32_t i_suggestedbuffersize;
    uint32_t i_width;
    uint32_t i_height;
    uint32_t i_scale;
    uint32_t i_rate;
    uint32_t i_start;
    uint32_t i_length;
} avi_chunk_avih_t;

typedef struct avi_chunk_strh_s
{
    AVI_CHUNK_COMMON
    vlc_fourcc_t i_type;
    uint32_t i_handler;
    uint32_t i_flags;
    uint32_t i_reserved1;    /* wPriority wLanguage */
    uint32_t i_initialframes;
    uint32_t i_scale;
    uint32_t i_rate;
    uint32_t i_start;
    uint32_t i_length;       /* In units above... */
    uint32_t i_suggestedbuffersize;
    uint32_t i_quality;
    uint32_t i_samplesize;
} avi_chunk_strh_t;

typedef struct avi_chunk_strf_auds_s
{
    AVI_CHUNK_COMMON
    int             i_cat;
    WAVEFORMATEX    *p_wf;
} avi_chunk_strf_auds_t;

typedef struct avi_chunk_strf_vids_s
{
    AVI_CHUNK_COMMON
    int              i_cat;
    BITMAPINFOHEADER *p_bih;
} avi_chunk_strf_vids_t;

typedef union avi_chunk_strf_u
{
    avi_chunk_strf_auds_t   auds;
    avi_chunk_strf_vids_t   vids;
    struct
    {
        AVI_CHUNK_COMMON
        int i_cat;
    }                       common;
} avi_chunk_strf_t;

typedef struct avi_chunk_strd_s
{
    AVI_CHUNK_COMMON
    uint8_t  *p_data;
} avi_chunk_strd_t;


#define AVI_INDEX_OF_INDEXES    0x00
#define AVI_INDEX_OF_CHUNKS     0x01
#define AVI_INDEX_IS_DATA       0x80

#define AVI_INDEX_2FIELD        0x01
typedef struct
{
    uint32_t i_offset;
    uint32_t i_size;
} indx_std_entry_t;

typedef struct
{
    uint32_t i_offset;
    uint32_t i_size;
    uint32_t i_offsetfield2;
} indx_field_entry_t;

typedef struct
{
    uint64_t i_offset;
    uint32_t i_size;
    uint32_t i_duration;
} indx_super_entry_t;

typedef struct avi_chunk_indx_s
{
    AVI_CHUNK_COMMON
    int16_t i_longsperentry;
    int8_t  i_indexsubtype;
    int8_t  i_indextype;
    int32_t i_entriesinuse;
    vlc_fourcc_t i_id;

    int64_t i_baseoffset;

    union
    {
        indx_std_entry_t    *std;
        indx_field_entry_t  *field;
        indx_super_entry_t  *super;
    } idx;
} avi_chunk_indx_t;

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
    avi_chunk_indx_t    indx;
    avi_chunk_STRING_t  strz;
} avi_chunk_t;

/****************************************************************************
 * Stream(input) access functions
 ****************************************************************************/
int     _AVI_ChunkRead( stream_t *,
                        avi_chunk_t *p_chk,
                        avi_chunk_t *p_father );

int     _AVI_ChunkCount( avi_chunk_t *, vlc_fourcc_t );
void   *_AVI_ChunkFind ( avi_chunk_t *, vlc_fourcc_t, int );

int     AVI_ChunkReadRoot( stream_t *, avi_chunk_t *p_root );
void    AVI_ChunkFreeRoot( stream_t *, avi_chunk_t  *p_chk );

#define AVI_ChunkRead( s, p_chk, p_father ) \
    _AVI_ChunkRead( s, p_chk, (avi_chunk_t*)p_father )
#define AVI_ChunkCount( p_chk, i_fourcc ) \
    _AVI_ChunkCount( (avi_chunk_t*)p_chk, i_fourcc )
#define AVI_ChunkFind( p_chk, i_fourcc, i_number ) \
    _AVI_ChunkFind( (avi_chunk_t*)p_chk, i_fourcc, i_number )


    /* *** avi stuff *** */

#define AVIFOURCC_RIFF         VLC_FOURCC('R','I','F','F')
#define AVIFOURCC_ON2          VLC_FOURCC('O','N','2',' ')
#define AVIFOURCC_LIST         VLC_FOURCC('L','I','S','T')
#define AVIFOURCC_JUNK         VLC_FOURCC('J','U','N','K')
#define AVIFOURCC_AVI          VLC_FOURCC('A','V','I',' ')
#define AVIFOURCC_AVIX         VLC_FOURCC('A','V','I','X')
#define AVIFOURCC_ON2f         VLC_FOURCC('O','N','2','f')
#define AVIFOURCC_WAVE         VLC_FOURCC('W','A','V','E')
#define AVIFOURCC_INFO         VLC_FOURCC('I','N','F','O')

#define AVIFOURCC_avih         VLC_FOURCC('a','v','i','h')
#define AVIFOURCC_ON2h         VLC_FOURCC('O','N','2','h')
#define AVIFOURCC_hdrl         VLC_FOURCC('h','d','r','l')
#define AVIFOURCC_movi         VLC_FOURCC('m','o','v','i')
#define AVIFOURCC_idx1         VLC_FOURCC('i','d','x','1')

#define AVIFOURCC_strl         VLC_FOURCC('s','t','r','l')
#define AVIFOURCC_strh         VLC_FOURCC('s','t','r','h')
#define AVIFOURCC_strf         VLC_FOURCC('s','t','r','f')
#define AVIFOURCC_strd         VLC_FOURCC('s','t','r','d')
#define AVIFOURCC_strn         VLC_FOURCC('s','t','r','n')
#define AVIFOURCC_indx         VLC_FOURCC('i','n','d','x')

#define AVIFOURCC_rec          VLC_FOURCC('r','e','c',' ')
#define AVIFOURCC_auds         VLC_FOURCC('a','u','d','s')
#define AVIFOURCC_vids         VLC_FOURCC('v','i','d','s')
#define AVIFOURCC_txts         VLC_FOURCC('t','x','t','s')
#define AVIFOURCC_mids         VLC_FOURCC('m','i','d','s')
#define AVIFOURCC_iavs         VLC_FOURCC('i','a','v','s')
#define AVIFOURCC_ivas         VLC_FOURCC('i','v','a','s')

#define AVIFOURCC_IARL         VLC_FOURCC('I','A','R','L')
#define AVIFOURCC_IART         VLC_FOURCC('I','A','R','T')
#define AVIFOURCC_ICMS         VLC_FOURCC('I','C','M','S')
#define AVIFOURCC_ICMT         VLC_FOURCC('I','C','M','T')
#define AVIFOURCC_ICOP         VLC_FOURCC('I','C','O','P')
#define AVIFOURCC_ICRD         VLC_FOURCC('I','C','R','D')
#define AVIFOURCC_ICRP         VLC_FOURCC('I','C','R','P')
#define AVIFOURCC_IDIM         VLC_FOURCC('I','D','I','M')
#define AVIFOURCC_IDPI         VLC_FOURCC('I','D','P','I')
#define AVIFOURCC_IENG         VLC_FOURCC('I','E','N','G')
#define AVIFOURCC_IGNR         VLC_FOURCC('I','G','N','R')
#define AVIFOURCC_IKEY         VLC_FOURCC('I','K','E','Y')
#define AVIFOURCC_ILGT         VLC_FOURCC('I','L','G','T')
#define AVIFOURCC_IMED         VLC_FOURCC('I','M','E','D')
#define AVIFOURCC_INAM         VLC_FOURCC('I','N','A','M')
#define AVIFOURCC_IPLT         VLC_FOURCC('I','P','L','T')
#define AVIFOURCC_IPRD         VLC_FOURCC('I','P','R','D')
#define AVIFOURCC_ISBJ         VLC_FOURCC('I','S','B','J')
#define AVIFOURCC_ISFT         VLC_FOURCC('I','S','F','T')
#define AVIFOURCC_ISHP         VLC_FOURCC('I','S','H','P')
#define AVIFOURCC_ISRC         VLC_FOURCC('I','S','R','C')
#define AVIFOURCC_ISRF         VLC_FOURCC('I','S','R','F')
#define AVIFOURCC_ITCH         VLC_FOURCC('I','T','C','H')
#define AVIFOURCC_ISMP         VLC_FOURCC('I','S','M','P')
#define AVIFOURCC_IDIT         VLC_FOURCC('I','D','I','T')

 
#define AVITWOCC_wb            VLC_TWOCC('w','b')
#define AVITWOCC_db            VLC_TWOCC('d','b')
#define AVITWOCC_dc            VLC_TWOCC('d','c')
#define AVITWOCC_pc            VLC_TWOCC('p','c')
    /* *** codex stuff ***  */

    /* Microsoft RLE video */
#define FOURCC_1            VLC_FOURCC( 1,  0,  0,  0 )

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
#define FOURCC_dx50         VLC_FOURCC('d','x','5','0')
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

    /* 3IVX */
#define FOURCC_3IV1         VLC_FOURCC('3','I','V','1')
#define FOURCC_3iv1         VLC_FOURCC('2','i','v','1')
#define FOURCC_3IV2         VLC_FOURCC('3','I','V','2')
#define FOURCC_3iv2         VLC_FOURCC('3','i','v','2')
#define FOURCC_3IVD         VLC_FOURCC('3','I','V','D')
#define FOURCC_3ivd         VLC_FOURCC('3','i','v','d')
#define FOURCC_3VID         VLC_FOURCC('3','V','I','D')
#define FOURCC_3vid         VLC_FOURCC('3','v','i','d')

    /* H263 and H263i */
#define FOURCC_H263         VLC_FOURCC('H','2','6','3')
#define FOURCC_h263         VLC_FOURCC('h','2','6','3')
#define FOURCC_U263         VLC_FOURCC('U','2','6','3')
#define FOURCC_I263         VLC_FOURCC('I','2','6','3')
#define FOURCC_i263         VLC_FOURCC('i','2','6','3')

    /* H264 */
#define FOURCC_H264         VLC_FOURCC('H','2','6','4')
#define FOURCC_h264         VLC_FOURCC('h','2','6','4')
#define FOURCC_VSSH         VLC_FOURCC('V','S','S','H')

    /* DV */
#define FOURCC_dvsd         VLC_FOURCC('d','v','s','d')
#define FOURCC_dvhd         VLC_FOURCC('d','v','h','d')
#define FOURCC_dvsl         VLC_FOURCC('d','v','s','l')
#define FOURCC_dv25         VLC_FOURCC('d','v','2','5')
#define FOURCC_dv50         VLC_FOURCC('d','v','5','0')
