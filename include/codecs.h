/*****************************************************************************
 * codecs.h: codec related structures needed by the demuxers and decoders
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id$
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
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

#ifndef _VLC_CODECS_H
#define _VLC_CODECS_H 1

/* Structures exported to the demuxers and decoders */

#if !(defined _GUID_DEFINED || defined GUID_DEFINED)
#define GUID_DEFINED
typedef struct _GUID
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t  Data4[8];
} GUID, *REFGUID, *LPGUID;
#endif /* GUID_DEFINED */

#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_
typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
_WAVEFORMATEX {
    uint16_t   wFormatTag;
    uint16_t   nChannels;
    uint32_t   nSamplesPerSec;
    uint32_t   nAvgBytesPerSec;
    uint16_t   nBlockAlign;
    uint16_t   wBitsPerSample;
    uint16_t   cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *NPWAVEFORMATEX, *LPWAVEFORMATEX;
#endif /* _WAVEFORMATEX_ */

#ifndef _WAVEFORMATEXTENSIBLE_
#define _WAVEFORMATEXTENSIBLE_
typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
_WAVEFORMATEXTENSIBLE {
    WAVEFORMATEX Format;
    union {
        uint16_t wValidBitsPerSample;
        uint16_t wSamplesPerBlock;
        uint16_t wReserved;
    } Samples;
    uint32_t     dwChannelMask;
    GUID SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;
#endif /* _WAVEFORMATEXTENSIBLE_ */

#ifndef _WAVEHEADER_
#define _WAVEHEADER_
typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
_WAVEHEADER {
    uint32_t MainChunkID;
    uint32_t Length;
    uint32_t ChunkTypeID;
    uint32_t SubChunkID;
    uint32_t SubChunkLength;
    uint16_t Format;
    uint16_t Modus;
    uint32_t SampleFreq;
    uint32_t BytesPerSec;
    uint16_t BytesPerSample;
    uint16_t BitsPerSample;
    uint32_t DataChunkID;
    uint32_t DataLength;
} WAVEHEADER;
#endif /* _WAVEHEADER_ */

#if !defined(_BITMAPINFOHEADER_) && !defined(WIN32)
#define _BITMAPINFOHEADER_
typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
{
    uint32_t   biSize;
    uint32_t   biWidth;
    uint32_t   biHeight;
    uint16_t   biPlanes;
    uint16_t   biBitCount;
    uint32_t   biCompression;
    uint32_t   biSizeImage;
    uint32_t   biXPelsPerMeter;
    uint32_t   biYPelsPerMeter;
    uint32_t   biClrUsed;
    uint32_t   biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct {
    BITMAPINFOHEADER bmiHeader;
    int        bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO;
#endif

#ifndef _RECT32_
#define _RECT32_
typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
{
    int left, top, right, bottom;
} RECT32;
#endif

#ifndef _REFERENCE_TIME_
#define _REFERENCE_TIME_
typedef int64_t REFERENCE_TIME;
#endif

#ifndef _VIDEOINFOHEADER_
#define _VIDEOINFOHEADER_
typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
{
    RECT32            rcSource;
    RECT32            rcTarget;
    uint32_t          dwBitRate;
    uint32_t          dwBitErrorRate;
    REFERENCE_TIME    AvgTimePerFrame;
    BITMAPINFOHEADER  bmiHeader;
    //int               reserved[3];
} VIDEOINFOHEADER;
#endif

/* WAVE format wFormatTag IDs */
#define WAVE_FORMAT_UNKNOWN             0x0000 /* Microsoft Corporation */
#define WAVE_FORMAT_PCM                 0x0001 /* Microsoft Corporation */
#define WAVE_FORMAT_ADPCM               0x0002 /* Microsoft Corporation */
#define WAVE_FORMAT_IEEE_FLOAT          0x0003 /* Microsoft Corporation */
#define WAVE_FORMAT_ALAW                0x0006 /* Microsoft Corporation */
#define WAVE_FORMAT_MULAW               0x0007 /* Microsoft Corporation */
#define WAVE_FORMAT_DTS_MS              0x0008 /* Microsoft Corporation */
#define WAVE_FORMAT_IMA_ADPCM           0x0011 /* Intel Corporation */
#define WAVE_FORMAT_GSM610              0x0031 /* Microsoft Corporation */
#define WAVE_FORMAT_MSNAUDIO            0x0032 /* Microsoft Corporation */
#define WAVE_FORMAT_G726                0x0045 /* ITU-T standard  */
#define WAVE_FORMAT_MPEG                0x0050 /* Microsoft Corporation */
#define WAVE_FORMAT_MPEGLAYER3          0x0055 /* ISO/MPEG Layer3 Format Tag */
#define WAVE_FORMAT_DOLBY_AC3_SPDIF     0x0092 /* Sonic Foundry */

#define WAVE_FORMAT_A52                 0x2000
#define WAVE_FORMAT_DTS                 0x2001
#define WAVE_FORMAT_WMA1                0x0160
#define WAVE_FORMAT_WMA2                0x0161
#define WAVE_FORMAT_WMA3                0x0162
#define WAVE_FORMAT_DIVIO_AAC           0x4143

/* Need to check these */
#define WAVE_FORMAT_DK3                 0x0061
#define WAVE_FORMAT_DK4                 0x0062

#if !defined(WAVE_FORMAT_EXTENSIBLE)
#define WAVE_FORMAT_EXTENSIBLE          0xFFFE /* Microsoft */
#endif

/* Microsoft speaker definitions */
#define WAVE_SPEAKER_FRONT_LEFT             0x1
#define WAVE_SPEAKER_FRONT_RIGHT            0x2
#define WAVE_SPEAKER_FRONT_CENTER           0x4
#define WAVE_SPEAKER_LOW_FREQUENCY          0x8
#define WAVE_SPEAKER_BACK_LEFT              0x10
#define WAVE_SPEAKER_BACK_RIGHT             0x20
#define WAVE_SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define WAVE_SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define WAVE_SPEAKER_BACK_CENTER            0x100
#define WAVE_SPEAKER_SIDE_LEFT              0x200
#define WAVE_SPEAKER_SIDE_RIGHT             0x400
#define WAVE_SPEAKER_TOP_CENTER             0x800
#define WAVE_SPEAKER_TOP_FRONT_LEFT         0x1000
#define WAVE_SPEAKER_TOP_FRONT_CENTER       0x2000
#define WAVE_SPEAKER_TOP_FRONT_RIGHT        0x4000
#define WAVE_SPEAKER_TOP_BACK_LEFT          0x8000
#define WAVE_SPEAKER_TOP_BACK_CENTER        0x10000
#define WAVE_SPEAKER_TOP_BACK_RIGHT         0x20000
#define WAVE_SPEAKER_RESERVED               0x80000000

static struct
{
    uint16_t     i_tag;
    vlc_fourcc_t i_fourcc;
    char         *psz_name;
}
wave_format_tag_to_fourcc[] =
{
    { WAVE_FORMAT_PCM,      VLC_FOURCC( 'a', 'r', 'a', 'w' ), "Raw audio" },
    { WAVE_FORMAT_ADPCM,    VLC_FOURCC( 'm', 's', 0x00,0x02), "Adpcm" },
    { WAVE_FORMAT_IEEE_FLOAT, VLC_FOURCC( 'a', 'f', 'l', 't' ), "IEEE Float audio" },
    { WAVE_FORMAT_ALAW,     VLC_FOURCC( 'a', 'l', 'a', 'w' ), "A-Law" },
    { WAVE_FORMAT_MULAW,    VLC_FOURCC( 'm', 'l', 'a', 'w' ), "Mu-Law" },
    { WAVE_FORMAT_IMA_ADPCM,VLC_FOURCC( 'm', 's', 0x00,0x11), "Ima-Adpcm" },
    { WAVE_FORMAT_G726,     VLC_FOURCC( 'g', '7', '2', '6' ), "G.726 Adpcm" },
    { WAVE_FORMAT_MPEGLAYER3,VLC_FOURCC('m', 'p', 'g', 'a' ), "Mpeg Audio" },
    { WAVE_FORMAT_MPEG,     VLC_FOURCC( 'm', 'p', 'g', 'a' ), "Mpeg Audio" },
    { WAVE_FORMAT_A52,      VLC_FOURCC( 'a', '5', '2', ' ' ), "A/52" },
    { WAVE_FORMAT_WMA1,     VLC_FOURCC( 'w', 'm', 'a', '1' ), "Window Media Audio 1" },
    { WAVE_FORMAT_WMA2,     VLC_FOURCC( 'w', 'm', 'a', '2' ), "Window Media Audio 2" },
    { WAVE_FORMAT_WMA3,     VLC_FOURCC( 'w', 'm', 'a', '3' ), "Window Media Audio 3" },
    { WAVE_FORMAT_DK3,      VLC_FOURCC( 'm', 's', 0x00,0x61), "Duck DK3" },
    { WAVE_FORMAT_DK4,      VLC_FOURCC( 'm', 's', 0x00,0x62), "Duck DK4" },
    { WAVE_FORMAT_DTS,      VLC_FOURCC( 'd', 't', 's', ' ' ), "DTS Coherent Acoustics" },
    { WAVE_FORMAT_DTS_MS,   VLC_FOURCC( 'd', 't', 's', ' ' ), "DTS Coherent Acoustics" },
    { WAVE_FORMAT_DIVIO_AAC,VLC_FOURCC( 'm', 'p', '4', 'a' ), "MPEG-4 Audio (Divio)" },
    { WAVE_FORMAT_UNKNOWN,  VLC_FOURCC( 'u', 'n', 'd', 'f' ), "Unknown" }
};

static inline void wf_tag_to_fourcc( uint16_t i_tag, vlc_fourcc_t *fcc,
                                     char **ppsz_name )
{
    int i;
    for( i = 0; wave_format_tag_to_fourcc[i].i_tag != 0; i++ )
    {
        if( wave_format_tag_to_fourcc[i].i_tag == i_tag ) break;
    }
    if( fcc ) *fcc = wave_format_tag_to_fourcc[i].i_fourcc;
    if( ppsz_name ) *ppsz_name = wave_format_tag_to_fourcc[i].psz_name;
}

static inline void fourcc_to_wf_tag( vlc_fourcc_t fcc, uint16_t *pi_tag )
{
    int i;
    for( i = 0; wave_format_tag_to_fourcc[i].i_tag != 0; i++ )
    {
        if( wave_format_tag_to_fourcc[i].i_fourcc == fcc ) break;
    }
    if( pi_tag ) *pi_tag = wave_format_tag_to_fourcc[i].i_tag;
}

/**
 * Structure to hold information concerning subtitles.
 * Used between demuxers and decoders of subtitles.
 */
typedef struct es_sys_t
{
    char        *psz_header; /* for 'ssa ' and 'subt' */

    /* for spudec */
    unsigned int        i_orig_height;
    unsigned int        i_orig_width;
    unsigned int        i_origin_x;
    unsigned int        i_origin_y;
    unsigned int        i_scale_h;
    unsigned int        i_scale_v;
    unsigned int        i_alpha;
    vlc_bool_t          b_smooth;
    mtime_t             i_fade_in;
    mtime_t             i_fade_out;
    unsigned int        i_align;
    mtime_t             i_time_offset;
    vlc_bool_t          b_forced_subs;
    unsigned int        palette[16];
    unsigned int        colors[4];

} subtitle_data_t;

#endif /* "codecs.h" */
