/*****************************************************************************
 * common.h : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2004, 2010 the VideoLAN team
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
#include <string>
#include <list>
#include <deque>
using namespace std;

#ifndef _MSC_VER
#   include <wtypes.h>
#   include <unknwn.h>
#   include <ole2.h>
#   include <limits.h>
#   ifdef _WINGDI_
#      undef _WINGDI_
#   endif
#   define _WINGDI_ 1
#   define AM_NOVTABLE
#   define _OBJBASE_H_
#   undef _X86_
#   ifndef _I64_MAX
#     define _I64_MAX LONG_LONG_MAX
#   endif
#   define LONGLONG long long
#endif

#include <dshow.h>

typedef struct dshow_stream_t dshow_stream_t;

/****************************************************************************
 * Crossbar stuff
 ****************************************************************************/
#define MAX_CROSSBAR_DEPTH 10

typedef struct CrossbarRouteRec
{
    IAMCrossbar *pXbar;
    LONG        VideoInputIndex;
    LONG        VideoOutputIndex;
    LONG        AudioInputIndex;
    LONG        AudioOutputIndex;

} CrossbarRoute;

void DeleteCrossbarRoutes( access_sys_t * );
HRESULT FindCrossbarRoutes( vlc_object_t *, access_sys_t *,
                            IPin *, LONG, int = 0 );

/****************************************************************************
 * Access descriptor declaration
 ****************************************************************************/
struct access_sys_t
{
    /* These 2 must be left at the beginning */
    vlc_mutex_t lock;
    vlc_cond_t  wait;

    IFilterGraph           *p_graph;
    ICaptureGraphBuilder2  *p_capture_graph_builder2;
    IMediaControl          *p_control;

    int                     i_crossbar_route_depth;
    CrossbarRoute           crossbar_routes[MAX_CROSSBAR_DEPTH];

    /* list of elementary streams */
    dshow_stream_t **pp_streams;
    int            i_streams;
    int            i_current_stream;

    /* misc properties */
    int            i_width;
    int            i_height;
    int            i_chroma;
    bool           b_chroma; /* Force a specific chroma on the dshow input */
};


#define INSTANCEDATA_OF_PROPERTY_PTR(x) ((PKSPROPERTY((x))) + 1)
#define INSTANCEDATA_OF_PROPERTY_SIZE(x) (sizeof((x)) - sizeof(KSPROPERTY))

/*****************************************************************************
 * DirectShow GUIDs.
 *****************************************************************************/
const GUID PROPSETID_TUNER = {0x6a2e0605, 0x28e4, 0x11d0, {0xa1, 0x8c, 0x00, 0xa0, 0xc9, 0x11, 0x89, 0x56}};

/****************************************************************************
 * The following should be in ks.h and ksmedia.h, but since they are not in
 * the current version of Mingw, we will be defined here.
 ****************************************************************************/

/* http://msdn.microsoft.com/en-us/library/ff567297%28VS.85%29.aspx */
typedef enum  {
        KS_AnalogVideo_None          = 0x00000000,
        KS_AnalogVideo_NTSC_M        = 0x00000001,
        KS_AnalogVideo_NTSC_M_J      = 0x00000002,
        KS_AnalogVideo_NTSC_433      = 0x00000004,
        KS_AnalogVideo_PAL_B         = 0x00000010,
        KS_AnalogVideo_PAL_D         = 0x00000020,
        KS_AnalogVideo_PAL_G         = 0x00000040,
        KS_AnalogVideo_PAL_H         = 0x00000080,
        KS_AnalogVideo_PAL_I         = 0x00000100,
        KS_AnalogVideo_PAL_M         = 0x00000200,
        KS_AnalogVideo_PAL_N         = 0x00000400,
        KS_AnalogVideo_PAL_60        = 0x00000800,
        KS_AnalogVideo_SECAM_B       = 0x00001000,
        KS_AnalogVideo_SECAM_D       = 0x00002000,
        KS_AnalogVideo_SECAM_G       = 0x00004000,
        KS_AnalogVideo_SECAM_H       = 0x00008000,
        KS_AnalogVideo_SECAM_K       = 0x00010000,
        KS_AnalogVideo_SECAM_K1      = 0x00020000,
        KS_AnalogVideo_SECAM_L       = 0x00040000,
        KS_AnalogVideo_SECAM_L1      = 0x00080000,
        KS_AnalogVideo_PAL_N_COMBO   = 0x00100000
} KS_AnalogVideoStandard;

/* http://msdn.microsoft.com/en-us/library/ff567800%28VS.85%29.aspx and following */
typedef enum {
    KSPROPERTY_TUNER_CAPS,              // R  -overall device capabilities
    KSPROPERTY_TUNER_MODE_CAPS,         // R  -capabilities in this mode
    KSPROPERTY_TUNER_MODE,              // RW -set a mode (TV, FM, AM, DSS)
    KSPROPERTY_TUNER_STANDARD,          // R  -get TV standard (only if TV mode)
    KSPROPERTY_TUNER_FREQUENCY,         // RW -set/get frequency
    KSPROPERTY_TUNER_INPUT,             // RW -select an input
    KSPROPERTY_TUNER_STATUS,            // R  -tuning status

    /* Optional */
    KSPROPERTY_TUNER_IF_MEDIUM,         // R O-Medium for IF or Transport Pin

    /* Mandatory for Vista and + */
    KSPROPERTY_TUNER_SCAN_CAPS,         // R  -overall device capabilities for scanning

    /* Optional ones */
    KSPROPERTY_TUNER_SCAN_STATUS,       // R  -status of scan
    KSPROPERTY_TUNER_STANDARD_MODE,     // RW -autodetect mode for signal standard
    KSPROPERTY_TUNER_NETWORKTYPE_SCAN_CAPS // R -network type specific tuner capabilities
} KSPROPERTY_TUNER;

/* http://msdn.microsoft.com/en-us/library/ff567689%28v=VS.85%29.aspx */
typedef enum {
    KS_TUNER_TUNING_EXACT = 1,        // Tunes directly to the right freq
    KS_TUNER_TUNING_FINE,             // Comprehensive search to the right freq
    KS_TUNER_TUNING_COARSE,           // Fast search
}KS_TUNER_TUNING_FLAGS;

/* http://msdn.microsoft.com/en-us/library/ff567687%28v=VS.85%29.aspx */
typedef enum {
    KS_TUNER_STRATEGY_PLL             = 0X01, // Phase locked loop (PLL) offset tuning
    KS_TUNER_STRATEGY_SIGNAL_STRENGTH = 0X02, // Signal strength tuning
    KS_TUNER_STRATEGY_DRIVER_TUNES    = 0X04, // Driver tunes
}KS_TUNER_STRATEGY;

/* http://msdn.microsoft.com/en-us/library/ff562676%28VS.85%29.aspx */
typedef struct {
    union {
        struct {
            GUID    Set;
            ULONG   Id;
            ULONG   Flags;
        };
        LONGLONG    Alignment;
    };
} KSIDENTIFIER, *PKSIDENTIFIER;

typedef KSIDENTIFIER KSPROPERTY, *PKSPROPERTY;

/* http://msdn.microsoft.com/en-us/library/ff565872%28v=VS.85%29.aspx */
typedef struct {
    KSPROPERTY Property;
    ULONG  Mode;                        // KSPROPERTY_TUNER_MODE_*
    ULONG  StandardsSupported;          // KS_AnalogVideo_* (if Mode is TV or DSS)
    ULONG  MinFrequency;                // Hz
    ULONG  MaxFrequency;                // Hz
    ULONG  TuningGranularity;           // Hz
    ULONG  NumberOfInputs;              // number of inputs
    ULONG  SettlingTime;                // milliSeconds
    ULONG  Strategy;                    // KS_TUNER_STRATEGY
} KSPROPERTY_TUNER_MODE_CAPS_S, *PKSPROPERTY_TUNER_MODE_CAPS_S;

/* http://msdn.microsoft.com/en-us/library/ff565839%28v=VS.85%29.aspx */
typedef struct {
    KSPROPERTY Property;
    ULONG  Frequency;                   // Hz
    ULONG  LastFrequency;               // Hz (last tuned)
    ULONG  TuningFlags;                 // KS_TUNER_TUNING_FLAGS
    ULONG  VideoSubChannel;             // DSS
    ULONG  AudioSubChannel;             // DSS
    ULONG  Channel;                     // VBI decoders
    ULONG  Country;                     // VBI decoders
} KSPROPERTY_TUNER_FREQUENCY_S, *PKSPROPERTY_TUNER_FREQUENCY_S;

/* http://msdn.microsoft.com/en-us/library/ff565918%28v=VS.85%29.aspx */
typedef struct {
    KSPROPERTY Property;
    ULONG  Standard;                    // KS_AnalogVideo_*
} KSPROPERTY_TUNER_STANDARD_S, *PKSPROPERTY_TUNER_STANDARD_S;

