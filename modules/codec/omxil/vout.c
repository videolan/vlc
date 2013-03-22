/*****************************************************************************
 * vout.c: OpenMAX IL video output
 *****************************************************************************
 * Copyright © 2013 VideoLAN
 *
 * Authors: Martin Storsjo <martin@martin.st>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>

#include "omxil.h"
#include "omxil_core.h"
#include "OMX_Broadcom.h"

// Defined in the broadcom version of OMX_Index.h
#define OMX_IndexConfigDisplayRegion 0x7f000010

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open (vlc_object_t *);
static void Close(vlc_object_t *);

vlc_module_begin()
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_shortname("omxil_vout")
    set_description(N_("OpenMAX IL video output"))
    set_capability("vout display", 0)
    set_callbacks(Open, Close)
vlc_module_end()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static picture_pool_t   *Pool  (vout_display_t *, unsigned);
static void             Display(vout_display_t *, picture_t *, subpicture_t *);
static int              Control(vout_display_t *, int, va_list);

/* */
struct vout_display_sys_t {
    picture_pool_t *pool;

    OMX_HANDLETYPE omx_handle;

    char psz_component[OMX_MAX_STRINGNAME_SIZE];

    OmxPort port;

    OmxEventQueue event_queue;
};

struct picture_sys_t {
    OMX_BUFFERHEADERTYPE *buf;
    vout_display_sys_t *sys;
};

static int LockSurface(picture_t *);
static void UnlockSurface(picture_t *);

static OMX_ERRORTYPE OmxEventHandler(OMX_HANDLETYPE, OMX_PTR, OMX_EVENTTYPE,
                                     OMX_U32, OMX_U32, OMX_PTR);
static OMX_ERRORTYPE OmxEmptyBufferDone(OMX_HANDLETYPE, OMX_PTR,
                                        OMX_BUFFERHEADERTYPE *);
static OMX_ERRORTYPE OmxFillBufferDone(OMX_HANDLETYPE, OMX_PTR,
                                       OMX_BUFFERHEADERTYPE *);

static OMX_ERRORTYPE OmxEventHandler(OMX_HANDLETYPE omx_handle,
    OMX_PTR app_data, OMX_EVENTTYPE event, OMX_U32 data_1,
    OMX_U32 data_2, OMX_PTR event_data)
{
    vout_display_t *vd = (vout_display_t *)app_data;
    vout_display_sys_t *p_sys = vd->sys;
    (void)omx_handle;

    PrintOmxEvent((vlc_object_t *) vd, event, data_1, data_2, event_data);
    PostOmxEvent(&p_sys->event_queue, event, data_1, data_2, event_data);
    return OMX_ErrorNone;
}

static OMX_ERRORTYPE OmxEmptyBufferDone(OMX_HANDLETYPE omx_handle,
    OMX_PTR app_data, OMX_BUFFERHEADERTYPE *omx_header)
{
    vout_display_t *vd = (vout_display_t *)app_data;
    vout_display_sys_t *p_sys = vd->sys;
    (void)omx_handle;

#ifndef NDEBUG
    msg_Dbg(vd, "OmxEmptyBufferDone %p, %p", omx_header, omx_header->pBuffer);
#endif

    OMX_FIFO_PUT(&p_sys->port.fifo, omx_header);

    return OMX_ErrorNone;
}

static OMX_ERRORTYPE OmxFillBufferDone(OMX_HANDLETYPE omx_handle,
    OMX_PTR app_data, OMX_BUFFERHEADERTYPE *omx_header)
{
    (void)omx_handle;
    (void)app_data;
    (void)omx_header;

    return OMX_ErrorNone;
}

static int Open(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_t *p_dec = vd;
    char ppsz_components[MAX_COMPONENTS_LIST_SIZE][OMX_MAX_STRINGNAME_SIZE];
    picture_t** pictures = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *def;

    static OMX_CALLBACKTYPE callbacks =
        { OmxEventHandler, OmxEmptyBufferDone, OmxFillBufferDone };

    if (InitOmxCore(p_this) != VLC_SUCCESS)
        return VLC_EGENERIC;

    int components = CreateComponentsList(p_this, "iv_renderer", ppsz_components);
    if (components <= 0) {
        DeinitOmxCore();
        return VLC_EGENERIC;
    }

    /* Allocate structure */
    vout_display_sys_t *p_sys = (struct vout_display_sys_t*) calloc(1, sizeof(*p_sys));
    if (!p_sys) {
        DeinitOmxCore();
        return VLC_ENOMEM;
    }

    vd->sys     = p_sys;
    strcpy(p_sys->psz_component, ppsz_components[0]);

    /* Load component */
    OMX_ERRORTYPE omx_error = pf_get_handle(&p_sys->omx_handle,
                                            p_sys->psz_component, vd, &callbacks);
    CHECK_ERROR(omx_error, "OMX_GetHandle(%s) failed (%x: %s)",
                p_sys->psz_component, omx_error, ErrorToString(omx_error));

    InitOmxEventQueue(&p_sys->event_queue);
    vlc_mutex_init (&p_sys->port.fifo.lock);
    vlc_cond_init (&p_sys->port.fifo.wait);
    p_sys->port.fifo.offset = offsetof(OMX_BUFFERHEADERTYPE, pOutputPortPrivate) / sizeof(void *);
    p_sys->port.fifo.pp_last = &p_sys->port.fifo.p_first;
    p_sys->port.b_direct = false;
    p_sys->port.b_flushed = true;

    OMX_PORT_PARAM_TYPE param;
    OMX_INIT_STRUCTURE(param);
    omx_error = OMX_GetParameter(p_sys->omx_handle, OMX_IndexParamVideoInit, &param);
    CHECK_ERROR(omx_error, "OMX_GetParameter(OMX_IndexParamVideoInit) failed (%x: %s)",
                omx_error, ErrorToString(omx_error));

    p_sys->port.i_port_index = param.nStartPortNumber;
    p_sys->port.b_valid = true;
    p_sys->port.omx_handle = p_sys->omx_handle;

    def = &p_sys->port.definition;
    OMX_INIT_STRUCTURE(*def);
    def->nPortIndex = p_sys->port.i_port_index;
    omx_error = OMX_GetParameter(p_sys->omx_handle, OMX_IndexParamPortDefinition, def);
    CHECK_ERROR(omx_error, "OMX_GetParameter(OMX_IndexParamPortDefinition) failed (%x: %s)",
                omx_error, ErrorToString(omx_error));

#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))

    def->format.video.nFrameWidth = vd->fmt.i_width;
    def->format.video.nFrameHeight = vd->fmt.i_height;
    def->format.video.nStride = 0;
    def->format.video.nSliceHeight = 0;
    p_sys->port.definition.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;

    if (!strcmp(p_sys->psz_component, "OMX.broadcom.video_render")) {
        def->format.video.nSliceHeight = ALIGN(def->format.video.nFrameHeight, 16);
    }

    omx_error = OMX_SetParameter(p_sys->omx_handle, OMX_IndexParamPortDefinition, &p_sys->port.definition);
    CHECK_ERROR(omx_error, "OMX_SetParameter(OMX_IndexParamPortDefinition) failed (%x: %s)",
                omx_error, ErrorToString(omx_error));
    OMX_GetParameter(p_sys->omx_handle, OMX_IndexParamPortDefinition, &p_sys->port.definition);

    if (def->format.video.nStride < (int) def->format.video.nFrameWidth)
        def->format.video.nStride = def->format.video.nFrameWidth;
    if (def->format.video.nSliceHeight < def->format.video.nFrameHeight)
        def->format.video.nSliceHeight = def->format.video.nFrameHeight;

    p_sys->port.pp_buffers =
            malloc(p_sys->port.definition.nBufferCountActual *
                   sizeof(OMX_BUFFERHEADERTYPE*));
    p_sys->port.i_buffers = p_sys->port.definition.nBufferCountActual;

    omx_error = OMX_SendCommand(p_sys->omx_handle, OMX_CommandStateSet, OMX_StateIdle, 0);
    CHECK_ERROR(omx_error, "OMX_CommandStateSet Idle failed (%x: %s)",
                omx_error, ErrorToString(omx_error));

    unsigned int i;
    for (i = 0; i < p_sys->port.i_buffers; i++) {
        omx_error = OMX_AllocateBuffer(p_sys->omx_handle, &p_sys->port.pp_buffers[i],
                                       p_sys->port.i_port_index, 0,
                                       p_sys->port.definition.nBufferSize);
        if (omx_error != OMX_ErrorNone)
            break;
        OMX_FIFO_PUT(&p_sys->port.fifo, p_sys->port.pp_buffers[i]);
    }
    if (omx_error != OMX_ErrorNone) {
        p_sys->port.i_buffers = i;
        for (i = 0; i < p_sys->port.i_buffers; i++)
            OMX_FreeBuffer(p_sys->omx_handle, p_sys->port.i_port_index, p_sys->port.pp_buffers[i]);
        msg_Err(vd, "OMX_AllocateBuffer failed (%x: %s)",
                omx_error, ErrorToString(omx_error));
        goto error;
    }

    omx_error = WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, 0, 0, 0);
    CHECK_ERROR(omx_error, "Wait for Idle failed (%x: %s)",
                omx_error, ErrorToString(omx_error));

    omx_error = OMX_SendCommand(p_sys->omx_handle, OMX_CommandStateSet,
                                OMX_StateExecuting, 0);
    CHECK_ERROR(omx_error, "OMX_CommandStateSet Executing failed (%x: %s)",
                omx_error, ErrorToString(omx_error));
    omx_error = WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, 0, 0, 0);
    CHECK_ERROR(omx_error, "Wait for Executing failed (%x: %s)",
                omx_error, ErrorToString(omx_error));

    if (!strcmp(p_sys->psz_component, "OMX.broadcom.video_render")) {
        OMX_CONFIG_DISPLAYREGIONTYPE config_display;
        OMX_INIT_STRUCTURE(config_display);
        config_display.nPortIndex = p_sys->port.i_port_index;

        config_display.set = OMX_DISPLAY_SET_SRC_RECT;
        config_display.src_rect.width = vd->cfg->display.width;
        config_display.src_rect.height = vd->cfg->display.height;
        OMX_SetConfig(p_sys->omx_handle, OMX_IndexConfigDisplayRegion, &config_display);
        config_display.set = OMX_DISPLAY_SET_FULLSCREEN;
        config_display.fullscreen = OMX_TRUE;
        OMX_SetConfig(p_sys->omx_handle, OMX_IndexConfigDisplayRegion, &config_display);

        if (vd->fmt.i_width != vd->cfg->display.width || vd->fmt.i_height != vd->cfg->display.height) {
            config_display.set = OMX_DISPLAY_SET_PIXEL;
            config_display.pixel_x = vd->cfg->display.width  * vd->fmt.i_height;
            config_display.pixel_y = vd->cfg->display.height * vd->fmt.i_width;
            OMX_SetConfig(p_sys->omx_handle, OMX_IndexConfigDisplayRegion, &config_display);
        }
    }


    /* Setup chroma */
    video_format_t fmt = vd->fmt;

    fmt.i_chroma = VLC_CODEC_I420;
    video_format_FixRgb(&fmt);

    /* Setup vout_display */
    vd->fmt     = fmt;
    vd->pool    = Pool;
    vd->display = Display;
    vd->control = Control;
    vd->prepare = NULL;
    vd->manage  = NULL;

    /* Create the associated picture */
    pictures = calloc(sizeof(*pictures), p_sys->port.i_buffers);
    if (!pictures)
        goto error;
    for (unsigned int i = 0; i < p_sys->port.i_buffers; i++) {
        picture_resource_t resource = { 0 };
        picture_resource_t *rsc = &resource;
        rsc->p_sys = malloc(sizeof(*rsc->p_sys));
        if (!rsc->p_sys)
            goto error;
        rsc->p_sys->sys = p_sys;

        for (int i = 0; i < PICTURE_PLANE_MAX; i++) {
            rsc->p[i].p_pixels = NULL;
            rsc->p[i].i_pitch = 0;
            rsc->p[i].i_lines = 0;
        }
        picture_t *picture = picture_NewFromResource(&fmt, rsc);
        if (!picture)
            goto error;
        pictures[i] = picture;
    }

    /* Wrap it into a picture pool */
    picture_pool_configuration_t pool_cfg;
    memset(&pool_cfg, 0, sizeof(pool_cfg));
    pool_cfg.picture_count = p_sys->port.i_buffers;
    pool_cfg.picture       = pictures;
    pool_cfg.lock          = LockSurface;
    pool_cfg.unlock        = UnlockSurface;

    p_sys->pool = picture_pool_NewExtended(&pool_cfg);
    if (!p_sys->pool) {
        for (unsigned int i = 0; i < p_sys->port.i_buffers; i++)
            picture_Release(pictures[i]);
        goto error;
    }

    /* Fix initial state */
    vout_display_SendEventFullscreen(vd, true);

    free(pictures);
    return VLC_SUCCESS;

error:
    free(pictures);
    Close(p_this);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *p_this)
{
    vout_display_t *vd = (vout_display_t *)p_this;
    vout_display_sys_t *p_sys = vd->sys;

    if (p_sys->omx_handle) {
        OMX_STATETYPE state;
        OMX_GetState(p_sys->omx_handle, &state);
        if (state == OMX_StateExecuting) {
            OMX_SendCommand(p_sys->omx_handle, OMX_CommandStateSet, OMX_StateIdle, 0);
            while (1) {
                OMX_U32 cmd, state;
                WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, &cmd, &state, 0);
                if (cmd == OMX_CommandStateSet && state == OMX_StateIdle)
                    break;
            }
        }
        OMX_GetState(p_sys->omx_handle, &state);
        if (state == OMX_StateIdle) {
            OMX_SendCommand(p_sys->omx_handle, OMX_CommandStateSet, OMX_StateLoaded, 0);
            for (unsigned int i = 0; i < p_sys->port.i_buffers; i++) {
                OMX_BUFFERHEADERTYPE *p_buffer;
                OMX_FIFO_GET(&p_sys->port.fifo, p_buffer);
                OMX_FreeBuffer(p_sys->omx_handle, p_sys->port.i_port_index, p_buffer);
            }
            WaitForSpecificOmxEvent(&p_sys->event_queue, OMX_EventCmdComplete, 0, 0, 0);
        }
        free(p_sys->port.pp_buffers);
        pf_free_handle(p_sys->omx_handle);
        DeinitOmxEventQueue(&p_sys->event_queue);
        vlc_mutex_destroy(&p_sys->port.fifo.lock);
        vlc_cond_destroy(&p_sys->port.fifo.wait);
    }

    if (p_sys->pool)
        picture_pool_Delete(p_sys->pool);
    free(p_sys);
    DeinitOmxCore();
}

static picture_pool_t *Pool(vout_display_t *vd, unsigned count)
{
    VLC_UNUSED(count);

    return vd->sys->pool;
}

static int LockSurface(picture_t *picture)
{
    picture_sys_t *picsys = picture->p_sys;
    vout_display_sys_t *p_sys = picsys->sys;
    OMX_BUFFERHEADERTYPE *p_buffer;

    OMX_FIFO_GET(&p_sys->port.fifo, p_buffer);
    for (int i = 0; i < 3; i++) {
        picture->p[i].p_pixels = p_buffer->pBuffer;
        picture->p[i].i_pitch = p_sys->port.definition.format.video.nStride;
        picture->p[i].i_lines = p_sys->port.definition.format.video.nSliceHeight;
        if (i > 0) {
            picture->p[i].p_pixels = picture->p[i-1].p_pixels + picture->p[i-1].i_pitch*picture->p[i-1].i_lines;
            picture->p[i].i_pitch /= 2;
            picture->p[i].i_lines /= 2;
        }
    }
    p_buffer->nOffset = 0;
    p_buffer->nFlags = 0;
    p_buffer->nTimeStamp = ToOmxTicks(0);
    p_buffer->nFilledLen = 0;
    picsys->buf = p_buffer;

    return VLC_SUCCESS;
}

static void UnlockSurface(picture_t *picture)
{
    picture_sys_t *picsys = picture->p_sys;
    vout_display_sys_t *p_sys = picsys->sys;
    OMX_BUFFERHEADERTYPE *p_buffer = picsys->buf;

    if (!p_buffer->nFilledLen)
        OMX_FIFO_PUT(&p_sys->port.fifo, p_buffer);
    else
        OMX_EmptyThisBuffer(p_sys->omx_handle, p_buffer);
}

static void Display(vout_display_t *vd, picture_t *picture, subpicture_t *subpicture)
{
    VLC_UNUSED(vd);
    VLC_UNUSED(subpicture);
    picture_sys_t *picsys = picture->p_sys;
    vout_display_sys_t *p_sys = picsys->sys;
    OMX_BUFFERHEADERTYPE *p_buffer = picsys->buf;

    p_buffer->nFilledLen = 3*p_sys->port.definition.format.video.nStride*p_sys->port.definition.format.video.nSliceHeight/2;
    p_buffer->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;

    /* refcount lowers to 0, and pool_cfg.unlock is called */

    picture_Release(picture);
}

static int Control(vout_display_t *vd, int query, va_list args)
{
    VLC_UNUSED(args);

    switch (query) {
    case VOUT_DISPLAY_HIDE_MOUSE:
        return VLC_SUCCESS;

    default:
        msg_Err(vd, "Unknown request in omxil vout display");

    case VOUT_DISPLAY_CHANGE_FULLSCREEN:
    case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
    case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
    case VOUT_DISPLAY_CHANGE_ZOOM:
    case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
    case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
    case VOUT_DISPLAY_GET_OPENGL:
        return VLC_EGENERIC;
    }
}
