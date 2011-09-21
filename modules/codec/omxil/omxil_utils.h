/*****************************************************************************
 * omxil_utils.h: helper functions
 *****************************************************************************
 * Copyright (C) 2010 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
 * OMX macros
 *****************************************************************************/
#define OMX_INIT_COMMON(a) \
  (a).nSize = sizeof(a); \
  (a).nVersion.s.nVersionMajor = 1; \
  (a).nVersion.s.nVersionMinor = 1; \
  (a).nVersion.s.nRevision = 1; \
  (a).nVersion.s.nStep = 0

#define OMX_INIT_STRUCTURE(a) \
  memset(&(a), 0, sizeof(a)); \
  OMX_INIT_COMMON(a)

#define OMX_ComponentRoleEnum(hComponent, cRole, nIndex) \
    ((OMX_COMPONENTTYPE*)hComponent)->ComponentRoleEnum ? \
    ((OMX_COMPONENTTYPE*)hComponent)->ComponentRoleEnum(  \
        hComponent, cRole, nIndex ) : OMX_ErrorNotImplemented

#define CHECK_ERROR(a, ...) \
    if(a != OMX_ErrorNone) {msg_Dbg( p_dec, __VA_ARGS__ ); goto error;}

/*****************************************************************************
 * OMX buffer FIFO macros
 *****************************************************************************/
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

OMX_ERRORTYPE PostOmxEvent(decoder_t *p_dec, OMX_EVENTTYPE event,
    OMX_U32 data_1, OMX_U32 data_2, OMX_PTR event_data);
OMX_ERRORTYPE WaitForOmxEvent(decoder_t *p_dec, OMX_EVENTTYPE *event,
    OMX_U32 *data_1, OMX_U32 *data_2, OMX_PTR *event_data);
OMX_ERRORTYPE WaitForSpecificOmxEvent(decoder_t *p_dec,
    OMX_EVENTTYPE specific_event, OMX_U32 *data_1, OMX_U32 *data_2,
    OMX_PTR *event_data);

/*****************************************************************************
 * Picture utility functions
 *****************************************************************************/
void CopyOmxPicture( decoder_t *, picture_t *, OMX_BUFFERHEADERTYPE *, int );
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
int GetVlcAudioFormat( OMX_AUDIO_CODINGTYPE i_omx_codec,
                       vlc_fourcc_t *pi_fourcc, const char **ppsz_name );
const char *GetOmxRole( vlc_fourcc_t i_fourcc, int i_cat, bool b_enc );
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
    uint8_t i_channels, unsigned int i_samplerate, unsigned int i_bitrate,
    unsigned int i_bps, unsigned int i_blocksize);
OMX_ERRORTYPE GetAudioParameters(OMX_HANDLETYPE handle,
    OmxFormatParam *param, OMX_U32 i_port, OMX_AUDIO_CODINGTYPE encoding,
    uint8_t *pi_channels, unsigned int *pi_samplerate,
    unsigned int *pi_bitrate, unsigned int *pi_bps, unsigned int *pi_blocksize);
unsigned int GetAudioParamSize(OMX_INDEXTYPE index);
