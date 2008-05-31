/*****************************************************************************
 * filter.c : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_vout.h>

#ifndef _MSC_VER
    /* Work-around a bug in w32api-2.5 */
#   define QACONTAINERFLAGS QACONTAINERFLAGS_SOMETHINGELSE
#endif

#include "common.h"
#include "filter.h"

#define DEBUG_DSHOW 1

#define FILTER_NAME  L"VideoLAN Capture Filter"
#define PIN_NAME     L"Capture"

/*****************************************************************************
 * DirectShow GUIDs.
 * Easier to define them here as mingw doesn't provide them all.
 *****************************************************************************/
const GUID CLSID_SystemDeviceEnum = {0x62be5d10, 0x60eb, 0x11d0, {0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86}};
const GUID CLSID_VideoInputDeviceCategory = {0x860BB310,0x5D01,0x11d0,{0xBD,0x3B,0x00,0xA0,0xC9,0x11,0xCE,0x86}};
const GUID CLSID_AudioInputDeviceCategory = {0x33d9a762, 0x90c8, 0x11d0, {0xbd, 0x43, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86}};
//const GUID IID_IPropertyBag = {0x55272A00, 0x42CB, 0x11CE, {0x81, 0x35, 0x00, 0xAA, 0x00, 0x4B, 0xB8, 0x51}};
extern const GUID IID_IPropertyBag;
const GUID IID_ICreateDevEnum = {0x29840822, 0x5b84, 0x11d0, {0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86}};
const GUID IID_IFilterGraph = {0x56a8689f, 0x0ad4, 0x11ce, {0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID IID_IMediaControl = {0x56a868b1, 0x0ad4, 0x11ce, {0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID CLSID_FilterGraph = {0xe436ebb3, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};

//const GUID IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46}};
extern const GUID IID_IUnknown;
//const GUID IID_IPersist = {0x0000010c, 0x0000, 0x0000, {0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46}};
extern const GUID IID_IPersist;
const GUID IID_IMediaFilter = {0x56a86899, 0x0ad4, 0x11ce, {0xb0,0x3a, 0x00,0x20,0xaf,0x0b,0xa7,0x70}};
const GUID IID_IBaseFilter = {0x56a86895, 0x0ad4, 0x11ce, {0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID IID_IPin = {0x56a86891, 0x0ad4, 0x11ce, {0xb0,0x3a, 0x00,0x20,0xaf,0x0b,0xa7,0x70}};
const GUID IID_IMemInputPin = {0x56a8689d, 0x0ad4, 0x11ce, {0xb0,0x3a, 0x00,0x20,0xaf,0x0b,0xa7,0x70}};
extern const GUID IID_IMemInputPin;

const GUID IID_IEnumPins = {0x56a86892, 0x0ad4, 0x11ce, {0xb0,0x3a, 0x00,0x20,0xaf,0x0b,0xa7,0x70}};
const GUID IID_IEnumMediaTypes = {0x89c31040, 0x846b, 0x11ce, {0x97,0xd3, 0x00,0xaa,0x00,0x55,0x59,0x5a}};

const GUID IID_IAMBufferNegotiation = {0x56ed71a0, 0xaf5f, 0x11d0, {0xb3, 0xf0, 0x00, 0xaa, 0x00, 0x37, 0x61, 0xc5}};

//const GUID IID_ISpecifyPropertyPages = {0xb196b28b, 0xbab4, 0x101a, {0xb6, 0x9c, 0x00, 0xaa, 0x00, 0x34, 0x1d, 0x07}};
extern const GUID IID_ISpecifyPropertyPages;

const GUID IID_IQualityControl = {0x56a868a5, 0x0ad4, 0x11ce, {0xb, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};

const GUID CLSID_CaptureGraphBuilder2 = {0xBF87B6E1, 0x8C27, 0x11d0, {0xB3, 0xF0, 0x0, 0xAA, 0x00, 0x37, 0x61, 0xC5}};

const GUID IID_IGraphBuilder = {0x56a868a9, 0x0ad4, 0x11ce, {0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};

const GUID IID_ICaptureGraphBuilder2 = {0x93E5A4E0, 0x2D50, 0x11d2, {0xAB, 0xFA, 0x00, 0xA0, 0xC9, 0xC6, 0xE3, 0x8D}};

const GUID IID_IAMTVAudio = {0x83EC1C30, 0x23D1, 0x11d1, {0x99, 0xE6, 0x00, 0xA0, 0xC9, 0x56, 0x02, 0x66}};
const GUID IID_IAMStreamConfig = {0xC6E13340, 0x30AC, 0x11d0, {0xA1, 0x8C, 0x00, 0xA0, 0xC9, 0x11, 0x89, 0x56}};
const GUID IID_IAMCrossbar = {0xC6E13380, 0x30AC, 0x11d0, {0xA1, 0x8C, 0x00, 0xA0, 0xC9, 0x11, 0x89, 0x56}};
const GUID IID_IAMTVTuner = {0x211A8766, 0x03AC, 0x11d1, {0x8D, 0x13, 0x00, 0xAA, 0x00, 0xBD, 0x83, 0x39}};

const GUID IID_IKsPropertySet = {0x31EFAC30, 0x515C, 0x11d0, {0xA9, 0xAA, 0x00, 0xAA, 0x00, 0x61, 0xBE, 0x93}};

/* Video Format */

const GUID FORMAT_VideoInfo  = {0x05589f80, 0xc356, 0x11ce, {0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a}};
/*
 * MEDIATYPEs and MEDIASUBTYPEs
 */
const GUID MEDIATYPE_Video = {0x73646976, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIATYPE_Interleaved = {0x73766169, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIATYPE_Stream = {0xe436eb83, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_PREVIEW_VIDEO = {0x2859e1da, 0xb81f, 0x4fbd, {0x94, 0x3b, 0xe2, 0x37, 0x24, 0xa1, 0xab, 0xb3}};

/* Packed RGB formats */
const GUID MEDIASUBTYPE_RGB1 = {0xe436eb78, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB4 = {0xe436eb79, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB8 = {0xe436eb7a, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB565 = {0xe436eb7b, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB555 = {0xe436eb7c, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB24 = {0xe436eb7d, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_RGB32 = {0xe436eb7e, 0x524f, 0x11ce, {0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}};
const GUID MEDIASUBTYPE_ARGB32 = {0x773c9ac0, 0x3274, 0x11d0, {0xb7, 0x24, 0x0, 0xaa, 0x0, 0x6c, 0x1a, 0x1}};

/* Packed YUV formats */
const GUID MEDIASUBTYPE_YUYV = {0x56595559, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_Y411 = {0x31313459, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_Y211 = {0x31313259, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_YUY2 = {0x32595559, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_YVYU = {0x55595659, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_UYVY = {0x59565955, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

/* Planar YUV formats */
const GUID MEDIASUBTYPE_YVU9 = {0x39555659, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_YV12 = {0x32315659, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_IYUV = {0x56555949, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}}; /* identical to YV12 */
const GUID MEDIASUBTYPE_Y41P = {0x50313459, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_I420 = {0x30323449, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

const GUID MEDIATYPE_Audio = {0x73647561, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID FORMAT_WaveFormatEx = {0x05589f81, 0xc356, 0x11ce, {0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a}};
const GUID MEDIASUBTYPE_PCM = {0x00000001, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_IEEE_FLOAT = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

/* DV formats */
const GUID MEDIASUBTYPE_dvsd = {0x64737664, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_dvhd = {0x64687664, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
const GUID MEDIASUBTYPE_dvsl = {0x6c737664, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

/* MPEG2 formats */
const GUID MEDIASUBTYPE_MPEG2_VIDEO = {0xe06d8026, 0xdb46, 0x11cf, {0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea}};
const GUID MEDIASUBTYPE_MPEG2_PROGRAM = {0xe06d8022, 0xdb46, 0x11cf, {0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea}};
const GUID MEDIASUBTYPE_MPEG2_TRANSPORT = {0xe06d8023, 0xdb46, 0x11cf, {0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea}};
const GUID FORMAT_MPEG2Video = {0xe06d80e3, 0xdb46, 0x11cf, {0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea}};

/* MJPG format */
const GUID MEDIASUBTYPE_MJPG = {0x47504A4D, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

/* DivX formats */
const GUID MEDIASUBTYPE_DIVX = {0x58564944, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

/* Analog Video */
const GUID FORMAT_AnalogVideo = {0x482dde0, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};

const GUID MEDIATYPE_AnalogVideo = {0x482dde1, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xab, 0x0, 0x6e, 0xcb, 0x65}};

const GUID MEDIASUBTYPE_AnalogVideo_NTSC_M = {0x482dde2, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_PAL_B = {0x482dde5, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_PAL_D = {0x482dde6, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_PAL_G = {0x482dde7, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_PAL_H = {0x482dde8, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_PAL_I = {0x482dde9, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_PAL_M = {0x482ddea, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_PAL_N = {0x482ddeb, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_PAL_N_COMBO = {0x482ddec, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_SECAM_B = {0x482ddf0, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_SECAM_D = {0x482ddf1, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_SECAM_G = {0x482ddf2, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_SECAM_H = {0x482ddf3, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_SECAM_K = {0x482ddf4, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_SECAM_K1 = {0x482ddf5, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};
const GUID MEDIASUBTYPE_AnalogVideo_SECAM_L = {0x482ddf6, 0x7817, 0x11cf, {0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65}};

const GUID AMPROPSETID_Pin= {0x9b00f101, 0x1567, 0x11d1, {0xb3, 0xf1, 0x0, 0xaa, 0x0, 0x37, 0x61, 0xc5}};
const GUID PIN_CATEGORY_ANALOGVIDEOIN= {0xfb6c4283, 0x0353, 0x11d1, {0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba}};
const GUID PIN_CATEGORY_CAPTURE= {0xfb6c4281, 0x0353, 0x11d1, {0x90, 0x5f, 0x0, 0x0, 0xc0, 0xcc, 0x16, 0xba}};
const GUID LOOK_UPSTREAM_ONLY= {0xac798be0, 0x98e3, 0x11d1, {0xb3, 0xf1, 0x0, 0xaa, 0x0, 0x37, 0x61, 0xc}};

//const GUID GUID_NULL = {0x0000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
extern const GUID GUID_NULL;

void WINAPI FreeMediaType( AM_MEDIA_TYPE& mt )
{
    if( mt.cbFormat != 0 )
    {
        CoTaskMemFree( (PVOID)mt.pbFormat );
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if( mt.pUnk != NULL )
    {
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }
}

HRESULT WINAPI CopyMediaType( AM_MEDIA_TYPE *pmtTarget,
                              const AM_MEDIA_TYPE *pmtSource )
{
    *pmtTarget = *pmtSource;

    if( !pmtSource || !pmtTarget ) return S_FALSE;

    if( pmtSource->cbFormat && pmtSource->pbFormat )
    {
        pmtTarget->pbFormat = (PBYTE)CoTaskMemAlloc( pmtSource->cbFormat );
        if( pmtTarget->pbFormat == NULL )
        {
            pmtTarget->cbFormat = 0;
            return E_OUTOFMEMORY;
        }
        else
        {
            CopyMemory( (PVOID)pmtTarget->pbFormat, (PVOID)pmtSource->pbFormat,
                        pmtTarget->cbFormat );
        }
    }
    if( pmtTarget->pUnk != NULL )
    {
        pmtTarget->pUnk->AddRef();
    }

    return S_OK;
}

int GetFourCCFromMediaType( const AM_MEDIA_TYPE &media_type )
{
    int i_fourcc = 0;

    if( media_type.majortype == MEDIATYPE_Video )
    {
        /* currently only support this type of video info format */
        if( 1 /* media_type.formattype == FORMAT_VideoInfo */ )
        {
            /* Packed RGB formats */
            if( media_type.subtype == MEDIASUBTYPE_RGB1 )
               i_fourcc = VLC_FOURCC( 'R', 'G', 'B', '1' );
            else if( media_type.subtype == MEDIASUBTYPE_RGB4 )
               i_fourcc = VLC_FOURCC( 'R', 'G', 'B', '4' );
            else if( media_type.subtype == MEDIASUBTYPE_RGB8 )
               i_fourcc = VLC_FOURCC( 'R', 'G', 'B', '8' );
            else if( media_type.subtype == MEDIASUBTYPE_RGB555 )
               i_fourcc = VLC_FOURCC( 'R', 'V', '1', '5' );
            else if( media_type.subtype == MEDIASUBTYPE_RGB565 )
               i_fourcc = VLC_FOURCC( 'R', 'V', '1', '6' );
            else if( media_type.subtype == MEDIASUBTYPE_RGB24 )
               i_fourcc = VLC_FOURCC( 'R', 'V', '2', '4' );
            else if( media_type.subtype == MEDIASUBTYPE_RGB32 )
               i_fourcc = VLC_FOURCC( 'R', 'V', '3', '2' );
            else if( media_type.subtype == MEDIASUBTYPE_ARGB32 )
               i_fourcc = VLC_FOURCC( 'R', 'G', 'B', 'A' );

            /* Planar YUV formats */
            else if( media_type.subtype == MEDIASUBTYPE_I420 )
               i_fourcc = VLC_FOURCC( 'I', '4', '2', '0' );
            else if( media_type.subtype == MEDIASUBTYPE_Y41P )
               i_fourcc = VLC_FOURCC( 'I', '4', '1', '1' );
            else if( media_type.subtype == MEDIASUBTYPE_YV12 )
               i_fourcc = VLC_FOURCC( 'Y', 'V', '1', '2' );
            else if( media_type.subtype == MEDIASUBTYPE_IYUV )
               i_fourcc = VLC_FOURCC( 'Y', 'V', '1', '2' );
            else if( media_type.subtype == MEDIASUBTYPE_YVU9 )
               i_fourcc = VLC_FOURCC( 'Y', 'V', 'U', '9' );

            /* Packed YUV formats */
            else if( media_type.subtype == MEDIASUBTYPE_YVYU )
               i_fourcc = VLC_FOURCC( 'Y', 'V', 'Y', 'U' );
            else if( media_type.subtype == MEDIASUBTYPE_YUYV )
               i_fourcc = VLC_FOURCC( 'Y', 'U', 'Y', '2' );
            else if( media_type.subtype == MEDIASUBTYPE_Y411 )
               i_fourcc = VLC_FOURCC( 'I', '4', '1', 'N' );
            else if( media_type.subtype == MEDIASUBTYPE_Y211 )
               i_fourcc = VLC_FOURCC( 'Y', '2', '1', '1' );
            else if( media_type.subtype == MEDIASUBTYPE_YUY2 )
               i_fourcc = VLC_FOURCC( 'Y', 'U', 'Y', '2' );
            else if( media_type.subtype == MEDIASUBTYPE_UYVY )
               i_fourcc = VLC_FOURCC( 'U', 'Y', 'V', 'Y' );

            /* MPEG2 video elementary stream */
            else if( media_type.subtype == MEDIASUBTYPE_MPEG2_VIDEO )
               i_fourcc = VLC_FOURCC( 'm', 'p', '2', 'v' );

        /* DivX video */
            else if( media_type.subtype == MEDIASUBTYPE_DIVX )
               i_fourcc = VLC_FOURCC( 'D', 'I', 'V', 'X' );

            /* DV formats */
            else if( media_type.subtype == MEDIASUBTYPE_dvsl )
               i_fourcc = VLC_FOURCC( 'd', 'v', 's', 'l' );
            else if( media_type.subtype == MEDIASUBTYPE_dvsd )
               i_fourcc = VLC_FOURCC( 'd', 'v', 's', 'd' );
            else if( media_type.subtype == MEDIASUBTYPE_dvhd )
               i_fourcc = VLC_FOURCC( 'd', 'v', 'h', 'd' );

            /* MJPEG format */
            else if( media_type.subtype == MEDIASUBTYPE_MJPG )
                i_fourcc = VLC_FOURCC( 'M', 'J', 'P', 'G' );

        }
    }
    else if( media_type.majortype == MEDIATYPE_Audio )
    {
        /* currently only support this type of audio info format */
        if( media_type.formattype == FORMAT_WaveFormatEx )
        {
            if( media_type.subtype == MEDIASUBTYPE_PCM )
                i_fourcc = VLC_FOURCC( 'a', 'r', 'a', 'w' );
            else if( media_type.subtype == MEDIASUBTYPE_IEEE_FLOAT )
                i_fourcc = VLC_FOURCC( 'f', 'l', '3', '2' );
        }
    }
    else if( media_type.majortype == MEDIATYPE_Stream )
    {
        if( media_type.subtype == MEDIASUBTYPE_MPEG2_PROGRAM )
            i_fourcc = VLC_FOURCC( 'm', 'p', '2', 'p' );
        else if( media_type.subtype == MEDIASUBTYPE_MPEG2_TRANSPORT )
            i_fourcc = VLC_FOURCC( 'm', 'p', '2', 't' );
    }

    return i_fourcc;
}

/****************************************************************************
 * Implementation of our dummy directshow filter pin class
 ****************************************************************************/

CapturePin::CapturePin( vlc_object_t *_p_input, access_sys_t *_p_sys,
                        CaptureFilter *_p_filter,
                        AM_MEDIA_TYPE *mt, size_t mt_count )
  : p_input( _p_input ), p_sys( _p_sys ), p_filter( _p_filter ),
    p_connected_pin( NULL ),  media_types(mt), media_type_count(mt_count),
    i_ref( 1 )
{
    cx_media_type.majortype = mt[0].majortype;
    cx_media_type.subtype   = GUID_NULL;
    cx_media_type.pbFormat  = NULL;
    cx_media_type.cbFormat  = 0;
    cx_media_type.pUnk      = NULL;
}

CapturePin::~CapturePin()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::~CapturePin" );
#endif
    for( size_t c=0; c<media_type_count; c++ )
    {
        FreeMediaType(media_types[c]);
    }
    FreeMediaType(cx_media_type);
}

HRESULT CapturePin::CustomGetSample( VLCMediaSample *vlc_sample )
{
#if 0 //def DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::CustomGetSample" );
#endif

    vlc_mutex_lock( &p_sys->lock );
    if( samples_queue.size() )
    {
        *vlc_sample = samples_queue.back();
        samples_queue.pop_back();
        vlc_mutex_unlock( &p_sys->lock );
        return S_OK;
    }
    vlc_mutex_unlock( &p_sys->lock );
    return S_FALSE;
}

AM_MEDIA_TYPE &CapturePin::CustomGetMediaType()
{
    return cx_media_type;
}

/* IUnknown methods */
STDMETHODIMP CapturePin::QueryInterface(REFIID riid, void **ppv)
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CapturePin::QueryInterface" );
#endif

    if( riid == IID_IUnknown ||
        riid == IID_IPin )
    {
        AddRef();
        *ppv = (IPin *)this;
        return NOERROR;
    }
    if( riid == IID_IMemInputPin )
    {
        AddRef();
        *ppv = (IMemInputPin *)this;
        return NOERROR;
    }
    else
    {
#ifdef DEBUG_DSHOW_L1
        msg_Dbg( p_input, "CapturePin::QueryInterface() failed for: "
                 "%04X-%02X-%02X-%02X%02X%02X%02X%02X%02X%02X%02X",
                 (int)riid.Data1, (int)riid.Data2, (int)riid.Data3,
                 static_cast<int>(riid.Data4[0]), (int)riid.Data4[1],
                 (int)riid.Data4[2], (int)riid.Data4[3],
                 (int)riid.Data4[4], (int)riid.Data4[5],
                 (int)riid.Data4[6], (int)riid.Data4[7] );
#endif
        *ppv = NULL;
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG) CapturePin::AddRef()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CapturePin::AddRef (ref: %i)", i_ref );
#endif

    return i_ref++;
};
STDMETHODIMP_(ULONG) CapturePin::Release()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CapturePin::Release (ref: %i)", i_ref );
#endif

    if( !InterlockedDecrement(&i_ref) ) delete this;

    return 0;
};

/* IPin methods */
STDMETHODIMP CapturePin::Connect( IPin * pReceivePin,
                                  const AM_MEDIA_TYPE *pmt )
{
    if( State_Running == p_filter->state )
    {
        msg_Dbg( p_input, "CapturePin::Connect [not stopped]" );
        return VFW_E_NOT_STOPPED;
    }

    if( p_connected_pin )
    {
        msg_Dbg( p_input, "CapturePin::Connect [already connected]" );
        return VFW_E_ALREADY_CONNECTED;
    }

    if( !pmt ) return S_OK;
 
    if( GUID_NULL != pmt->majortype &&
        media_types[0].majortype != pmt->majortype )
    {
        msg_Dbg( p_input, "CapturePin::Connect [media major type mismatch]" );
        return S_FALSE;
    }

    if( GUID_NULL != pmt->subtype && !GetFourCCFromMediaType(*pmt) )
    {
        msg_Dbg( p_input, "CapturePin::Connect [media subtype type "
                 "not supported]" );
        return S_FALSE;
    }

    if( pmt->pbFormat && pmt->majortype == MEDIATYPE_Video  )
    {
        if( !((VIDEOINFOHEADER *)pmt->pbFormat)->bmiHeader.biHeight ||
            !((VIDEOINFOHEADER *)pmt->pbFormat)->bmiHeader.biWidth )
        {
            msg_Dbg( p_input, "CapturePin::Connect "
                     "[video width/height == 0 ]" );
            return S_FALSE;
        }
    }

    msg_Dbg( p_input, "CapturePin::Connect [OK]" );
    return S_OK;
}
STDMETHODIMP CapturePin::ReceiveConnection( IPin * pConnector,
                                            const AM_MEDIA_TYPE *pmt )
{
    if( State_Stopped != p_filter->state )
    {
        msg_Dbg( p_input, "CapturePin::ReceiveConnection [not stopped]" );
        return VFW_E_NOT_STOPPED;
    }

    if( !pConnector || !pmt )
    {
        msg_Dbg( p_input, "CapturePin::ReceiveConnection [null pointer]" );
        return E_POINTER;
    }

    if( p_connected_pin )
    {
        msg_Dbg( p_input, "CapturePin::ReceiveConnection [already connected]");
        return VFW_E_ALREADY_CONNECTED;
    }

    if( S_OK != QueryAccept(pmt) )
    {
        msg_Dbg( p_input, "CapturePin::ReceiveConnection "
                 "[media type not accepted]" );
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    msg_Dbg( p_input, "CapturePin::ReceiveConnection [OK]" );

    p_connected_pin = pConnector;
    p_connected_pin->AddRef();

    FreeMediaType( cx_media_type );
    return CopyMediaType( &cx_media_type, pmt );
}
STDMETHODIMP CapturePin::Disconnect()
{
    if( ! p_connected_pin )
    {
        msg_Dbg( p_input, "CapturePin::Disconnect [not connected]" );
        return S_FALSE;
    }

    msg_Dbg( p_input, "CapturePin::Disconnect [OK]" );

    /* samples_queue was already flushed in EndFlush() */

    p_connected_pin->Release();
    p_connected_pin = NULL;
    //FreeMediaType( cx_media_type );
    //cx_media_type.subtype = GUID_NULL;

    return S_OK;
}
STDMETHODIMP CapturePin::ConnectedTo( IPin **pPin )
{
    if( !p_connected_pin )
    {
        msg_Dbg( p_input, "CapturePin::ConnectedTo [not connected]" );
        return VFW_E_NOT_CONNECTED;
    }

    p_connected_pin->AddRef();
    *pPin = p_connected_pin;

    msg_Dbg( p_input, "CapturePin::ConnectedTo [OK]" );

    return S_OK;
}
STDMETHODIMP CapturePin::ConnectionMediaType( AM_MEDIA_TYPE *pmt )
{
    if( !p_connected_pin )
    {
        msg_Dbg( p_input, "CapturePin::ConnectionMediaType [not connected]" );
        return VFW_E_NOT_CONNECTED;
    }

    return CopyMediaType( pmt, &cx_media_type );
}
STDMETHODIMP CapturePin::QueryPinInfo( PIN_INFO * pInfo )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::QueryPinInfo" );
#endif

    pInfo->pFilter = p_filter;
    if( p_filter ) p_filter->AddRef();

    memcpy(pInfo->achName, PIN_NAME, sizeof(PIN_NAME));
    pInfo->dir = PINDIR_INPUT;

    return NOERROR;
}
STDMETHODIMP CapturePin::QueryDirection( PIN_DIRECTION * pPinDir )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::QueryDirection" );
#endif

    *pPinDir = PINDIR_INPUT;
    return NOERROR;
}
STDMETHODIMP CapturePin::QueryId( LPWSTR * Id )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::QueryId" );
#endif

    *Id = L"VideoLAN Capture Pin";

    return S_OK;
}
STDMETHODIMP CapturePin::QueryAccept( const AM_MEDIA_TYPE *pmt )
{
    if( State_Stopped != p_filter->state )
    {
        msg_Dbg( p_input, "CapturePin::QueryAccept [not stopped]" );
        return S_FALSE;
    }

    if( media_types[0].majortype != pmt->majortype )
    {
        msg_Dbg( p_input, "CapturePin::QueryAccept [media type mismatch]" );
        return S_FALSE;
    }

    int i_fourcc = GetFourCCFromMediaType(*pmt);
    if( !i_fourcc )
    {
        msg_Dbg( p_input, "CapturePin::QueryAccept "
                 "[media type not supported]" );
        return S_FALSE;
    }

    if( pmt->majortype == MEDIATYPE_Video )
    {
        if( pmt->pbFormat &&
            ( (((VIDEOINFOHEADER *)pmt->pbFormat)->bmiHeader.biHeight == 0) ||
              (((VIDEOINFOHEADER *)pmt->pbFormat)->bmiHeader.biWidth == 0) ) )
        {
            msg_Dbg( p_input, "CapturePin::QueryAccept [video size wxh == 0]");
            return S_FALSE;
        }

        msg_Dbg( p_input, "CapturePin::QueryAccept [OK] "
                 "(width=%ld, height=%ld, chroma=%4.4s, fps=%f)",
                 ((VIDEOINFOHEADER *)pmt->pbFormat)->bmiHeader.biWidth,
                 ((VIDEOINFOHEADER *)pmt->pbFormat)->bmiHeader.biHeight,
                 (char *)&i_fourcc,
         10000000.0f/((float)((VIDEOINFOHEADER *)pmt->pbFormat)->AvgTimePerFrame) );
    }
    else if( pmt->majortype == MEDIATYPE_Audio )
    {
        msg_Dbg( p_input, "CapturePin::QueryAccept [OK] (channels=%d, "
                 "samples/sec=%lu, bits/samples=%d, format=%4.4s)",
                 ((WAVEFORMATEX *)pmt->pbFormat)->nChannels,
                 ((WAVEFORMATEX *)pmt->pbFormat)->nSamplesPerSec,
                 ((WAVEFORMATEX *)pmt->pbFormat)->wBitsPerSample,
                 (char *)&i_fourcc );
    }
    else
    {
        msg_Dbg( p_input, "CapturePin::QueryAccept [OK] (stream format=%4.4s)",
                 (char *)&i_fourcc );
    }

    if( p_connected_pin )
    {
        FreeMediaType( cx_media_type );
        CopyMediaType( &cx_media_type, pmt );
    }

    return S_OK;
}
STDMETHODIMP CapturePin::EnumMediaTypes( IEnumMediaTypes **ppEnum )
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CapturePin::EnumMediaTypes" );
#endif

    *ppEnum = new CaptureEnumMediaTypes( p_input, this, NULL );

    if( *ppEnum == NULL ) return E_OUTOFMEMORY;

    return NOERROR;
}
STDMETHODIMP CapturePin::QueryInternalConnections( IPin* *apPin, ULONG *nPin )
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CapturePin::QueryInternalConnections" );
#endif
    return E_NOTIMPL;
}
STDMETHODIMP CapturePin::EndOfStream( void )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::EndOfStream" );
#endif
    return S_OK;
}
STDMETHODIMP CapturePin::BeginFlush( void )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::BeginFlush" );
#endif
    return S_OK;
}
STDMETHODIMP CapturePin::EndFlush( void )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::EndFlush" );
#endif

    VLCMediaSample vlc_sample;

    vlc_mutex_lock( &p_sys->lock );
    while( samples_queue.size() )
    {
        vlc_sample = samples_queue.back();
        samples_queue.pop_back();
        vlc_sample.p_sample->Release();
    }
    vlc_mutex_unlock( &p_sys->lock );

    return S_OK;
}
STDMETHODIMP CapturePin::NewSegment( REFERENCE_TIME tStart,
                                     REFERENCE_TIME tStop,
                                     double dRate )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::NewSegment" );
#endif
    return S_OK;
}

/* IMemInputPin methods */
STDMETHODIMP CapturePin::GetAllocator( IMemAllocator **ppAllocator )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::GetAllocator" );
#endif

    return VFW_E_NO_ALLOCATOR;
}
STDMETHODIMP CapturePin::NotifyAllocator( IMemAllocator *pAllocator,
                                          BOOL bReadOnly )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::NotifyAllocator" );
#endif

    return S_OK;
}
STDMETHODIMP CapturePin::GetAllocatorRequirements( ALLOCATOR_PROPERTIES *pProps )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::GetAllocatorRequirements" );
#endif

    return E_NOTIMPL;
}
STDMETHODIMP CapturePin::Receive( IMediaSample *pSample )
{
#if 0 //def DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::Receive" );
#endif

    pSample->AddRef();
    mtime_t i_timestamp = mdate() * 10;
    VLCMediaSample vlc_sample = {pSample, i_timestamp};

    vlc_mutex_lock( &p_sys->lock );
    samples_queue.push_front( vlc_sample );

    /* Make sure we don't cache too many samples */
    if( samples_queue.size() > 10 )
    {
        vlc_sample = samples_queue.back();
        samples_queue.pop_back();
        msg_Dbg( p_input, "CapturePin::Receive trashing late input sample" );
        vlc_sample.p_sample->Release();
    }

    vlc_cond_signal( &p_sys->wait );
    vlc_mutex_unlock( &p_sys->lock );

    return S_OK;
}
STDMETHODIMP CapturePin::ReceiveMultiple( IMediaSample **pSamples,
                                          long nSamples,
                                          long *nSamplesProcessed )
{
    HRESULT hr = S_OK;

    *nSamplesProcessed = 0;
    while( nSamples-- > 0 )
    {
         hr = Receive( pSamples[*nSamplesProcessed] );
         if( hr != S_OK ) break;
         (*nSamplesProcessed)++;
    }
    return hr;
}
STDMETHODIMP CapturePin::ReceiveCanBlock( void )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CapturePin::ReceiveCanBlock" );
#endif

    return S_FALSE; /* Thou shalt not block */
}

/****************************************************************************
 * Implementation of our dummy directshow filter class
 ****************************************************************************/
CaptureFilter::CaptureFilter( vlc_object_t *_p_input, access_sys_t *p_sys,
                              AM_MEDIA_TYPE *mt, size_t mt_count )
  : p_input( _p_input ),
    p_pin( new CapturePin( _p_input, p_sys, this, mt, mt_count ) ),
    state( State_Stopped ), i_ref( 1 )
{
}

CaptureFilter::~CaptureFilter()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::~CaptureFilter" );
#endif
    p_pin->Release();
}

/* IUnknown methods */
STDMETHODIMP CaptureFilter::QueryInterface( REFIID riid, void **ppv )
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureFilter::QueryInterface" );
#endif

    if( riid == IID_IUnknown )
    {
        AddRef();
        *ppv = (IUnknown *)this;
        return NOERROR;
    }
    if( riid == IID_IPersist )
    {
        AddRef();
        *ppv = (IPersist *)this;
        return NOERROR;
    }
    if( riid == IID_IMediaFilter )
    {
        AddRef();
        *ppv = (IMediaFilter *)this;
        return NOERROR;
    }
    if( riid == IID_IBaseFilter )
    {
        AddRef();
        *ppv = (IBaseFilter *)this;
        return NOERROR;
    }
    else
    {
#ifdef DEBUG_DSHOW_L1
        msg_Dbg( p_input, "CaptureFilter::QueryInterface() failed for: "
                 "%04X-%02X-%02X-%02X%02X%02X%02X%02X%02X%02X%02X",
                 (int)riid.Data1, (int)riid.Data2, (int)riid.Data3,
                 static_cast<int>(riid.Data4[0]), (int)riid.Data4[1],
                 (int)riid.Data4[2], (int)riid.Data4[3],
                 (int)riid.Data4[4], (int)riid.Data4[5],
                 (int)riid.Data4[6], (int)riid.Data4[7] );
#endif
        *ppv = NULL;
        return E_NOINTERFACE;
    }
};
STDMETHODIMP_(ULONG) CaptureFilter::AddRef()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureFilter::AddRef (ref: %i)", i_ref );
#endif

    return i_ref++;
};
STDMETHODIMP_(ULONG) CaptureFilter::Release()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureFilter::Release (ref: %i)", i_ref );
#endif

    if( !InterlockedDecrement(&i_ref) ) delete this;

    return 0;
};

/* IPersist method */
STDMETHODIMP CaptureFilter::GetClassID(CLSID *pClsID)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::GetClassID" );
#endif
    return E_NOTIMPL;
};

/* IMediaFilter methods */
STDMETHODIMP CaptureFilter::GetState(DWORD dwMSecs, FILTER_STATE *State)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::GetState %i", state );
#endif

    *State = state;
    return S_OK;
};
STDMETHODIMP CaptureFilter::SetSyncSource(IReferenceClock *pClock)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::SetSyncSource" );
#endif

    return S_OK;
};
STDMETHODIMP CaptureFilter::GetSyncSource(IReferenceClock **pClock)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::GetSyncSource" );
#endif

    *pClock = NULL;
    return NOERROR;
};
STDMETHODIMP CaptureFilter::Stop()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::Stop" );
#endif

    p_pin->EndFlush();

    state = State_Stopped;
    return S_OK;
};
STDMETHODIMP CaptureFilter::Pause()
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::Pause" );
#endif

    state = State_Paused;
    return S_OK;
};
STDMETHODIMP CaptureFilter::Run(REFERENCE_TIME tStart)
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::Run" );
#endif

    state = State_Running;
    return S_OK;
};

/* IBaseFilter methods */
STDMETHODIMP CaptureFilter::EnumPins( IEnumPins ** ppEnum )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::EnumPins" );
#endif

    /* Create a new ref counted enumerator */
    *ppEnum = new CaptureEnumPins( p_input, this, NULL );
    return *ppEnum == NULL ? E_OUTOFMEMORY : NOERROR;
};
STDMETHODIMP CaptureFilter::FindPin( LPCWSTR Id, IPin ** ppPin )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::FindPin" );
#endif
    return E_NOTIMPL;
};
STDMETHODIMP CaptureFilter::QueryFilterInfo( FILTER_INFO * pInfo )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::QueryFilterInfo" );
#endif

    memcpy(pInfo->achName, FILTER_NAME, sizeof(FILTER_NAME));

    pInfo->pGraph = p_graph;
    if( p_graph ) p_graph->AddRef();

    return NOERROR;
};
STDMETHODIMP CaptureFilter::JoinFilterGraph( IFilterGraph * pGraph,
                                             LPCWSTR pName )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::JoinFilterGraph" );
#endif

    p_graph = pGraph;

    return NOERROR;
};
STDMETHODIMP CaptureFilter::QueryVendorInfo( LPWSTR* pVendorInfo )
{
#ifdef DEBUG_DSHOW
    msg_Dbg( p_input, "CaptureFilter::QueryVendorInfo" );
#endif
    return E_NOTIMPL;
};

/* Custom methods */
CapturePin *CaptureFilter::CustomGetPin()
{
    return p_pin;
}

/****************************************************************************
 * Implementation of our dummy directshow enumpins class
 ****************************************************************************/

CaptureEnumPins::CaptureEnumPins( vlc_object_t *_p_input,
                                  CaptureFilter *_p_filter,
                                  CaptureEnumPins *pEnumPins )
  : p_input( _p_input ), p_filter( _p_filter ), i_ref( 1 )
{
    /* Hold a reference count on our filter */
    p_filter->AddRef();

    /* Are we creating a new enumerator */

    if( pEnumPins == NULL )
    {
        i_position = 0;
    }
    else
    {
        i_position = pEnumPins->i_position;
    }
}

CaptureEnumPins::~CaptureEnumPins()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumPins::~CaptureEnumPins" );
#endif
    p_filter->Release();
}

/* IUnknown methods */
STDMETHODIMP CaptureEnumPins::QueryInterface( REFIID riid, void **ppv )
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumPins::QueryInterface" );
#endif

    if( riid == IID_IUnknown ||
        riid == IID_IEnumPins )
    {
        AddRef();
        *ppv = (IEnumPins *)this;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
};
STDMETHODIMP_(ULONG) CaptureEnumPins::AddRef()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumPins::AddRef (ref: %i)", i_ref );
#endif

    return i_ref++;
};
STDMETHODIMP_(ULONG) CaptureEnumPins::Release()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumPins::Release (ref: %i)", i_ref );
#endif

    if( !InterlockedDecrement(&i_ref) ) delete this;

    return 0;
};

/* IEnumPins */
STDMETHODIMP CaptureEnumPins::Next( ULONG cPins, IPin ** ppPins,
                                    ULONG * pcFetched )
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumPins::Next" );
#endif

    unsigned int i_fetched = 0;

    if( i_position < 1 && cPins > 0 )
    {
        IPin *pPin = p_filter->CustomGetPin();
        *ppPins = pPin;
        pPin->AddRef();
        i_fetched = 1;
        i_position++;
    }

    if( pcFetched ) *pcFetched = i_fetched;

    return (i_fetched == cPins) ? S_OK : S_FALSE;
};
STDMETHODIMP CaptureEnumPins::Skip( ULONG cPins )
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumPins::Skip" );
#endif

    i_position += cPins;

    if( i_position > 1 )
    {
        return S_FALSE;
    }

    return S_OK;
};
STDMETHODIMP CaptureEnumPins::Reset()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumPins::Reset" );
#endif

    i_position = 0;
    return S_OK;
};
STDMETHODIMP CaptureEnumPins::Clone( IEnumPins **ppEnum )
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumPins::Clone" );
#endif

    *ppEnum = new CaptureEnumPins( p_input, p_filter, this );
    if( *ppEnum == NULL ) return E_OUTOFMEMORY;

    return NOERROR;
};

/****************************************************************************
 * Implementation of our dummy directshow enummediatypes class
 ****************************************************************************/
CaptureEnumMediaTypes::CaptureEnumMediaTypes( vlc_object_t *_p_input,
    CapturePin *_p_pin, CaptureEnumMediaTypes *pEnumMediaTypes )
  : p_input( _p_input ), p_pin( _p_pin ), i_ref( 1 )
{
    /* Hold a reference count on our filter */
    p_pin->AddRef();

    /* Are we creating a new enumerator */
    if( pEnumMediaTypes == NULL )
    {
        CopyMediaType(&cx_media_type, &p_pin->cx_media_type);
        i_position = 0;
    }
    else
    {
        CopyMediaType(&cx_media_type, &pEnumMediaTypes->cx_media_type);
        i_position = pEnumMediaTypes->i_position;
    }
}

CaptureEnumMediaTypes::~CaptureEnumMediaTypes()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumMediaTypes::~CaptureEnumMediaTypes" );
#endif
    FreeMediaType(cx_media_type);
    p_pin->Release();
}

/* IUnknown methods */
STDMETHODIMP CaptureEnumMediaTypes::QueryInterface( REFIID riid, void **ppv )
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumMediaTypes::QueryInterface" );
#endif

    if( riid == IID_IUnknown ||
        riid == IID_IEnumMediaTypes )
    {
        AddRef();
        *ppv = (IEnumMediaTypes *)this;
        return NOERROR;
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
};
STDMETHODIMP_(ULONG) CaptureEnumMediaTypes::AddRef()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumMediaTypes::AddRef (ref: %i)", i_ref );
#endif

    return i_ref++;
};
STDMETHODIMP_(ULONG) CaptureEnumMediaTypes::Release()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumMediaTypes::Release (ref: %i)", i_ref );
#endif

    if( !InterlockedDecrement(&i_ref) ) delete this;

    return 0;
};

/* IEnumMediaTypes */
STDMETHODIMP CaptureEnumMediaTypes::Next( ULONG cMediaTypes,
                                          AM_MEDIA_TYPE ** ppMediaTypes,
                                          ULONG * pcFetched )
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumMediaTypes::Next " );
#endif
    ULONG copied = 0;
    ULONG offset = 0;
    ULONG max = p_pin->media_type_count;

    if( ! ppMediaTypes )
        return E_POINTER;

    if( (! pcFetched)  && (cMediaTypes > 1) )
       return E_POINTER;

    /*
    ** use connection media type as first entry in iterator if it exists
    */
    copied = 0;
    if( cx_media_type.subtype != GUID_NULL )
    {
        ++max;
        if( i_position == 0 )
        {
            ppMediaTypes[copied] =
                (AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
            if( CopyMediaType(ppMediaTypes[copied], &cx_media_type) != S_OK )
                return E_OUTOFMEMORY;
            ++i_position;
            ++copied;
        }
    }

    while( (copied < cMediaTypes) && (i_position < max)  )
    {
        ppMediaTypes[copied] =
            (AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE));
        if( CopyMediaType( ppMediaTypes[copied],
                           &p_pin->media_types[i_position-offset]) != S_OK )
            return E_OUTOFMEMORY;

        ++copied;
        ++i_position;
    }

    if( pcFetched )  *pcFetched = copied;

    return (copied == cMediaTypes) ? S_OK : S_FALSE;
};
STDMETHODIMP CaptureEnumMediaTypes::Skip( ULONG cMediaTypes )
{
    ULONG max =  p_pin->media_type_count;
    if( cx_media_type.subtype != GUID_NULL )
    {
        max = 1;
    }
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumMediaTypes::Skip" );
#endif

    i_position += cMediaTypes;
    return (i_position < max) ? S_OK : S_FALSE;
};
STDMETHODIMP CaptureEnumMediaTypes::Reset()
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumMediaTypes::Reset" );
#endif

    FreeMediaType(cx_media_type);
    CopyMediaType(&cx_media_type, &p_pin->cx_media_type);
    i_position = 0;
    return S_OK;
};
STDMETHODIMP CaptureEnumMediaTypes::Clone( IEnumMediaTypes **ppEnum )
{
#ifdef DEBUG_DSHOW_L1
    msg_Dbg( p_input, "CaptureEnumMediaTypes::Clone" );
#endif

    *ppEnum = new CaptureEnumMediaTypes( p_input, p_pin, this );
    if( *ppEnum == NULL ) return E_OUTOFMEMORY;

    return NOERROR;
};
