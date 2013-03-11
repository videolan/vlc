/*****************************************************************************
 * utils.c: helper functions
 *****************************************************************************
 * Copyright (C) 2010 VLC authors and VideoLAN
 * $Id$
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block_helper.h>
#include <vlc_cpu.h>

#include "omxil.h"
#include "qcom.h"

/*****************************************************************************
 * Events utility functions
 *****************************************************************************/
void InitOmxEventQueue(OmxEventQueue *queue)
{
    queue->pp_last_event = &queue->p_events;
    vlc_mutex_init(&queue->mutex);
    vlc_cond_init(&queue->cond);
}

void DeinitOmxEventQueue(OmxEventQueue *queue)
{
    vlc_mutex_destroy(&queue->mutex);
    vlc_cond_destroy(&queue->cond);
}

OMX_ERRORTYPE PostOmxEvent(OmxEventQueue *queue, OMX_EVENTTYPE event,
    OMX_U32 data_1, OMX_U32 data_2, OMX_PTR event_data)
{
    OmxEvent *p_event;

    p_event = malloc(sizeof(OmxEvent));
    if(!p_event) return OMX_ErrorInsufficientResources;

    p_event->event = event;
    p_event->data_1 = data_1;
    p_event->data_2 = data_2;
    p_event->event_data = event_data;
    p_event->next = 0;

    vlc_mutex_lock(&queue->mutex);
    *queue->pp_last_event = p_event;
    queue->pp_last_event = &p_event->next;
    vlc_cond_signal(&queue->cond);
    vlc_mutex_unlock(&queue->mutex);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE WaitForOmxEvent(OmxEventQueue *queue, OMX_EVENTTYPE *event,
    OMX_U32 *data_1, OMX_U32 *data_2, OMX_PTR *event_data)
{
    OmxEvent *p_event;

    vlc_mutex_lock(&queue->mutex);

    if(!queue->p_events)
        vlc_cond_timedwait(&queue->cond, &queue->mutex, mdate()+CLOCK_FREQ);

    p_event = queue->p_events;
    if(p_event)
    {
        queue->p_events = p_event->next;
        if(!queue->p_events) queue->pp_last_event = &queue->p_events;
    }

    vlc_mutex_unlock(&queue->mutex);

    if(p_event)
    {
        if(event) *event = p_event->event;
        if(data_1) *data_1 = p_event->data_1;
        if(data_2) *data_2 = p_event->data_2;
        if(event_data) *event_data = p_event->event_data;
        free(p_event);
        return OMX_ErrorNone;
    }

    return OMX_ErrorTimeout;
}

OMX_ERRORTYPE WaitForSpecificOmxEvent(OmxEventQueue *queue,
    OMX_EVENTTYPE specific_event, OMX_U32 *data_1, OMX_U32 *data_2,
    OMX_PTR *event_data)
{
    OMX_ERRORTYPE status;
    OMX_EVENTTYPE event;
    mtime_t before =  mdate();

    while(1)
    {
        status = WaitForOmxEvent(queue, &event, data_1, data_2, event_data);
        if(status != OMX_ErrorNone) return status;

        if(event == specific_event) break;
        if(mdate() - before > CLOCK_FREQ) return OMX_ErrorTimeout;
    }

    return OMX_ErrorNone;
}

void PrintOmxEvent(vlc_object_t *p_this, OMX_EVENTTYPE event, OMX_U32 data_1,
    OMX_U32 data_2, OMX_PTR event_data)
{
    switch (event)
    {
    case OMX_EventCmdComplete:
        switch ((OMX_STATETYPE)data_1)
        {
        case OMX_CommandStateSet:
            msg_Dbg( p_this, "OmxEventHandler (%s, %s, %s)", EventToString(event),
                     CommandToString(data_1), StateToString(data_2) );
            break;

        default:
            msg_Dbg( p_this, "OmxEventHandler (%s, %s, %u)", EventToString(event),
                     CommandToString(data_1), (unsigned int)data_2 );
            break;
        }
        break;

    case OMX_EventError:
        msg_Dbg( p_this, "OmxEventHandler (%s, %s, %u, %s)", EventToString(event),
                 ErrorToString((OMX_ERRORTYPE)data_1), (unsigned int)data_2,
                 (const char *)event_data);
        break;

    default:
        msg_Dbg( p_this, "OmxEventHandler (%s, %u, %u)", EventToString(event),
                 (unsigned int)data_1, (unsigned int)data_2 );
        break;
    }
}

/*****************************************************************************
 * Picture utility functions
 *****************************************************************************/
void CopyOmxPicture( int i_color_format, picture_t *p_pic,
                     int i_slice_height,
                     int i_src_stride, uint8_t *p_src, int i_chroma_div )
{
    uint8_t *p_dst;
    int i_dst_stride;
    int i_plane, i_width, i_line;
    if( i_color_format == QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka )
    {
        qcom_convert(p_src, p_pic);
        return;
    }

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        if(i_plane == 1) i_src_stride /= i_chroma_div;
        p_dst = p_pic->p[i_plane].p_pixels;
        i_dst_stride = p_pic->p[i_plane].i_pitch;
        i_width = p_pic->p[i_plane].i_visible_pitch;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines; i_line++ )
        {
            memcpy( p_dst, p_src, i_width );
            p_src += i_src_stride;
            p_dst += i_dst_stride;
        }
        /* Handle plane height, which may be indicated via nSliceHeight in OMX.
         * The handling for chroma planes currently assumes vertically
         * subsampled chroma, e.g. 422 planar wouldn't work right. */
        if( i_plane == 0 && i_slice_height > p_pic->p[i_plane].i_visible_lines )
            p_src += i_src_stride * (i_slice_height - p_pic->p[i_plane].i_visible_lines);
        else if ( i_plane > 0 && i_slice_height/2 > p_pic->p[i_plane].i_visible_lines )
            p_src += i_src_stride * (i_slice_height/2 - p_pic->p[i_plane].i_visible_lines);
    }
}

void CopyVlcPicture( decoder_t *p_dec, OMX_BUFFERHEADERTYPE *p_header,
                     picture_t *p_pic)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_src_stride, i_dst_stride;
    int i_plane, i_width, i_line;
    uint8_t *p_dst, *p_src;

    i_dst_stride  = p_sys->out.i_frame_stride;
    p_dst = p_header->pBuffer + p_header->nOffset;

    for( i_plane = 0; i_plane < p_pic->i_planes; i_plane++ )
    {
        if(i_plane == 1) i_dst_stride /= p_sys->in.i_frame_stride_chroma_div;
        p_src = p_pic->p[i_plane].p_pixels;
        i_src_stride = p_pic->p[i_plane].i_pitch;
        i_width = p_pic->p[i_plane].i_visible_pitch;

        for( i_line = 0; i_line < p_pic->p[i_plane].i_visible_lines; i_line++ )
        {
            memcpy( p_dst, p_src, i_width );
            p_src += i_src_stride;
            p_dst += i_dst_stride;
        }
    }
}

int IgnoreOmxDecoderPadding(const char *name)
{
    // The list of decoders that signal padding properly is not necessary,
    // since that is the default, but keep it here for reference. (This is
    // only relevant for manufacturers that are known to have decoders with
    // this kind of bug.)
/*
    static const char *padding_decoders[] = {
        "OMX.SEC.AVC.Decoder",
        "OMX.SEC.wmv7.dec",
        "OMX.SEC.wmv8.dec",
        NULL
    };
*/
    static const char *nopadding_decoders[] = {
        "OMX.SEC.avc.dec",
        "OMX.SEC.avcdec",
        "OMX.SEC.MPEG4.Decoder",
        "OMX.SEC.mpeg4.dec",
        "OMX.SEC.vc1.dec",
        NULL
    };
    for (const char **ptr = nopadding_decoders; *ptr; ptr++) {
        if (!strcmp(*ptr, name))
            return 1;
    }
    return 0;
}

/*****************************************************************************
 * Logging utility functions
 *****************************************************************************/
const char *StateToString(OMX_STATETYPE state)
{
    static const char *psz_names[] = {
        "OMX_StateInvalid", "OMX_StateLoaded", "OMX_StateIdle",
        "OMX_StateExecuting", "OMX_StatePause", "OMX_StateWaitForResources",
        "OMX_State unknown"
    };

    if((unsigned int)state > sizeof(psz_names)/sizeof(char*)-1)
        state = (OMX_STATETYPE)(sizeof(psz_names)/sizeof(char*)-1);
    return psz_names[state];
}

const char *CommandToString(OMX_COMMANDTYPE command)
{
    static const char *psz_names[] = {
        "OMX_CommandStateSet", "OMX_CommandFlush", "OMX_CommandPortDisable",
        "OMX_CommandPortEnable", "OMX_CommandMarkBuffer",
        "OMX_Command unknown"
    };

    if((unsigned int)command > sizeof(psz_names)/sizeof(char*)-1)
        command = (OMX_COMMANDTYPE)(sizeof(psz_names)/sizeof(char*)-1);
    return psz_names[command];
}

const char *EventToString(OMX_EVENTTYPE event)
{
    static const char *psz_names[] = {
        "OMX_EventCmdComplete", "OMX_EventError", "OMX_EventMark",
        "OMX_EventPortSettingsChanged", "OMX_EventBufferFlag",
        "OMX_EventResourcesAcquired", "OMX_EventComponentResumed",
        "OMX_EventDynamicResourcesAvailable", "OMX_EventPortFormatDetected",
        "OMX_Event unknown"
    };

    if((unsigned int)event > sizeof(psz_names)/sizeof(char*)-1)
        event = (OMX_EVENTTYPE)(sizeof(psz_names)/sizeof(char*)-1);
    return psz_names[event];
}

const char *ErrorToString(OMX_ERRORTYPE error)
{
    static const char *psz_names[] = {
        "OMX_ErrorInsufficientResources", "OMX_ErrorUndefined",
        "OMX_ErrorInvalidComponentName", "OMX_ErrorComponentNotFound",
        "OMX_ErrorInvalidComponent", "OMX_ErrorBadParameter",
        "OMX_ErrorNotImplemented", "OMX_ErrorUnderflow",
        "OMX_ErrorOverflow", "OMX_ErrorHardware", "OMX_ErrorInvalidState",
        "OMX_ErrorStreamCorrupt", "OMX_ErrorPortsNotCompatible",
        "OMX_ErrorResourcesLost", "OMX_ErrorNoMore", "OMX_ErrorVersionMismatch",
        "OMX_ErrorNotReady", "OMX_ErrorTimeout", "OMX_ErrorSameState",
        "OMX_ErrorResourcesPreempted", "OMX_ErrorPortUnresponsiveDuringAllocation",
        "OMX_ErrorPortUnresponsiveDuringDeallocation",
        "OMX_ErrorPortUnresponsiveDuringStop", "OMX_ErrorIncorrectStateTransition",
        "OMX_ErrorIncorrectStateOperation", "OMX_ErrorUnsupportedSetting",
        "OMX_ErrorUnsupportedIndex", "OMX_ErrorBadPortIndex",
        "OMX_ErrorPortUnpopulated", "OMX_ErrorComponentSuspended",
        "OMX_ErrorDynamicResourcesUnavailable", "OMX_ErrorMbErrorsInFrame",
        "OMX_ErrorFormatNotDetected", "OMX_ErrorContentPipeOpenFailed",
        "OMX_ErrorContentPipeCreationFailed", "OMX_ErrorSeperateTablesUsed",
        "OMX_ErrorTunnelingUnsupported",
        "OMX_Error unknown"
    };

    if(error == OMX_ErrorNone) return "OMX_ErrorNone";

    error -= OMX_ErrorInsufficientResources;

    if((unsigned int)error > sizeof(psz_names)/sizeof(char*)-1)
        error = (OMX_STATETYPE)(sizeof(psz_names)/sizeof(char*)-1);
    return psz_names[error];
}

/*****************************************************************************
 * fourcc -> omx id mapping
 *****************************************************************************/
static const struct
{
    vlc_fourcc_t i_fourcc;
    OMX_VIDEO_CODINGTYPE i_codec;
    const char *psz_role;

} video_format_table[] =
{
    { VLC_CODEC_MPGV, OMX_VIDEO_CodingMPEG2, "video_decoder.mpeg2" },
    { VLC_CODEC_MP4V, OMX_VIDEO_CodingMPEG4, "video_decoder.mpeg4" },
    { VLC_CODEC_H264, OMX_VIDEO_CodingAVC,   "video_decoder.avc"   },
    { VLC_CODEC_H263, OMX_VIDEO_CodingH263,  "video_decoder.h263"  },
    { VLC_CODEC_WMV1, OMX_VIDEO_CodingWMV,   "video_decoder.wmv1"  },
    { VLC_CODEC_WMV2, OMX_VIDEO_CodingWMV,   "video_decoder.wmv2"  },
    { VLC_CODEC_WMV3, OMX_VIDEO_CodingWMV,   "video_decoder.wmv"   },
    { VLC_CODEC_VC1,  OMX_VIDEO_CodingWMV,   "video_decoder.wmv"   },
    { VLC_CODEC_MJPG, OMX_VIDEO_CodingMJPEG, "video_decoder.jpeg"  },
    { VLC_CODEC_MJPG, OMX_VIDEO_CodingMJPEG, "video_decoder.mjpeg" },
    { VLC_CODEC_RV10, OMX_VIDEO_CodingRV,    "video_decoder.rv"    },
    { VLC_CODEC_RV20, OMX_VIDEO_CodingRV,    "video_decoder.rv"    },
    { VLC_CODEC_RV30, OMX_VIDEO_CodingRV,    "video_decoder.rv"    },
    { VLC_CODEC_RV40, OMX_VIDEO_CodingRV,    "video_decoder.rv"    },
    { 0, 0, 0 }
};

static const struct
{
    vlc_fourcc_t i_fourcc;
    OMX_AUDIO_CODINGTYPE i_codec;
    const char *psz_role;

} audio_format_table[] =
{
    { VLC_CODEC_AMR_NB, OMX_AUDIO_CodingAMR, "audio_decoder.amrnb" },
    { VLC_CODEC_AMR_WB, OMX_AUDIO_CodingAMR, "audio_decoder.amrwb" },
    { VLC_CODEC_MP4A,   OMX_AUDIO_CodingAAC, "audio_decoder.aac" },
    { VLC_CODEC_S16N,   OMX_AUDIO_CodingPCM, "audio_decoder.pcm" },
    { VLC_CODEC_MP3,    OMX_AUDIO_CodingMP3, "audio_decoder.mp3" },
    { 0, 0, 0 }
};

static const struct
{
    vlc_fourcc_t i_fourcc;
    OMX_VIDEO_CODINGTYPE i_codec;
    const char *psz_role;

} video_enc_format_table[] =
{
    { VLC_CODEC_MPGV, OMX_VIDEO_CodingMPEG2, "video_encoder.mpeg2" },
    { VLC_CODEC_MP4V, OMX_VIDEO_CodingMPEG4, "video_encoder.mpeg4" },
    { VLC_CODEC_H264, OMX_VIDEO_CodingAVC,   "video_encoder.avc"   },
    { VLC_CODEC_H263, OMX_VIDEO_CodingH263,  "video_encoder.h263"  },
    { VLC_CODEC_WMV1, OMX_VIDEO_CodingWMV,   "video_encoder.wmv"   },
    { VLC_CODEC_WMV2, OMX_VIDEO_CodingWMV,   "video_encoder.wmv"   },
    { VLC_CODEC_WMV3, OMX_VIDEO_CodingWMV,   "video_encoder.wmv"   },
    { VLC_CODEC_MJPG, OMX_VIDEO_CodingMJPEG, "video_encoder.jpeg"  },
    { VLC_CODEC_RV10, OMX_VIDEO_CodingRV,    "video_encoder.rv"    },
    { VLC_CODEC_RV20, OMX_VIDEO_CodingRV,    "video_encoder.rv"    },
    { VLC_CODEC_RV30, OMX_VIDEO_CodingRV,    "video_encoder.rv"    },
    { VLC_CODEC_RV40, OMX_VIDEO_CodingRV,    "video_encoder.rv"    },
    { 0, 0, 0 }
};

static const struct
{
    vlc_fourcc_t i_fourcc;
    OMX_AUDIO_CODINGTYPE i_codec;
    const char *psz_role;

} audio_enc_format_table[] =
{
    { VLC_CODEC_AMR_NB, OMX_AUDIO_CodingAMR, "audio_encoder.amrnb" },
    { VLC_CODEC_AMR_WB, OMX_AUDIO_CodingAMR, "audio_encoder.amrwb" },
    { VLC_CODEC_MP4A,   OMX_AUDIO_CodingAAC, "audio_encoder.aac" },
    { VLC_CODEC_S16N,   OMX_AUDIO_CodingPCM, "audio_encoder.pcm" },
    { 0, 0, 0 }
};

static const struct
{
    vlc_fourcc_t i_fourcc;
    OMX_COLOR_FORMATTYPE i_codec;
    unsigned int i_size_mul;
    unsigned int i_line_mul;
    unsigned int i_line_chroma_div;

} chroma_format_table[] =
{
    { VLC_CODEC_I420, OMX_COLOR_FormatYUV420Planar, 3, 1, 2 },
    { VLC_CODEC_I420, OMX_COLOR_FormatYUV420PackedPlanar, 3, 1, 2 },
    { VLC_CODEC_NV12, OMX_COLOR_FormatYUV420SemiPlanar, 3, 1, 1 },
    { VLC_CODEC_NV21, OMX_QCOM_COLOR_FormatYVU420SemiPlanar, 3, 1, 1 },
    { VLC_CODEC_NV12, OMX_TI_COLOR_FormatYUV420PackedSemiPlanar, 3, 1, 2 },
    { VLC_CODEC_NV12, QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka, 3, 1, 1 },
    { VLC_CODEC_YUYV, OMX_COLOR_FormatYCbYCr, 4, 2, 0 },
    { VLC_CODEC_YVYU, OMX_COLOR_FormatYCrYCb, 4, 2, 0 },
    { VLC_CODEC_UYVY, OMX_COLOR_FormatCbYCrY, 4, 2, 0 },
    { VLC_CODEC_VYUY, OMX_COLOR_FormatCrYCbY, 4, 2, 0 },
    { 0, 0, 0, 0, 0 }
};

int GetOmxVideoFormat( vlc_fourcc_t i_fourcc,
                       OMX_VIDEO_CODINGTYPE *pi_omx_codec,
                       const char **ppsz_name )
{
    unsigned int i;

    i_fourcc = vlc_fourcc_GetCodec( VIDEO_ES, i_fourcc );

    for( i = 0; video_format_table[i].i_codec != 0; i++ )
        if( video_format_table[i].i_fourcc == i_fourcc ) break;

    if( pi_omx_codec ) *pi_omx_codec = video_format_table[i].i_codec;
    if( ppsz_name ) *ppsz_name = vlc_fourcc_GetDescription( VIDEO_ES, i_fourcc );
    return !!video_format_table[i].i_codec;
}

int GetVlcVideoFormat( OMX_VIDEO_CODINGTYPE i_omx_codec,
                       vlc_fourcc_t *pi_fourcc, const char **ppsz_name )
{
    unsigned int i;

    for( i = 0; video_format_table[i].i_codec != 0; i++ )
        if( video_format_table[i].i_codec == i_omx_codec ) break;

    if( pi_fourcc ) *pi_fourcc = video_format_table[i].i_fourcc;
    if( ppsz_name ) *ppsz_name = vlc_fourcc_GetDescription( VIDEO_ES,
                                     video_format_table[i].i_fourcc );
    return !!video_format_table[i].i_fourcc;
}

static const char *GetOmxVideoRole( vlc_fourcc_t i_fourcc )
{
    unsigned int i;

    i_fourcc = vlc_fourcc_GetCodec( VIDEO_ES, i_fourcc );

    for( i = 0; video_format_table[i].i_codec != 0; i++ )
        if( video_format_table[i].i_fourcc == i_fourcc ) break;

    return video_format_table[i].psz_role;
}

static const char *GetOmxVideoEncRole( vlc_fourcc_t i_fourcc )
{
    unsigned int i;

    i_fourcc = vlc_fourcc_GetCodec( VIDEO_ES, i_fourcc );

    for( i = 0; video_enc_format_table[i].i_codec != 0; i++ )
        if( video_enc_format_table[i].i_fourcc == i_fourcc ) break;

    return video_enc_format_table[i].psz_role;
}

int GetOmxAudioFormat( vlc_fourcc_t i_fourcc,
                       OMX_AUDIO_CODINGTYPE *pi_omx_codec,
                       const char **ppsz_name )
{
    unsigned int i;

    i_fourcc = vlc_fourcc_GetCodec( AUDIO_ES, i_fourcc );

    for( i = 0; audio_format_table[i].i_codec != 0; i++ )
        if( audio_format_table[i].i_fourcc == i_fourcc ) break;

    if( pi_omx_codec ) *pi_omx_codec = audio_format_table[i].i_codec;
    if( ppsz_name ) *ppsz_name = vlc_fourcc_GetDescription( AUDIO_ES, i_fourcc );
    return !!audio_format_table[i].i_codec;
}

int OmxToVlcAudioFormat( OMX_AUDIO_CODINGTYPE i_omx_codec,
                       vlc_fourcc_t *pi_fourcc, const char **ppsz_name )
{
    unsigned int i;

    for( i = 0; audio_format_table[i].i_codec != 0; i++ )
        if( audio_format_table[i].i_codec == i_omx_codec ) break;

    if( pi_fourcc ) *pi_fourcc = audio_format_table[i].i_fourcc;
    if( ppsz_name ) *ppsz_name = vlc_fourcc_GetDescription( AUDIO_ES,
                                     audio_format_table[i].i_fourcc );
    return !!audio_format_table[i].i_fourcc;
}

static const char *GetOmxAudioRole( vlc_fourcc_t i_fourcc )
{
    unsigned int i;

    i_fourcc = vlc_fourcc_GetCodec( AUDIO_ES, i_fourcc );

    for( i = 0; audio_format_table[i].i_codec != 0; i++ )
        if( audio_format_table[i].i_fourcc == i_fourcc ) break;

    return audio_format_table[i].psz_role;
}

static const char *GetOmxAudioEncRole( vlc_fourcc_t i_fourcc )
{
    unsigned int i;

    i_fourcc = vlc_fourcc_GetCodec( AUDIO_ES, i_fourcc );

    for( i = 0; audio_enc_format_table[i].i_codec != 0; i++ )
        if( audio_enc_format_table[i].i_fourcc == i_fourcc ) break;

    return audio_enc_format_table[i].psz_role;
}

const char *GetOmxRole( vlc_fourcc_t i_fourcc, int i_cat, bool b_enc )
{
    if(b_enc)
        return i_cat == VIDEO_ES ?
            GetOmxVideoEncRole( i_fourcc ) : GetOmxAudioEncRole( i_fourcc );
    else
        return i_cat == VIDEO_ES ?
            GetOmxVideoRole( i_fourcc ) : GetOmxAudioRole( i_fourcc );
}

int GetOmxChromaFormat( vlc_fourcc_t i_fourcc,
                        OMX_COLOR_FORMATTYPE *pi_omx_codec,
                        const char **ppsz_name )
{
    unsigned int i;

    i_fourcc = vlc_fourcc_GetCodec( VIDEO_ES, i_fourcc );

    for( i = 0; chroma_format_table[i].i_codec != 0; i++ )
        if( chroma_format_table[i].i_fourcc == i_fourcc ) break;

    if( pi_omx_codec ) *pi_omx_codec = chroma_format_table[i].i_codec;
    if( ppsz_name ) *ppsz_name = vlc_fourcc_GetDescription( VIDEO_ES, i_fourcc );
    return !!chroma_format_table[i].i_codec;
}

int GetVlcChromaFormat( OMX_COLOR_FORMATTYPE i_omx_codec,
                        vlc_fourcc_t *pi_fourcc, const char **ppsz_name )
{
    unsigned int i;

    for( i = 0; chroma_format_table[i].i_codec != 0; i++ )
        if( chroma_format_table[i].i_codec == i_omx_codec ) break;

    if( pi_fourcc ) *pi_fourcc = chroma_format_table[i].i_fourcc;
    if( ppsz_name ) *ppsz_name = vlc_fourcc_GetDescription( VIDEO_ES,
                                     chroma_format_table[i].i_fourcc );
    return !!chroma_format_table[i].i_fourcc;
}

int GetVlcChromaSizes( vlc_fourcc_t i_fourcc,
                       unsigned int width, unsigned int height,
                       unsigned int *size, unsigned int *pitch,
                       unsigned int *chroma_pitch_div )
{
    unsigned int i;

    i_fourcc = vlc_fourcc_GetCodec( VIDEO_ES, i_fourcc );

    for( i = 0; chroma_format_table[i].i_codec != 0; i++ )
        if( chroma_format_table[i].i_fourcc == i_fourcc ) break;

    /* Align on macroblock boundary */
    width = (width + 15) & ~0xF;
    height = (height + 15) & ~0xF;

    if( size ) *size = width * height * chroma_format_table[i].i_size_mul / 2;
    if( pitch ) *pitch = width * chroma_format_table[i].i_line_mul;
    if( chroma_pitch_div )
        *chroma_pitch_div = chroma_format_table[i].i_line_chroma_div;
    return !!chroma_format_table[i].i_codec;
}

/*****************************************************************************
 * Functions to deal with audio format parameters
 *****************************************************************************/
static const struct {
    OMX_AUDIO_CODINGTYPE encoding;
    OMX_INDEXTYPE index;
    int size;
} audio_encoding_param[] =
{   { OMX_AUDIO_CodingPCM, OMX_IndexParamAudioPcm,
      sizeof(OMX_AUDIO_PARAM_PCMMODETYPE) },
    { OMX_AUDIO_CodingADPCM, OMX_IndexParamAudioAdpcm,
      sizeof(OMX_AUDIO_PARAM_ADPCMTYPE) },
    { OMX_AUDIO_CodingAMR, OMX_IndexParamAudioAmr,
      sizeof(OMX_AUDIO_PARAM_AMRTYPE) },
    { OMX_AUDIO_CodingG711, OMX_IndexParamAudioPcm,
      sizeof(OMX_AUDIO_PARAM_PCMMODETYPE) },
    { OMX_AUDIO_CodingG723, OMX_IndexParamAudioG723,
      sizeof(OMX_AUDIO_PARAM_G723TYPE) },
    { OMX_AUDIO_CodingG726, OMX_IndexParamAudioG726,
      sizeof(OMX_AUDIO_PARAM_G726TYPE) },
    { OMX_AUDIO_CodingG729, OMX_IndexParamAudioG729,
      sizeof(OMX_AUDIO_PARAM_G729TYPE) },
    { OMX_AUDIO_CodingAAC, OMX_IndexParamAudioAac,
      sizeof(OMX_AUDIO_PARAM_AACPROFILETYPE) },
    { OMX_AUDIO_CodingMP3, OMX_IndexParamAudioMp3,
      sizeof(OMX_AUDIO_PARAM_MP3TYPE) },
    { OMX_AUDIO_CodingSBC, OMX_IndexParamAudioSbc,
      sizeof(OMX_AUDIO_PARAM_SBCTYPE) },
    { OMX_AUDIO_CodingVORBIS, OMX_IndexParamAudioVorbis,
      sizeof(OMX_AUDIO_PARAM_VORBISTYPE) },
    { OMX_AUDIO_CodingWMA, OMX_IndexParamAudioWma,
      sizeof(OMX_AUDIO_PARAM_WMATYPE) },
    { OMX_AUDIO_CodingRA, OMX_IndexParamAudioRa,
      sizeof(OMX_AUDIO_PARAM_RATYPE) },
    { OMX_AUDIO_CodingUnused, 0, 0 }
};

static OMX_INDEXTYPE GetAudioParamFormatIndex(OMX_AUDIO_CODINGTYPE encoding)
{
  int i;

  for(i = 0; audio_encoding_param[i].encoding != OMX_AUDIO_CodingUnused &&
      audio_encoding_param[i].encoding != encoding; i++);

  return audio_encoding_param[i].index;
}

unsigned int GetAudioParamSize(OMX_INDEXTYPE index)
{
  int i;

  for(i = 0; audio_encoding_param[i].encoding != OMX_AUDIO_CodingUnused &&
      audio_encoding_param[i].index != index; i++);

  return audio_encoding_param[i].size;
}

OMX_ERRORTYPE SetAudioParameters(OMX_HANDLETYPE handle,
    OmxFormatParam *param, OMX_U32 i_port, OMX_AUDIO_CODINGTYPE encoding,
    vlc_fourcc_t i_codec, uint8_t i_channels, unsigned int i_samplerate,
    unsigned int i_bitrate, unsigned int i_bps, unsigned int i_blocksize)
{
    OMX_INDEXTYPE index;

    switch(encoding)
    {
    case OMX_AUDIO_CodingPCM:
    case OMX_AUDIO_CodingG711:
        OMX_INIT_STRUCTURE(param->pcm);
        param->pcm.nChannels = i_channels;
        param->pcm.nSamplingRate = i_samplerate;
        param->pcm.eNumData = OMX_NumericalDataSigned;
        param->pcm.ePCMMode = OMX_AUDIO_PCMModeLinear;
        param->pcm.eEndian = OMX_EndianLittle;
        param->pcm.bInterleaved = OMX_TRUE;
        param->pcm.nBitPerSample = i_bps;
        param->pcm.eChannelMapping[0] = OMX_AUDIO_ChannelCF;
        if(i_channels == 2)
        {
            param->pcm.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
            param->pcm.eChannelMapping[1] = OMX_AUDIO_ChannelRF;
        }
        break;
    case OMX_AUDIO_CodingADPCM:
        OMX_INIT_STRUCTURE(param->adpcm);
        param->adpcm.nChannels = i_channels;
        param->adpcm.nSampleRate = i_samplerate;
        param->adpcm.nBitsPerSample = i_bps;
        break;
    case OMX_AUDIO_CodingAMR:
        OMX_INIT_STRUCTURE(param->amr);
        param->amr.nChannels = i_channels;
        param->amr.nBitRate = i_bitrate;
        if (i_codec == VLC_CODEC_AMR_WB)
            param->amr.eAMRBandMode = OMX_AUDIO_AMRBandModeWB0;
        else
            param->amr.eAMRBandMode = OMX_AUDIO_AMRBandModeNB0;
        param->amr.eAMRDTXMode = OMX_AUDIO_AMRDTXModeOff;
        param->amr.eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatFSF;
        break;
    case OMX_AUDIO_CodingG723:
        OMX_INIT_STRUCTURE(param->g723);
        param->g723.nChannels = i_channels;
        param->g723.bDTX = OMX_FALSE;
        param->g723.eBitRate = OMX_AUDIO_G723ModeUnused;
        param->g723.bHiPassFilter = OMX_TRUE;
        param->g723.bPostFilter = OMX_TRUE;
        break;
    case OMX_AUDIO_CodingG726:
        OMX_INIT_STRUCTURE(param->g726);
        param->g726.nChannels = i_channels;
        param->g726.eG726Mode = OMX_AUDIO_G726ModeUnused;
        break;
    case OMX_AUDIO_CodingG729:
        OMX_INIT_STRUCTURE(param->g729);
        param->g729.nChannels = i_channels;
        param->g729.bDTX = OMX_FALSE;
        param->g729.eBitType = OMX_AUDIO_G729;
        break;
    case OMX_AUDIO_CodingAAC:
        OMX_INIT_STRUCTURE(param->aac);
        param->aac.nChannels = i_channels;
        param->aac.nSampleRate = i_samplerate;
        param->aac.nBitRate = i_bitrate;
        param->aac.nAudioBandWidth = 0;
        param->aac.nFrameLength = 1024;
        param->aac.nAACtools = OMX_AUDIO_AACToolAll;
        param->aac.nAACERtools = OMX_AUDIO_AACERAll;
        param->aac.eAACProfile = OMX_AUDIO_AACObjectLC;
        param->aac.eAACStreamFormat = OMX_AUDIO_AACStreamFormatMP4FF;
        param->aac.eChannelMode = i_channels > 1 ?
            OMX_AUDIO_ChannelModeStereo : OMX_AUDIO_ChannelModeMono;
        break;
    case OMX_AUDIO_CodingMP3:
        OMX_INIT_STRUCTURE(param->mp3);
        param->mp3.nChannels = i_channels;
        param->mp3.nSampleRate = i_samplerate;
        param->mp3.nBitRate = i_bitrate;
        param->mp3.eChannelMode = i_channels > 1 ?
            OMX_AUDIO_ChannelModeStereo : OMX_AUDIO_ChannelModeMono;
        param->mp3.eFormat = OMX_AUDIO_MP3StreamFormatMP1Layer3;
        break;
    case OMX_AUDIO_CodingWMA:
        OMX_INIT_STRUCTURE(param->wma);
        param->wma.nChannels = i_channels;
        param->wma.nBitRate = i_bitrate;
        param->wma.eFormat = OMX_AUDIO_WMAFormatUnused;
        param->wma.eProfile = OMX_AUDIO_WMAProfileUnused;
        param->wma.nSamplingRate = i_samplerate;
        param->wma.nBlockAlign = i_blocksize;
        param->wma.nEncodeOptions = 0;
        param->wma.nSuperBlockAlign = 0;
        break;
    case OMX_AUDIO_CodingRA:
        OMX_INIT_STRUCTURE(param->ra);
        param->ra.nChannels = i_channels;
        param->ra.nSamplingRate = i_samplerate;
        param->ra.nBitsPerFrame = i_bps;
        param->ra.nSamplePerFrame = 0;
        param->ra.nCouplingQuantBits = 0;
        param->ra.nCouplingStartRegion = 0;
        param->ra.nNumRegions = 0;
        param->ra.eFormat = OMX_AUDIO_RAFormatUnused;
        break;
    case OMX_AUDIO_CodingVORBIS:
        OMX_INIT_STRUCTURE(param->vorbis);
        param->vorbis.nChannels = i_channels;
        param->vorbis.nBitRate = i_bitrate;
        param->vorbis.nMinBitRate = 0;
        param->vorbis.nMaxBitRate = i_bitrate;
        param->vorbis.nSampleRate = i_samplerate;
        param->vorbis.nAudioBandWidth = 0;
        param->vorbis.nQuality = 3;
        param->vorbis.bManaged = OMX_FALSE;
        param->vorbis.bDownmix = OMX_FALSE;
        break;
    default:
        return OMX_ErrorBadParameter;
    }

    param->common.nPortIndex = i_port;

    index = GetAudioParamFormatIndex(encoding);
    return OMX_SetParameter(handle, index, param);
}

OMX_ERRORTYPE GetAudioParameters(OMX_HANDLETYPE handle,
    OmxFormatParam *param, OMX_U32 i_port, OMX_AUDIO_CODINGTYPE encoding,
    uint8_t *pi_channels, unsigned int *pi_samplerate,
    unsigned int *pi_bitrate, unsigned int *pi_bps, unsigned int *pi_blocksize)
{
    int i_channels = 0, i_samplerate = 0, i_bitrate = 0;
    int i_bps = 0, i_blocksize = 0;
    OMX_ERRORTYPE omx_error;
    OMX_INDEXTYPE index;

    OMX_INIT_COMMON(param->common);
    param->common.nPortIndex = i_port;
    index = GetAudioParamFormatIndex(encoding);
    if(!index) return OMX_ErrorNotImplemented;

    param->common.nSize = GetAudioParamSize(index);
    omx_error = OMX_GetParameter(handle, index, param);
    if(omx_error != OMX_ErrorNone) return omx_error;

    switch(encoding)
    {
    case OMX_AUDIO_CodingPCM:
    case OMX_AUDIO_CodingG711:
        i_channels = param->pcm.nChannels;
        i_samplerate = param->pcm.nSamplingRate;
        i_bps = param->pcm.nBitPerSample;
        break;
    case OMX_AUDIO_CodingADPCM:
        i_channels = param->adpcm.nChannels;
        i_samplerate = param->adpcm.nSampleRate;
        i_bps = param->adpcm.nBitsPerSample;
        break;
    case OMX_AUDIO_CodingAMR:
        i_channels = param->amr.nChannels;
        i_bitrate = param->amr.nBitRate;
        i_samplerate = 8000;
        break;
    case OMX_AUDIO_CodingG723:
        i_channels = param->g723.nChannels;
        break;
    case OMX_AUDIO_CodingG726:
        i_channels = param->g726.nChannels;
        break;
    case OMX_AUDIO_CodingG729:
        i_channels = param->g729.nChannels;
        break;
    case OMX_AUDIO_CodingAAC:
        i_channels = param->aac.nChannels;
        i_samplerate = param->aac.nSampleRate;
        i_bitrate = param->aac.nBitRate;
        i_channels = param->aac.eChannelMode == OMX_AUDIO_ChannelModeStereo ? 2 : 1;
        break;
    case OMX_AUDIO_CodingMP3:
        i_channels = param->mp3.nChannels;
        i_samplerate = param->mp3.nSampleRate;
        i_bitrate = param->mp3.nBitRate;
        i_channels = param->mp3.eChannelMode == OMX_AUDIO_ChannelModeStereo ? 2 : 1;
        break;
    case OMX_AUDIO_CodingVORBIS:
        i_channels = param->vorbis.nChannels;
        i_bitrate = param->vorbis.nBitRate;
        i_samplerate = param->vorbis.nSampleRate;
        break;
    case OMX_AUDIO_CodingWMA:
        i_channels = param->wma.nChannels;
        i_bitrate = param->wma.nBitRate;
        i_samplerate = param->wma.nSamplingRate;
        i_blocksize = param->wma.nBlockAlign;
        break;
    case OMX_AUDIO_CodingRA:
        i_channels = param->ra.nChannels;
        i_samplerate = param->ra.nSamplingRate;
        i_bps = param->ra.nBitsPerFrame;
        break;
    default:
        return OMX_ErrorBadParameter;
    }

    if(pi_channels) *pi_channels = i_channels;
    if(pi_samplerate) *pi_samplerate = i_samplerate;
    if(pi_bitrate) *pi_bitrate = i_bitrate;
    if(pi_bps) *pi_bps = i_bps;
    if(pi_blocksize) *pi_blocksize = i_blocksize;
    return OMX_ErrorNone;
}

/*****************************************************************************
 * PrintOmx: print component summary 
 *****************************************************************************/
void PrintOmx(decoder_t *p_dec, OMX_HANDLETYPE omx_handle, OMX_U32 i_port)
{
    OMX_PARAM_PORTDEFINITIONTYPE definition;
    OMX_PORT_PARAM_TYPE param;
    OMX_ERRORTYPE omx_error;
    unsigned int i, j;

    /* Find the input / output ports */
    OMX_INIT_STRUCTURE(param);
    OMX_INIT_STRUCTURE(definition);

    for(i = 0; i < 3; i++)
    {
        omx_error = OMX_GetParameter(omx_handle, OMX_IndexParamAudioInit + i, &param);
        if(omx_error != OMX_ErrorNone) continue;

        if(i_port == OMX_ALL)
            msg_Dbg( p_dec, "found %i %s ports", (int)param.nPorts,
                     i == 0 ? "audio" : i == 1 ? "image" : "video" );

        for(j = 0; j < param.nPorts; j++)
        {
            unsigned int i_samplerate, i_bitrate;
            unsigned int i_bitspersample, i_blockalign;
            uint8_t i_channels;
            OmxFormatParam format_param;
            vlc_fourcc_t i_fourcc;
            const char *psz_name;
            OMX_CONFIG_RECTTYPE crop_rect;

            if(i_port != OMX_ALL && i_port != param.nStartPortNumber + j)
                continue;

            /* Get port definition */
            definition.nPortIndex = param.nStartPortNumber + j;
            omx_error = OMX_GetParameter(omx_handle, OMX_IndexParamPortDefinition,
                                      &definition);
            if(omx_error != OMX_ErrorNone) continue;

            OMX_PARAM_U32TYPE u32param;
            OMX_INIT_STRUCTURE(u32param);
            u32param.nPortIndex = param.nStartPortNumber + j;
            omx_error = OMX_GetParameter(omx_handle, OMX_IndexParamNumAvailableStreams,
                                         (OMX_PTR)&u32param);

            msg_Dbg( p_dec, "-> %s %i (%i streams) (%i:%i:%i buffers) (%i,%i) %s",
                     definition.eDir == OMX_DirOutput ? "output" : "input",
                     (int)definition.nPortIndex, (int)u32param.nU32,
                     (int)definition.nBufferCountActual,
                     (int)definition.nBufferCountMin, (int)definition.nBufferSize,
                     (int)definition.bBuffersContiguous,
                     (int)definition.nBufferAlignment,
                     definition.bEnabled ? "enabled" : "disabled" );

            switch(definition.eDomain)
            {
            case OMX_PortDomainVideo:

                if(definition.format.video.eCompressionFormat)
                    GetVlcVideoFormat( definition.format.video.eCompressionFormat,
                                       &i_fourcc, &psz_name );
                else
                    GetVlcChromaFormat( definition.format.video.eColorFormat,
                                        &i_fourcc, &psz_name );

                OMX_INIT_STRUCTURE(crop_rect);
                crop_rect.nPortIndex = definition.nPortIndex;
                omx_error = OMX_GetConfig(omx_handle, OMX_IndexConfigCommonOutputCrop, &crop_rect);
                if (omx_error != OMX_ErrorNone)
                {
                    crop_rect.nLeft = crop_rect.nTop = 0;
                    crop_rect.nWidth  = definition.format.video.nFrameWidth;
                    crop_rect.nHeight = definition.format.video.nFrameHeight;
                }

                msg_Dbg( p_dec, "  -> video %s %ix%i@%.2f (%i,%i) (%i,%i) (%i,%i,%i,%i)", psz_name,
                         (int)definition.format.video.nFrameWidth,
                         (int)definition.format.video.nFrameHeight,
                         (float)definition.format.video.xFramerate/(float)(1<<16),
                         (int)definition.format.video.eCompressionFormat,
                         (int)definition.format.video.eColorFormat,
                         (int)definition.format.video.nStride,
                         (int)definition.format.video.nSliceHeight,
                         (int)crop_rect.nLeft, (int)crop_rect.nTop,
                         (int)crop_rect.nWidth, (int)crop_rect.nHeight);
                break;

            case OMX_PortDomainAudio:

                OmxToVlcAudioFormat( definition.format.audio.eEncoding,
                                   &i_fourcc, &psz_name );

                GetAudioParameters(omx_handle, &format_param,
                                   definition.nPortIndex,
                                   definition.format.audio.eEncoding,
                                   &i_channels, &i_samplerate, &i_bitrate,
                                   &i_bitspersample, &i_blockalign);

                msg_Dbg( p_dec, "  -> audio %s (%i) %i,%i,%i,%i,%i", psz_name,
                         (int)definition.format.audio.eEncoding,
                         i_channels, i_samplerate, i_bitrate, i_bitspersample,
                         i_blockalign);
                break;

            default: break;
            }
        }
    }
}
