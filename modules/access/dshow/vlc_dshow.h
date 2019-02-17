/*****************************************************************************
 * vlc_dshow.h : DirectShow access module for vlc
 *****************************************************************************
 * Copyright (C) 2002, 2004, 2010-2011 VLC authors and VideoLAN
 *
 * Author: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifndef VLC_DSHOW_H
#define VLC_DSHOW_H

#ifdef __MINGW32__
# include <_mingw.h>
#endif

#include <wtypes.h>
#include <unknwn.h>
#include <ole2.h>
#include <limits.h>
#include <strmif.h>
#include <ksmedia.h>
#include <wmcodecdsp.h>
#include <ddraw.h>

#ifndef __MINGW64_VERSION_MAJOR

#include <dshow.h>

namespace dshow {

/*****************************************************************************
 * DirectShow GUIDs.
 *****************************************************************************/

/* IAM */
DEFINE_GUID(IID_IAMBufferNegotiation ,0x56ed71a0, 0xaf5f, 0x11d0, 0xb3, 0xf0, 0x00, 0xaa, 0x00, 0x37, 0x61, 0xc5);
DEFINE_GUID(IID_IAMTVAudio      ,0x83EC1C30, 0x23D1, 0x11d1, 0x99, 0xE6, 0x00, 0xA0, 0xC9, 0x56, 0x02, 0x66);
DEFINE_GUID(IID_IAMCrossbar     ,0xC6E13380, 0x30AC, 0x11d0, 0xA1, 0x8C, 0x00, 0xA0, 0xC9, 0x11, 0x89, 0x56);
DEFINE_GUID(IID_IAMTVTuner      ,0x211A8766, 0x03AC, 0x11d1, 0x8D, 0x13, 0x00, 0xAA, 0x00, 0xBD, 0x83, 0x39);

/* Video Format
MEDIASUBTYPEs and FORMAT */
DEFINE_GUID(MEDIASUBTYPE_ARGB32 ,0x773c9ac0, 0x3274, 0x11d0, 0xb7, 0x24, 0x0, 0xaa, 0x0, 0x6c, 0x1a, 0x1);
/* Packed YUV formats */
DEFINE_GUID(MEDIASUBTYPE_YUYV ,0x56595559, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
/* Planar YUV formats */
DEFINE_GUID(MEDIASUBTYPE_IYUV ,0x56555949, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71); /* identical to YV12 */
DEFINE_GUID(MEDIASUBTYPE_I420 ,0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
/* MPEG2 formats */
DEFINE_GUID(MEDIASUBTYPE_MPEG2_VIDEO     ,0xe06d8026, 0xdb46, 0x11cf, 0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea);
DEFINE_GUID(MEDIASUBTYPE_MPEG2_PROGRAM   ,0xe06d8022, 0xdb46, 0x11cf, 0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea);
DEFINE_GUID(MEDIASUBTYPE_MPEG2_TRANSPORT ,0xe06d8023, 0xdb46, 0x11cf, 0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea);
DEFINE_GUID(FORMAT_MPEG2Video            ,0xe06d80e3, 0xdb46, 0x11cf, 0xb4, 0xd1, 0x00, 0x80, 0x5f, 0x6c, 0xbb, 0xea);

/* float */
DEFINE_GUID(MEDIASUBTYPE_IEEE_FLOAT ,0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);


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
/* End of ks */


/* Define missing interfaces from wine's header */
/* http://msdn.microsoft.com/en-us/library/dd373441%28v=vs.85%29.aspx */
typedef enum tagAMTunerModeType {
    AMTUNER_MODE_DEFAULT    = 0x0000,
    AMTUNER_MODE_TV         = 0x0001,
    AMTUNER_MODE_FM_RADIO   = 0x0002,
    AMTUNER_MODE_AM_RADIO   = 0x0004,
    AMTUNER_MODE_DSS        = 0x0008
} AMTunerModeType;

#define AMPROPERTY_PIN_CATEGORY 0
typedef enum tagAMTunerSubChannel {
    AMTUNER_SUBCHAN_NO_TUNE = -2,
    AMTUNER_SUBCHAN_DEFAULT = -1
} AMTunerSubChannel;

/* http://msdn.microsoft.com/en-us/library/dd407232%28v=vs.85%29.aspx */
typedef enum tagTunerInputType {
    TunerInputCable = 0,
    TunerInputAntenna = TunerInputCable + 1
} TunerInputType;

typedef enum tagAMTunerEventType {
    AMTUNER_EVENT_CHANGED = 0x1
} AMTunerEventType;

/* http://msdn.microsoft.com/en-us/library/dd377421%28v=vs.85%29.aspx */
typedef enum tagPhysicalConnectorType {
    PhysConn_Video_Tuner             = 1,
    PhysConn_Video_Composite,
    PhysConn_Video_SVideo,
    PhysConn_Video_RGB,
    PhysConn_Video_YRYBY,
    PhysConn_Video_SerialDigital,
    PhysConn_Video_ParallelDigital,
    PhysConn_Video_SCSI,
    PhysConn_Video_AUX,
    PhysConn_Video_1394,
    PhysConn_Video_USB,
    PhysConn_Video_VideoDecoder,
    PhysConn_Video_VideoEncoder,
    PhysConn_Video_SCART,
    PhysConn_Video_Black,
    PhysConn_Audio_Tuner             = 4096, /* 0x1000 */
    PhysConn_Audio_Line,
    PhysConn_Audio_Mic,
    PhysConn_Audio_AESDigital,
    PhysConn_Audio_SPDIFDigital,
    PhysConn_Audio_SCSI,
    PhysConn_Audio_AUX,
    PhysConn_Audio_1394,
    PhysConn_Audio_USB,
    PhysConn_Audio_AudioDecoder
} PhysicalConnectorType;

/* http://msdn.microsoft.com/en-us/library/dd407352%28v=vs.85%29.aspx */
typedef struct _VIDEO_STREAM_CONFIG_CAPS {
    GUID     guid;
    ULONG    VideoStandard;
    SIZE     InputSize;
    SIZE     MinCroppingSize;
    SIZE     MaxCroppingSize;
    int      CropGranularityX;
    int      CropGranularityY;
    int      CropAlignX;
    int      CropAlignY;
    SIZE     MinOutputSize;
    SIZE     MaxOutputSize;
    int      OutputGranularityX;
    int      OutputGranularityY;
    int      StretchTapsX;
    int      StretchTapsY;
    int      ShrinkTapsX;
    int      ShrinkTapsY;
    LONGLONG MinFrameInterval;
    LONGLONG MaxFrameInterval;
    LONG     MinBitsPerSecond;
    LONG     MaxBitsPerSecond;
} VIDEO_STREAM_CONFIG_CAPS;

/* http://msdn.microsoft.com/en-us/library/dd317597%28v=vs.85%29.aspx */
typedef struct _AUDIO_STREAM_CONFIG_CAPS {
    GUID  guid;
    ULONG MinimumChannels;
    ULONG MaximumChannels;
    ULONG ChannelsGranularity;
    ULONG MinimumBitsPerSample;
    ULONG MaximumBitsPerSample;
    ULONG BitsPerSampleGranularity;
    ULONG MinimumSampleFrequency;
    ULONG MaximumSampleFrequency;
    ULONG SampleFrequencyGranularity;
} AUDIO_STREAM_CONFIG_CAPS;

/* http://msdn.microsoft.com/en-us/library/dd389142%28v=vs.85%29.aspx */
#undef INTERFACE
#define INTERFACE IAMBufferNegotiation
DECLARE_INTERFACE_(IAMBufferNegotiation, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID, PVOID*) PURE;
    STDMETHOD_(ULONG,AddRef )(THIS);
    STDMETHOD_(ULONG,Release )(THIS);
    STDMETHOD(SuggestAllocatorProperties )(THIS_ const ALLOCATOR_PROPERTIES *);
    STDMETHOD(GetAllocatorProperties )(THIS_ ALLOCATOR_PROPERTIES *);
};

/* http://msdn.microsoft.com/en-us/library/dd389171%28v=vs.85%29.aspx */
#undef INTERFACE
#define INTERFACE IAMCrossbar
DECLARE_INTERFACE_(IAMCrossbar, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID, PVOID*) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS);
    STDMETHOD_(ULONG, Release) (THIS);
    STDMETHOD(get_PinCounts) (THIS_ long *, long *);
    STDMETHOD(CanRoute) (THIS_ long, long);
    STDMETHOD(Route) (THIS_ long, long);
    STDMETHOD(get_IsRoutedTo) (THIS_ long, long *);
    STDMETHOD(get_CrossbarPinInfo) (THIS_ BOOL, long, long *, long *);
};

/* http://msdn.microsoft.com/en-us/library/dd375945%28v=vs.85%29.aspx */
#undef INTERFACE
#define INTERFACE IAMTunerNotification
DECLARE_INTERFACE_( IAMTunerNotification, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID, PVOID*) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS);
    STDMETHOD_(ULONG, Release) (THIS);
    STDMETHOD(OnEvent) (THIS_ AMTunerEventType);
};

/* http://msdn.microsoft.com/en-us/library/dd375971%28v=vs.85%29.aspx */
#undef INTERFACE
#define INTERFACE IAMTVTuner
DECLARE_INTERFACE_(IAMTVTuner, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID, PVOID*) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS);
    STDMETHOD_(ULONG, Release) (THIS);
    STDMETHOD(put_Channel) (THIS_ long, long, long);
    STDMETHOD(get_Channel) (THIS_ long *, long *, long *);
    STDMETHOD(ChannelMinMax) (THIS_ long *, long *);
    STDMETHOD(put_CountryCode) (THIS_ long);
    STDMETHOD(get_CountryCode) (THIS_ long *);
    STDMETHOD(put_TuningSpace) (THIS_ long);
    STDMETHOD(get_TuningSpace) (THIS_ long *);
    STDMETHOD(Logon) (THIS_ HANDLE);
    STDMETHOD(Logout) (IAMTVTuner *);
    STDMETHOD(SignalPresen) (THIS_ long *);
    STDMETHOD(put_Mode) (THIS_ AMTunerModeType);
    STDMETHOD(get_Mode) (THIS_ AMTunerModeType *);
    STDMETHOD(GetAvailableModes) (THIS_ long *);
    STDMETHOD(RegisterNotificationCallBack) (THIS_ IAMTunerNotification *, long);
    STDMETHOD(UnRegisterNotificationCallBack) (THIS_ IAMTunerNotification *);
    STDMETHOD(get_AvailableTVFormats) (THIS_ long *);
    STDMETHOD(get_TVFormat) (THIS_ long *);
    STDMETHOD(AutoTune) (THIS_ long, long *);
    STDMETHOD(StoreAutoTune) (IAMTVTuner *);
    STDMETHOD(get_NumInputConnections) (THIS_ long *);
    STDMETHOD(put_InputType) (THIS_ long, TunerInputType);
    STDMETHOD(get_InputType) (THIS_ long, TunerInputType *);
    STDMETHOD(put_ConnectInput) (THIS_ long);
    STDMETHOD(get_ConnectInput) (THIS_ long *);
    STDMETHOD(get_VideoFrequency) (THIS_ long *);
    STDMETHOD(get_AudioFrequency) (THIS_ long *);
};

/* http://msdn.microsoft.com/en-us/library/dd375962%28v=vs.85%29.aspx */
#undef INTERFACE
#define INTERFACE IAMTVAudio
DECLARE_INTERFACE_(IAMTVAudio, IUnknown)
{
    STDMETHOD(QueryInterface) (THIS_ REFIID, PVOID*) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS);
    STDMETHOD_(ULONG, Release) (THIS);
    STDMETHOD(GetHardwareSupportedTVAudioModes) (THIS_ long *);
    STDMETHOD(GetAvailableTVAudioModes) (THIS_ long *);
    STDMETHOD(get_TVAudioMode) (THIS_ long *);
    STDMETHOD(put_TVAudioMode) (THIS_ long);
    STDMETHOD(RegisterNotificationCallBack) (THIS_ IAMTunerNotification*, long);
    STDMETHOD(UnRegisterNotificationCallBack) (THIS_ IAMTunerNotification*);
};

} // namespace

#endif /* __MINGW64_VERSION_MAJOR */
#endif /* VLC_DSHOW_H */
