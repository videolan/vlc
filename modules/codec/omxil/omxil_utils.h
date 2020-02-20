/*****************************************************************************
 * omxil_utils.h: helper functions
 *****************************************************************************
 * Copyright (C) 2010 VLC authors and VideoLAN
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#include <vlc_es.h>

/*****************************************************************************
 * OMX macros
 *****************************************************************************/
#ifdef __ANDROID__
#define OMX_VERSION_MAJOR 1
#define OMX_VERSION_MINOR 0
#define OMX_VERSION_REV   0
#define OMX_VERSION_STEP  0
#elif defined(RPI_OMX)
#define OMX_VERSION_MAJOR 1
#define OMX_VERSION_MINOR 1
#define OMX_VERSION_REV   2
#define OMX_VERSION_STEP  0
#else
#define OMX_VERSION_MAJOR 1
#define OMX_VERSION_MINOR 1
#define OMX_VERSION_REV   1
#define OMX_VERSION_STEP  0
#endif

#define OMX_INIT_COMMON(a) \
  (a).nSize = sizeof(a); \
  (a).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
  (a).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
  (a).nVersion.s.nRevision = OMX_VERSION_REV; \
  (a).nVersion.s.nStep = OMX_VERSION_STEP

#define OMX_INIT_STRUCTURE(a) \
  memset(&(a), 0, sizeof(a)); \
  OMX_INIT_COMMON(a)

#define OMX_ComponentRoleEnum(hComponent, cRole, nIndex) \
    ((OMX_COMPONENTTYPE*)hComponent)->ComponentRoleEnum ? \
    ((OMX_COMPONENTTYPE*)hComponent)->ComponentRoleEnum(  \
        hComponent, cRole, nIndex ) : OMX_ErrorNotImplemented

#define CHECK_ERROR(a, ...) \
    if(a != OMX_ErrorNone) {msg_Dbg( p_dec, __VA_ARGS__ ); goto error;}

#ifdef OMX_SKIP64BIT
static inline int64_t FromOmxTicks(OMX_TICKS value)
{
    return (((int64_t)value.nHighPart) << 32) | value.nLowPart;
}
static inline OMX_TICKS ToOmxTicks(int64_t value)
{
    OMX_TICKS s;
    s.nLowPart = value;
    s.nHighPart = value >> 32;
    return s;
}
#else
#define FromOmxTicks(x) (x)
#define ToOmxTicks(x) (x)
#endif

/*****************************************************************************
 * OMX buffer FIFO macros
 *****************************************************************************/
#define OMX_FIFO_INIT(p_fifo, next) \
    do { vlc_mutex_init( &(p_fifo)->lock ); \
         vlc_cond_init( &(p_fifo)->wait ); \
         (p_fifo)->offset = offsetof(OMX_BUFFERHEADERTYPE, next) / sizeof(void *); \
         (p_fifo)->pp_last = &(p_fifo)->p_first; } while(0)

#define OMX_FIFO_DESTROY(p_fifo) \
    while(0) { }

#define OMX_FIFO_PEEK(p_fifo, p_buffer) \
         p_buffer = (p_fifo)->p_first;

#define OMX_FIFO_GET(p_fifo, p_buffer) \
    do { vlc_mutex_lock( &(p_fifo)->lock ); \
         while( !(p_fifo)->p_first ) \
             vlc_cond_wait( &(p_fifo)->wait, &(p_fifo)->lock ); \
         p_buffer = (p_fifo)->p_first; \
         OMX_BUFFERHEADERTYPE **pp_next = (OMX_BUFFERHEADERTYPE **) \
             ((void **)p_buffer + (p_fifo)->offset); \
         (p_fifo)->p_first = *pp_next; *pp_next = 0; \
         if( !(p_fifo)->p_first ) (p_fifo)->pp_last = &(p_fifo)->p_first; \
         vlc_mutex_unlock( &(p_fifo)->lock ); } while(0)

#define OMX_FIFO_GET_TIMEOUT(p_fifo, p_buffer, timeout) \
    do { vlc_mutex_lock( &(p_fifo)->lock ); \
         vlc_tick_t end = vlc_tick_now() + timeout; \
         if( !(p_fifo)->p_first ) \
             vlc_cond_timedwait( &(p_fifo)->wait, &(p_fifo)->lock, end ); \
         p_buffer = (p_fifo)->p_first; \
         if( p_buffer ) { \
             OMX_BUFFERHEADERTYPE **pp_next = (OMX_BUFFERHEADERTYPE **) \
                 ((void **)p_buffer + (p_fifo)->offset); \
             (p_fifo)->p_first = *pp_next; *pp_next = 0; \
             if( !(p_fifo)->p_first ) (p_fifo)->pp_last = &(p_fifo)->p_first; \
         } \
         vlc_mutex_unlock( &(p_fifo)->lock ); } while(0)

#define OMX_FIFO_PUT(p_fifo, p_buffer) \
    do { vlc_mutex_lock (&(p_fifo)->lock);              \
         OMX_BUFFERHEADERTYPE **pp_next = (OMX_BUFFERHEADERTYPE **) \
             ((void **)p_buffer + (p_fifo)->offset); \
         *(p_fifo)->pp_last = p_buffer; \
         (p_fifo)->pp_last = pp_next; *pp_next = 0; \
         vlc_cond_signal( &(p_fifo)->wait ); \
         vlc_mutex_unlock( &(p_fifo)->lock ); } while(0)

/*****************************************************************************
 * OMX format parameters
 *****************************************************************************/
typedef union {
    OMX_PARAM_U32TYPE common;
    OMX_AUDIO_PARAM_PCMMODETYPE pcm;
    OMX_AUDIO_PARAM_MP3TYPE mp3;
    OMX_AUDIO_PARAM_AACPROFILETYPE aac;
    OMX_AUDIO_PARAM_VORBISTYPE vorbis;
    OMX_AUDIO_PARAM_WMATYPE wma;
    OMX_AUDIO_PARAM_RATYPE ra;
    OMX_AUDIO_PARAM_ADPCMTYPE adpcm;
    OMX_AUDIO_PARAM_G723TYPE g723;
    OMX_AUDIO_PARAM_G726TYPE g726;
    OMX_AUDIO_PARAM_G729TYPE g729;
    OMX_AUDIO_PARAM_AMRTYPE amr;

    OMX_VIDEO_PARAM_H263TYPE h263;
    OMX_VIDEO_PARAM_MPEG2TYPE mpeg2;
    OMX_VIDEO_PARAM_MPEG4TYPE mpeg4;
    OMX_VIDEO_PARAM_WMVTYPE wmv;
    OMX_VIDEO_PARAM_RVTYPE rv;
    OMX_VIDEO_PARAM_AVCTYPE avc;

} OmxFormatParam;

/*****************************************************************************
 * Events utility functions
 *****************************************************************************/
typedef struct OmxEvent
{
    OMX_EVENTTYPE event;
    OMX_U32 data_1;
    OMX_U32 data_2;
    OMX_PTR event_data;

    struct OmxEvent *next;
} OmxEvent;

typedef struct OmxEventQueue
{
    OmxEvent *p_events;
    OmxEvent **pp_last_event;

    vlc_mutex_t mutex;
    vlc_cond_t cond;
} OmxEventQueue;

void InitOmxEventQueue(OmxEventQueue *queue);
void DeinitOmxEventQueue(OmxEventQueue *queue);
OMX_ERRORTYPE PostOmxEvent(OmxEventQueue *queue, OMX_EVENTTYPE event,
    OMX_U32 data_1, OMX_U32 data_2, OMX_PTR event_data);
OMX_ERRORTYPE WaitForOmxEvent(OmxEventQueue *queue, OMX_EVENTTYPE *event,
    OMX_U32 *data_1, OMX_U32 *data_2, OMX_PTR *event_data);
OMX_ERRORTYPE WaitForSpecificOmxEvent(OmxEventQueue *queue,
    OMX_EVENTTYPE specific_event, OMX_U32 *data_1, OMX_U32 *data_2,
    OMX_PTR *event_data);
void PrintOmxEvent(vlc_object_t *p_this, OMX_EVENTTYPE event, OMX_U32 data_1,
    OMX_U32 data_2, OMX_PTR event_data);

/*****************************************************************************
 * Picture utility functions
 *****************************************************************************/
typedef struct ArchitectureSpecificCopyData
{
    void *data;
} ArchitectureSpecificCopyData;

void ArchitectureSpecificCopyHooks( decoder_t *p_dec, int i_color_format,
                                    int i_slice_height, int i_src_stride,
                                    ArchitectureSpecificCopyData *p_architecture_specific );

void ArchitectureSpecificCopyHooksDestroy( int i_color_format,
                                           ArchitectureSpecificCopyData *p_architecture_specific );

void CopyOmxPicture( int i_color_format, picture_t *p_pic,
                     int i_slice_height,
                     int i_src_stride, uint8_t *p_src, int i_chroma_div,
                     ArchitectureSpecificCopyData *p_architecture_specific );

void CopyVlcPicture( decoder_t *, OMX_BUFFERHEADERTYPE *, picture_t * );

/*****************************************************************************
 * Logging utility functions
 *****************************************************************************/
const char *StateToString(OMX_STATETYPE state);
const char *CommandToString(OMX_COMMANDTYPE command);
const char *EventToString(OMX_EVENTTYPE event);
const char *ErrorToString(OMX_ERRORTYPE error);

void PrintOmx(decoder_t *p_dec, OMX_HANDLETYPE omx_handle, OMX_U32 i_port);

/*****************************************************************************
 * Utility functions
 *****************************************************************************/
bool OMXCodec_IsBlacklisted( const char *p_name, unsigned int i_name_len );

enum {
    OMXCODEC_NO_QUIRKS = 0,
    OMXCODEC_QUIRKS_NEED_CSD = 0x1,
    OMXCODEC_VIDEO_QUIRKS_IGNORE_PADDING = 0x2,
    OMXCODEC_VIDEO_QUIRKS_SUPPORT_INTERLACED = 0x4,
    OMXCODEC_AUDIO_QUIRKS_NEED_CHANNELS = 0x8,
};
int OMXCodec_GetQuirks( enum es_format_category_e i_cat, vlc_fourcc_t i_codec,
                        const char *p_name, unsigned int i_name_len );

/*****************************************************************************
 * fourcc -> omx id mapping
 *****************************************************************************/
int GetOmxVideoFormat( vlc_fourcc_t i_fourcc,
                       OMX_VIDEO_CODINGTYPE *pi_omx_codec,
                       const char **ppsz_name );
int GetVlcVideoFormat( OMX_VIDEO_CODINGTYPE i_omx_codec,
                       vlc_fourcc_t *pi_fourcc, const char **ppsz_name );
int GetOmxAudioFormat( vlc_fourcc_t i_fourcc,
                       OMX_AUDIO_CODINGTYPE *pi_omx_codec,
                       const char **ppsz_name );
int OmxToVlcAudioFormat( OMX_AUDIO_CODINGTYPE i_omx_codec,
                       vlc_fourcc_t *pi_fourcc, const char **ppsz_name );
const char *GetOmxRole( vlc_fourcc_t i_fourcc, enum es_format_category_e i_cat,
                        bool b_enc );
int GetOmxChromaFormat( vlc_fourcc_t i_fourcc,
                        OMX_COLOR_FORMATTYPE *pi_omx_codec,
                        const char **ppsz_name );
int GetVlcChromaFormat( OMX_COLOR_FORMATTYPE i_omx_codec,
                        vlc_fourcc_t *pi_fourcc, const char **ppsz_name );
int GetVlcChromaSizes( vlc_fourcc_t i_fourcc,
                       unsigned int width, unsigned int height,
                       unsigned int *size, unsigned int *pitch,
                       unsigned int *chroma_pitch_div );

/*****************************************************************************
 * Functions to deal with audio format parameters
 *****************************************************************************/
OMX_ERRORTYPE SetAudioParameters(OMX_HANDLETYPE handle,
    OmxFormatParam *param, OMX_U32 i_port, OMX_AUDIO_CODINGTYPE encoding,
    vlc_fourcc_t i_codec, uint8_t i_channels, unsigned int i_samplerate,
    unsigned int i_bitrate, unsigned int i_bps, unsigned int i_blocksize);
OMX_ERRORTYPE GetAudioParameters(OMX_HANDLETYPE handle,
    OmxFormatParam *param, OMX_U32 i_port, OMX_AUDIO_CODINGTYPE encoding,
    uint8_t *pi_channels, unsigned int *pi_samplerate,
    unsigned int *pi_bitrate, unsigned int *pi_bps, unsigned int *pi_blocksize);
unsigned int GetAudioParamSize(OMX_INDEXTYPE index);

/*****************************************************************************
 * Vendor specific color formats
 *****************************************************************************/
#define OMX_QCOM_COLOR_FormatYVU420SemiPlanar 0x7FA30C00
#define OMX_TI_COLOR_FormatYUV420PackedSemiPlanar 0x7F000100
#define QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka 0x7FA30C03
#define OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m 0x7FA30C04
#define OMX_IndexVendorSetYUV420pMode 0x7f000003

/*****************************************************************************
 * H264 specific code
 *****************************************************************************/
size_t convert_omx_to_profile_idc(OMX_VIDEO_AVCPROFILETYPE profile_type);

size_t convert_omx_to_level_idc(OMX_VIDEO_AVCLEVELTYPE level_type);
