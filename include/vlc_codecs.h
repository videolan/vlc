/*****************************************************************************
 * codecs.h: codec related structures needed by the demuxers and decoders
 *****************************************************************************
 * Copyright (C) 1999-2001 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_CODECS_H
#define VLC_CODECS_H 1

/**
 * \file
 * This file defines codec related structures needed by the demuxers and decoders
 */

#ifdef HAVE_ATTRIBUTE_PACKED
#   define ATTR_PACKED __attribute__((__packed__))
#else
#   error FIXME
#endif

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
ATTR_PACKED
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
ATTR_PACKED
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
ATTR_PACKED
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
ATTR_PACKED
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

typedef struct
ATTR_PACKED
{
    BITMAPINFOHEADER bmiHeader;
    int        bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO;
#endif

#ifndef _RECT32_
#define _RECT32_
typedef struct
ATTR_PACKED
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
ATTR_PACKED
{
    RECT32            rcSource;
    RECT32            rcTarget;
    uint32_t          dwBitRate;
    uint32_t          dwBitErrorRate;
    REFERENCE_TIME    AvgTimePerFrame;
    BITMAPINFOHEADER  bmiHeader;
} VIDEOINFOHEADER;
#endif

#ifndef _RGBQUAD_
#define _RGBQUAD_
typedef struct
ATTR_PACKED
{
    uint8_t rgbBlue;
    uint8_t rgbGreen;
    uint8_t rgbRed;
    uint8_t rgbReserved;
} RGBQUAD1;
#endif

#ifndef _TRUECOLORINFO_
#define _TRUECOLORINFO_
typedef struct
ATTR_PACKED
{
    uint32_t dwBitMasks[3];
    RGBQUAD1 bmiColors[256];
} TRUECOLORINFO;
#endif

#ifndef _VIDEOINFO_
#define _VIDEOINFO_
typedef struct
ATTR_PACKED
{
    RECT32            rcSource;
    RECT32            rcTarget;
    uint32_t          dwBitRate;
    uint32_t          dwBitErrorRate;
    REFERENCE_TIME    AvgTimePerFrame;
    BITMAPINFOHEADER  bmiHeader;

    union
    {
        RGBQUAD1 bmiColors[256]; /* Colour palette */
        uint32_t dwBitMasks[3]; /* True colour masks */
        TRUECOLORINFO TrueColorInfo; /* Both of the above */
    };

} VIDEOINFO;
#endif

/* WAVE format wFormatTag IDs */
#define WAVE_FORMAT_UNKNOWN             0x0000 /* Microsoft Corporation */
#define WAVE_FORMAT_PCM                 0x0001 /* Microsoft Corporation */
#define WAVE_FORMAT_ADPCM               0x0002 /* Microsoft Corporation */
#define WAVE_FORMAT_IEEE_FLOAT          0x0003 /* Microsoft Corporation */
#define WAVE_FORMAT_ALAW                0x0006 /* Microsoft Corporation */
#define WAVE_FORMAT_MULAW               0x0007 /* Microsoft Corporation */
#define WAVE_FORMAT_DTS_MS              0x0008 /* Microsoft Corporation */
#define WAVE_FORMAT_WMAS                0x000a /* WMA 9 Speech */
#define WAVE_FORMAT_IMA_ADPCM           0x0011 /* Intel Corporation */
#define WAVE_FORMAT_GSM610              0x0031 /* Microsoft Corporation */
#define WAVE_FORMAT_MSNAUDIO            0x0032 /* Microsoft Corporation */
#define WAVE_FORMAT_G726                0x0045 /* ITU-T standard  */
#define WAVE_FORMAT_MPEG                0x0050 /* Microsoft Corporation */
#define WAVE_FORMAT_MPEGLAYER3          0x0055 /* ISO/MPEG Layer3 Format Tag */
#define WAVE_FORMAT_DOLBY_AC3_SPDIF     0x0092 /* Sonic Foundry */

#define WAVE_FORMAT_A52                 0x2000
#define WAVE_FORMAT_DTS                 0x2001
#define WAVE_FORMAT_WMA1                0x0160 /* WMA version 1 */
#define WAVE_FORMAT_WMA2                0x0161 /* WMA (v2) 7, 8, 9 Series */
#define WAVE_FORMAT_WMAP                0x0162 /* WMA 9 Professional */
#define WAVE_FORMAT_WMAL                0x0163 /* WMA 9 Lossless */
#define WAVE_FORMAT_DIVIO_AAC           0x4143
#define WAVE_FORMAT_AAC                 0x00FF
#define WAVE_FORMAT_FFMPEG_AAC          0x706D

/* Need to check these */
#define WAVE_FORMAT_DK3                 0x0061
#define WAVE_FORMAT_DK4                 0x0062

/* At least FFmpeg use that ID: from libavformat/riff.c ('Vo' == 0x566f)
 * { CODEC_ID_VORBIS, ('V'<<8)+'o' }, //HACK/FIXME, does vorbis in WAV/AVI have an (in)official id?
 */
#define WAVE_FORMAT_VORBIS              0x566f

/* It seems that these IDs are used by braindead & obsolete VorbisACM encoder
 * (Windows only)
 * A few info is available except VorbisACM source (remember, Windows only)
 * (available on http://svn.xiph.org), but it seems that vo3+ at least is
 * made of Vorbis data encapsulated in Ogg container...
 */
#define WAVE_FORMAT_VORB_1              0x674f
#define WAVE_FORMAT_VORB_2              0x6750
#define WAVE_FORMAT_VORB_3              0x6751
#define WAVE_FORMAT_VORB_1PLUS          0x676f
#define WAVE_FORMAT_VORB_2PLUS          0x6770
#define WAVE_FORMAT_VORB_3PLUS          0x6771

#define WAVE_FORMAT_SPEEX               0xa109 /* Speex audio */


#if !defined(WAVE_FORMAT_EXTENSIBLE)
#define WAVE_FORMAT_EXTENSIBLE          0xFFFE /* Microsoft */
#endif

/* GUID SubFormat IDs */
/* We need both b/c const variables are not compile-time constants in C, giving
 * us an error if we use the const GUID in an enum */

#ifndef _KSDATAFORMAT_SUBTYPE_PCM_
#define _KSDATAFORMAT_SUBTYPE_PCM_ {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}}
static const GUID VLC_KSDATAFORMAT_SUBTYPE_PCM = {0xE923AABF, 0xCB58, 0x4471, {0xA1, 0x19, 0xFF, 0xFA, 0x01, 0xE4, 0xCE, 0x62}};
#define KSDATAFORMAT_SUBTYPE_PCM VLC_KSDATAFORMAT_SUBTYPE_PCM
#endif

#ifndef _KSDATAFORMAT_SUBTYPE_UNKNOWN_
#define _KSDATAFORMAT_SUBTYPE_UNKNOWN_ {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}
static const GUID VLC_KSDATAFORMAT_SUBTYPE_UNKNOWN = {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
#define KSDATAFORMAT_SUBTYPE_UNKNOWN VLC_KSDATAFORMAT_SUBTYPE_UNKNOWN
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

static const struct
{
    uint16_t     i_tag;
    vlc_fourcc_t i_fourcc;
    const char  *psz_name;
}
wave_format_tag_to_fourcc[] =
{
    { WAVE_FORMAT_PCM,        VLC_FOURCC( 'a', 'r', 'a', 'w' ), "Raw audio" },
    { WAVE_FORMAT_ADPCM,      VLC_FOURCC( 'm', 's', 0x00,0x02), "ADPCM" },
    { WAVE_FORMAT_IEEE_FLOAT, VLC_FOURCC( 'a', 'f', 'l', 't' ), "IEEE Float audio" },
    { WAVE_FORMAT_ALAW,       VLC_FOURCC( 'a', 'l', 'a', 'w' ), "A-Law" },
    { WAVE_FORMAT_MULAW,      VLC_FOURCC( 'm', 'l', 'a', 'w' ), "Mu-Law" },
    { WAVE_FORMAT_IMA_ADPCM,  VLC_FOURCC( 'm', 's', 0x00,0x11), "Ima-ADPCM" },
    { WAVE_FORMAT_G726,       VLC_FOURCC( 'g', '7', '2', '6' ), "G.726 ADPCM" },
    { WAVE_FORMAT_MPEGLAYER3, VLC_FOURCC( 'm', 'p', 'g', 'a' ), "Mpeg Audio" },
    { WAVE_FORMAT_MPEG,       VLC_FOURCC( 'm', 'p', 'g', 'a' ), "Mpeg Audio" },
    { WAVE_FORMAT_A52,        VLC_FOURCC( 'a', '5', '2', ' ' ), "A/52" },
    { WAVE_FORMAT_WMA1,       VLC_FOURCC( 'w', 'm', 'a', '1' ), "Window Media Audio v1" },
    { WAVE_FORMAT_WMA2,       VLC_FOURCC( 'w', 'm', 'a', '2' ), "Window Media Audio v2" },
    { WAVE_FORMAT_WMA2,       VLC_FOURCC( 'w', 'm', 'a', ' ' ), "Window Media Audio v2" },
    { WAVE_FORMAT_WMAP,       VLC_FOURCC( 'w', 'm', 'a', 'p' ), "Window Media Audio 9 Professional" },
    { WAVE_FORMAT_WMAL,       VLC_FOURCC( 'w', 'm', 'a', 'l' ), "Window Media Audio 9 Lossless" },
    { WAVE_FORMAT_WMAS,       VLC_FOURCC( 'w', 'm', 'a', 's' ), "Window Media Audio 9 Speech" },
    { WAVE_FORMAT_DK3,        VLC_FOURCC( 'm', 's', 0x00,0x61), "Duck DK3" },
    { WAVE_FORMAT_DK4,        VLC_FOURCC( 'm', 's', 0x00,0x62), "Duck DK4" },
    { WAVE_FORMAT_DTS,        VLC_FOURCC( 'd', 't', 's', ' ' ), "DTS Coherent Acoustics" },
    { WAVE_FORMAT_DTS_MS,     VLC_FOURCC( 'd', 't', 's', ' ' ), "DTS Coherent Acoustics" },
    { WAVE_FORMAT_DIVIO_AAC,  VLC_FOURCC( 'm', 'p', '4', 'a' ), "MPEG-4 Audio (Divio)" },
    { WAVE_FORMAT_AAC,        VLC_FOURCC( 'm', 'p', '4', 'a' ), "MPEG-4 Audio" },
    { WAVE_FORMAT_FFMPEG_AAC, VLC_FOURCC( 'm', 'p', '4', 'a' ), "MPEG-4 Audio" },
    { WAVE_FORMAT_VORBIS,     VLC_FOURCC( 'v', 'o', 'r', 'b' ), "Vorbis Audio" },
    { WAVE_FORMAT_VORB_1,     VLC_FOURCC( 'v', 'o', 'r', '1' ), "Vorbis 1 Audio" },
    { WAVE_FORMAT_VORB_1PLUS, VLC_FOURCC( 'v', 'o', '1', '+' ), "Vorbis 1+ Audio" },
    { WAVE_FORMAT_VORB_2,     VLC_FOURCC( 'v', 'o', 'r', '2' ), "Vorbis 2 Audio" },
    { WAVE_FORMAT_VORB_2PLUS, VLC_FOURCC( 'v', 'o', '2', '+' ), "Vorbis 2+ Audio" },
    { WAVE_FORMAT_VORB_3,     VLC_FOURCC( 'v', 'o', 'r', '3' ), "Vorbis 3 Audio" },
    { WAVE_FORMAT_VORB_3PLUS, VLC_FOURCC( 'v', 'o', '3', '+' ), "Vorbis 3+ Audio" },
    { WAVE_FORMAT_SPEEX,      VLC_FOURCC( 's', 'p', 'x', ' ' ), "Speex Audio" },
    { WAVE_FORMAT_UNKNOWN,    VLC_FOURCC( 'u', 'n', 'd', 'f' ), "Unknown" }
};

static inline void wf_tag_to_fourcc( uint16_t i_tag, vlc_fourcc_t *fcc,
                                     const char **ppsz_name )
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

/* If wFormatTag is WAVEFORMATEXTENSIBLE, we must look at the SubFormat tag
 * to determine the actual format.  Microsoft has stopped giving out wFormatTag
 * assignments in lieu of letting 3rd parties generate their own GUIDs
 */
static const struct
{
    GUID         guid_tag;
    vlc_fourcc_t i_fourcc;
    const char  *psz_name;
}
sub_format_tag_to_fourcc[] =
{
    { _KSDATAFORMAT_SUBTYPE_PCM_, VLC_FOURCC( 'p', 'c', 'm', ' ' ), "PCM" },
    { _KSDATAFORMAT_SUBTYPE_UNKNOWN_, VLC_FOURCC( 'u', 'n', 'd', 'f' ), "Unknown" }
};

/* compares two GUIDs, returns 1 if identical, 0 otherwise */
static inline int guidcmp( const GUID *s1, const GUID *s2 )
{
    return( s1->Data1 == s2->Data1 && s1->Data2 == s2->Data2 &&
            s1->Data3 == s2->Data3 && !memcmp( s1->Data4, s2->Data4, 8 ) );
}

static inline void sf_tag_to_fourcc( GUID *guid_tag,
                                     vlc_fourcc_t *fcc, const char **ppsz_name )
{
    int i;

    for( i = 0; !guidcmp( &sub_format_tag_to_fourcc[i].guid_tag,
                          &KSDATAFORMAT_SUBTYPE_UNKNOWN ); i++ )
    {
        if( guidcmp( &sub_format_tag_to_fourcc[i].guid_tag, guid_tag ) ) break;
    }
    if( fcc ) *fcc = sub_format_tag_to_fourcc[i].i_fourcc;
    if( ppsz_name ) *ppsz_name = sub_format_tag_to_fourcc[i].psz_name;
}

/**
 * Structure to hold information concerning subtitles.
 * Used between demuxers and decoders of subtitles.
 */
typedef struct es_sys_t
{
    char               *psz_header; /* for 'ssa ' and 'subt' */

    /* for spudec */
    unsigned int        i_orig_height;
    unsigned int        i_orig_width;
    unsigned int        i_origin_x;
    unsigned int        i_origin_y;
    unsigned int        i_scale_h;
    unsigned int        i_scale_v;
    unsigned int        i_alpha;
    bool          b_smooth;
    mtime_t             i_fade_in;
    mtime_t             i_fade_out;
    unsigned int        i_align;
    mtime_t             i_time_offset;
    bool          b_forced_subs;
    unsigned int        palette[16];
    unsigned int        colors[4];

} subtitle_data_t;

#endif /* "codecs.h" */
