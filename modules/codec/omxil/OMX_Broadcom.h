/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// OpenMAX IL - Broadcom specific types

#ifndef OMX_Broadcom_h
#define OMX_Broadcom_h

#include "OMX_Component.h"

// for use in buffer headers - marks the contained data
// as being a codec header
#define OMX_BUFFERFLAG_TIME_UNKNOWN 0x00000100

//for use in buffer headers - marks the buffer as being the
//snapshot preview image from a still capture.
//Mainly to be used with the DisplayFunction callback from camera.
#define OMX_BUFFERFLAG_CAPTURE_PREVIEW 0x00000200

/* Mark the end of a NAL unit produced by a video encoder.
 */
#define OMX_BUFFERFLAG_ENDOFNAL    0x00000400

/* Marks pBuffer in OMX_BUFFERHEADERTYPE as containing a fragment list instead of the actual buffer
 */
#define OMX_BUFFERFLAG_FRAGMENTLIST 0x00000800

/* Marks the start of a new sequence of data following any kind of seek operation.
 */
#define OMX_BUFFERFLAG_DISCONTINUITY 0x00001000

/** Codec side information Flag:
* OMX_BUFFERFLAG_CODECSIDEINFO is an optional flag that is set by an
* output port when all bytes in the buffer form part or all of a set of
* codec specific side information. For example, distortion information
* estimated by H.264 encoder can be sent using this flag to signal
* the decoder quality
*/
#define OMX_BUFFERFLAG_CODECSIDEINFO 0x00002000

/**
 * Macros to convert to <code>OMX_TICKS</code> from a signed 64 bit value and
 * vice-versa. These macros don't actually do anything unless <code>OMX_TICKS</code>
 * is defined as a two-part structure (instead of a native signed 64-bit type).
 **/
#ifndef OMX_SKIP64BIT
   #define omx_ticks_from_s64(s) (s)
   #define omx_ticks_to_s64(t) (t)
#else
   static inline OMX_TICKS omx_ticks_from_s64(signed long long s) { OMX_TICKS t; t.nLowPart = (OMX_U32)s; t.nHighPart = (OMX_U32)(s>>32); return t; }
   #define omx_ticks_to_s64(t) ((t).nLowPart | ((uint64_t)((t).nHighPart) << 32))
#endif /* OMX_SKIP64BIT */

/* Buffer fragment descriptor */
typedef struct OMX_BUFFERFRAGMENTTYPE {
   OMX_PTR pBuffer; /**< Pointer to actual block of memory that is acting as the fragment buffer */
   OMX_U32 nLen;    /**< number of bytes in the buffer */
} OMX_BUFFERFRAGMENTTYPE;

/* OMX_IndexParamBrcmEnableIJGTableScaling: JPEG Quality Table Setting. */
typedef struct OMX_PARAM_IJGSCALINGTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BOOL bEnabled;
} OMX_PARAM_IJGSCALINGTYPE;
/*
The boolean \code{bEnabled} value determines whether the component uses
the standard IJG quality tables when encoding images.
*/


/* OMX_IndexConfigTimeInvalidStartTime: Invalid Start Times */
/*
This allows clock clients to supply a start time notification to the
clock whilst indicating that this time is invalid.
*/

/* OMX_IndexParamBrcmMaxFrameSkips: Frame timestamp jumps */
/*
This number represents the number of times a jump in frame timestamps
has been observed that is greater than expected.
*/

/* OMX_IndexConfigAsynchronousFailureURI: Asynchronous Failure Filename */
/*
This allows the client to query for the filename that cause an asynchronous
output error.
*/

/* OMX_IndexParamAsynchronousOutput: Asynchronous Output */
/*
The allows the client to specify to a component that is writing files
that this writing may happen asynchronously, including opening and closing
of files.
*/

/* OMX_IndexConfigClockAdjustment: Clock Adjustment */
/*
This allows the client to read from the clock the total time
adjustment made to the clock whilst running by the reference clock.
If the reference clock sends a time that causes the media time to jump
this difference is added to the total, which can be reported via this
index.  When the stream restarts by setting the clock state to
\code{OMX_TIME_ClockStateRunning} or
\code{OMX_TIME_ClockStateWaitingForStartTime} this adjustment total is
set to zero.
*/

/* OMX_IndexParamBrcmDataUnit: Data Unit */
/*
The data unit is an indication to components connected to this
component of the type of data delivery available.
\code{OMX_DataUnitCodedPicture} indicates that we are able to give
framing information, using the \code{OMX_BUFFERFLAG_ENDOFFRAME} flag to
indicate that the data contained finishes a complete
frame. \code{OMX_DataUnitArbitraryStreamSection} indicates that no
end-of-frame markers will be present, and the decoder should perform
the steps necessary to decode the stream. The other enum values are
not used.
*/

/* OMX_IndexConfigPresentationOffset: Presentation Offset */
/*
The value of \code{nTimestamp} is added to the offset requested for
each new input frame. Takes effect for all new input frames, and has
no effect on the offset used for currently-queued frames. A positive
value will make the requested port earlier relative to other streams,
a negative value will make the requested port later relative to other
streams.
*/

/* OMX_IndexConfigSingleStep: Single Step */
/*
When setting this config on a paused clock, where the \code{nU32}
value is non-zero and \code{nPortIndex} is OMX_ALL, the media clock
will advance through the next \code{nU32} next requested media
times. A paused clock is in running state but has a time scale of
0. This will trigger the display of some video frames, so allowing
single-stepping functionality. This config can be set multiple times,
and will buffer up stepping requests until we have media requests to
fulfil, or the clock is stopped or un-paused.

This config can also be used on some video output ports and, if
\code{nU32} is non-zero, requests that the output port forwards the
next \code{nU32} frames appending an EOS marker on the last frame, and
then ceases to forward data on this port.  If \code{nU32} is zero, any
previous request to forward a limited number of frames is cancelled
and the default behaviour of this port will resume.
*/

/* OMX_IndexParamCameraCamplusId: Camera Subsystem Identification */
/*
This parameter allows the configuration of the identifier to be used
to initialise the Broadcom Camplus subsystem that sits beneath the
camera component. If only one instance of the camera component is
used, the default value can be used. If more than one instance is
required, they must each have their own unique values for this
parameter. It is also used to tie the component to the image pool
created with \code{OMX_Set upCamPools}.
*/

/* OMX_IndexConfigAudioRenderingLatency: Audio Rendering Latency */
/*
This config allows the client to query the current latency of audio
rendering.  The latency is returned as the number of samples that
an audio rendering component has received but have not been played.
*/

/* OMX_IndexConfigBrcmPoolMemAllocSize: Pool memory usage values */
/*
This config allows the client to query how much memory is being used by
the component for any image pools. 
*/

/* OMX_IndexConfigDisplayRegion: Display Region */
typedef enum OMX_DISPLAYTRANSFORMTYPE{
   OMX_DISPLAY_ROT0 = 0,
   OMX_DISPLAY_MIRROR_ROT0 = 1,
   OMX_DISPLAY_MIRROR_ROT180 = 2,
   OMX_DISPLAY_ROT180 = 3,
   OMX_DISPLAY_MIRROR_ROT90 = 4,
   OMX_DISPLAY_ROT270 = 5,
   OMX_DISPLAY_ROT90 = 6,
   OMX_DISPLAY_MIRROR_ROT270 = 7,
   OMX_DISPLAY_DUMMY = 0x7FFFFFFF
} OMX_DISPLAYTRANSFORMTYPE;

typedef struct OMX_DISPLAYRECTTYPE {
   OMX_S16 x_offset;
   OMX_S16 y_offset;
   OMX_S16 width;
   OMX_S16 height;
} OMX_DISPLAYRECTTYPE;

typedef enum OMX_DISPLAYMODETYPE {
   OMX_DISPLAY_MODE_FILL = 0,
   OMX_DISPLAY_MODE_LETTERBOX = 1,
   OMX_DISPLAY_MODE_DUMMY = 0x7FFFFFFF
} OMX_DISPLAYMODETYPE;

typedef enum OMX_DISPLAYSETTYPE {
   OMX_DISPLAY_SET_NONE = 0,
   OMX_DISPLAY_SET_NUM = 1,
   OMX_DISPLAY_SET_FULLSCREEN = 2,
   OMX_DISPLAY_SET_TRANSFORM = 4,
   OMX_DISPLAY_SET_DEST_RECT = 8,
   OMX_DISPLAY_SET_SRC_RECT = 0x10,
   OMX_DISPLAY_SET_MODE = 0x20,
   OMX_DISPLAY_SET_PIXEL = 0x40,
   OMX_DISPLAY_SET_NOASPECT = 0x80,
   OMX_DISPLAY_SET_LAYER = 0x100,
   OMX_DISPLAY_SET_COPYPROTECT = 0x200,
   OMX_DISPLAY_SET_ALPHA = 0x400,
   OMX_DISPLAY_SET_DUMMY = 0x7FFFFFFF
} OMX_DISPLAYSETTYPE;

typedef struct OMX_CONFIG_DISPLAYREGIONTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_DISPLAYSETTYPE set;
   OMX_U32 num;
   OMX_BOOL fullscreen;
   OMX_DISPLAYTRANSFORMTYPE transform;
   OMX_DISPLAYRECTTYPE dest_rect;
   OMX_DISPLAYRECTTYPE src_rect;
   OMX_BOOL noaspect;
   OMX_DISPLAYMODETYPE mode;
   OMX_U32 pixel_x;
   OMX_U32 pixel_y;
   OMX_S32 layer;
   OMX_BOOL copyprotect_required;
   OMX_U32 alpha;
   OMX_U32 wfc_context_width;
   OMX_U32 wfc_context_height;
} OMX_CONFIG_DISPLAYREGIONTYPE;
/*
This config sets the output display device, as well as the region used
on the output display, any display transformation, and some flags to
indicate how to scale the image.

The structure uses a bitfield, \code{set}, to indicate which fields are set
and should be used. All other fields will maintain their current
value.

\code{num} describes the display output device, with 0 typically being
a directly connected LCD display.

\code{fullscreen} indicates that we are using the full device screen
area, rather than a window of the display.  If fullscreen is false,
then dest_rect is used to specify a region of the display to use.

\code{transform} indicates any rotation or flipping used to map frames
onto the natural display orientation.

The \code{src_rect} indicates which area of the frame to display. If
all values are zero, the whole frame will be used.

The \code{noaspect} flag, if set, indicates that any display scaling
should disregard the aspect ratio of the frame region being displayed.

\code{mode} indicates how the image should be scaled to fit the
display. \code{OMX_DISPLAY_MODE_FILL} indicates that the image should
fill the screen by potentially cropping the frames.  Setting
\code{mode} to \code{OMX_DISPLAY_MODE_LETTERBOX} indicates that all
the source region should be displayed and black bars added if
necessary.

The \code{pixel_x} and \code{pixel_y} values, if non-zero, are used to
describe the size of a source pixel. If values are zero, then pixels
default to being square.

Set the \code{layer} that the image will appear on with the
\code{layer} field.
*/



/* OMX_IndexParamSource: Source Image Configuration */
typedef enum OMX_SOURCETYPE {
   OMX_SOURCE_WHITE = 0,    // all white images
   OMX_SOURCE_BLACK = 1,    // all black images
   OMX_SOURCE_DIAGONAL = 2, // greyscale diagonal stripes
   OMX_SOURCE_NOISE = 3,    // random pixel values
   OMX_SOURCE_RANDOM = 4,   // a shaded random pattern of colours
   OMX_SOURCE_COLOUR = 5,   // a solid colour determined by nParam
   OMX_SOURCE_BLOCKS = 6,   // random coloured blocks of 16x16 size
   OMX_SOURCE_SWIRLY,       // a swirly pattern used for encode testing
   OMX_SOURCE_DUMMY = 0x7FFFFFFF
} OMX_SOURCETYPE;

typedef struct OMX_PARAM_SOURCETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_SOURCETYPE eType;
   OMX_U32 nParam;
   OMX_U32 nFrameCount;
   OMX_U32 xFrameRate;
} OMX_PARAM_SOURCETYPE;
/*
The source type determines the kind of image that is produced. Not all
combinations of source type and image type are supported.  The
\code{OMX_SOURCE_SWIRLY} setting can only be used with YUV420 packed
planar image formats.  When producing RGB565 image format, the
\code{OMX_SOURCE_DIAGONAL} and \code{OMX_SOURCE_RANDOM} modes are
treated as \code{OMX_SOURCE_NOISE}.

The \code{nParam} field is used to specify the colour for the source
colour mode, and the offset of the diagonal pattern for diagonal mode.
For the blocks mode, \code{nParam} is used as the seed for the random
colour generator.

The \code{nFrameCount} parameter determines how many frames to send.
If it is zero, then frames are sent continuously. For any other value,
it counts down until it has sent that many frames, and then stops,
sending out an EOS. The \code{xFrameRate} setting is used to determine
the timestamp for each frame produced, or can be set to zero if
timestamps should all remain at zero.
*/

/* OMX_IndexParamSourceSeed: Source Random Seed */
typedef struct OMX_PARAM_SOURCESEEDTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U16 nData[16];
} OMX_PARAM_SOURCESEEDTYPE;
/*
This structure sets the current state of the random number generator
used for \code{OMX_SOURCE_RANDOM} source type, allowing repeatable
random image creation.
*/

/* OMX_IndexParamResize: Resize Control */
typedef enum OMX_RESIZEMODETYPE {
   OMX_RESIZE_NONE,
   OMX_RESIZE_CROP,
   OMX_RESIZE_BOX,
   OMX_RESIZE_BYTES,
   OMX_RESIZE_DUMMY = 0x7FFFFFFF
} OMX_RESIZEMODETYPE;

typedef struct OMX_PARAM_RESIZETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_RESIZEMODETYPE eMode;
   OMX_U32 nMaxWidth;
   OMX_U32 nMaxHeight;
   OMX_U32 nMaxBytes;
   OMX_BOOL bPreserveAspectRatio;
   OMX_BOOL bAllowUpscaling;
} OMX_PARAM_RESIZETYPE;
/*
The mode determines the kind of resize. \code{OMX_RESIZE_BOX} allow
the \code{nMaxWidth} and \code{nMaxHeight} to set a bounding box into
which the output must fit. \code{OMX_RESIZE_BYTES} allows
\code{nMaxBytes} to set the maximum number of bytes into which the
full output frame must fit.  Two flags aid the setting of the output
size. \code{bPreseveAspectRatio} sets whether the resize should
preserve the aspect ratio of the incoming
image. \code{bAllowUpscaling} sets whether the resize is allowed to
increase the size of the output image compared to the size of the
input image.
*/

typedef struct OMX_PARAM_TESTINTERFACETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_BOOL bTest;
   OMX_BOOL bSetExtra;
   OMX_U32 nExtra;
   OMX_BOOL bSetError;
   OMX_BOOL stateError[2];
} OMX_PARAM_TESTINTERFACETYPE;

/* OMX_IndexConfigVisualisation: Visualisation Mode */
typedef struct OMX_CONFIG_VISUALISATIONTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U8 name[16];
   OMX_U8 property[64];
} OMX_CONFIG_VISUALISATIONTYPE;

/*
\code{name} is a string of characters specifying the type of
visualization. The component appends \code{"_vis.vll"} to the name
provided, and attempts to load a visualisation library contained in
this VLL.  \code{property} contains configuration parameters and
values, which is interpreted by the visualisation library. Typically
all visualisations will accept a property string containing
\code{'mode=<number>'}, where \code{<number>} may be a random 32 bit
integer in decimal format. If provided, this may select a random mode
from that visualisation library.
*/

/*
This parameter is used when creating proprietary communication with
the display component, and provides the display function for passing
images to be displayed, together with a function used to flush all
pending image updates and release all images.
*/

/* OMX_IndexConfigBrcmAudioDestination: Audio Destination */
typedef struct OMX_CONFIG_BRCMAUDIODESTINATIONTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U8 sName[16];
} OMX_CONFIG_BRCMAUDIODESTINATIONTYPE;
/*
This config sets the platform-specific audio destination or output
device for audio sink components (e.g. audio_render).

\code{sName} describes the audio destination, with \code{"local"}
typically being directly connected to headphones.
*/

/* OMX_IndexConfigBrcmAudioSource: Audio Source */
typedef struct OMX_CONFIG_BRCMAUDIOSOURCETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U8 sName[16];
} OMX_CONFIG_BRCMAUDIOSOURCETYPE;
/*
This config sets the platform-specific audio source or input device
for audio source components (e.g. audio_capture).

\code{sName} describes the audio source, with \code{"local"}
typically being directly connected to microphone.
*/

/* OMX_IndexConfigBrcmAudioDownmixCoefficients: Audio Downmix Coefficients */
typedef struct OMX_CONFIG_BRCMAUDIODOWNMIXCOEFFICIENTS {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 coeff[16];
} OMX_CONFIG_BRCMAUDIODOWNMIXCOEFFICIENTS;
/*
This config sets the platform-specific audio downmixing coefficients for the 
audio mixer component. The coefficients are 16.16 fixed point.
The even coefficients contribute to the left channel. 
The odd coefficients contribute to the right channel. 
L' = coeff[0] * sample[N] + coeff[2] * sample[N+1] + coeff[4] * sample[N+2] + coeff[6] * sample[N+3] 
   + coeff[8] * sample[N+4] + coeff[10] * sample[N+5] + coeff[12] * sample[N+6] + coeff[14] * sample[N+7]
R' = coeff[1] * sample[N] + coeff[3] * sample[N+1] + coeff[5] * sample[N+2] + coeff[7] * sample[N+3] 
   + coeff[9] * sample[N+4] + coeff[11] * sample[N+5] + coeff[13] * sample[N+6] + coeff[15] * sample[N+7]

\code{coeff} describes the downmixing coefficients
*/

/* OMX_IndexConfigPlayMode: Play Mode */
typedef enum OMX_PLAYMODETYPE {
   OMX_PLAYMODE_NORMAL,
   OMX_PLAYMODE_FF,
   OMX_PLAYMODE_REW,
   OMX_PLAYMODE_DUMMY = 0x7FFFFFFF
} OMX_PLAYMODETYPE;

typedef struct OMX_CONFIG_PLAYMODETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_PLAYMODETYPE eMode;
} OMX_CONFIG_PLAYMODETYPE;
/*
The playmode affects which frames are extracted from the media file
and passed on the output ports. \code{OMX_PLAYMODE_NORMAL} will
extract all frames, \code{OMX_PLAYMODE_FF} extracts only IDR frames
when video is present, or only occasional packets of audio if no video
is present. \code{OMX_PLAYMODE_REW} is similar to
\code{OMX_PLAYMODE_FF} but extracts packets in reverse time
order.
*/

typedef enum OMX_DELIVERYFORMATTYPE {
   OMX_DELIVERYFORMAT_STREAM,         // no framing information is known
   OMX_DELIVERYFORMAT_SINGLE_PACKET,  // packetised, at most one frame per buffer
   OMX_DELIVERYFORMAT_DUMMY = 0x7FFFFFFF
} OMX_DELIVERYFORMATTYPE;

typedef struct OMX_PARAM_DELIVERYFORMATTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_DELIVERYFORMATTYPE eFormat;
} OMX_PARAM_DELIVERYFORMATTYPE;

/* OMX_IndexParamCodecConfig: Codec Configuration */

typedef struct OMX_PARAM_CODECCONFIGTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 bCodecConfigIsComplete;
   OMX_U8 nData[1];
} OMX_PARAM_CODECCONFIGTYPE;

/*
This parameter contains opaque data in a format specified by Broadcom
and allows out-of-band information such as cropping rectangles, aspect
ratio information, codec-specific header bytes, and other essential
information to be passed between connected components.

\code{bCodecConfigIsCompete} specifies if the codec config is fully
contained in here and there is no need to wait for OMX_BUFFERFLAG_CODECCONFIG
buffers
*/

typedef struct OMX_PARAM_STILLSFUNCTIONTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_BOOL bBuffer;
   OMX_PTR (*pOpenFunc)(void);
   OMX_PTR (*pCloseFunc)(void);
   OMX_PTR (*pReadFunc)(void);
   OMX_PTR (*pSeekFunc)(void);
   OMX_PTR (*pWriteFunc)(void);
} OMX_PARAM_STILLSFUNCTIONTYPE;

typedef void* OMX_BUFFERADDRESSHANDLETYPE;

typedef struct OMX_PARAM_BUFFERADDRESSTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 nAllocLen;
   OMX_BUFFERADDRESSHANDLETYPE handle;
} OMX_PARAM_BUFFERADDRESSTYPE;

typedef struct OMX_PARAM_TUNNELSETUPTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_TUNNELSETUPTYPE sSetup;
} OMX_PARAM_TUNNELSETUPTYPE;

/* OMX_IndexParamBrcmPortEGL: Used for querying whether a port is an EGL port or not. */
typedef struct OMX_PARAM_BRCMPORTEGLTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BOOL bPortIsEGL;
} OMX_PARAM_BRCMPORTEGLTYPE;
/*
*/

/* OMX_IndexConfigCommonImageFilterParameters: Parameterized Image Filter */
typedef struct OMX_CONFIG_IMAGEFILTERPARAMSTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_IMAGEFILTERTYPE eImageFilter;
   OMX_U32 nNumParams;
   OMX_U32 nParams[5];
} OMX_CONFIG_IMAGEFILTERPARAMSTYPE;
/*
This structure contains optional parameters for some image
filters. The following table lists all image filters that support
parameters.

<table border="1" cellspacing="0" cellpadding="2">
<tr><td>Filter<td>Parameters<td>Notes

<tr><td>\code{OMX_ImageFilterSolarize}
<td>\code{[x0 y0 y1 y2]}
<td>Linear mapping of \code{[0,x0]} to \code{[0,y0>]}
and \code{[x0,255]} to \code{[y1,y2]}.
Default is \code{"128 128 128 0"}.

<tr><td>\code{OMX_ImageFilterSharpen}
<td>\code{[sz [str [th]]}
<td>\code{sz} size of filter, either 1 or 2.
\code{str} strength of filter.
\code{th} threshold of filter.
Default is \code{"1 40 20"}.

<tr><td>\code{OMX_ImageFilterFilm}
<td>\code{[[str] [u v]]}
<td>\code{str} strength of effect.
\code{u} sets u to constant value.
\code{v} sets v to constant value.
Default is \code{"24"}.

<tr><td>\code{OMX_ImageFilterBlur}
<td>\code{[sz]}
<td>\code{sz} size of filter, either 1 or 2.
Default is \code{"2"}.

<tr><td>\code{OMX_ImageFilterSaturation}
<td>\code{[str]}
<td>\code{str} strength of effect, in 8.8 fixed point format. u/v value
differences from 128 are multiplied by \code{str}.
Default is \code{"272"}.
</table>
*/


/* OMX_IndexConfigTransitionControl: Transition Control */
typedef struct OMX_CONFIG_TRANSITIONCONTROLTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 nPosStart;
   OMX_U32 nPosEnd;
   OMX_S32 nPosIncrement;
   OMX_TICKS nFrameIncrement;
   OMX_BOOL bSwapInputs;
   OMX_U8 name[16];
   OMX_U8 property[64];
} OMX_CONFIG_TRANSITIONCONTROLTYPE;
/*
This structure represents the internal configuration of the
transition. Transitions are generated by a loadable plug-in described
by the \code{name} field. The component appends \code{"_tran.vll"} to
the name provided, and attempts to load a transition library contained
in this VLL.  The exact type of transition is configured in a
plug-in-dependent manner with the \code{property} field. All plug-ins
should accept a \code{property} field equal to
\code{"flags=<number>"}, where \code{<number>} can be a random 32 bit
number.  If \code{bSwapInputs} is false, then the start image is on
port 210, the stop image on port 211. These are reversed if
\code{bSwapInputs} is true.

Transition frames are generated from the plug-in by referencing a
frame position in [0,65536], where position 0 is the start image,
position 65536 is the stop image. The first frame position generated
is \code{nPosStart}. The last frame position generated is
\code{nPosEnd}. Each frame will increment the position by
\code{nPosIncrement}. The timestamp attached to each frame will
increment by \code{nFrameIncrement}.
*/


/*
This parameter is used to provide a callback function pointer for
release events. It is used for internal clients on VideoCore.
*/


/* OMX_IndexConfigAudioMonoTrackControl: Dual Mono Control */
typedef enum OMX_AUDIOMONOTRACKOPERATIONSTYPE {
   OMX_AUDIOMONOTRACKOPERATIONS_NOP,
   OMX_AUDIOMONOTRACKOPERATIONS_L_TO_R,
   OMX_AUDIOMONOTRACKOPERATIONS_R_TO_L,
   OMX_AUDIOMONOTRACKOPERATIONS_DUMMY = 0x7FFFFFFF
} OMX_AUDIOMONOTRACKOPERATIONSTYPE ;

typedef struct OMX_CONFIG_AUDIOMONOTRACKCONTROLTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_AUDIOMONOTRACKOPERATIONSTYPE eMode;
} OMX_CONFIG_AUDIOMONOTRACKCONTROLTYPE;
/*
This config controls the options to support dual mono audio
streams. The output can be unchanged, or the left channel copied over
the right channel, or the right channel copied over the left
channel. This config can be applied at any time with stereo
16-bit-per-sample data. Since audio output is typically buffered, any
change will not be audible until that buffering has been played out.
*/

/* OMX_IndexParamCameraImagePool: Camera Image Pools */
typedef enum OMX_CAMERAIMAGEPOOLINPUTMODETYPE {
   OMX_CAMERAIMAGEPOOLINPUTMODE_ONEPOOL,     /*All input images are allocated from one pool
                                               Works for simple stills capture use cases
                                               Can not be used with parallel stills capture
                                               and video encode, as the pool will be sized for
                                               capture or viewfinder, not both simultaneously.
                                               The pool wouldn't divide sensibly in this mode
                                               anyway.
                                             */
   OMX_CAMERAIMAGEPOOLINPUTMODE_TWOPOOLS,    /*All stills & video input images are allocated
                                               from two seperate pools.
                                               This ensures that parallel capture can work, but
                                               would consume more memory if used on a simple
                                               stills capture use case.
                                             */
} OMX_CAMERAIMAGEPOOLINPUTMODETYPE;

typedef struct OMX_PARAM_CAMERAIMAGEPOOLTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nNumHiResVideoFrames;
   OMX_U32 nHiResVideoWidth;
   OMX_U32 nHiResVideoHeight;
   OMX_COLOR_FORMATTYPE eHiResVideoType;
   OMX_U32 nNumHiResStillsFrames;
   OMX_U32 nHiResStillsWidth;
   OMX_U32 nHiResStillsHeight;
   OMX_COLOR_FORMATTYPE eHiResStillsType;
   OMX_U32 nNumLoResFrames;
   OMX_U32 nLoResWidth;
   OMX_U32 nLoResHeight;
   OMX_COLOR_FORMATTYPE eLoResType;
   OMX_U32 nNumSnapshotFrames;
   OMX_COLOR_FORMATTYPE eSnapshotType;
   OMX_CAMERAIMAGEPOOLINPUTMODETYPE eInputPoolMode;
   OMX_U32 nNumInputVideoFrames;
   OMX_U32 nInputVideoWidth;
   OMX_U32 nInputVideoHeight;
   OMX_COLOR_FORMATTYPE eInputVideoType;
   OMX_U32 nNumInputStillsFrames;
   OMX_U32 nInputStillsWidth;
   OMX_U32 nInputStillsHeight;
   OMX_COLOR_FORMATTYPE eInputStillsType;
} OMX_PARAM_CAMERAIMAGEPOOLTYPE;
/*
\sloppy This parameter specifies the size, type, and number, of images to
allow in the images pools required by Camplus. Supported types are
\code{OMX_COLOR_FormatYUV420PackedPlanar},
\code{OMX_COLOR_FormatYUV422PackedPlanar},
\code{OMX_COLOR_FormatRawBayer8bit},
\code{OMX_COLOR_FormatRawBayer10bit},
\code{OMX_COLOR_FormatRawBayer8bitcompressed}, and 0 (reserved for the
Broadcom-specific format required by the video encoder). The input
pool width, height, and type can be set as 0 to make the component
query Camplus for the sensor mode that would correspond to the largest
of the viewfinder port definition, the capture port definition, or the
high resolution image pool.
*/

/* OMX_IndexParamImagePoolSize: Specifying Image Pool Properties */
typedef struct OMX_PARAM_IMAGEPOOLSIZETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 width;
   OMX_U32 height;
   OMX_U32 num_pages;
} OMX_PARAM_IMAGEPOOLSIZETYPE;
/*
This parameter is used to control the size of pool that the component
will allocate in the absence of setting an external pool.  The default
can be reset by setting this parameter with all three fields set to
zero.
*/


/* OMX_IndexParamImagePoolExternal: Client Allocated Image Pools */
struct opaque_vc_pool_s;
typedef struct opaque_vc_pool_s OMX_BRCM_POOL_T;

typedef struct OMX_PARAM_IMAGEPOOLEXTERNALTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_BRCM_POOL_T *image_pool;
   OMX_BRCM_POOL_T *image_pool2;
   OMX_BRCM_POOL_T *image_pool3;
   OMX_BRCM_POOL_T *image_pool4;
   OMX_BRCM_POOL_T *image_pool5;
} OMX_PARAM_IMAGEPOOLEXTERNALTYPE;
/*
This config allows the client to pass in handles to pre-allocated
image pools for use within the component.
*/


struct _IL_FIFO_T;
typedef struct OMX_PARAM_RUTILFIFOINFOTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   struct _IL_FIFO_T *pILFifo;
} OMX_PARAM_RUTILFIFOINFOTYPE;

/* OMX_IndexParamILFifoConfig: Allows configuration of the FIFO settings. */
typedef struct OMX_PARAM_ILFIFOCONFIG {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 nDataSize;         /**< The size of the FIFO's data area */
   OMX_U32 nHeaderCount;      /**< The number of headers allocated */
} OMX_PARAM_ILFIFOCONFIG;
/**
 * Allows configuring the size of the ILFIFO used in a component.
 */

/* OMX_IndexConfigCameraSensorModes: Camera Sensor Mode */
typedef struct OMX_CONFIG_CAMERASENSORMODETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 nModeIndex;
   OMX_U32 nNumModes;
   OMX_U32 nWidth;
   OMX_U32 nHeight;
   OMX_U32 nPaddingRight;
   OMX_U32 nPaddingDown;
   OMX_COLOR_FORMATTYPE eColorFormat;
   OMX_U32 nFrameRateMax;
   OMX_U32 nFrameRateMin;
} OMX_CONFIG_CAMERASENSORMODETYPE;
/*
This parameter is used by clients to determine the sensor mode, and
hence the memory usage, of the camera module. This is primarily used
for determining the size of the input image pool.

It can be used in two ways dependent on \code{nPortIndex}. If
\code{nPortIndex} is \code{OMX_ALL}, it returns the sensor mode
corresponding to \code{nModeIndex}, and the number of modes in
\code{nNumModes}. If \code{nModeIndex} is greater than or equal to
\code{nNumModes} only \code{nNumModes} is returned. If
\code{nPortIndex} is equal to a camera video output port index, it
returns the sensor mode that would be selected for the values
currently in \code{OMX_IndexParamPortDefinition} for that port.

The \code{nPaddingRight} and \code{nPaddingDown} values determine the
extra padding the sensor adds to the image. These values must be added
to \code{nWidth} and \code{nHeight} respectively if the client is
specifying the input image pool size.
*/

typedef struct OMX_BRCMBUFFERSTATSTYPE {
   OMX_U32 nOrdinal;
   OMX_TICKS nTimeStamp;
   OMX_U32 nFilledLen;
   OMX_U32 nFlags;
   union
   {
      OMX_U32 nU32;
      struct
      {
         OMX_U32 nYpart;
         OMX_U32 nUVpart;
      } image;
   } crc;
} OMX_BRCMBUFFERSTATSTYPE;

/*
Ports that gather statistics for debugging and diagnostics
might also collect information about buffer header fields
and data.

Note that:

The \code{nOrdinal} field increases monotonically whenever
a new buffer is received or emitted and shall not be reset
upon a port flush.

The \code{nFilledLen} might indicate the size of a data area
larger than the data area that actually contributed to the
checksums (e.g. when image data is provided with cropping
information).
*/

/* OMX_IndexConfigBrcmPortBufferStats: Query port buffer stats history */
typedef struct OMX_CONFIG_BRCMPORTBUFFERSTATSTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 nCount;
   OMX_BRCMBUFFERSTATSTYPE sData[1];
} OMX_CONFIG_BRCMPORTBUFFERSTATSTYPE;
/*
Ports that gather statistics for debugging and diagnostics
might also collect information about buffer header fields
and data.

The \code{sStatsData} field is a variable length array and
the number of items is denoted by \code{nStatsCount}.
*/

/* OMX_IndexConfigBrcmPortStats: Query port statistics */
typedef struct OMX_CONFIG_BRCMPORTSTATSTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 nImageCount;
   OMX_U32 nBufferCount;
   OMX_U32 nFrameCount;
   OMX_U32 nFrameSkips;
   OMX_U32 nDiscards;
   OMX_U32 nEOS;
   OMX_U32 nMaxFrameSize;

   OMX_TICKS nByteCount;
   OMX_TICKS nMaxTimeDelta;
   OMX_U32 nCorruptMBs;   /**< Number of corrupt macroblocks in the stream */
} OMX_CONFIG_BRCMPORTSTATSTYPE;
/*
Some ports gather various statistics that can be used by clients for
debugging purposes.  This structure is the set of all statistics that
are gathered.

The \code{nFrameSkips} field indicates the number of frames that did
not have an expected PTS value based on the port frame rate.

The \code{nByteCount} field is a 64 bit value, that will either use a
64 bit type or two 32 bit types, similarly to \code{OMX_TICKS}.
*/

/* OMX_IndexConfigBrcmClockMissCount: Missed clock request accounting */
/*
For each port on the clock component, requests for media times may be
made.  These are typically done one per video frame to allow for
scheduling the display of that frame at the correct time.  If a
request is made after the time has occurred, then that frame will be
displayed late, and the clock component keeps a per-port record of the
number of times this occurs.  This record can be read using this
index.
*/

typedef struct OMX_CONFIG_BRCMCAMERASTATSTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nOutFrameCount;
   OMX_U32 nDroppedFrameCount;
} OMX_CONFIG_BRCMCAMERASTATSTYPE;

// for backward compatibility
typedef struct OMX_CONFIG_BRCMCAMERASTATSTYPE OMX_CONFIG_BRCMCAMERASTATS;


#define OMX_BRCM_MAXIOPERFBANDS 10
typedef struct {
   OMX_U32 count[OMX_BRCM_MAXIOPERFBANDS];
   OMX_U32 num[OMX_BRCM_MAXIOPERFBANDS];
} OMX_BRCM_PERFSTATS;

/* OMX_IndexConfigBrcmIOPerfStats: Query I/O performance statistics */
typedef struct OMX_CONFIG_BRCMIOPERFSTATSTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_BOOL bEnabled;                              /**< Enable/disable I/O performance statistics */
   OMX_BRCM_PERFSTATS write; /**< count:bytes     num:microseconds */
   OMX_BRCM_PERFSTATS flush; /**< count:frequency num:microseconds waiting to flush data */
   OMX_BRCM_PERFSTATS wait;  /**< count:frequency num:microseconds waiting in calling function */
} OMX_CONFIG_BRCMIOPERFSTATSTYPE;
/*
A sink component can gather various statistics about I/O (eg. file media) performance that can be used by
clients for debugging purposes.  The \code{bEnabled} field is used to turn the gathering of statistics
on/off.
*/

typedef struct OMX_CONFIG_SHARPNESSTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_S32 nSharpness;
} OMX_CONFIG_SHARPNESSTYPE;

/* OMX_IndexConfigCommonFlickerCancellation: Flicker cancellation */
typedef enum OMX_COMMONFLICKERCANCELTYPE {
   OMX_COMMONFLICKERCANCEL_OFF,
   OMX_COMMONFLICKERCANCEL_AUTO,
   OMX_COMMONFLICKERCANCEL_50,
   OMX_COMMONFLICKERCANCEL_60,
   OMX_COMMONFLICKERCANCEL_DUMMY = 0x7FFFFFFF
} OMX_COMMONFLICKERCANCELTYPE;

typedef struct OMX_CONFIG_FLICKERCANCELTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_COMMONFLICKERCANCELTYPE eFlickerCancel;
} OMX_CONFIG_FLICKERCANCELTYPE;
/*
Query / set the flicker cancellation frequency. Values are defined for Off,
50Hz, 60Hz, or auto. The method for auto detecting the flicker frequency is
not defined, and currently results in the feature being turned off.
*/

/* OMX_IndexConfigCommonRedEyeRemoval: Red eye removal/reduction */
typedef enum OMX_REDEYEREMOVALTYPE {
   OMX_RedEyeRemovalNone,                           /**< No red eye removal */
   OMX_RedEyeRemovalOn,                             /**< Red eye removal on */
   OMX_RedEyeRemovalAuto,                           /**< Red eye removal will be done automatically when detected */
   OMX_RedEyeRemovalKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   OMX_RedEyeRemovalVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   OMX_RedEyeRemovalSimple,                         /**< Use simple red eye reduction mechanism if supported by algorithm */
   OMX_RedEyeRemovalMax = 0x7FFFFFFF
} OMX_REDEYEREMOVALTYPE;

typedef struct OMX_CONFIG_REDEYEREMOVALTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_REDEYEREMOVALTYPE eMode;
} OMX_CONFIG_REDEYEREMOVALTYPE;
/*
   Configures the red eye reduction algorithm in the camera processing
   pipeline. The stage is only enabled if the flash mode is not FlashOff.
   The OMX_RedEyeRemovalSimple mode requests that the algorithm uses a
   reduced complexity algorithm to reduce the processing time.
*/


typedef enum OMX_FACEDETECTIONCONTROLTYPE {
   OMX_FaceDetectionControlNone,                           /**< Disables face detection */
   OMX_FaceDetectionControlOn,                             /**< Enables face detection */
   OMX_FaceDetectionControlKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   OMX_FaceDetectionControlVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   OMX_FaceDetectionControlMax = 0x7FFFFFFF
} OMX_FACEDETECTIONCONTROLTYPE;

typedef struct OMX_CONFIG_FACEDETECTIONCONTROLTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_FACEDETECTIONCONTROLTYPE eMode;
   OMX_U32 nFrames;      /**< number of frames to apply this setting for,
                              0 for unlimited */
   OMX_U32 nMaxRegions;  /**< maximum number of regions to detect, 0 for unlimited */
   OMX_U32 nQuality;     /**< hint for algorithmic complexity, range is 0-100.
                              0 for simplest algorithm, 100 for best quality */
} OMX_CONFIG_FACEDETECTIONCONTROLTYPE;

typedef enum OMX_FACEREGIONFLAGSTYPE {
   OMX_FaceRegionFlagsNone    = 0,
   OMX_FaceRegionFlagsBlink   = 1,
   OMX_FaceRegionFlagsSmile   = 2,
   OMX_FaceRegionFlagsKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   OMX_FaceRegionFlagsVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   OMX_FaceRegionFlagsMax = 0x7FFFFFFF
} OMX_FACEREGIONFLAGSTYPE;

typedef struct OMX_FACEREGIONTYPE {
   OMX_S16 nLeft;              /**< X Coordinate of the top left corner of the rectangle */
   OMX_S16 nTop;               /**< Y Coordinate of the top left corner of the rectangle */
   OMX_U16 nWidth;             /**< Width of the rectangle */
   OMX_U16 nHeight;            /**< Height of the rectangle */
   OMX_FACEREGIONFLAGSTYPE nFlags;  /**< Flags for the region */
#ifndef OMX_SKIP64BIT
   OMX_U64 nFaceRecognitionId; /**< ID returned by face recognition for this face */
#else
   struct
   {
      OMX_U32 nLowPart;   /**< low bits of the signed 64 bit value */
      OMX_U32 nHighPart;  /**< high bits of the signed 64 bit value */
   } nFaceRecognitionId;
#endif
} OMX_FACEREGIONTYPE;

typedef struct OMX_CONFIG_FACEDETECTIONREGIONTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;            /**< index of port with face detection enabled */
   OMX_U32 nIndex;                /**< first requested region number, allowing retrieval of many regions
                                       over several requests */
   OMX_U32 nDetectedRegions;      /**< total number of detected regions */
   OMX_S32 nValidRegions;         /**< number of valid regions in sRegion array
                                       When getting, the client sets this to the number of regions available.
                                       The component writes region data and updates this field with how many
                                       regions have been written to. */
   OMX_U32 nImageWidth;           /**< Width of the image, hence reference for the face coordinates */
   OMX_U32 nImageHeight;          /**< Height of the image, hence reference for the face coordinates */
   OMX_FACEREGIONTYPE sRegion[1]; /**< variable length array of face regions */
} OMX_CONFIG_FACEDETECTIONREGIONTYPE;

typedef enum OMX_INTERLACETYPE {
   OMX_InterlaceProgressive,                    /**< The data is not interlaced, it is progressive scan */
   OMX_InterlaceFieldSingleUpperFirst,          /**< The data is interlaced, fields sent
                                                     separately in temporal order, with upper field first */
   OMX_InterlaceFieldSingleLowerFirst,          /**< The data is interlaced, fields sent
                                                     separately in temporal order, with lower field first */
   OMX_InterlaceFieldsInterleavedUpperFirst,    /**< The data is interlaced, two fields sent together line
                                                     interleaved, with the upper field temporally earlier */
   OMX_InterlaceFieldsInterleavedLowerFirst,    /**< The data is interlaced, two fields sent together line
                                                     interleaved, with the lower field temporally earlier */
   OMX_InterlaceMixed,                          /**< The stream may contain a mixture of progressive
                                                     and interlaced frames */
   OMX_InterlaceKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   OMX_InterlaceVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   OMX_InterlaceMax = 0x7FFFFFFF
} OMX_INTERLACETYPE;

typedef struct OMX_CONFIG_INTERLACETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;            /**< index of port emitting or accepting the content */
   OMX_INTERLACETYPE eMode;       /**< The interlace type of the content */
   OMX_BOOL bRepeatFirstField;    /**< Whether to repeat the first field */
} OMX_CONFIG_INTERLACETYPE;

/* OMX_IndexParamIspTuner: Custom ISP tuner */
typedef struct OMX_PARAM_CAMERAISPTUNERTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U8 tuner_name[64];
} OMX_PARAM_CAMERAISPTUNERTYPE;
/*
This parameter allows a custom ISP tuner to be loaded instead of
the default one specified for the camera module. Setting an empty
string uses the default value.
*/

/* OMX_IndexConfigCameraInputFrame: Pointer to the raw input image */
typedef struct OMX_CONFIG_IMAGEPTRTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_PTR pImage;
} OMX_CONFIG_IMAGEPTRTYPE;
/*
This parameter parameter allows the return of a pointer to a
VideoCore image resource.
*/

/* OMX_IndexConfigAFAssistLight: Autofocus assist light mode selection */
typedef enum OMX_AFASSISTTYPE {
   OMX_AFAssistAuto,
   OMX_AFAssistOn,
   OMX_AFAssistOff,
   OMX_AFAssistTorch,
   OMX_AFAssistKhronosExtensions = 0x6F000000,
   OMX_AFAssistVendorStartUnused = 0x7F000000,
   OMX_AFAssistMax = 0x7FFFFFFF
} OMX_AFASSISTTYPE;

typedef struct OMX_CONFIG_AFASSISTTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_AFASSISTTYPE eMode;
} OMX_CONFIG_AFASSISTTYPE;
/*
Set the mode to adopt for the autofocus assist light.
\code{OMX_AFAssistTorch} will turn the AF assist light on permanently, allowing
it to be used as a torch.
*/

/* OMX_IndexConfigInputCropPercentage: Specify input crop as a percentage */
typedef struct OMX_CONFIG_INPUTCROPTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 xLeft;     /**< Fraction of the width for the top left corner of the rectangle */
   OMX_U32 xTop;      /**< Fraction of the height for the top left corner of the rectangle */
   OMX_U32 xWidth;    /**< Fraction of the image width desired */
   OMX_U32 xHeight;   /**< Fraction of the image height desired */
} OMX_CONFIG_INPUTCROPTYPE;
/*
This parameter allows the input cropping to be specified as a
percentage of the current width/height.  Required for the camera
component where the output resolution varies dependent on the port.
All percentage values are as 16p16 fixed point numbers (0x10000 =
100\%)
*/

/* OMX_IndexParamCodecRequirements: Advanced codec requirements */
typedef struct OMX_PARAM_CODECREQUIREMENTSTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nCallbackID;
   OMX_BOOL bTryHWCodec;
} OMX_PARAM_CODECREQUIREMENTSTYPE;
/*
This parameter allows internal users of RIL components controlling
video codecs to request that the component loads the component and
queries for requirements.  The component will perform a callback with
the given nCallbackID value passing a pointer to the requirements
structure as the data field.
*/

/* OMX_IndexConfigBrcmEGLImageMemHandle: Mapping from an EGLImage to a VideoCore mem handle */
typedef struct OMX_CONFIG_BRCMEGLIMAGEMEMHANDLETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_PTR eglImage;
   OMX_PTR memHandle;
} OMX_CONFIG_BRCMEGLIMAGEMEMHANDLETYPE;
/*
This config allows the EGL server to notify a RIL component that an
EGLImage is available for rendering into and to provide a mapping from
an EGLImage to a mem handle.
*/

/* OMX_IndexConfigPrivacyIndicator: Privacy indicator control */
typedef enum OMX_PRIVACYINDICATORTYPE {
   OMX_PrivacyIndicatorOff,
   OMX_PrivacyIndicatorOn,
   OMX_PrivacyIndicatorForceOn,
   OMX_PrivacyIndicatorKhronosExtensions = 0x6F000000,
   OMX_PrivacyIndicatorVendorStartUnused = 0x7F000000,
   OMX_PrivacyIndicatorMax = 0x7FFFFFFF
} OMX_PRIVACYINDICATORTYPE;

typedef struct OMX_CONFIG_PRIVACYINDICATORTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_PRIVACYINDICATORTYPE ePrivacyIndicatorMode;
} OMX_CONFIG_PRIVACYINDICATORTYPE;
/*
This config allows control over the privacy indicator light.  This
light indicates when a capture is in progress.

\code{OMX_PrivacyIndicatorOff} indicator is disabled.

\code{OMX_PrivacyIndicatorOn} indicator will be
turned on whenever an image is being captured as determined by the
capturing bit. Minimum on duration of approx 200ms.

\code{OMX_PrivacyIndicatorForceOn} results in turning the indicator on
immediately, whether an image is being captured or not. The mode will
automatically revert to \code{OMX_PrivacyIndicatorOff} once the
indicator has been turned on, so \code{OMX_PrivacyIndicatorForceOn}
must be requested at least every 200ms if the indicator is to remain
on.
*/


/* OMX_IndexParamCameraFlashType: Select flash type */
typedef enum OMX_CAMERAFLASHTYPE {
   OMX_CameraFlashDefault,
   OMX_CameraFlashXenon,
   OMX_CameraFlashLED,
   OMX_CameraFlashNone,
   OMX_CameraFlashKhronosExtensions = 0x6F000000,
   OMX_CameraFlashVendorStartUnused = 0x7F000000,
   OMX_CameraFlashMax = 0x7FFFFFFF
} OMX_CAMERAFLASHTYPE;

typedef struct OMX_PARAM_CAMERAFLASHTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_CAMERAFLASHTYPE eFlashType;
   OMX_BOOL bRedEyeUsesTorchMode;
} OMX_PARAM_CAMERAFLASHTYPE;
/*
This parameter allows the selection of xenon or LED flash devices
to be used with the currently selected camera. If that device is not
available, then the component will revert back to whatever flash
device is available for that camera.
\code{bRedEyeUsesTorchMode} allows the blinking for red eye reduction to
be switched between using the indicator mode, and the torch mode for the
flash driver.
*/

/* OMX_IndexConfigCameraFlashConfig: Flash cycle configuration */
typedef enum OMX_CAMERAFLASHCONFIGSYNCTYPE {
   OMX_CameraFlashConfigSyncFrontSlow,
   OMX_CameraFlashConfigSyncRearSlow,
   OMX_CameraFlashConfigSyncFrontFast,
   OMX_CameraFlashConfigSyncKhronosExtensions = 0x6F000000,
   OMX_CameraFlashConfigSyncVendorStartUnused = 0x7F000000,
   OMX_CameraFlashConfigSyncMax = 0x7FFFFFFF
} OMX_CAMERAFLASHCONFIGSYNCTYPE;

typedef struct OMX_CONFIG_CAMERAFLASHCONFIGTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BOOL bUsePreFlash;
   OMX_BOOL bUseFocusDistanceInfo;
   OMX_CAMERAFLASHCONFIGSYNCTYPE eFlashSync;
   OMX_BOOL bIgnoreChargeState;
} OMX_CONFIG_CAMERAFLASHCONFIGTYPE;
/*
This parameter allows the configuration of various parameters relating to
the flash cycle. Some of the options are only applicable to xenon flash.

\code{bUsePreFlash} uses a low intensity pre-flash to determine flash intensity. This setting
is recommended for almost all flash situations.

\code{bUseFocusDistanceInfo} uses the distance of the subject, as measured by the AF algorithm
to set the intensity of the flash.

\code{eFlashSync} configures which edge of the shutter is used to synchronise the flash, and
the duration of the exposure.

\code{eIgnoreChargeState} will make the flash fire, even if it is not fully charged.
*/

/* OMX_IndexConfigBrcmAudioTrackGaplessPlayback: Encoder/decoder delay and padding information for gapless playback. */
typedef struct OMX_CONFIG_BRCMAUDIOTRACKGAPLESSPLAYBACKTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 nDelay;   /**< number of samples delay added by the codec */
   OMX_U32 nPadding; /**< number of silent samples added to the end */
} OMX_CONFIG_BRCMAUDIOTRACKGAPLESSPLAYBACKTYPE;
/*
This config allows communication between components to facilitate gapless playback.
*/


/* OMX_IndexConfigBrcmAudioTrackChangeControl: Configure gapless/crossfaded audio track change. */
typedef struct OMX_CONFIG_BRCMAUDIOTRACKCHANGECONTROLTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nSrcPortIndex;
   OMX_U32 nDstPortIndex;
   OMX_U32 nXFade;
} OMX_CONFIG_BRCMAUDIOTRACKCHANGECONTROLTYPE;
/*
This config allows the client to specify the gapless or crossfade
parameters to be used on a track change.  If \code{nXFade} is 0, then
a normal or gapless track change will result, otherwise a crossfade of
\code{nXFade} ms is used.
*/

/* OMX_IndexParamBrcmPixelValueRange: Describing the pixel value range */
typedef enum OMX_BRCMPIXELVALUERANGETYPE
{
   OMX_PixelValueRangeUnspecified = 0,
   OMX_PixelValueRangeITU_R_BT601,
   OMX_PixelValueRangeFull8Bit,
   OMX_PixelValueRangeKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   OMX_PixelValueRangeVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   OMX_PixelValueRangeMax = 0x7FFFFFFF
} OMX_BRCMPIXELVALUERANGETYPE;

typedef struct OMX_PARAM_BRCMPIXELVALUERANGETYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BRCMPIXELVALUERANGETYPE ePixelValueRange;
} OMX_PARAM_BRCMPIXELVALUERANGETYPE;
/*
This structure allows a description of the range that pixel values may
have.  This is typically useful since some standards use the full 8
bit range, whereas others introduce pedastals which reduce the range
at the top and bottom end.
*/

/* OMX_IndexParamCameraDisableAlgorithm: Disabling camera processing stages. */
typedef enum OMX_CAMERADISABLEALGORITHMTYPE {
      OMX_CameraDisableAlgorithmFacetracking,
      OMX_CameraDisableAlgorithmRedEyeReduction,
      OMX_CameraDisableAlgorithmVideoStabilisation,
      OMX_CameraDisableAlgorithmWriteRaw,
      OMX_CameraDisableAlgorithmVideoDenoise,
      OMX_CameraDisableAlgorithmStillsDenoise,
      OMX_CameraDisableAlgorithmAntiShake,
      OMX_CameraDisableAlgorithmImageEffects,
      OMX_CameraDisableAlgorithmDarkSubtract,
      OMX_CameraDisableAlgorithmDynamicRangeExpansion,
      OMX_CameraDisableAlgorithmFaceRecognition,
      OMX_CameraDisableAlgorithmFaceBeautification,
      OMX_CameraDisableAlgorithmSceneDetection,
      OMX_CameraDisableAlgorithmHighDynamicRange,
   OMX_CameraDisableAlgorithmKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   OMX_CameraDisableAlgorithmVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   OMX_CameraDisableAlgorithmMax = 0x7FFFFFFF
} OMX_CAMERADISABLEALGORITHMTYPE;

typedef struct OMX_PARAM_CAMERADISABLEALGORITHMTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_CAMERADISABLEALGORITHMTYPE eAlgorithm;
   OMX_BOOL bDisabled;
} OMX_PARAM_CAMERADISABLEALGORITHMTYPE;
/*
Allows plugin algorithms to be disabled to save memory
within the camera component
*/

/* OMX_IndexConfigBrcmAudioEffectControl: Audio Effect Control */
typedef struct OMX_CONFIG_BRCMAUDIOEFFECTCONTROLTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BOOL bEnable;
   OMX_U8 name[16];
   OMX_U8 property[256];
} OMX_CONFIG_BRCMAUDIOEFFECTCONTROLTYPE;
/*
This structure represents the internal configuration of an audio effect.
The audio effect is provided by a loadable plug-in described
in the \code{name} field and is configured in a plug-in-dependent
manner with the \code{property} field. The \code{bEnable} field is used to
turn the effect on/off.
*/

/* OMX_IndexConfigBrcmMinimumProcessingLatency: Processing Latency Bound */
typedef struct OMX_CONFIG_BRCMMINIMUMPROCESSINGLATENCY {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_TICKS nOffset;
} OMX_CONFIG_BRCMMINIMUMPROCESSINGLATENCY;
/*
Query/set the difference between the actual media time and when the
component receives request fulfillments for media time requests. This
can be used with e.g. splitter/mixer components to control when the
component stops waiting for input or output packets from active
streams and continues with processing (to maintain a constant
processing rate).
*/

/** Enable or disable Supplemental Enhancment Information (SEI) messages to be inserted in
  * the H.264 bitstream.
  */
typedef struct OMX_PARAM_BRCMVIDEOAVCSEIENABLETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BOOL bEnable;
} OMX_PARAM_BRCMVIDEOAVCSEIENABLETYPE;

/* OMX_IndexParamBrcmAllowMemChange: Allowing changing memory allocation on state transition */
typedef struct OMX_PARAM_BRCMALLOWMEMCHANGETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_BOOL bEnable;
} OMX_PARAM_BRCMALLOWMEMCHANGETYPE;
/*
Let the component change the amount of memory it has allocated when
going from LOADED to IDLE. By default this is enabled, but if it is
disabled the component will fail to transition to IDLE if the
component requires more memory than has already been allocated.  This
might occur if (for example) the component was configured, taken to
IDLE, then taken back to LOADED, the profile increased and the
component taken back to IDLE.
*/

typedef enum OMX_CONFIG_CAMERAUSECASE {
   OMX_CameraUseCaseAuto,
   OMX_CameraUseCaseVideo,
   OMX_CameraUseCaseStills,
   OMX_CameraUseCaseKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   OMX_CameraUseCaseVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   OMX_CameraUseCaseMax = 0x7FFFFFFF
} OMX_CONFIG_CAMERAUSECASE;

typedef struct OMX_CONFIG_CAMERAUSECASETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_CONFIG_CAMERAUSECASE eUseCase;
} OMX_CONFIG_CAMERAUSECASETYPE;

/* OMX_IndexParamBrcmDisableProprietaryTunnels: Disabling proprietary tunnelling */
typedef struct OMX_PARAM_BRCMDISABLEPROPRIETARYTUNNELSTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BOOL bUseBuffers;
}  OMX_PARAM_BRCMDISABLEPROPRIETARYTUNNELSTYPE;
/*
Tell a source component to refuse to support proprietary tunnelling. Buffers will be used instead.
*/


//
// Control for memory allocation and component-internal buffering
//

/* OMX_IndexParamBrcmRetainMemory: Controlling memory use on state transition */
typedef struct OMX_PARAM_BRCMRETAINMEMORYTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_BOOL bEnable;
} OMX_PARAM_BRCMRETAINMEMORYTYPE;
/*
Ask a component to retain its memory when going from IDLE to LOADED, if possible.
This has the benefit that you are then guaranteed that the transition to IDLE cannot
fail due to lack of memory, but has the disadvantage that you cannot leave the component
lying around in LOADED, unused, since it is using significant amounts of memory.
*/

/** Tell write media how large the output buffer should be. This is a hint, and
  * may be ignored. A good size is bandwidth*<SDcard-delay>, which works out at
  * around 1Mbyte for up to 16Mbit/s. Sizes may (and probably will) be rounded down
  * to the nearest power of 2.
  */
typedef struct OMX_PARAM_BRCMOUTPUTBUFFERSIZETYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nBufferSize;
} OMX_PARAM_BRCMOUTPUTBUFFERSIZETYPE;

/* OMX_IndexConfigCameraInfo: Camera device driver information */
#define OMX_CONFIG_CAMERAINFOTYPE_NAME_LEN 16
typedef struct OMX_CONFIG_LENSCALIBRATIONVALUETYPE
{
   OMX_U16  nShutterDelayTime;
   OMX_U16  nNdTransparency;
   OMX_U16  nPwmPulseNearEnd;  /**< Num pulses to move lens 1um at near end */
   OMX_U16  nPwmPulseFarEnd;   /**< Num pulses to move lens 1um at far end */
   OMX_U16  nVoltagePIOutNearEnd[3];
   OMX_U16  nVoltagePIOut10cm[3];
   OMX_U16  nVoltagePIOutInfinity[3];
   OMX_U16  nVoltagePIOutFarEnd[3];
   OMX_U32  nAdcConversionNearEnd;
   OMX_U32  nAdcConversionFarEnd;
} OMX_CONFIG_LENSCALIBRATIONVALUETYPE;
/*
Ask the camera component for the driver info on the current camera device
*/

#define OMX_CONFIG_CAMERAINFOTYPE_NAME_LEN 16
#define OMX_CONFIG_CAMERAINFOTYPE_SERIALNUM_LEN 20
#define OMX_CONFIG_CAMERAINFOTYPE_EPROMVER_LEN 8
typedef struct OMX_CONFIG_CAMERAINFOTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U8 cameraname[OMX_CONFIG_CAMERAINFOTYPE_NAME_LEN];
   OMX_U8 lensname[OMX_CONFIG_CAMERAINFOTYPE_NAME_LEN];
   OMX_U16 nModelId;
   OMX_U8 nManufacturerId;
   OMX_U8 nRevNum;
   OMX_U8 sSerialNumber[OMX_CONFIG_CAMERAINFOTYPE_SERIALNUM_LEN];
   OMX_U8 sEpromVersion[OMX_CONFIG_CAMERAINFOTYPE_EPROMVER_LEN];
   OMX_CONFIG_LENSCALIBRATIONVALUETYPE sLensCalibration;
   OMX_U32 xFNumber;
   OMX_U32 xFocalLength;
} OMX_CONFIG_CAMERAINFOTYPE;


typedef enum OMX_CONFIG_CAMERAFEATURESSHUTTER {
   OMX_CameraFeaturesShutterUnknown,
   OMX_CameraFeaturesShutterNotPresent,
   OMX_CameraFeaturesShutterPresent,
   OMX_CameraFeaturesShutterKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   OMX_CameraFeaturesShutterVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   OMX_CameraFeaturesShutterMax = 0x7FFFFFFF
} OMX_CONFIG_CAMERAFEATURESSHUTTER;

typedef struct OMX_CONFIG_CAMERAFEATURESTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_CONFIG_CAMERAFEATURESSHUTTER eHasMechanicalShutter;
   OMX_BOOL bHasLens;
} OMX_CONFIG_CAMERAFEATURESTYPE;


//Should be added to the spec as part of IL416c
/* OMX_IndexConfigRequestCallback: Enable config change notifications. */
typedef struct OMX_CONFIG_REQUESTCALLBACKTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_INDEXTYPE nIndex;
   OMX_BOOL bEnable;
} OMX_CONFIG_REQUESTCALLBACKTYPE;
/*
This config implements IL416c to allow clients to request notification
of when a config or parameter is changed. When the parameter specified
in \code{nIndex} for port \code{nPortIndex} changes, an
\code{OMX_EventParamOrConfigChanged} event is generated for the client.
*/

/* OMX_IndexConfigCommonFocusRegionXY: Define focus regions */
typedef enum OMX_FOCUSREGIONTYPE {
   OMX_FocusRegionNormal,
   OMX_FocusRegionFace,
   OMX_FocusRegionMax
} OMX_FOCUSREGIONTYPE;

typedef struct OMX_FOCUSREGIONXY {
   OMX_U32 xLeft;
   OMX_U32 xTop;
   OMX_U32 xWidth;
   OMX_U32 xHeight;
   OMX_U32 nWeight;
   OMX_U32 nMask;
   OMX_FOCUSREGIONTYPE eType;
} OMX_FOCUSREGIONXY;

typedef struct OMX_CONFIG_FOCUSREGIONXYTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 nIndex;
   OMX_U32 nTotalRegions;
   OMX_S32 nValidRegions;
   OMX_BOOL bLockToFaces;
   OMX_U32 xFaceTolerance;
   OMX_FOCUSREGIONXY sRegion[1];
} OMX_CONFIG_FOCUSREGIONXYTYPE;
/*
Query / set the focus regions to use as a set of x/y/width/height boxes relative
to the overall image.

\code{nIndex} - first region number being set/read, allowing retrieval/setting
of many regions over several requests.

\code{nTotalRegions} - total number of regions currently defined.

\code{nValidRegions} - number of valid regions in the \code{sRegion} array.
When getting, the client sets this to the number of regions available.
The component writes region data and updates this field with how many
regions have been written to.
When setting, this is the number of regions defined with this structure

\code{bLockToFaces} - compare the region(s) given to the latest face tracking results.
If a face is found within xFaceTolerance of the defined region, then amend the
region to correspond to the face.

\code{xFaceTolerance} - 0p16 value to define the max difference between the region centre
and face tracking result centre to take the FT results

\code{sRegions} - variable length array of focus regions.
*/

typedef struct OMX_CONFIG_U8TYPE {
    OMX_U32 nSize;                    /**< Size of this structure, in Bytes */
    OMX_VERSIONTYPE nVersion;         /**< OMX specification version information */
    OMX_U32 nPortIndex;               /**< port that this structure applies to */
    OMX_U8  nU8;                     /**< U8 value */
} OMX_PARAM_U8TYPE;

typedef struct OMX_CONFIG_CAMERASETTINGSTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nExposure;
    OMX_U32 nAnalogGain;
    OMX_U32 nDigitalGain;
    OMX_U32 nLux;
    OMX_U32 nRedGain;
    OMX_U32 nBlueGain;
    OMX_U32 nFocusPosition;
} OMX_CONFIG_CAMERASETTINGSTYPE;

/* OMX_IndexConfigDrawBoxLineParams: Face box style parameters. */
typedef struct OMX_YUVCOLOUR {
   OMX_U8 nY;
   OMX_U8 nU;
   OMX_U8 nV;
} OMX_YUVCOLOUR;

typedef struct OMX_CONFIG_DRAWBOXLINEPARAMS {
    OMX_U32 nSize;                           /**< Size of this structure, in Bytes */
    OMX_VERSIONTYPE nVersion;                /**< OMX specification version information */
    OMX_U32 nPortIndex;                      /**< Port to which this config applies */
    OMX_U32 xCornerSize;                     /**< Size of the corners as a fraction of the complete side */
    OMX_U32 nPrimaryFaceLineWidth;           /**< Width of the box line for the primary face in pixels */
    OMX_U32 nOtherFaceLineWidth;             /**< Width of the box line for other faces in pixels */
    OMX_U32 nFocusRegionLineWidth;           /**< Width of the box line for focus regions in pixels */
    OMX_YUVCOLOUR sPrimaryFaceColour;        /**< YUV colour for the primary face */
    OMX_YUVCOLOUR sPrimaryFaceSmileColour;   /**< YUV colour for the primary face if smiling */
    OMX_YUVCOLOUR sPrimaryFaceBlinkColour;   /**< YUV colour for the primary face if blinking */
    OMX_YUVCOLOUR sOtherFaceColour;          /**< YUV colour for the all other faces */
    OMX_YUVCOLOUR sOtherFaceSmileColour;     /**< YUV colour for the all other faces if smiling */
    OMX_YUVCOLOUR sOtherFaceBlinkColour;     /**< YUV colour for the all other faces if blinking */
    OMX_BOOL bShowFocusRegionsWhenIdle;      /**< Are focus regions displayed when just in viewfinder/AF idle */
    OMX_YUVCOLOUR sFocusRegionColour;        /**< YUV colour for focus regions */
    OMX_BOOL bShowAfState;                   /**< Change to the colours specified below if AF cycle has run */
    OMX_BOOL bShowOnlyPrimaryAfState;        /**< Only show the primary face when displaying the AF status */
    OMX_BOOL bCombineNonFaceRegions;         /**< Combine all regions not defined as faces into one single box covering them all */
    OMX_YUVCOLOUR sAfLockPrimaryFaceColour;  /**< YUV colour for the primary face */
    OMX_YUVCOLOUR sAfLockOtherFaceColour;    /**< YUV colour for the all other faces */
    OMX_YUVCOLOUR sAfLockFocusRegionColour;  /**< YUV colour for focus regions */
    OMX_YUVCOLOUR sAfFailPrimaryFaceColour;  /**< YUV colour for the primary face */
    OMX_YUVCOLOUR sAfFailOtherFaceColour;    /**< YUV colour for the all other faces */
    OMX_YUVCOLOUR sAfFailFocusRegionColour;  /**< YUV colour for focus regions */
 } OMX_CONFIG_DRAWBOXLINEPARAMS;
/*
Query / set the parameters for the box to be drawn around faces/focus regions.
*/

 #define OMX_PARAM_CAMERARMITYPE_RMINAME_LEN 16
 //OMX_IndexParamCameraRmiControl
 typedef struct OMX_PARAM_CAMERARMITYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_BOOL bEnabled;
    OMX_U8 sRmiName[OMX_PARAM_CAMERARMITYPE_RMINAME_LEN];
    OMX_U32 nInputBufferHeight;
    OMX_U32 nRmiBufferSize;
    OMX_BRCM_POOL_T *pImagePool;
 } OMX_PARAM_CAMERARMITYPE;

/* OMX_IndexConfigBrcmSyncOutput: Forcing a write sync */
typedef struct OMX_CONFIG_BRCMSYNCOUTPUTTYPE {
    OMX_U32 nSize;                           /**< Size of this structure, in Bytes */
    OMX_VERSIONTYPE nVersion;                /**< OMX specification version information */
}  OMX_CONFIG_BRCMSYNCOUTPUTTYPE;
/*
Setting this config forces a sync of data to the filesystem.
*/

/* OMX_IndexConfigDrmView: View information for DRM rental files */
typedef struct OMX_CONFIG_DRMVIEWTYPE {
   OMX_U32 nSize;             /**< Size of this structure, in Bytes */
   OMX_VERSIONTYPE nVersion;  /**< OMX specification version information */
   OMX_U32 nCurrentView;      /**< Current view count */
   OMX_U32 nMaxView;          /**< Max. no. of view allowed */
} OMX_CONFIG_DRMVIEWTYPE;
/*
This structure contains information about the number of available
views in the selected DRM rental file, which typically have a given
maximum view count.  It allows the user to explicitly agree to playing
the file, which will increment the number of current views the file
has had.
*/

typedef struct OMX_PARAM_BRCMU64TYPE {
    OMX_U32 nSize;                    /**< Size of this structure, in Bytes */
    OMX_VERSIONTYPE nVersion;         /**< OMX specification version information */
    OMX_U32 nPortIndex;               /**< port that this structure applies to */
    OMX_U32 nLowPart;                 /**< low bits of the unsigned 64 bit value */
    OMX_U32 nHighPart;                /**< high bits of the unsigned 64 bit value */
} OMX_PARAM_BRCMU64TYPE;

/* OMX_IndexParamBrcmDisableEXIF: Disable generation of EXIF data */
/*
This parameter is used by clients to control the generation of exif
data in JPEG images.
*/

/* OMX_IndexParamBrcmThumbnail: Control generation of thumbnail */
typedef struct OMX_PARAM_BRCMTHUMBNAILTYPE {
    OMX_U32 nSize;                    /**< Size of this structure, in Bytes */
    OMX_VERSIONTYPE nVersion;         /**< OMX specification version information */
    OMX_BOOL bEnable;                 /**< Enable generation of thumbnails during still capture */
    OMX_BOOL bUsePreview;             /**< Use the preview image (as is) as thumbnail */
    OMX_U32 nWidth;                   /**< Desired width of the thumbnail */
    OMX_U32 nHeight;                  /**< Desired height of the thumbnail */
} OMX_PARAM_BRCMTHUMBNAILTYPE;
/*
This parameter is used by clients to control how thumbnails are
generated when creating still images.

Thumbnail generation will be turned on or off depending on the
\code{bEnable} field.

The \code{bUsePreview} field will let the component know whether it
should use the low resolution preview image (if the component has one
available) as is for the thumbnail. When this is set to true, it should
make the generation of thumbnails faster (if a preview image is available)
and should use less memory as well.

The \code{nWidth} and \code{nHeight} fields allow the client to
specify the dimensions of the thumbnail.  If both \code{nWidth} and
\code{nHeight} are 0, we will calculate a sensible size for the
thumbnail.
*/

typedef struct OMX_PARAM_BRCMASPECTRATIOTYPE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 nWidth;
    OMX_U32 nHeight;
} OMX_PARAM_BRCMASPECTRATIOTYPE;

/* OMX_IndexParamBrcmVideoDecodeErrorConcealment: Control error concealment for video decode */
typedef struct OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_BOOL bStartWithValidFrame; /**< Decoder will only start emitting frames from a non-corrupted frame */
} OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE;
/*
 This parameter is used by clients to control the type of error concealment
 that will be done by the video decoder.
 */

#define OMX_CONFIG_FLASHINFOTYPE_NAME_LEN 16
typedef struct OMX_CONFIG_FLASHINFOTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U8 sFlashName[OMX_CONFIG_FLASHINFOTYPE_NAME_LEN];
   OMX_CAMERAFLASHTYPE eFlashType;
   OMX_U8 nDeviceId;
   OMX_U8 nDeviceVersion;
} OMX_CONFIG_FLASHINFOTYPE;

/* OMX_IndexParamBrcmInterpolateMissingTimestamps: Configure component to interpolate missing timestamps */
/*
Configures a component so that it tries to timestamp all the buffers it outputs.
If the timestamp information is missing from the original buffer, the
component will try its best to interpolate a value for the missing timestamp.
 */

/* OMX_IndexParamBrcmSetCodecPerformanceMonitoring: Configure component to output performance statistics */
/*
Configures a codec component so that it outputs performance statistics to
the given DECODE_PROGRESS_REPORT_T structure (passed as a pointer).
This structure can then be read by the client to find out where the codec is
at in its processing.
 */

/* OMX_IndexConfigDynamicRangeExpansion: Configure image dynamic range expansion processing */
typedef enum OMX_DYNAMICRANGEEXPANSIONMODETYPE {
   OMX_DynRangeExpOff,
   OMX_DynRangeExpLow,
   OMX_DynRangeExpMedium,
   OMX_DynRangeExpHigh,
   OMX_DynRangeExpKhronosExtensions = 0x6F000000,
   OMX_DynRangeExpVendorStartUnused = 0x7F000000,
   OMX_DynRangeExpMax = 0x7FFFFFFF
} OMX_DYNAMICRANGEEXPANSIONMODETYPE;

typedef struct OMX_CONFIG_DYNAMICRANGEEXPANSIONTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_DYNAMICRANGEEXPANSIONMODETYPE eMode;
} OMX_CONFIG_DYNAMICRANGEEXPANSIONTYPE;
/*
Configures the intensity of an image dynamic range expansion processing stage
*/

/* OMX_IndexParamBrcmTransposeBufferCount: Configure the number of pre-allocated transpose buffers  */
/*
This config allows the client to explicitly set the number of destination buffers pre-allocated for
ports that support 90/270 degree rotation (e.g. in video_render). The buffers will be pre-allocated during
a state transition from LOADED to IDLE (the transition will fail if there is not enough memory available
for the buffers).
.
*/


/* OMX_IndexParamBrcmThreadAffinity: Control the CPU affinity of component thread(s) */
typedef enum OMX_BRCMTHREADAFFINITYTYPE {
   OMX_BrcmThreadAffinityCPU0,
   OMX_BrcmThreadAffinityCPU1,
   OMX_BrcmThreadAffinityMax = 0x7FFFFFFF
} OMX_BRCMTHREADAFFINITYTYPE;

typedef struct OMX_PARAM_BRCMTHREADAFFINITYTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_BRCMTHREADAFFINITYTYPE eAffinity;  /**< Thread CPU affinity */
} OMX_PARAM_BRCMTHREADAFFINITYTYPE;
/*
 This parameter is used by clients to hint the CPU that a component thread should run on.
 */

 /* OMX_IndexConfigCommonSceneDetected: Reports the scene type detected by a scene detection algorithm. */
typedef enum OMX_SCENEDETECTTYPE {
   OMX_SceneDetectUnknown,
   OMX_SceneDetectLandscape,
   OMX_SceneDetectPortrait,
   OMX_SceneDetectMacro,
   OMX_SceneDetectNight,
   OMX_SceneDetectPortraitNight,
   OMX_SceneDetectBacklit,
   OMX_SceneDetectPortraitBacklit,
   OMX_SceneDetectSunset,
   OMX_SceneDetectBeach,
   OMX_SceneDetectSnow,
   OMX_SceneDetectFireworks,
   OMX_SceneDetectMax = 0x7FFFFFFF
} OMX_SCENEDETECTTYPE;

/* OMX_IndexConfigCommonSceneDetected: Reports the scene type detected by a scene detection algorithm. */
typedef struct OMX_CONFIG_SCENEDETECTTYPE {
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_SCENEDETECTTYPE eScene;  /**< Scene type detected */
} OMX_CONFIG_SCENEDETECTTYPE;
/*
 This config is used to report to clients the scene type that has been detected.
 */

/* OMX_IndexParamNalStreamFormat: Control the NAL unit packaging. This is a Khronos extension. */
typedef enum OMX_INDEXEXTTYPE {
    /* Video parameters and configurations */
    OMX_IndexExtVideoStartUnused = OMX_IndexKhronosExtensions + 0x00600000,
    OMX_IndexParamNalStreamFormatSupported,         /**< reference: OMX_NALSTREAMFORMATTYPE */
    OMX_IndexParamNalStreamFormat,                  /**< reference: OMX_NALSTREAMFORMATTYPE */
    OMX_IndexParamNalStreamFormatSelect,            /**< reference: OMX_NALSTREAMFORMATTYPE */

    OMX_IndexExtMax = 0x7FFFFFFF
} OMX_INDEXEXTTYPE;

/* OMX_IndexParamNalStreamFormat: Control the NAL unit packaging. This is a Khronos extension. */
typedef enum OMX_NALUFORMATSTYPE {
    OMX_NaluFormatStartCodes = 1,
    OMX_NaluFormatOneNaluPerBuffer = 2,
    OMX_NaluFormatOneByteInterleaveLength = 4,
    OMX_NaluFormatTwoByteInterleaveLength = 8,
    OMX_NaluFormatFourByteInterleaveLength = 16,
    OMX_NaluFormatCodingMax = 0x7FFFFFFF
} OMX_NALUFORMATSTYPE;

/* OMX_IndexParamNalStreamFormat: Control the NAL unit packaging. This is a Khronos extension. */
typedef struct OMX_NALSTREAMFORMATTYPE{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_NALUFORMATSTYPE eNaluFormat;
} OMX_NALSTREAMFORMATTYPE;
/*
 This parameter is used to control the NAL unit packaging of an H264 video port.
 */

/* OMX_IndexParamVideoMvc: MVC codec parameters */
typedef  struct OMX_VIDEO_PARAM_AVCTYPE  OMX_VIDEO_PARAM_MVCTYPE;
/*
This parameter is currently identical to the AVC parameter type.
*/

 /* OMX_IndexConfigBrcmDrawStaticBox: Define static box to be drawn */
typedef enum OMX_STATICBOXTYPE {
   OMX_StaticBoxNormal,
   OMX_StaticBoxPrimaryFaceAfIdle,
   OMX_StaticBoxNonPrimaryFaceAfIdle,
   OMX_StaticBoxFocusRegionAfIdle,
   OMX_StaticBoxPrimaryFaceAfSuccess,
   OMX_StaticBoxNonPrimaryFaceAfSuccess,
   OMX_StaticBoxFocusRegionAfSuccess,
   OMX_StaticBoxPrimaryFaceAfFail,
   OMX_StaticBoxNonPrimaryFaceAfFail,
   OMX_StaticBoxFocusRegionAfFail,
   OMX_StaticBoxMax
} OMX_STATICBOXTYPE;

typedef struct OMX_STATICBOX {
   OMX_U32 xLeft;
   OMX_U32 xTop;
   OMX_U32 xWidth;
   OMX_U32 xHeight;
   OMX_STATICBOXTYPE eType;
} OMX_STATICBOX;

typedef struct OMX_CONFIG_STATICBOXTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 nIndex;
   OMX_U32 nTotalBoxes;
   OMX_S32 nValidBoxes;
   OMX_BOOL bDrawOtherBoxes;
   OMX_STATICBOX sBoxes[1];
} OMX_CONFIG_STATICBOXTYPE;
/*
Query / set the parameters for a box to always be drawn on viewfinder images
The x/y/width/height values for the boxes are relative to the overall image.

\code{nIndex} - first box number being set/read, allowing retrieval/setting
of many boxes over several requests.

\code{nValidBoxes} - total number of boxes currently defined.

\code{nValidBoxes} - number of valid boxes in the \code{sBoxes} array.
When getting, the client sets this to the number of boxes available.
The component writes box data and updates this field with how many
boxes have been written to.
When setting, this is the number of boxes defined with this structure

\code{sBoxes} - variable length array of static boxes.
*/

/* OMX_IndexConfigPortCapturing: Per-port capturing state */
typedef struct OMX_CONFIG_PORTBOOLEANTYPE{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_BOOL bEnabled;
} OMX_CONFIG_PORTBOOLEANTYPE;
/*
This is proposed in IL533f for controlling
which ports of a multi-port camera component are capturing frames.
*/

/* OMX_IndexConfigCaptureMode: Capturing mode type */
typedef enum OMX_CAMERACAPTUREMODETYPE {
   OMX_CameraCaptureModeWaitForCaptureEnd,
   OMX_CameraCaptureModeWaitForCaptureEndAndUsePreviousInputImage,
   OMX_CameraCaptureModeResumeViewfinderImmediately,
   OMX_CameraCaptureModeMax,
} OMX_CAMERACAPTUREMODETYPE;

typedef struct OMX_PARAM_CAMERACAPTUREMODETYPE{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_CAMERACAPTUREMODETYPE eMode;
} OMX_PARAM_CAMERACAPTUREMODETYPE;
/*
This controls the mode of operation for
still image capture in the camera component.
*/

/* OMX_IndexParamBrcmDrmEncryption: Set DRM encryption scheme */
typedef enum OMX_BRCMDRMENCRYPTIONTYPE
{
   OMX_DrmEncryptionNone = 0,
   OMX_DrmEncryptionHdcp2,
   OMX_DrmEncryptionKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   OMX_DrmEncryptionVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   OMX_DrmEncryptionRangeMax = 0x7FFFFFFF
} OMX_BRCMDRMENCRYPTIONTYPE;

typedef struct OMX_PARAM_BRCMDRMENCRYPTIONTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BRCMDRMENCRYPTIONTYPE eEncryption;
   OMX_U32 nConfigDataLen;
   OMX_U8 configData[1];
} OMX_PARAM_BRCMDRMENCRYPTIONTYPE;
/*
Query/set the DRM encryption scheme used by a port writing out data.
*/


/* OMX_IndexConfigBufferStall: Advertise buffer stall state */
typedef struct OMX_CONFIG_BUFFERSTALLTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BOOL bStalled;      /**< Whether we are stalled */
   OMX_U32 nDelay;         /**< Delay in real time (us) from last buffer to current time */
} OMX_CONFIG_BUFFERSTALLTYPE;
/*
Query/set the buffer stall threashold.  When set the \code{nDelay}
parameter specifies a time to class whether buffer output is stalled.
When get, the \code{nDelay} parameter indicates the current buffer
delay, and the {bStalled} parameter indicates whether this time is
over a previously set threashold.  When
\code{OMX_IndexConfigRequestCallback} is used with this index, a
notification is given when \code{bStalled} changes.
*/

/* OMX_IndexConfigLatencyTarget: Maintain target latency by adjusting clock speed */
typedef struct OMX_CONFIG_LATENCYTARGETTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BOOL bEnabled; /**< whether this mode is enabled */
   OMX_U32 nFilter; /**< number of latency samples to filter on, good value: 1 */
   OMX_U32 nTarget; /**< target latency, us */
   OMX_U32 nShift;  /**< shift for storing latency values, good value: 7 */
   OMX_S32 nSpeedFactor; /**< multiplier for speed changes, in 24.8 format, good value: 256-512 */
   OMX_S32 nInterFactor; /**< divider for comparing latency versus gradiant, good value: 300 */
   OMX_S32 nAdjCap; /**< limit for speed change before nSpeedFactor is applied, good value: 100 */
} OMX_CONFIG_LATENCYTARGETTYPE;
/*
Query/set parameters used when adjusting clock speed to match the
measured latency to a specified value.
*/

/* OMX_IndexConfigBrcmUseProprietaryCallback: Force use of proprietary callback */
typedef struct OMX_CONFIG_BRCMUSEPROPRIETARYCALLBACKTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_BOOL bEnable;
} OMX_CONFIG_BRCMUSEPROPRIETARYCALLBACKTYPE;
/*
Disable/enable the use of proprietary callbacks rather than OpenMAX IL buffer handling.
*/

/* OMX_IndexParamCommonUseStcTimestamps: Select timestamp mode */
typedef enum OMX_TIMESTAMPMODETYPE
{
   OMX_TimestampModeZero = 0,       /**< Use a timestamp of 0 */
   OMX_TimestampModeRawStc,         /**< Use the raw STC as the timestamp */
   OMX_TimestampModeResetStc,       /**< Store the STC when video capture port goes active, and subtract that from STC for the timestamp */
   OMX_TimestampModeKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   OMX_TimestampModeVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   OMX_TimestampModeMax = 0x7FFFFFFF
} OMX_TIMESTAMPMODETYPE;

typedef struct OMX_PARAM_TIMESTAMPMODETYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_TIMESTAMPMODETYPE eTimestampMode;
} OMX_PARAM_TIMESTAMPMODETYPE;
/*
 Specifies what to use as timestamps in the abscence of a clock component.
*/

/* EGL image buffer for passing to video port.
 * Used when port color format is OMX_COLOR_FormatBRCMEGL.
 */
typedef struct OMX_BRCMVEGLIMAGETYPE
{
   /* Passed between ARM + VC; use fixed width types. */
   OMX_U32 nWidth;
   OMX_U32 nHeight;
   OMX_U32 nStride;
   OMX_U32 nUmemHandle;
   OMX_U32 nUmemOffset;
   OMX_U32 nFlipped;    /* Non-zero -> vertically flipped image */
} OMX_BRCMVEGLIMAGETYPE;

/* Provides field of view 
 */
typedef struct OMX_CONFIG_BRCMFOVTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 xFieldOfViewHorizontal;  /**< Horizontal field of view in degrees. 16p16 value */
   OMX_U32 xFieldOfViewVertical;    /**< Vertical field of view in degrees. 16p16 value */
} OMX_CONFIG_BRCMFOVTYPE;

/* OMX_IndexConfigBrcmDecoderPassThrough: Enabling Audio Passthrough */
/*
This allows an audio decoder to disable decoding the stream and pass through correctly framed
data to enable playback of compressed audio to supported output devices.
*/

/* OMX_IndexConfigBrcmClockReferenceSource: Select Clock Reference Source */
/*
This control allows communicating directly to an audio renderer component whether it should
act as a clock reference source or act as a slave.
*/

/* OMX_IndexConfigEncLevelExtension: AVC Override encode capabilities */
typedef struct OMX_VIDEO_CONFIG_LEVEL_EXTEND {
   OMX_U32 nSize; 
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_U32 nCustomMaxMBPS;     /**< Specifies maximum macro-blocks per second */
   OMX_U32 nCustomMaxFS;       /**< Specifies maximum frame size (macro-blocks per frame) */
   OMX_U32 nCustomMaxBRandCPB; /**< Specifies maximum bitrate in units of 1000 bits/s and Codec Picture Buffer (CPB derived from bitrate) */
} OMX_VIDEO_CONFIG_LEVEL_EXTEND;
/*
This allows finer control of the H264 encode internal parameters.
*/

/* OMX_IndexParamBrcmEEDEEnable: Enable/Disable end to end distortion estimator */
typedef struct OMX_VIDEO_EEDE_ENABLE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
    OMX_U32 enable;
} OMX_VIDEO_EEDE_ENABLE;
/*
This enables or disables the use of end to end distortion estimation.
*/

/* OMX_IndexParamBrcmEEDELossRate: Loss rate configuration for end to end distortion */
typedef struct OMX_VIDEO_EEDE_LOSSRATE {
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nPortIndex;
   OMX_U32 loss_rate; /**< loss rate, 5 means 5% */
} OMX_VIDEO_EEDE_LOSSRATE;
/*
Set the packet loss rate used by the end to end distortion estimator.
*/

/* OMX_IndexParamColorSpace: Colour space information */
typedef enum OMX_COLORSPACETYPE
{
   OMX_COLORSPACE_UNKNOWN,
   OMX_COLORSPACE_JPEG_JFIF,
   OMX_COLORSPACE_ITU_R_BT601,
   OMX_COLORSPACE_ITU_R_BT709,
   OMX_COLORSPACE_FCC,
   OMX_COLORSPACE_SMPTE240M,
   OMX_COLORSPACE_BT470_2_M,
   OMX_COLORSPACE_BT470_2_BG,
   OMX_COLORSPACE_JFIF_Y16_255,
   OMX_COLORSPACE_MAX = 0x7FFFFFFF
} OMX_COLORSPACETYPE;

typedef struct OMX_PARAM_COLORSPACETYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_COLORSPACETYPE eColorSpace;
} OMX_PARAM_COLORSPACETYPE;

typedef enum OMX_CAPTURESTATETYPE
{
   OMX_NotCapturing,
   OMX_CaptureStarted,
   OMX_CaptureComplete,
   OMX_CaptureMax = 0x7FFFFFFF
} OMX_CAPTURESTATETYPE;

typedef struct OMX_PARAM_CAPTURESTATETYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;
   OMX_U32 nPortIndex;
   OMX_CAPTURESTATETYPE eCaptureState;
} OMX_PARAM_CAPTURESTATETYPE;

/*
Provides information on the colour space that's in use during image/video processing.
*/

/* OMX_IndexConfigMinimiseFragmentation: Minimising Fragmentation */
/*
This control can be supported to enable the client to request that the component works
to minimise fragmentation of output buffers.
*/

/* OMX_IndexConfigBrcmBufferFlagFilter: Filters buffers based on flags */
/*
This control can be set to request that buffers are conditionally forwarded on 
output ports based on matching flags set on that buffer.
*/

/* OMX_IndexParamPortMaxFrameSize: Specifying maximum frame size */
/*
This control can be used to control the maximum frame size allowed on an output port.
*/

/* OMX_IndexConfigBrcmCameraRnDPreprocess: Enable use of development ISP software stage */
/*
This control can be used to enable a developmental software stage to be inserted into
the preprocessor stage of the ISP.
*/

/* OMX_IndexConfigBrcmCameraRnDPostprocess: Enable use of development ISP software stage */
/*
This control can be used to enable a developmental software stage to be inserted into
the postprocessor stage of the ISP.
*/

/* OMX_IndexParamDisableVllPool: Controlling use of memory for loadable modules */
/*
This control can be used to control whether loadable modules used a dedicated memory
pool or use heap allocated memory.
*/

typedef struct OMX_PARAM_BRCMCONFIGFILETYPE {
   OMX_U32 nSize;                      /**< size of the structure in bytes, including
                                            actual URI name */
   OMX_VERSIONTYPE nVersion;           /**< OMX specification version information */
   OMX_U32 fileSize;                   /**< Size of complete file data */
} OMX_PARAM_BRCMCONFIGFILETYPE;

typedef struct OMX_PARAM_BRCMCONFIGFILECHUNKTYPE {
   OMX_U32 nSize;                      /**< size of the structure in bytes, including
                                            actual chunk data */
   OMX_VERSIONTYPE nVersion;           /**< OMX specification version information */
   OMX_U32 size;                       /**< Number of bytes being transferred in this chunk */
   OMX_U32 offset;                     /**< Offset of this chunk in the file */
   OMX_U8 data[1];                     /**< Chunk data */
} OMX_PARAM_BRCMCONFIGFILECHUNKTYPE;

typedef struct OMX_PARAM_BRCMFRAMERATERANGETYPE {
   OMX_U32 nSize;                      /**< size of the structure in bytes, including
                                            actual chunk data */
   OMX_VERSIONTYPE nVersion;           /**< OMX specification version information */
   OMX_U32 nPortIndex;
   OMX_U32 xFramerateLow;              /**< Low end of framerate range. Q16 format */
   OMX_U32 xFramerateHigh;             /**< High end of framerate range. Q16 format */
} OMX_PARAM_BRCMFRAMERATERANGETYPE;

typedef struct OMX_PARAM_S32TYPE {
    OMX_U32 nSize;                    /**< Size of this structure, in Bytes */
    OMX_VERSIONTYPE nVersion;         /**< OMX specification version information */
    OMX_U32 nPortIndex;               /**< port that this structure applies to */
    OMX_S32 nS32;                     /**< S32 value */
} OMX_PARAM_S32TYPE;

typedef struct OMX_PARAM_BRCMVIDEODRMPROTECTBUFFERTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;

   OMX_U32 size_wanted;     /**< Input. Zero size means internal video decoder buffer,
                                 mem_handle and phys_addr not returned in this case */
   OMX_U32 protect;         /**< Input. 1 = protect, 0 = unprotect */

   OMX_U32 mem_handle;      /**< Output. Handle for protected buffer */
   OMX_PTR phys_addr;       /**< Output. Physical memory address of protected buffer */
} OMX_PARAM_BRCMVIDEODRMPROTECTBUFFERTYPE;

typedef struct OMX_CONFIG_ZEROSHUTTERLAGTYPE
{
   OMX_U32 nSize;
   OMX_VERSIONTYPE nVersion;

   OMX_U32 bZeroShutterMode;        /**< Select ZSL mode from the camera. */
   OMX_U32 bConcurrentCapture;      /**< Perform concurrent captures for full ZSL. */

} OMX_CONFIG_ZEROSHUTTERLAGTYPE;

typedef struct OMX_PARAM_BRCMVIDEODECODECONFIGVD3TYPE {
   OMX_U32 nSize;                      /**< size of the structure in bytes, including
                                            configuration data */
   OMX_VERSIONTYPE nVersion;           /**< OMX specification version information */
   OMX_U8 config[1];                   /**< Configuration data (a VD3_CONFIGURE_T) */
} OMX_PARAM_BRCMVIDEODECODECONFIGVD3TYPE;

#endif
/* File EOF */
