/*****************************************************************************
 * codecs.h: codec related structures needed by the demuxers and decoders
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: codecs.h,v 1.3 2002/12/03 17:00:15 fenrir Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_
typedef struct __attribute__((__packed__)) _WAVEFORMATEX {
    uint16_t   wFormatTag;
    uint16_t   nChannels;
    uint32_t   nSamplesPerSec;
    uint32_t   nAvgBytesPerSec;
    uint16_t   nBlockAlign;
    uint16_t   wBitsPerSample;
    uint16_t   cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *NPWAVEFORMATEX, *LPWAVEFORMATEX;
#endif /* _WAVEFORMATEX_ */

#if !defined(_BITMAPINFOHEADER_) && !defined(WIN32)
#define _BITMAPINFOHEADER_
typedef struct __attribute__((__packed__))
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

/* WAVE form wFormatTag IDs */

#define  WAVE_FORMAT_UNKNOWN            0x0000 /* Microsoft Corporation */
#define  WAVE_FORMAT_PCM                0x0001 /* Microsoft Corporation */
#define  WAVE_FORMAT_ADPCM              0x0002 /* Microsoft Corporation */
#define  WAVE_FORMAT_IEEE_FLOAT         0x0003 /* Microsoft Corporation */
#define  WAVE_FORMAT_ALAW               0x0006 /* Microsoft Corporation */
#define  WAVE_FORMAT_MULAW              0x0007 /* Microsoft Corporation */
#define  WAVE_FORMAT_DTS                0x0008 /* Microsoft Corporation */
#define  WAVE_FORMAT_IMA_ADPCM          0x0011
#define  WAVE_FORMAT_MPEG               0x0050 /* Microsoft Corporation */
#define  WAVE_FORMAT_MPEGLAYER3         0x0055 /* ISO/MPEG Layer3 Format Tag */
#define  WAVE_FORMAT_DOLBY_AC3_SPDIF    0x0092 /* Sonic Foundry */

/* Need to check these */
#define WAVE_FORMAT_A52             0x2000
#define WAVE_FORMAT_WMA1            0x0160
#define WAVE_FORMAT_WMA2            0x0161
#define WAVE_FORMAT_WMA3            0x0162

#if !defined(WAVE_FORMAT_EXTENSIBLE)
#define  WAVE_FORMAT_EXTENSIBLE                 0xFFFE /* Microsoft */
#endif

#endif /* "codecs.h" */
