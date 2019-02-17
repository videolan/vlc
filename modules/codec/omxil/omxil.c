/*****************************************************************************
 * omxil.c: Video decoder module making use of OpenMAX IL components.
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_block_helper.h>
#include <vlc_cpu.h>
#include "../../packetizer/h264_nal.h"
#include "../../packetizer/hevc_nal.h"

#include "omxil.h"
#include "omxil_core.h"
#include "OMX_Broadcom.h"

#ifndef NDEBUG
# define OMX_DBG(...) msg_Dbg( p_dec, __VA_ARGS__ )
#else
# define OMX_DBG(...)
#endif

#define SENTINEL_FLAG 0x10000

/* Defined in the broadcom version of OMX_Index.h */
#define OMX_IndexConfigRequestCallback 0x7f000063
#define OMX_IndexParamBrcmPixelAspectRatio 0x7f00004d
#define OMX_IndexParamBrcmVideoDecodeErrorConcealment 0x7f000080

/* Defined in the broadcom version of OMX_Core.h */
#define OMX_EventParamOrConfigChanged 0x7F000001

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder( vlc_object_t * );
static int  OpenEncoder( vlc_object_t * );
static int  OpenGeneric( vlc_object_t *, bool b_encode );
static void CloseGeneric( vlc_object_t * );

static int DecodeVideo( decoder_t *, block_t * );
static int DecodeAudio ( decoder_t *, block_t * );
static block_t *EncodeVideo( encoder_t *, picture_t * );
static void Flush( decoder_t * );

static OMX_ERRORTYPE OmxEventHandler( OMX_HANDLETYPE, OMX_PTR, OMX_EVENTTYPE,
                                      OMX_U32, OMX_U32, OMX_PTR );
static OMX_ERRORTYPE OmxEmptyBufferDone( OMX_HANDLETYPE, OMX_PTR,
                                         OMX_BUFFERHEADERTYPE * );
static OMX_ERRORTYPE OmxFillBufferDone( OMX_HANDLETYPE, OMX_PTR,
                                        OMX_BUFFERHEADERTYPE * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define DIRECTRENDERING_TEXT N_("OMX direct rendering")
#define DIRECTRENDERING_LONGTEXT N_(\
        "Enable OMX direct rendering.")

#define CFG_PREFIX "omxil-"
vlc_module_begin ()
    set_description( N_("Audio/Video decoder (using OpenMAX IL)") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_VCODEC )
    set_section( N_("Decoding") , NULL )
    set_capability( "video decoder", 80 )
    set_callbacks( OpenDecoder, CloseGeneric )
#ifndef __ANDROID__
    add_submodule()
    set_capability("audio decoder", 80)
    set_callbacks(OpenDecoder, CloseGeneric)
#endif

    add_submodule ()
    set_section( N_("Encoding") , NULL )
    set_description( N_("Video encoder (using OpenMAX IL)") )
    set_capability( "encoder", 0 )
    set_callbacks( OpenEncoder, CloseGeneric )
vlc_module_end ()

/*****************************************************************************
 * ImplementationSpecificWorkarounds: place-holder for implementation
 * specific workarounds
 *****************************************************************************/
static OMX_ERRORTYPE ImplementationSpecificWorkarounds(decoder_t *p_dec,
    OmxPort *p_port, es_format_t *p_fmt)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_PARAM_PORTDEFINITIONTYPE *def = &p_port->definition;
    uint8_t i_profile = 0xFF, i_level = 0xFF;

    /* Try to find out the profile of the video */
    if(p_fmt->i_cat == VIDEO_ES && def->eDir == OMX_DirInput &&
            p_fmt->i_codec == VLC_CODEC_H264)
        h264_get_profile_level(&p_dec->fmt_in, &i_profile, &i_level, &p_sys->i_nal_size_length);

    if(!strcmp(p_sys->psz_component, "OMX.TI.Video.Decoder"))
    {
        if(p_fmt->i_cat == VIDEO_ES && def->eDir == OMX_DirInput &&
           p_fmt->i_codec == VLC_CODEC_H264 &&
           (i_profile != PROFILE_H264_BASELINE || i_level > 30))
        {
            msg_Dbg(p_dec, "h264 profile/level not supported (0x%" PRIx8 ", 0x%" PRIx8 ")",
                    i_profile, i_level);
            return OMX_ErrorNotImplemented;
        }

        if(p_fmt->i_cat == VIDEO_ES && def->eDir == OMX_DirOutput &&
           p_fmt->i_codec == VLC_CODEC_I420)
        {
            /* I420 xvideo is slow on OMAP */
            def->format.video.eColorFormat = OMX_COLOR_FormatCbYCrY;
            GetVlcChromaFormat( def->format.video.eColorFormat,
                                &p_fmt->i_codec, 0 );
            GetVlcChromaSizes( p_fmt->i_codec,
                               def->format.video.nFrameWidth,
                               def->format.video.nFrameHeight,
                               &p_port->i_frame_size, &p_port->i_frame_stride,
                               &p_port->i_frame_stride_chroma_div );
            def->format.video.nStride = p_port->i_frame_stride;
            def->nBufferSize = p_port->i_frame_size;
        }
    }
    else if(!strcmp(p_sys->psz_component, "OMX.st.video_encoder"))
    {
        if(p_fmt->i_cat == VIDEO_ES)
        {
            /* Bellagio's encoder doesn't encode the framerate in Q16 */
            def->format.video.xFramerate >>= 16;
        }
    }
#if 0 /* FIXME: doesn't apply for HP Touchpad */
    else if (!strncmp(p_sys->psz_component, "OMX.qcom.video.decoder.",
                      strlen("OMX.qcom.video.decoder")))
    {
        /* qdsp6 refuses buffer size larger than 450K on input port */
        if (def->nBufferSize > 450 * 1024)
        {
            def->nBufferSize = 450 * 1024;
            p_port->i_frame_size = def->nBufferSize;
        }
    }
#endif
#ifdef RPI_OMX
    else if (!strcmp(p_sys->psz_component, "OMX.broadcom.video_decode"))
    {
        /* Clear these fields before setting parameters, to allow the codec
         * fill in what it wants (instead of rejecting whatever happened to
         * be there. */
        def->format.video.nStride = def->format.video.nSliceHeight = 0;
    }
#endif

    return OMX_ErrorNone;
}

/*****************************************************************************
 * SetPortDefinition: set definition of the omx port based on the vlc format
 *****************************************************************************/
static OMX_ERRORTYPE SetPortDefinition(decoder_t *p_dec, OmxPort *p_port,
                                       es_format_t *p_fmt)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_PARAM_PORTDEFINITIONTYPE *def = &p_port->definition;
    OMX_ERRORTYPE omx_error;

    omx_error = OMX_GetParameter(p_port->omx_handle,
                                 OMX_IndexParamPortDefinition, def);
    CHECK_ERROR(omx_error, "OMX_GetParameter failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    switch(p_fmt->i_cat)
    {
    case VIDEO_ES:
        def->format.video.nFrameWidth = p_fmt->video.i_width;
        def->format.video.nFrameHeight = p_fmt->video.i_height;
        if(def->format.video.eCompressionFormat == OMX_VIDEO_CodingUnused)
            def->format.video.nStride = def->format.video.nFrameWidth;
        if( p_fmt->video.i_frame_rate > 0 &&
            p_fmt->video.i_frame_rate_base > 0 )
            def->format.video.xFramerate = (p_fmt->video.i_frame_rate << 16) /
                p_fmt->video.i_frame_rate_base;

        if(def->eDir == OMX_DirInput || p_sys->b_enc)
        {
            if (def->eDir == OMX_DirInput && p_sys->b_enc)
                def->nBufferSize = def->format.video.nFrameWidth *
                  def->format.video.nFrameHeight * 2;
            p_port->i_frame_size = def->nBufferSize;

            if(!GetOmxVideoFormat(p_fmt->i_codec,
                                  &def->format.video.eCompressionFormat, 0) )
            {
                if(!GetOmxChromaFormat(p_fmt->i_codec,
                                       &def->format.video.eColorFormat, 0) )
                {
                    omx_error = OMX_ErrorNotImplemented;
                    CHECK_ERROR(omx_error, "codec %4.4s doesn't match any OMX format",
                                (char *)&p_fmt->i_codec );
                }
                GetVlcChromaSizes( p_fmt->i_codec,
                                   def->format.video.nFrameWidth,
                                   def->format.video.nFrameHeight,
                                   &p_port->i_frame_size, &p_port->i_frame_stride,
                                   &p_port->i_frame_stride_chroma_div );
                def->format.video.nStride = p_port->i_frame_stride;
                def->nBufferSize = p_port->i_frame_size;
            }
        }
        else
        {
            if( !GetVlcChromaFormat( def->format.video.eColorFormat,
                                     &p_fmt->i_codec, 0 ) )
            {
                omx_error = OMX_ErrorNotImplemented;
                CHECK_ERROR(omx_error, "OMX color format %i not supported",
                            (int)def->format.video.eColorFormat );
            }
            GetVlcChromaSizes( p_fmt->i_codec,
                               def->format.video.nFrameWidth,
                               def->format.video.nFrameHeight,
                               &p_port->i_frame_size, &p_port->i_frame_stride,
                               &p_port->i_frame_stride_chroma_div );
            def->format.video.nStride = p_port->i_frame_stride;
            if (p_port->i_frame_size > def->nBufferSize)
                def->nBufferSize = p_port->i_frame_size;
        }
        break;

    case AUDIO_ES:
        p_port->i_frame_size = def->nBufferSize;
        if(def->eDir == OMX_DirInput)
        {
            if(!GetOmxAudioFormat(p_fmt->i_codec,
                                  &def->format.audio.eEncoding, 0) )
            {
                omx_error = OMX_ErrorNotImplemented;
                CHECK_ERROR(omx_error, "codec %4.4s doesn't match any OMX format",
                            (char *)&p_fmt->i_codec );
            }
        }
        else
        {
            if( !OmxToVlcAudioFormat(def->format.audio.eEncoding,
                                   &p_fmt->i_codec, 0 ) )
            {
                omx_error = OMX_ErrorNotImplemented;
                CHECK_ERROR(omx_error, "OMX audio encoding %i not supported",
                            (int)def->format.audio.eEncoding );
            }
        }
        break;

    default: return OMX_ErrorNotImplemented;
    }

    omx_error = ImplementationSpecificWorkarounds(p_dec, p_port, p_fmt);
    CHECK_ERROR(omx_error, "ImplementationSpecificWorkarounds failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    omx_error = OMX_SetParameter(p_port->omx_handle,
                                 OMX_IndexParamPortDefinition, def);
    CHECK_ERROR(omx_error, "OMX_SetParameter failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    omx_error = OMX_GetParameter(p_port->omx_handle,
                                 OMX_IndexParamPortDefinition, def);
    CHECK_ERROR(omx_error, "OMX_GetParameter failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    if(p_port->i_frame_size > def->nBufferSize)
        def->nBufferSize = p_port->i_frame_size;
    p_port->i_frame_size = def->nBufferSize;

    /* Deal with audio params */
    if(p_fmt->i_cat == AUDIO_ES)
    {
        omx_error = SetAudioParameters(p_port->omx_handle,
                                       &p_port->format_param, def->nPortIndex,
                                       def->format.audio.eEncoding,
                                       p_fmt->i_codec,
                                       p_fmt->audio.i_channels,
                                       p_fmt->audio.i_rate,
                                       p_fmt->i_bitrate,
                                       p_fmt->audio.i_bitspersample,
                                       p_fmt->audio.i_blockalign);
        if (def->eDir == OMX_DirInput) {
            CHECK_ERROR(omx_error, "SetAudioParameters failed (%x : %s)",
                        omx_error, ErrorToString(omx_error));
        } else if (omx_error != OMX_ErrorNone) {
            msg_Warn(p_dec, "SetAudioParameters failed (%x : %s) on output port",
                     omx_error, ErrorToString(omx_error));
            omx_error = OMX_ErrorNone;
        }
    }
    if (!strcmp(p_sys->psz_component, "OMX.TI.DUCATI1.VIDEO.DECODER") &&
                def->eDir == OMX_DirOutput)
    {
        /* When setting the output buffer size above, the decoder actually
         * sets the buffer size to a lower value than what was chosen. If
         * we try to allocate buffers of this size, it fails. Thus, forcibly
         * use a larger buffer size. */
        def->nBufferSize *= 2;
    }

 error:
    return omx_error;
}


/*****************************************************************************
 * UpdatePixelAspect: Update vlc pixel aspect based on the aspect reported on
 * the omx port - NOTE: Broadcom specific
 *****************************************************************************/
static OMX_ERRORTYPE UpdatePixelAspect(decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_CONFIG_POINTTYPE pixel_aspect;
    OMX_INIT_STRUCTURE(pixel_aspect);
    OMX_ERRORTYPE omx_err;

    if (strncmp(p_sys->psz_component, "OMX.broadcom.", 13))
        return OMX_ErrorNotImplemented;

    pixel_aspect.nPortIndex = p_sys->out.i_port_index;
    omx_err = OMX_GetParameter(p_sys->omx_handle,
            OMX_IndexParamBrcmPixelAspectRatio, &pixel_aspect);
    if (omx_err != OMX_ErrorNone) {
        msg_Warn(p_dec, "Failed to retrieve aspect ratio");
    } else {
        p_dec->fmt_out.video.i_sar_num = pixel_aspect.nX;
        p_dec->fmt_out.video.i_sar_den = pixel_aspect.nY;
    }

    return omx_err;
}

/*****************************************************************************
 * AllocateBuffers: Allocate Omx buffers
 *****************************************************************************/
static OMX_ERRORTYPE AllocateBuffers(decoder_t *p_dec, OmxPort *p_port)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_ERRORTYPE omx_error = OMX_ErrorUndefined;
    OMX_PARAM_PORTDEFINITIONTYPE *def = &p_port->definition;
    unsigned int i = 0;

    OMX_DBG( "AllocateBuffers(%d)", def->eDir );

    p_port->i_buffers = p_port->definition.nBufferCountActual;

    p_port->pp_buffers = calloc(p_port->i_buffers, sizeof(OMX_BUFFERHEADERTYPE*));
    if( !p_port->pp_buffers )
    {
        p_port->i_buffers = 0;
        return OMX_ErrorInsufficientResources;
    }

    for(i = 0; i < p_port->i_buffers; i++)
    {
#if 0
#define ALIGN(x,BLOCKLIGN) (((x) + BLOCKLIGN - 1) & ~(BLOCKLIGN - 1))
        char *p_buf = malloc(p_port->definition.nBufferSize +
                             p_port->definition.nBufferAlignment);
        p_port->pp_buffers[i] = (void *)ALIGN((uintptr_t)p_buf, p_port->definition.nBufferAlignment);
#endif

        if( p_port->b_direct )
        {
            omx_error =
                OMX_UseBuffer( p_sys->omx_handle, &p_port->pp_buffers[i],
                               p_port->i_port_index, 0,
                               p_port->definition.nBufferSize, (void*)1);
            OMX_DBG( "OMX_UseBuffer(%d) %p, %p", def->eDir,
                     (void *)p_port->pp_buffers[i], p_port->pp_buffers[i] ?
                     (void *)p_port->pp_buffers[i]->pBuffer : NULL );
        }
        else
        {
            omx_error =
                OMX_AllocateBuffer( p_sys->omx_handle, &p_port->pp_buffers[i],
                                    p_port->i_port_index, 0,
                                    p_port->definition.nBufferSize);
            OMX_DBG( "OMX_AllocateBuffer(%d) %p, %p", def->eDir,
                     (void *)p_port->pp_buffers[i], p_port->pp_buffers[i] ?
                     (void *)p_port->pp_buffers[i]->pBuffer : NULL );
        }

        if(omx_error != OMX_ErrorNone)
        {
            p_port->i_buffers = i;
            break;
        }
        OMX_FIFO_PUT(&p_port->fifo, p_port->pp_buffers[i]);
    }

    CHECK_ERROR(omx_error, "AllocateBuffers failed (%x, %i)",
                omx_error, (int)p_port->i_port_index );


    OMX_DBG( "AllocateBuffers(%d)::done", def->eDir );
error:
    return omx_error;
}

/*****************************************************************************
 * FreeBuffers: Free Omx buffers
 *****************************************************************************/
static OMX_ERRORTYPE FreeBuffers(decoder_t *p_dec, OmxPort *p_port)
{
    OMX_PARAM_PORTDEFINITIONTYPE *def = &p_port->definition;
    OMX_ERRORTYPE omx_error = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *p_buffer;
    unsigned int i, i_wait_buffers;

    i_wait_buffers = p_port->i_buffers;

    OMX_DBG( "FreeBuffers(%d), waiting for %u buffers", def->eDir,
             i_wait_buffers);

    for(i = 0; i < i_wait_buffers; i++)
    {
        OMX_FIFO_GET(&p_port->fifo, p_buffer);
        if (p_buffer->nFlags & SENTINEL_FLAG) {
            free(p_buffer);
            i--;
            continue;
        }
    }

    for(i = 0; i < p_port->i_buffers; i++)
    {
        p_buffer = p_port->pp_buffers[i];
        if( p_buffer )
        {
            if (p_buffer->pAppPrivate != NULL)
                picture_Release( p_buffer->pAppPrivate );

            omx_error = OMX_FreeBuffer( p_port->omx_handle,
                                        p_port->i_port_index, p_buffer );
            OMX_DBG( "OMX_FreeBuffer(%d) %p, %p", def->eDir,
                     (void *)p_buffer, (void *)p_buffer->pBuffer );

            if(omx_error != OMX_ErrorNone) break;
        }
    }

    if( omx_error != OMX_ErrorNone )
       msg_Err( p_dec, "OMX_FreeBuffer failed (%x, %i, %i)",
                omx_error, (int)p_port->i_port_index, i );

    p_port->i_buffers = 0;
    free( p_port->pp_buffers );
    p_port->pp_buffers = NULL;

    OMX_DBG( "FreeBuffers(%d)::done", def->eDir );

    return omx_error;
}

/*****************************************************************************
 * GetPortDefinition: set vlc format based on the definition of the omx port
 *****************************************************************************/
static OMX_ERRORTYPE GetPortDefinition(decoder_t *p_dec, OmxPort *p_port,
                                       es_format_t *p_fmt)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_PARAM_PORTDEFINITIONTYPE *def = &p_port->definition;
    OMX_ERRORTYPE omx_error;
    OMX_CONFIG_RECTTYPE crop_rect;

    omx_error = OMX_GetParameter(p_port->omx_handle,
                                 OMX_IndexParamPortDefinition, def);
    CHECK_ERROR(omx_error, "OMX_GetParameter failed (%x : %s)",
                omx_error, ErrorToString(omx_error));

    switch(p_fmt->i_cat)
    {
    case VIDEO_ES:
        p_fmt->video.i_width = def->format.video.nFrameWidth;
        p_fmt->video.i_visible_width = def->format.video.nFrameWidth;
        p_fmt->video.i_height = def->format.video.nFrameHeight;
        p_fmt->video.i_visible_height = def->format.video.nFrameHeight;
        p_fmt->video.i_frame_rate = p_dec->fmt_in.video.i_frame_rate;
        p_fmt->video.i_frame_rate_base = p_dec->fmt_in.video.i_frame_rate_base;

        OMX_INIT_STRUCTURE(crop_rect);
        crop_rect.nPortIndex = def->nPortIndex;
        omx_error = OMX_GetConfig(p_port->omx_handle, OMX_IndexConfigCommonOutputCrop, &crop_rect);
        if (omx_error == OMX_ErrorNone)
        {
            if (!def->format.video.nSliceHeight)
                def->format.video.nSliceHeight = def->format.video.nFrameHeight;
            if (!def->format.video.nStride)
                def->format.video.nStride = def->format.video.nFrameWidth;
            p_fmt->video.i_width = crop_rect.nWidth;
            p_fmt->video.i_visible_width = crop_rect.nWidth;
            p_fmt->video.i_height = crop_rect.nHeight;
            p_fmt->video.i_visible_height = crop_rect.nHeight;
            if (def->format.video.eColorFormat == OMX_TI_COLOR_FormatYUV420PackedSemiPlanar)
                def->format.video.nSliceHeight -= crop_rect.nTop/2;
        }
        else
        {
            /* Don't pass the error back to the caller, this isn't mandatory */
            omx_error = OMX_ErrorNone;
        }

        /* Hack: Nexus One (stock firmware with binary OMX driver blob)
         * claims to output 420Planar even though it in in practice is
         * NV21. */
        if(def->format.video.eColorFormat == OMX_COLOR_FormatYUV420Planar &&
           !strncmp(p_sys->psz_component, "OMX.qcom.video.decoder",
                    strlen("OMX.qcom.video.decoder")))
            def->format.video.eColorFormat = OMX_QCOM_COLOR_FormatYVU420SemiPlanar;

        if ((p_sys->i_quirks & OMXCODEC_VIDEO_QUIRKS_IGNORE_PADDING)) {
            def->format.video.nSliceHeight = 0;
            def->format.video.nStride = p_fmt->video.i_width;
        }

        if(!GetVlcVideoFormat( def->format.video.eCompressionFormat,
                               &p_fmt->i_codec, 0 ) )
        {
            if( !GetVlcChromaFormat( def->format.video.eColorFormat,
                                     &p_fmt->i_codec, 0 ) )
            {
                omx_error = OMX_ErrorNotImplemented;
                CHECK_ERROR(omx_error, "OMX color format %i not supported",
                            (int)def->format.video.eColorFormat );
            }
            GetVlcChromaSizes( p_fmt->i_codec,
                               def->format.video.nFrameWidth,
                               def->format.video.nFrameHeight,
                               &p_port->i_frame_size, &p_port->i_frame_stride,
                               &p_port->i_frame_stride_chroma_div );
        }
        if(p_port->i_frame_size > def->nBufferSize)
            def->nBufferSize = p_port->i_frame_size;
        p_port->i_frame_size = def->nBufferSize;
#if 0
        if((int)p_port->i_frame_stride > def->format.video.nStride)
            def->format.video.nStride = p_port->i_frame_stride;
#endif
        p_port->i_frame_stride = def->format.video.nStride;
        UpdatePixelAspect(p_dec);
        break;

    case AUDIO_ES:
        if( !OmxToVlcAudioFormat( def->format.audio.eEncoding,
                                &p_fmt->i_codec, 0 ) )
        {
            omx_error = OMX_ErrorNotImplemented;
            CHECK_ERROR(omx_error, "OMX audio format %i not supported",
                        (int)def->format.audio.eEncoding );
        }

        omx_error = GetAudioParameters(p_port->omx_handle,
                                       &p_port->format_param, def->nPortIndex,
                                       def->format.audio.eEncoding,
                                       &p_fmt->audio.i_channels,
                                       &p_fmt->audio.i_rate,
                                       &p_fmt->i_bitrate,
                                       &p_fmt->audio.i_bitspersample,
                                       &p_fmt->audio.i_blockalign);
        CHECK_ERROR(omx_error, "GetAudioParameters failed (%x : %s)",
                    omx_error, ErrorToString(omx_error));

        if(p_fmt->audio.i_channels < 9)
            p_fmt->audio.i_physical_channels =
                vlc_chan_maps[p_fmt->audio.i_channels];

        date_Init( &p_sys->end_date, p_fmt->audio.i_rate, 1 );

        break;

    default: return OMX_ErrorNotImplemented;
    }

 error:
    return omx_error;
}

/*****************************************************************************
 * DeinitialiseComponent: Deinitialise and unload an OMX component
 *****************************************************************************/
static OMX_ERRORTYPE DeinitialiseComponent(decoder_t *p_dec,
                                           OMX_HANDLETYPE omx_handle)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_BUFFERHEADERTYPE *p_buffer;
    OMX_ERRORTYPE omx_error;
    OMX_STATETYPE state;
    unsigned int i;

    if(!omx_handle) return OMX_ErrorNone;

    omx_error = OMX_GetState(omx_handle, &state);
    CHECK_ERROR(omx_error, "OMX_GetState failed (%x)", omx_error );

    if(state == OMX_StateExecuting)
    {
        omx_error = OMX_SendCommand( omx_handle, OMX_CommandStateSet,
                                     OMX_StateIdle, 0 );
        CHECK_ERROR(omx_error, "OMX_CommandStateSet Idle failed (%x)", omx_error );
        while (1) {
            OMX_U32 cmd, state;
            omx_error = WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, &cmd, &state, 0);
            CHECK_ERROR(omx_error, "Wait for Idle failed (%x)", omx_error );
            // The event queue can contain other OMX_EventCmdComplete items,
            // such as for OMX_CommandFlush
            if (cmd == OMX_CommandStateSet && state == OMX_StateIdle)
                break;
        }
    }

    omx_error = OMX_GetState(omx_handle, &state);
    CHECK_ERROR(omx_error, "OMX_GetState failed (%x)", omx_error );

    if(state == OMX_StateIdle)
    {
        omx_error = OMX_SendCommand( omx_handle, OMX_CommandStateSet,
                                     OMX_StateLoaded, 0 );
        CHECK_ERROR(omx_error, "OMX_CommandStateSet Loaded failed (%x)", omx_error );

        for(i = 0; i < p_sys->ports; i++)
        {
            OmxPort *p_port = &p_sys->p_ports[i];

            omx_error = FreeBuffers( p_dec, p_port );
            CHECK_ERROR(omx_error, "FreeBuffers failed (%x, %i)",
                        omx_error, (int)p_port->i_port_index );
        }

        omx_error = WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, 0, 0, 0);
        CHECK_ERROR(omx_error, "Wait for Loaded failed (%x)", omx_error );
    }

 error:
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];
        free(p_port->pp_buffers);
        p_port->pp_buffers = 0;

        while (1) {
            OMX_FIFO_PEEK(&p_port->fifo, p_buffer);
            if (!p_buffer) break;

            OMX_FIFO_GET(&p_port->fifo, p_buffer);
            if (p_buffer->nFlags & SENTINEL_FLAG) {
                free(p_buffer);
                continue;
            }
            msg_Warn( p_dec, "Stray buffer left in fifo, %p",
                      (void *)p_buffer );
        }
    }
    omx_error = pf_free_handle( omx_handle );
    return omx_error;
}

/*****************************************************************************
 * InitialiseComponent: Load and initialise an OMX component
 *****************************************************************************/
static OMX_ERRORTYPE InitialiseComponent(decoder_t *p_dec,
    OMX_STRING psz_component, OMX_HANDLETYPE *p_handle)
{
    static OMX_CALLBACKTYPE callbacks =
        { OmxEventHandler, OmxEmptyBufferDone, OmxFillBufferDone };
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_HANDLETYPE omx_handle;
    OMX_ERRORTYPE omx_error;
    unsigned int i;
    int i_quirks;
    OMX_U8 psz_role[OMX_MAX_STRINGNAME_SIZE];
    OMX_PARAM_COMPONENTROLETYPE role;
    OMX_PARAM_PORTDEFINITIONTYPE definition;
    OMX_PORT_PARAM_TYPE param;

    /* Load component */
    omx_error = pf_get_handle( &omx_handle, psz_component, p_dec, &callbacks );
    if(omx_error != OMX_ErrorNone)
    {
        msg_Warn( p_dec, "OMX_GetHandle(%s) failed (%x: %s)", psz_component,
                  omx_error, ErrorToString(omx_error) );
        return omx_error;
    }
    strncpy(p_sys->psz_component, psz_component, OMX_MAX_STRINGNAME_SIZE-1);
    i_quirks = OMXCodec_GetQuirks(p_dec->fmt_in.i_cat,
                                  p_sys->b_enc ? p_dec->fmt_out.i_codec : p_dec->fmt_in.i_codec,
                                  p_sys->psz_component,
                                  strlen(p_sys->psz_component));
    if ((i_quirks & OMXCODEC_QUIRKS_NEED_CSD)
      && !p_dec->fmt_in.i_extra)
    {
        /* TODO handle late configuration */
        msg_Warn( p_dec, "codec need CSD" );
        return OMX_ErrorUndefined;
    }

    omx_error = OMX_ComponentRoleEnum(omx_handle, psz_role, 0);
    if(omx_error == OMX_ErrorNone)
        msg_Dbg(p_dec, "loaded component %s of role %s", psz_component, psz_role);
    else
        msg_Dbg(p_dec, "loaded component %s", psz_component);
    PrintOmx(p_dec, omx_handle, OMX_ALL);

    /* Set component role */
    OMX_INIT_STRUCTURE(role);
    strcpy((char*)role.cRole,
           GetOmxRole(p_sys->b_enc ? p_dec->fmt_out.i_codec : p_dec->fmt_in.i_codec,
                      p_dec->fmt_in.i_cat, p_sys->b_enc));

    omx_error = OMX_SetParameter(omx_handle, OMX_IndexParamStandardComponentRole,
                                 &role);
    omx_error = OMX_GetParameter(omx_handle, OMX_IndexParamStandardComponentRole,
                                 &role);
    if(omx_error == OMX_ErrorNone)
        msg_Dbg(p_dec, "component standard role set to %s", role.cRole);

    /* Find the input / output ports */
    OMX_INIT_STRUCTURE(param);
    OMX_INIT_STRUCTURE(definition);
    omx_error = OMX_GetParameter(omx_handle, p_dec->fmt_in.i_cat == VIDEO_ES ?
                                 OMX_IndexParamVideoInit : OMX_IndexParamAudioInit, &param);
    if(omx_error != OMX_ErrorNone) {
#ifdef __ANDROID__
        param.nPorts = 2;
        param.nStartPortNumber = 0;
#else
        param.nPorts = 0;
#endif
    }

    for(i = 0; i < param.nPorts; i++)
    {
        OmxPort *p_port;

        /* Get port definition */
        definition.nPortIndex = param.nStartPortNumber + i;
        omx_error = OMX_GetParameter(omx_handle, OMX_IndexParamPortDefinition,
                                     &definition);
        if(omx_error != OMX_ErrorNone) continue;

        if(definition.eDir == OMX_DirInput) p_port = &p_sys->in;
        else  p_port = &p_sys->out;

        p_port->b_valid = true;
        p_port->i_port_index = definition.nPortIndex;
        p_port->definition = definition;
        p_port->omx_handle = omx_handle;
    }

    if(!p_sys->in.b_valid || !p_sys->out.b_valid)
    {
        omx_error = OMX_ErrorInvalidComponent;
        CHECK_ERROR(omx_error, "couldn't find an input and output port");
    }

    if( !strncmp(p_sys->psz_component, "OMX.SEC.", 8) && p_dec->fmt_in.i_cat == VIDEO_ES )
    {
        OMX_INDEXTYPE index;
        omx_error = OMX_GetExtensionIndex(omx_handle, (OMX_STRING) "OMX.SEC.index.ThumbnailMode", &index);
        if(omx_error == OMX_ErrorNone)
        {
            OMX_BOOL enable = OMX_TRUE;
            omx_error = OMX_SetConfig(omx_handle, index, &enable);
            CHECK_ERROR(omx_error, "Unable to set ThumbnailMode");
        } else {
            OMX_BOOL enable = OMX_TRUE;
            /* Needed on Samsung Galaxy S II */
            omx_error = OMX_SetConfig(omx_handle, OMX_IndexVendorSetYUV420pMode, &enable);
            if (omx_error == OMX_ErrorNone)
                msg_Dbg(p_dec, "Set OMX_IndexVendorSetYUV420pMode successfully");
            else
                msg_Dbg(p_dec, "Unable to set OMX_IndexVendorSetYUV420pMode: %x", omx_error);
        }
    }

    if(!strncmp(p_sys->psz_component, "OMX.broadcom.", 13))
    {
        OMX_CONFIG_REQUESTCALLBACKTYPE notifications;
        OMX_INIT_STRUCTURE(notifications);

        notifications.nPortIndex = p_sys->out.i_port_index;
        notifications.nIndex = OMX_IndexParamBrcmPixelAspectRatio;
        notifications.bEnable = OMX_TRUE;

        omx_error = OMX_SetParameter(omx_handle,
                OMX_IndexConfigRequestCallback, &notifications);
        if (omx_error == OMX_ErrorNone) {
            msg_Dbg(p_dec, "Enabled aspect ratio notifications");
            p_sys->b_aspect_ratio_handled = true;
        } else
            msg_Dbg(p_dec, "Could not enable aspect ratio notifications");
    }

    /* Set port definitions */
    for(i = 0; i < p_sys->ports; i++)
    {
        omx_error = SetPortDefinition(p_dec, &p_sys->p_ports[i],
                                      p_sys->p_ports[i].p_fmt);
        if(omx_error != OMX_ErrorNone) goto error;
    }

    if(!strncmp(p_sys->psz_component, "OMX.broadcom.", 13) &&
        p_sys->in.p_fmt->i_codec == VLC_CODEC_H264)
    {
        OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE concanParam;
        OMX_INIT_STRUCTURE(concanParam);
        concanParam.bStartWithValidFrame = OMX_FALSE;

        omx_error = OMX_SetParameter(omx_handle,
                OMX_IndexParamBrcmVideoDecodeErrorConcealment, &concanParam);
        if (omx_error == OMX_ErrorNone)
            msg_Dbg(p_dec, "StartWithValidFrame disabled.");
        else
            msg_Dbg(p_dec, "Could not disable StartWithValidFrame.");
    }

    /* Allocate our array for the omx buffers and enable ports */
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];

        /* Enable port */
        if(!p_port->definition.bEnabled)
        {
            omx_error = OMX_SendCommand( omx_handle, OMX_CommandPortEnable,
                                         p_port->i_port_index, NULL);
            CHECK_ERROR(omx_error, "OMX_CommandPortEnable on %i failed (%x)",
                        (int)p_port->i_port_index, omx_error );
            omx_error = WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, 0, 0, 0);
            CHECK_ERROR(omx_error, "Wait for PortEnable on %i failed (%x)",
                        (int)p_port->i_port_index, omx_error );
        }
    }

    p_sys->i_quirks = i_quirks;
    *p_handle = omx_handle;
    return OMX_ErrorNone;

 error:
    DeinitialiseComponent(p_dec, omx_handle);
    *p_handle = 0;
    return omx_error;
}

/*****************************************************************************
 * OpenDecoder: Create the decoder instance
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    int status;

    if( 0 || !GetOmxRole(p_dec->fmt_in.i_codec, p_dec->fmt_in.i_cat, false) )
        return VLC_EGENERIC;

    status = OpenGeneric( p_this, false );
    if(status != VLC_SUCCESS) return status;

    switch( p_dec->fmt_in.i_cat )
    {
        case AUDIO_ES: p_dec->pf_decode = DecodeAudio; break;
        case VIDEO_ES: p_dec->pf_decode = DecodeVideo; break;
        default: vlc_assert_unreachable();
    }
    p_dec->pf_flush        = Flush;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * OpenEncoder: Create the encoder instance
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t*)p_this;
    int status;

    if( !GetOmxRole(p_enc->fmt_out.i_codec, p_enc->fmt_in.i_cat, true) )
        return VLC_EGENERIC;

    status = OpenGeneric( p_this, true );
    if(status != VLC_SUCCESS) return status;

    p_enc->pf_encode_video = EncodeVideo;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * OpenGeneric: Create the generic decoder/encoder instance
 *****************************************************************************/
static int OpenGeneric( vlc_object_t *p_this, bool b_encode )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;
    OMX_ERRORTYPE omx_error;
    OMX_BUFFERHEADERTYPE *p_header;
    unsigned int i;

    if (InitOmxCore(p_this) != VLC_SUCCESS) {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = calloc( 1, sizeof(*p_sys)) ) == NULL )
    {
        DeinitOmxCore();
        return VLC_ENOMEM;
    }

    /* Initialise the thread properties */
    if(!b_encode)
    {
        p_dec->fmt_out.video = p_dec->fmt_in.video;
        p_dec->fmt_out.audio = p_dec->fmt_in.audio;
        p_dec->fmt_out.i_codec = 0;

        /* set default aspect of 1, if parser did not set it */
        if (p_dec->fmt_out.video.i_sar_num == 0)
            p_dec->fmt_out.video.i_sar_num = 1;
        if (p_dec->fmt_out.video.i_sar_den == 0)
            p_dec->fmt_out.video.i_sar_den = 1;
    }
    p_sys->b_enc = b_encode;
    InitOmxEventQueue(&p_sys->event_queue);
    OMX_FIFO_INIT (&p_sys->in.fifo, pOutputPortPrivate );
    p_sys->in.b_direct = false;
    p_sys->in.b_flushed = true;
    p_sys->in.p_fmt = &p_dec->fmt_in;
    OMX_FIFO_INIT (&p_sys->out.fifo, pInputPortPrivate );
    p_sys->out.b_direct = false;
    p_sys->out.b_flushed = true;
    p_sys->out.p_fmt = &p_dec->fmt_out;
    p_sys->ports = 2;
    p_sys->p_ports = &p_sys->in;
    p_sys->b_use_pts = 1;

    msg_Dbg(p_dec, "fmt in:%4.4s, out: %4.4s", (char *)&p_dec->fmt_in.i_codec,
            (char *)&p_dec->fmt_out.i_codec);

    /* Enumerate components and build a list of the one we want to try */
    p_sys->components =
        CreateComponentsList(p_this,
             GetOmxRole(p_sys->b_enc ? p_dec->fmt_out.i_codec :
                        p_dec->fmt_in.i_codec, p_dec->fmt_in.i_cat,
                        p_sys->b_enc), p_sys->ppsz_components);
    if( !p_sys->components )
    {
        msg_Warn( p_this, "couldn't find an omx component for codec %4.4s",
                  (char *)&p_dec->fmt_in.i_codec );
        CloseGeneric(p_this);
        return VLC_EGENERIC;
    }

    /* Try to load and initialise a component */
    omx_error = OMX_ErrorUndefined;
    for(i = 0; i < p_sys->components; i++)
    {
#ifdef __ANDROID__
        if (OMXCodec_IsBlacklisted(p_sys->ppsz_components[i], strlen(p_sys->ppsz_components[i])))
            continue;
#endif
        omx_error = InitialiseComponent(p_dec, p_sys->ppsz_components[i],
                                        &p_sys->omx_handle);
        if(omx_error == OMX_ErrorNone) break;
    }
    CHECK_ERROR(omx_error, "no component could be initialised" );

    /* Move component to Idle then Executing state */
    OMX_SendCommand( p_sys->omx_handle, OMX_CommandStateSet, OMX_StateIdle, 0 );
    CHECK_ERROR(omx_error, "OMX_CommandStateSet Idle failed (%x)", omx_error );

    /* Allocate omx buffers */
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];
        omx_error = AllocateBuffers( p_dec, p_port );
        CHECK_ERROR(omx_error, "AllocateBuffers failed (%x, %i)",
                    omx_error, (int)p_port->i_port_index );
    }

    omx_error = WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, 0, 0, 0);
    CHECK_ERROR(omx_error, "Wait for Idle failed (%x)", omx_error );

    omx_error = OMX_SendCommand( p_sys->omx_handle, OMX_CommandStateSet,
                                 OMX_StateExecuting, 0);
    CHECK_ERROR(omx_error, "OMX_CommandStateSet Executing failed (%x)", omx_error );
    omx_error = WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, 0, 0, 0);
    CHECK_ERROR(omx_error, "Wait for Executing failed (%x)", omx_error );

    /* Send codec configuration data */
    if( p_dec->fmt_in.i_extra )
    {
        OMX_FIFO_GET(&p_sys->in.fifo, p_header);
        p_header->nFilledLen = p_dec->fmt_in.i_extra;

        /* Convert H.264 NAL format to annex b */
        if( p_sys->i_nal_size_length && !p_sys->in.b_direct &&
            h264_isavcC(p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra) )
        {
            size_t i_filled_len = 0;
            p_header->pBuffer = h264_avcC_to_AnnexB_NAL(
                        p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra,
                        &i_filled_len, NULL );
            p_header->nFilledLen = i_filled_len;
        }
        else if( p_dec->fmt_in.i_codec == VLC_CODEC_HEVC && !p_sys->in.b_direct )
        {
            size_t i_filled_len = 0;
            p_header->pBuffer = hevc_hvcC_to_AnnexB_NAL(
                        p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra,
                        &i_filled_len, &p_sys->i_nal_size_length );
            p_header->nFilledLen = i_filled_len;
        }
        else if(p_sys->in.b_direct)
        {
            p_header->pOutputPortPrivate = p_header->pBuffer;
            p_header->pBuffer = p_dec->fmt_in.p_extra;
        }
        else if (p_dec->fmt_in.i_codec == VLC_CODEC_WMV3 &&
                 p_dec->fmt_in.i_extra >= 4 &&
                 p_header->nAllocLen >= 36)
        {
            int profile;
            // According to OMX IL 1.2.0 spec (4.3.33.2), the codec config
            // data for VC-1 Main/Simple (aka WMV3) is according to table 265
            // in the VC-1 spec. Most of the fields are just set with placeholders
            // (like framerate, hrd_buffer/rate).
            static const uint8_t wmv3seq[] = {
                0xff, 0xff, 0xff, 0xc5, // numframes=ffffff, marker byte
                0x04, 0x00, 0x00, 0x00, // marker byte
                0x00, 0x00, 0x00, 0x00, // struct C, almost equal to p_extra
                0x00, 0x00, 0x00, 0x00, // struct A, vert size
                0x00, 0x00, 0x00, 0x00, // struct A, horiz size
                0x0c, 0x00, 0x00, 0x00, // marker byte
                0xff, 0xff, 0x00, 0x80, // struct B, level=4, cbr=0, hrd_buffer=ffff
                0xff, 0xff, 0x00, 0x00, // struct B, hrd_rate=ffff
                0xff, 0xff, 0xff, 0xff, // struct B, framerate=ffffffff
            };
            p_header->nFilledLen = sizeof(wmv3seq);
            memcpy(p_header->pBuffer, wmv3seq, p_header->nFilledLen);
            // Struct C - almost equal to the extradata
            memcpy(&p_header->pBuffer[8], p_dec->fmt_in.p_extra, 4);
            // Expand profile from the highest 2 bits to the highest 4 bits
            profile = p_header->pBuffer[8] >> 6;
            p_header->pBuffer[8] = (p_header->pBuffer[8] & 0x0f) | (profile << 4);
            // Fill in the height/width for struct A
            SetDWLE(&p_header->pBuffer[12], p_dec->fmt_in.video.i_height);
            SetDWLE(&p_header->pBuffer[16], p_dec->fmt_in.video.i_width);
        }
        else
        {
            if(p_header->nFilledLen > p_header->nAllocLen)
            {
                msg_Dbg(p_dec, "buffer too small (%i,%i)", (int)p_header->nFilledLen,
                        (int)p_header->nAllocLen);
                p_header->nFilledLen = p_header->nAllocLen;
            }
            memcpy(p_header->pBuffer, p_dec->fmt_in.p_extra, p_header->nFilledLen);
        }

        p_header->nOffset = 0;
        p_header->nFlags = OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME;
        msg_Dbg(p_dec, "sending codec config data %p, %p, %u",
                (void *)p_header, (void *)p_header->pBuffer,
                (unsigned)p_header->nFilledLen);
        OMX_EmptyThisBuffer(p_sys->omx_handle, p_header);
    }

    /* Get back output port definition */
    omx_error = GetPortDefinition(p_dec, &p_sys->out, p_sys->out.p_fmt);
    if(omx_error != OMX_ErrorNone) goto error;

    PrintOmx(p_dec, p_sys->omx_handle, p_sys->in.i_port_index);
    PrintOmx(p_dec, p_sys->omx_handle, p_sys->out.i_port_index);

    if(p_sys->b_error) goto error;

    if (!p_sys->b_use_pts)
        msg_Dbg( p_dec, "using dts timestamp mode for %s", p_sys->psz_component);

    return VLC_SUCCESS;

 error:
    CloseGeneric(p_this);
    return VLC_EGENERIC;
}

/*****************************************************************************
 * PortReconfigure
 *****************************************************************************/
static OMX_ERRORTYPE PortReconfigure(decoder_t *p_dec, OmxPort *p_port)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_PARAM_PORTDEFINITIONTYPE definition;
    OMX_ERRORTYPE omx_error;

    OMX_DBG( "PortReconfigure(%d)", p_port->definition.eDir );

    /* Sanity checking */
    OMX_INIT_STRUCTURE(definition);
    definition.nPortIndex = p_port->i_port_index;
    omx_error = OMX_GetParameter(p_sys->omx_handle, OMX_IndexParamPortDefinition,
                                 &definition);
    if(omx_error != OMX_ErrorNone || (p_dec->fmt_in.i_cat == VIDEO_ES &&
       (!definition.format.video.nFrameWidth ||
       !definition.format.video.nFrameHeight)) )
        return OMX_ErrorUndefined;

    omx_error = OMX_SendCommand( p_sys->omx_handle, OMX_CommandPortDisable,
                                 p_port->i_port_index, NULL);
    CHECK_ERROR(omx_error, "OMX_CommandPortDisable on %i failed (%x)",
                (int)p_port->i_port_index, omx_error );

    omx_error = FreeBuffers( p_dec, p_port );
    CHECK_ERROR(omx_error, "FreeBuffers failed (%x, %i)",
                omx_error, (int)p_port->i_port_index );

    omx_error = WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, 0, 0, 0);
    CHECK_ERROR(omx_error, "Wait for PortDisable failed (%x)", omx_error );

    /* Get the new port definition */
    omx_error = GetPortDefinition(p_dec, &p_sys->out, p_sys->out.p_fmt);
    if(omx_error != OMX_ErrorNone) goto error;

    if( p_dec->fmt_in.i_cat != AUDIO_ES )
    {
        /* Don't explicitly set the new parameters that we got with
         * OMX_GetParameter above when using audio codecs.
         * That struct hasn't been changed since, so there should be
         * no need to set it here, unless some codec expects the
         * SetParameter call as a trigger event for some part of
         * the reconfiguration.
         * This fixes using audio decoders on Samsung Galaxy S II,
         *
         * Only skipping this for audio codecs, to minimize the
         * change for current working configurations for video.
         */
        omx_error = OMX_SetParameter(p_sys->omx_handle, OMX_IndexParamPortDefinition,
                                     &definition);
        CHECK_ERROR(omx_error, "OMX_SetParameter failed (%x : %s)",
                    omx_error, ErrorToString(omx_error));
    }

    omx_error = OMX_SendCommand( p_sys->omx_handle, OMX_CommandPortEnable,
                                 p_port->i_port_index, NULL);
    CHECK_ERROR(omx_error, "OMX_CommandPortEnable on %i failed (%x)",
                (int)p_port->i_port_index, omx_error );

    omx_error = AllocateBuffers( p_dec, p_port );
    CHECK_ERROR(omx_error, "OMX_AllocateBuffers failed (%x, %i)",
                omx_error, (int)p_port->i_port_index );

    omx_error = WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, 0, 0, 0);
    CHECK_ERROR(omx_error, "Wait for PortEnable failed (%x)", omx_error );

    PrintOmx(p_dec, p_sys->omx_handle, p_sys->in.i_port_index);
    PrintOmx(p_dec, p_sys->omx_handle, p_sys->out.i_port_index);

    OMX_DBG( "PortReconfigure(%d)::done", p_port->definition.eDir );
 error:
    return omx_error;
}

/*****************************************************************************
 * DecodeVideoOutput
 *****************************************************************************/
static int DecodeVideoOutput( decoder_t *p_dec, OmxPort *p_port, picture_t **pp_pic )
{
    VLC_UNUSED( p_dec );
    OMX_BUFFERHEADERTYPE *p_header;
    picture_t *p_pic = NULL, *p_next_pic;
    OMX_ERRORTYPE omx_error;

    while(!p_pic)
    {
        OMX_FIFO_PEEK(&p_port->fifo, p_header);
        if(!p_header) break; /* No frame available */

        if(p_port->b_update_def)
        {
            omx_error = GetPortDefinition(p_dec, p_port, p_port->p_fmt);
            p_port->b_update_def = 0;
            CHECK_ERROR(omx_error, "GetPortDefinition failed");
        }
        if( decoder_UpdateVideoFormat( p_dec ) )
            goto error;

        if(p_header->nFilledLen)
        {
            p_pic = p_header->pAppPrivate;
            if(!p_pic)
            {
                /* We're not in direct rendering mode.
                 * Get a new picture and copy the content */
                p_pic = decoder_NewPicture( p_dec );

                if (p_pic)
                    CopyOmxPicture(p_port->definition.format.video.eColorFormat,
                                   p_pic, p_port->definition.format.video.nSliceHeight,
                                   p_port->i_frame_stride,
                                   p_header->pBuffer + p_header->nOffset,
                                   p_port->i_frame_stride_chroma_div, NULL);
            }

            if (p_pic)
                p_pic->date = FromOmxTicks(p_header->nTimeStamp);
            p_header->nFilledLen = 0;
            p_header->pAppPrivate = 0;
        }

        /* Get a new picture */
        if(p_port->b_direct && !p_header->pAppPrivate)
        {
            p_next_pic = decoder_NewPicture( p_dec );
            if(!p_next_pic) break;

            OMX_FIFO_GET(&p_port->fifo, p_header);
            p_header->pAppPrivate = p_next_pic;
            p_header->pInputPortPrivate = p_header->pBuffer;
            p_header->pBuffer = p_next_pic->p[0].p_pixels;
        }
        else
        {
            OMX_FIFO_GET(&p_port->fifo, p_header);
        }
        OMX_DBG( "FillThisBuffer %p, %p", (void *)p_header,
                 (void *)p_header->pBuffer );
        OMX_FillThisBuffer(p_port->omx_handle, p_header);
    }

    *pp_pic = p_pic;
    return 0;
error:
    return -1;
}

static int DecodeVideoInput( decoder_t *p_dec, OmxPort *p_port, block_t **pp_block,
                             unsigned int i_input_used, bool *p_reconfig )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_BUFFERHEADERTYPE *p_header;
    block_t *p_block = *pp_block;

    /* Send the input buffer to the component */
    OMX_FIFO_GET_TIMEOUT(&p_port->fifo, p_header, 10000);

    if (p_header && p_header->nFlags & SENTINEL_FLAG) {
        free(p_header);
        *p_reconfig = true;
        return 0;
    }
    *p_reconfig = false;

    if(p_header)
    {
        bool decode_more = false;
        p_header->nFilledLen = p_block->i_buffer - i_input_used;
        p_header->nOffset = 0;
        p_header->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
        if (p_sys->b_use_pts && p_block->i_pts)
            p_header->nTimeStamp = ToOmxTicks(p_block->i_pts);
        else
            p_header->nTimeStamp = ToOmxTicks(p_block->i_dts);

        /* In direct mode we pass the input pointer as is.
         * Otherwise we memcopy the data */
        if(p_port->b_direct)
        {
            p_header->pOutputPortPrivate = p_header->pBuffer;
            p_header->pBuffer = p_block->p_buffer;
            p_header->pAppPrivate = p_block;
            i_input_used = p_header->nFilledLen;
        }
        else
        {
            if(p_header->nFilledLen > p_header->nAllocLen)
            {
                p_header->nFilledLen = p_header->nAllocLen;
            }
            memcpy(p_header->pBuffer, p_block->p_buffer + i_input_used, p_header->nFilledLen);
            i_input_used += p_header->nFilledLen;
            if (i_input_used == p_block->i_buffer)
            {
                block_Release(p_block);
            }
            else
            {
                decode_more = true;
                p_header->nFlags &= ~OMX_BUFFERFLAG_ENDOFFRAME;
            }
        }

        /* Convert H.264 NAL format to annex b. Doesn't do anything if
         * i_nal_size_length == 0, which is the case for codecs other
         * than H.264 */
        h264_AVC_to_AnnexB( p_header->pBuffer, p_header->nFilledLen,
                                p_sys->i_nal_size_length);
        OMX_DBG( "EmptyThisBuffer %p, %p, %u, %"PRId64, (void *)p_header,
                 (void *)p_header->pBuffer, (unsigned)p_header->nFilledLen,
                 FromOmxTicks(p_header->nTimeStamp) );
        OMX_EmptyThisBuffer(p_port->omx_handle, p_header);
        p_port->b_flushed = false;
        if (decode_more)
            return DecodeVideoInput( p_dec, p_port, pp_block, i_input_used,
                                     p_reconfig );
        else
            *pp_block = NULL; /* Avoid being fed the same packet again */
    }

    return 0;
}

/*****************************************************************************
 * Flush:
 *****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    if(!p_sys->in.b_flushed)
    {
        msg_Dbg(p_dec, "flushing");
        OMX_SendCommand( p_sys->omx_handle, OMX_CommandFlush,
                         p_sys->in.definition.nPortIndex, 0 );
    }
    p_sys->in.b_flushed = true;
}
/*****************************************************************************
 * DecodeVideo: Called to decode one frame
 *****************************************************************************/
static int DecodeVideo( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_ERRORTYPE omx_error;
    unsigned int i;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    /* Check for errors from codec */
    if(p_sys->b_error)
    {
        msg_Dbg(p_dec, "error during decoding");
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
    {
        block_Release( p_block );
        Flush( p_dec );
        return VLCDEC_SUCCESS;
    }

    /* Use the aspect ratio provided by the input (ie read from packetizer).
     * In case the we get aspect ratio info from the decoder (as in the
     * broadcom OMX implementation on RPi), don't let the packetizer values
     * override what the decoder says, if anything - otherwise always update
     * even if it already is set (since it can change within a stream). */
    if((p_dec->fmt_in.video.i_sar_num != 0 && p_dec->fmt_in.video.i_sar_den != 0) &&
       (p_dec->fmt_out.video.i_sar_num == 0 || p_dec->fmt_out.video.i_sar_den == 0 ||
             !p_sys->b_aspect_ratio_handled))
    {
        p_dec->fmt_out.video.i_sar_num = p_dec->fmt_in.video.i_sar_num;
        p_dec->fmt_out.video.i_sar_den = p_dec->fmt_in.video.i_sar_den;
    }

    /* Loop as long as we haven't either got an input buffer (and cleared
     * *pp_block) or got an output picture */
    int max_polling_attempts = 100;
    int attempts = 0;
    while( p_block ) {
        bool b_reconfig = false;

        if( DecodeVideoInput( p_dec, &p_sys->in, &p_block, 0, &b_reconfig ) != 0 )
            goto error;

        picture_t *p_pic = NULL;
        /* If we don't have a p_pic from the first try. Try again */
        if( !b_reconfig &&
            DecodeVideoOutput( p_dec, &p_sys->out, &p_pic ) != 0 )
            goto error;

        /* Handle the PortSettingsChanged events */
        for(i = 0; i < p_sys->ports; i++)
        {
            OmxPort *p_port = &p_sys->p_ports[i];
            if(p_port->b_reconfigure)
            {
                omx_error = PortReconfigure(p_dec, p_port);
                p_port->b_reconfigure = 0;
                CHECK_ERROR(omx_error, "PortReconfigure failed");
            }
            if(p_port->b_update_def)
            {
                omx_error = GetPortDefinition(p_dec, p_port, p_port->p_fmt);
                p_port->b_update_def = 0;
                CHECK_ERROR(omx_error, "GetPortDefinition failed");
            }
        }

        if( p_pic != NULL )
        {
            decoder_QueueVideo( p_dec, p_pic );
            continue;
        }
        attempts++;
    }

    return VLCDEC_SUCCESS;
error:
    p_sys->b_error = true;
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * DecodeAudio: Called to decode one frame
 *****************************************************************************/
int DecodeAudio ( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_BUFFERHEADERTYPE *p_header;
    OMX_ERRORTYPE omx_error;
    unsigned int i;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    /* Check for errors from codec */
    if(p_sys->b_error)
    {
        msg_Dbg(p_dec, "error during decoding");
        block_Release( p_block );
        return VLCDEC_SUCCESS;
    }

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
    {
        block_Release( p_block );
        date_Set( &p_sys->end_date, VLC_TICK_INVALID );
        if(!p_sys->in.b_flushed)
        {
            msg_Dbg(p_dec, "flushing");
            OMX_SendCommand( p_sys->omx_handle, OMX_CommandFlush,
                             p_sys->in.definition.nPortIndex, 0 );
        }
        p_sys->in.b_flushed = true;
        return VLCDEC_SUCCESS;
    }

    if( date_Get( &p_sys->end_date ) == VLC_TICK_INVALID )
    {
        if( !p_block->i_pts )
        {
            /* We've just started the stream, wait for the first PTS. */
            block_Release( p_block );
            return VLCDEC_SUCCESS;
        }
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    /* Take care of decoded frames first */
    while (p_block != NULL)
    {
        unsigned int i_samples = 0;

        OMX_FIFO_PEEK(&p_sys->out.fifo, p_header);
        if(!p_header) break; /* No frame available */

        if (p_sys->out.p_fmt->audio.i_channels)
            i_samples = p_header->nFilledLen / p_sys->out.p_fmt->audio.i_channels / 2;
        if(i_samples)
        {
            if( decoder_UpdateAudioFormat( p_dec ) )
                break;
            block_t *p_buffer = decoder_NewAudioBuffer( p_dec, i_samples );
            if( !p_buffer ) break; /* No audio buffer available */

            memcpy( p_buffer->p_buffer, p_header->pBuffer, p_buffer->i_buffer );
            p_header->nFilledLen = 0;

            int64_t timestamp = FromOmxTicks(p_header->nTimeStamp);
            if( timestamp != 0 &&
                timestamp != date_Get( &p_sys->end_date ) )
                date_Set( &p_sys->end_date, timestamp );

            p_buffer->i_pts = date_Get( &p_sys->end_date );
            p_buffer->i_length = date_Increment( &p_sys->end_date, i_samples ) -
                p_buffer->i_pts;
            decoder_QueueAudio( p_dec, p_buffer );
        }

        OMX_DBG( "FillThisBuffer %p, %p", (void *)p_header,
                 (void *)p_header->pBuffer );
        OMX_FIFO_GET(&p_sys->out.fifo, p_header);
        OMX_FillThisBuffer(p_sys->omx_handle, p_header);

        /* Send the input buffer to the component */
        OMX_FIFO_GET_TIMEOUT(&p_sys->in.fifo, p_header, 200000);

        if (p_header && p_header->nFlags & SENTINEL_FLAG) {
            free(p_header);
            goto reconfig;
        }

        if (!p_header)
            continue;

        p_header->nFilledLen = p_block->i_buffer;
        p_header->nOffset = 0;
        p_header->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
        p_header->nTimeStamp = ToOmxTicks(p_block->i_dts);

        /* In direct mode we pass the input pointer as is.
         * Otherwise we memcopy the data */
        if(p_sys->in.b_direct)
        {
            p_header->pOutputPortPrivate = p_header->pBuffer;
            p_header->pBuffer = p_block->p_buffer;
            p_header->pAppPrivate = p_block;
        }
        else
        {
            if(p_header->nFilledLen > p_header->nAllocLen)
            {
                msg_Dbg(p_dec, "buffer too small (%u,%u)",
                        (unsigned)p_header->nFilledLen,
                        (unsigned)p_header->nAllocLen);
                p_header->nFilledLen = p_header->nAllocLen;
            }
            memcpy(p_header->pBuffer, p_block->p_buffer, p_header->nFilledLen );
            block_Release(p_block);
        }

        OMX_DBG( "EmptyThisBuffer %p, %p, %u", (void *)p_header,
                 (void *)p_header->pBuffer, (unsigned)p_header->nFilledLen );
        OMX_EmptyThisBuffer(p_sys->omx_handle, p_header);
        p_sys->in.b_flushed = false;
        p_block = NULL;
    }

reconfig:
    /* Handle the PortSettingsChanged events */
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];
        if(!p_port->b_reconfigure) continue;
        p_port->b_reconfigure = 0;
        omx_error = PortReconfigure(p_dec, p_port);
        CHECK_ERROR(omx_error, "PortReconfigure failed");
    }

    return VLCDEC_SUCCESS;
error:
    p_sys->b_error = true;
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * EncodeVideo: Called to encode one frame
 *****************************************************************************/
static block_t *EncodeVideo( encoder_t *p_enc, picture_t *p_pic )
{
    decoder_t *p_dec = ( decoder_t *)p_enc;
    decoder_sys_t *p_sys = p_dec->p_sys;
    OMX_ERRORTYPE omx_error;
    unsigned int i;

    OMX_BUFFERHEADERTYPE *p_header;
    block_t *p_block = 0;

    if( !p_pic ) return NULL;

    /* Check for errors from codec */
    if(p_sys->b_error)
    {
        msg_Dbg(p_dec, "error during encoding");
        return NULL;
    }

    /* Send the input buffer to the component */
    OMX_FIFO_GET(&p_sys->in.fifo, p_header);
    if(p_header)
    {
        /* In direct mode we pass the input pointer as is.
         * Otherwise we memcopy the data */
        if(p_sys->in.b_direct)
        {
            p_header->pOutputPortPrivate = p_header->pBuffer;
            p_header->pBuffer = p_pic->p[0].p_pixels;
        }
        else
        {
            CopyVlcPicture(p_dec, p_header, p_pic);
        }

        p_header->nFilledLen = p_sys->in.i_frame_size;
        p_header->nOffset = 0;
        p_header->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
        p_header->nTimeStamp = ToOmxTicks(p_pic->date);
        OMX_DBG( "EmptyThisBuffer %p, %p, %u", (void *)p_header,
                 (void *)p_header->pBuffer, (unsigned)p_header->nFilledLen );
        OMX_EmptyThisBuffer(p_sys->omx_handle, p_header);
        p_sys->in.b_flushed = false;
    }

    /* Handle the PortSettingsChanged events */
    for(i = 0; i < p_sys->ports; i++)
    {
        OmxPort *p_port = &p_sys->p_ports[i];
        if(!p_port->b_reconfigure) continue;
        p_port->b_reconfigure = 0;
        omx_error = PortReconfigure(p_dec, p_port);
        CHECK_ERROR(omx_error, "PortReconfigure failed");
    }

    /* Wait for the decoded frame */
    while(!p_block)
    {
        OMX_FIFO_GET(&p_sys->out.fifo, p_header);

        if(p_header->nFilledLen)
        {
            if(p_header->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
            {
                /* TODO: need to store codec config */
                msg_Dbg(p_dec, "received codec config %i", (int)p_header->nFilledLen);
            }

            p_block = p_header->pAppPrivate;
            if(!p_block)
            {
                /* We're not in direct rendering mode.
                 * Get a new block and copy the content */
                p_block = block_Alloc( p_header->nFilledLen );
                memcpy(p_block->p_buffer, p_header->pBuffer, p_header->nFilledLen );
            }

            p_block->i_buffer = p_header->nFilledLen;
            p_block->i_pts = p_block->i_dts = FromOmxTicks(p_header->nTimeStamp);
            p_header->nFilledLen = 0;
            p_header->pAppPrivate = 0;
        }

        OMX_DBG( "FillThisBuffer %p, %p", (void *)p_header,
                 (void *)p_header->pBuffer );
        OMX_FillThisBuffer(p_sys->omx_handle, p_header);
    }

    msg_Dbg(p_dec, "done");
    return p_block;
error:
    p_sys->b_error = true;
    return NULL;
}

/*****************************************************************************
 * CloseGeneric: omxil decoder destruction
 *****************************************************************************/
static void CloseGeneric( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if(p_sys->omx_handle) DeinitialiseComponent(p_dec, p_sys->omx_handle);

    DeinitOmxCore();

    DeinitOmxEventQueue(&p_sys->event_queue);

    OMX_FIFO_DESTROY( &p_sys->in.fifo );
    OMX_FIFO_DESTROY( &p_sys->out.fifo );

    free( p_sys );
}

/*****************************************************************************
 * OmxEventHandler:
 *****************************************************************************/
static OMX_ERRORTYPE OmxEventHandler( OMX_HANDLETYPE omx_handle,
    OMX_PTR app_data, OMX_EVENTTYPE event, OMX_U32 data_1,
    OMX_U32 data_2, OMX_PTR event_data )
{
    decoder_t *p_dec = (decoder_t *)app_data;
    decoder_sys_t *p_sys = p_dec->p_sys;
    unsigned int i;
    (void)omx_handle;

    PrintOmxEvent((vlc_object_t *) p_dec, event, data_1, data_2, event_data);
    switch (event)
    {
    case OMX_EventError:
        //p_sys->b_error = true;
        break;

    case OMX_EventPortSettingsChanged:
        if( data_2 == 0 || data_2 == OMX_IndexParamPortDefinition ||
            data_2 == OMX_IndexParamAudioPcm )
        {
            OMX_BUFFERHEADERTYPE *sentinel;
            for(i = 0; i < p_sys->ports; i++)
                if(p_sys->p_ports[i].definition.eDir == OMX_DirOutput)
                    p_sys->p_ports[i].b_reconfigure = true;
            sentinel = calloc(1, sizeof(*sentinel));
            if (sentinel) {
                sentinel->nFlags = SENTINEL_FLAG;
                OMX_FIFO_PUT(&p_sys->in.fifo, sentinel);
            }
        }
        else if( data_2 == OMX_IndexConfigCommonOutputCrop )
        {
            for(i = 0; i < p_sys->ports; i++)
                if(p_sys->p_ports[i].definition.nPortIndex == data_1)
                    p_sys->p_ports[i].b_update_def = true;
        }
        else
        {
            msg_Dbg( p_dec, "Unhandled setting change %x", (unsigned int)data_2 );
        }
        break;
    case OMX_EventParamOrConfigChanged:
        UpdatePixelAspect(p_dec);
        break;

    default:
        break;
    }

    PostOmxEvent(&p_sys->event_queue, event, data_1, data_2, event_data);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE OmxEmptyBufferDone( OMX_HANDLETYPE omx_handle,
    OMX_PTR app_data, OMX_BUFFERHEADERTYPE *omx_header )
{
    decoder_t *p_dec = (decoder_t *)app_data;
    decoder_sys_t *p_sys = p_dec->p_sys;
    (void)omx_handle;

    OMX_DBG( "OmxEmptyBufferDone %p, %p", (void *)omx_header,
             (void *)omx_header->pBuffer );

    if(omx_header->pAppPrivate || omx_header->pOutputPortPrivate)
    {
        block_t *p_block = (block_t *)omx_header->pAppPrivate;
        omx_header->pBuffer = omx_header->pOutputPortPrivate;
        if(p_block) block_Release(p_block);
        omx_header->pAppPrivate = 0;
    }
    OMX_FIFO_PUT(&p_sys->in.fifo, omx_header);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE OmxFillBufferDone( OMX_HANDLETYPE omx_handle,
    OMX_PTR app_data, OMX_BUFFERHEADERTYPE *omx_header )
{
    decoder_t *p_dec = (decoder_t *)app_data;
    decoder_sys_t *p_sys = p_dec->p_sys;
    (void)omx_handle;

    OMX_DBG( "OmxFillBufferDone %p, %p, %u, %"PRId64, (void *)omx_header,
             (void *)omx_header->pBuffer, (unsigned)omx_header->nFilledLen,
             FromOmxTicks(omx_header->nTimeStamp) );

    if(omx_header->pInputPortPrivate)
    {
        omx_header->pBuffer = omx_header->pInputPortPrivate;
    }
    OMX_FIFO_PUT(&p_sys->out.fifo, omx_header);

    return OMX_ErrorNone;
}
