/*****************************************************************************
 * avi.h : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.h,v 1.1 2002/08/04 17:23:42 sam Exp $
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

#define 	MAX_PACKETS_IN_FIFO 	2

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

/* AVI stuff */
#define FOURCC_RIFF         VLC_FOURCC('R','I','F','F')
#define FOURCC_LIST         VLC_FOURCC('L','I','S','T')
#define FOURCC_JUNK         VLC_FOURCC('J','U','N','K')
#define FOURCC_AVI          VLC_FOURCC('A','V','I',' ')
#define FOURCC_WAVE         VLC_FOURCC('W','A','V','E')

#define FOURCC_avih         VLC_FOURCC('a','v','i','h')
#define FOURCC_hdrl         VLC_FOURCC('h','d','r','l')
#define FOURCC_movi         VLC_FOURCC('m','o','v','i')
#define FOURCC_idx1         VLC_FOURCC('i','d','x','1')

#define FOURCC_strl         VLC_FOURCC('s','t','r','l')
#define FOURCC_strh         VLC_FOURCC('s','t','r','h')
#define FOURCC_strf         VLC_FOURCC('s','t','r','f')
#define FOURCC_strd         VLC_FOURCC('s','t','r','d')

#define FOURCC_rec          VLC_FOURCC('r','e','c',' ')
#define FOURCC_auds         VLC_FOURCC('a','u','d','s')
#define FOURCC_vids         VLC_FOURCC('v','i','d','s')

#define TWOCC_wb            VLC_TWOCC('w','b')
#define TWOCC_db            VLC_TWOCC('d','b')
#define TWOCC_dc            VLC_TWOCC('d','c')
#define TWOCC_pc            VLC_TWOCC('p','c')

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

typedef struct bitmapinfoheader_s
{
    u32 i_size; /* size of header */
    u32 i_width;
    u32 i_height;
    u16 i_planes;
    u16 i_bitcount;
    u32 i_compression;
    u32 i_sizeimage;
    u32 i_xpelspermeter;
    u32 i_ypelspermeter;
    u32 i_clrused;
    u32 i_clrimportant;
} bitmapinfoheader_t;

typedef struct waveformatex_s
{
    u16 i_formattag;
    u16 i_channels;
    u32 i_samplespersec;
    u32 i_avgbytespersec;
    u16 i_blockalign;
    u16 i_bitspersample;
    u16 i_size; /* the extra size in bytes */
} waveformatex_t;


typedef struct MainAVIHeader_s
{
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

} MainAVIHeader_t;

typedef struct AVIStreamHeader_s
{
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

} AVIStreamHeader_t;

typedef struct AVIIndexEntry_s
{
    u32 i_id;
    u32 i_flags;
    u32 i_pos;
    u32 i_length;
    u32 i_lengthtotal;
} AVIIndexEntry_t;

typedef struct AVIESBuffer_s
{
    struct AVIESBuffer_s *p_next;

    pes_packet_t *p_pes;
    int i_posc;
    int i_posb;
} AVIESBuffer_t;


typedef struct AVIStreamInfo_s
{

    riffchunk_t *p_strl;
    riffchunk_t *p_strh;
    riffchunk_t *p_strf;
    riffchunk_t *p_strd; /* not used */
    
    AVIStreamHeader_t header;
    
    u8 i_cat;           /* AUDIO_ES, VIDEO_ES */
    bitmapinfoheader_t  video_format;
    waveformatex_t      audio_format;
    es_descriptor_t     *p_es;   
    int                 b_selected; /* newly selected */
    AVIIndexEntry_t     *p_index;
    int                 i_idxnb;
    int                 i_idxmax; 

    int                 i_idxposc;  /* numero of chunk */
    int                 i_idxposb;  /* byte in the current chunk */

    /* add some buffering */
    AVIESBuffer_t       *p_pes_first;
    AVIESBuffer_t       *p_pes_last;
    int                 i_pes_count;
    int                 i_pes_totalsize;
} AVIStreamInfo_t;

typedef struct demux_data_avi_file_s
{
    mtime_t i_pcr; 
    int     i_rate;
    riffchunk_t *p_riff;
    riffchunk_t *p_hdrl;
    riffchunk_t *p_movi;
    riffchunk_t *p_idx1;

    int     b_seekable;

    /* Info extrated from avih */
    MainAVIHeader_t avih;

    /* number of stream and informations*/
    int i_streams;
    AVIStreamInfo_t   **pp_info; 

    /* current audio and video es */
    AVIStreamInfo_t *p_info_video;
    AVIStreamInfo_t *p_info_audio;

} demux_data_avi_file_t;

