/*****************************************************************************
 * mmal.c: MMAL-based vout plugin for Raspberry Pi
 *****************************************************************************
 * Copyright © 2014 jusst technologies GmbH
 *
 * Authors: Dennis Hamester <dennis.hamester@gmail.com>
 *          Julian Scheel <julian@jusst.de>
 *          John Cox <jc@kynesim.co.uk>
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
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>

#include "mmal_picture.h"

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_default_components.h>

#include "subpic.h"

#define MAX_BUFFERS_IN_TRANSIT 1
#define VC_TV_MAX_MODE_IDS 127

#define MMAL_LAYER_NAME "mmal-layer"
#define MMAL_LAYER_TEXT N_("VideoCore layer where the video is displayed.")
#define MMAL_LAYER_LONGTEXT N_("VideoCore layer where the video is displayed. Subpictures are displayed directly above and a black background directly below.")

#define MMAL_DISPLAY_NAME "mmal-display"
#define MMAL_DISPLAY_TEXT N_("Output device for Rpi fullscreen.")
#define MMAL_DISPLAY_LONGTEXT N_("Output device for Rpi fullscreen. " \
"Valid values are HDMI-1,HDMI-2.  By default HDMI-1.")

#define MMAL_ADJUST_REFRESHRATE_NAME "mmal-adjust-refreshrate"
#define MMAL_ADJUST_REFRESHRATE_TEXT N_("Adjust HDMI refresh rate to the video.")
#define MMAL_ADJUST_REFRESHRATE_LONGTEXT N_("Adjust HDMI refresh rate to the video.")

#define MMAL_NATIVE_INTERLACED "mmal-native-interlaced"
#define MMAL_NATIVE_INTERLACE_TEXT N_("Force interlaced HDMI mode.")
#define MMAL_NATIVE_INTERLACE_LONGTEXT N_("Force the HDMI output into an " \
        "interlaced video mode for interlaced video content.")

/* Ideal rendering phase target is at rough 25% of frame duration */
#define PHASE_OFFSET_TARGET ((double)0.25)
#define PHASE_CHECK_INTERVAL 100

static int OpenMmalVout(vout_display_t *, const vout_display_cfg_t *,
                video_format_t *, vlc_video_context *);

#define SUBS_MAX 4


vlc_module_begin()
    set_shortname(N_("MMAL vout"))
    set_description(N_("MMAL-based vout plugin for Raspberry Pi"))
    add_shortcut("mmal_vout")
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VOUT )

    add_integer(MMAL_LAYER_NAME, 1, MMAL_LAYER_TEXT, MMAL_LAYER_LONGTEXT, false)
    add_bool(MMAL_ADJUST_REFRESHRATE_NAME, false, MMAL_ADJUST_REFRESHRATE_TEXT,
                    MMAL_ADJUST_REFRESHRATE_LONGTEXT, false)
    add_bool(MMAL_NATIVE_INTERLACED, false, MMAL_NATIVE_INTERLACE_TEXT,
                    MMAL_NATIVE_INTERLACE_LONGTEXT, false)
    add_string(MMAL_DISPLAY_NAME, "auto", MMAL_DISPLAY_TEXT,
                    MMAL_DISPLAY_LONGTEXT, false)
    set_callback_display(OpenMmalVout, 16)  // 1 point better than ASCII art
vlc_module_end()

typedef struct vout_subpic_s {
    MMAL_COMPONENT_T *component;
    subpic_reg_stash_t sub;
} vout_subpic_t;

struct vout_display_sys_t {
    vlc_mutex_t manage_mutex;

    vlc_decoder_device *dec_dev;
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *input;
    MMAL_POOL_T *pool; /* mmal buffer headers, used for pushing pictures to component*/
    int i_planes; /* Number of actually used planes, 1 for opaque, 3 for i420 */

    int buffers_in_transit; /* number of buffers currently pushed to mmal component */
    unsigned num_buffers; /* number of buffers allocated at mmal port */

    int display_id;
    unsigned display_width;
    unsigned display_height;

    MMAL_RECT_T dest_rect;      // Output rectangle in display coords

    unsigned int i_frame_rate_base; /* cached framerate to detect changes for rate adjustment */
    unsigned int i_frame_rate;

    int next_phase_check; /* lowpass for phase check frequency */
    int phase_offset; /* currently applied offset to presentation time in ns */
    int layer; /* the dispman layer (z-index) used for video rendering */

    bool need_configure_display; /* indicates a required display reconfigure to main thread */
    bool adjust_refresh_rate;
    bool native_interlaced;
    bool b_top_field_first; /* cached interlaced settings to detect changes for native mode */
    bool b_progressive;
    bool force_config;

    vout_subpic_t subs[SUBS_MAX];
    // Stash for subpics derived from the passed subpicture rather than
    // included with the main pic
    MMAL_BUFFER_HEADER_T * subpic_bufs[SUBS_MAX];

    picture_pool_t * pic_pool;

    struct vout_isp_conf_s {
        MMAL_COMPONENT_T *component;
        MMAL_PORT_T * input;
        MMAL_PORT_T * output;
        MMAL_QUEUE_T * out_q;
        MMAL_POOL_T * in_pool;
        MMAL_POOL_T * out_pool;
        bool pending;
    } isp;

    MMAL_POOL_T * copy_pool;
    MMAL_BUFFER_HEADER_T * copy_buf;

    // Subpic blend if we have to do it here
    vzc_pool_ctl_t * vzc;
};


// ISP setup

static bool want_copy(const video_format_t * const fmt)
{
    return (fmt->i_chroma == VLC_CODEC_I420 || fmt->i_chroma == VLC_CODEC_I420_10L);
}

static vlc_fourcc_t req_chroma(const video_format_t * const fmt)
{
    return hw_mmal_chroma_is_mmal(fmt->i_chroma) || want_copy(fmt) ?
        fmt->i_chroma : VLC_CODEC_I420;
}

static MMAL_FOURCC_T vout_vlc_to_mmal_pic_fourcc(const unsigned int fcc)
{
    switch (fcc){
    case VLC_CODEC_MMAL_OPAQUE:
        return MMAL_ENCODING_OPAQUE;
    case VLC_CODEC_I420:
        return MMAL_ENCODING_I420;
    default:
        break;
    }
    return MMAL_ENCODING_I420;
}

static void display_set_format(const vout_display_t * const vd, MMAL_ES_FORMAT_T *const es_fmt, const bool is_intermediate)
{
    const unsigned int w = is_intermediate ? vd->fmt.i_visible_width  : vd->fmt.i_width ;
    const unsigned int h = is_intermediate ? vd->fmt.i_visible_height : vd->fmt.i_height;
    MMAL_VIDEO_FORMAT_T * const v_fmt = &es_fmt->es->video;

    es_fmt->type = MMAL_ES_TYPE_VIDEO;
    es_fmt->encoding = is_intermediate ? MMAL_ENCODING_I420 : vout_vlc_to_mmal_pic_fourcc(vd->fmt.i_chroma);
    es_fmt->encoding_variant = 0;

    v_fmt->width  = (w + 31) & ~31;
    v_fmt->height = (h + 15) & ~15;
    v_fmt->crop.x = 0;
    v_fmt->crop.y = 0;
    v_fmt->crop.width = w;
    v_fmt->crop.height = h;
    if (vd->fmt.i_sar_num == 0 || vd->fmt.i_sar_den == 0) {
        v_fmt->par.num        = 1;
        v_fmt->par.den        = 1;
    } else {
        v_fmt->par.num        = vd->fmt.i_sar_num;
        v_fmt->par.den        = vd->fmt.i_sar_den;
    }
    v_fmt->frame_rate.num = vd->fmt.i_frame_rate;
    v_fmt->frame_rate.den = vd->fmt.i_frame_rate_base;
    v_fmt->color_space    = vlc_to_mmal_color_space(vd->fmt.space);
}

static void display_src_rect(const vout_display_t * const vd, MMAL_RECT_T *const rect)
{
    const bool wants_isp = false;
    rect->x = wants_isp ? 0 : vd->fmt.i_x_offset;
    rect->y = wants_isp ? 0 : vd->fmt.i_y_offset;
    rect->width = vd->fmt.i_visible_width;
    rect->height = vd->fmt.i_visible_height;
}

static void isp_input_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
    VLC_UNUSED(port);

    mmal_buffer_header_release(buf);
}

static void isp_control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    vout_display_t *vd = (vout_display_t *)port->userdata;
    MMAL_STATUS_T status;

    if (buffer->cmd == MMAL_EVENT_ERROR) {
        status = *(uint32_t *)buffer->data;
        msg_Err(vd, "MMAL error %"PRIx32" \"%s\"", status, mmal_status_to_string(status));
    }

    mmal_buffer_header_release(buffer);
}

static void isp_output_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
    if (buf->cmd == 0 && buf->length != 0)
    {
        // The filter structure etc. should always exist if we have contents
        // but might not on later flushes as we shut down
        vout_display_t * const vd = (vout_display_t *)port->userdata;
        struct vout_isp_conf_s *const isp = &vd->sys->isp;

        mmal_queue_put(isp->out_q, buf);
    }
    else
    {
        mmal_buffer_header_reset(buf);
        mmal_buffer_header_release(buf);
    }
}

static void isp_empty_out_q(struct vout_isp_conf_s * const isp)
{
    MMAL_BUFFER_HEADER_T * buf;
    // We can be called as part of error recovery so allow for missing Q
    if (isp->out_q == NULL)
        return;

    while ((buf = mmal_queue_get(isp->out_q)) != NULL)
        mmal_buffer_header_release(buf);
}

static void isp_flush(struct vout_isp_conf_s * const isp)
{
    if (!isp->input->is_enabled)
        mmal_port_disable(isp->input);

    if (isp->output->is_enabled)
        mmal_port_disable(isp->output);

    isp_empty_out_q(isp);
    isp->pending = false;
}

static MMAL_STATUS_T isp_prepare(vout_display_t * const vd, struct vout_isp_conf_s * const isp)
{
    MMAL_STATUS_T err;
    MMAL_BUFFER_HEADER_T * buf;

    if (!isp->output->is_enabled) {
        if ((err = mmal_port_enable(isp->output, isp_output_cb)) != MMAL_SUCCESS)
        {
            msg_Err(vd, "ISP output port enable failed");
            return err;
        }
    }

    while ((buf = mmal_queue_get(isp->out_pool->queue)) != NULL) {
        if ((err = mmal_port_send_buffer(isp->output, buf)) != MMAL_SUCCESS)
        {
            msg_Err(vd, "ISP output port stuff failed");
            return err;
        }
    }

    if (!isp->input->is_enabled) {
        if ((err = mmal_port_enable(isp->input, isp_input_cb)) != MMAL_SUCCESS)
        {
            msg_Err(vd, "ISP input port enable failed");
            return err;
        }
    }
    return MMAL_SUCCESS;
}

static void isp_close(vout_display_t * const vd, vout_display_sys_t * const vd_sys)
{
    struct vout_isp_conf_s * const isp = &vd_sys->isp;
    VLC_UNUSED(vd);

    if (isp->component == NULL)
        return;

    isp_flush(isp);

    if (isp->component->control->is_enabled)
        mmal_port_disable(isp->component->control);

    if (isp->out_q != NULL) {
        // 1st junk anything lying around
        isp_empty_out_q(isp);

        mmal_queue_destroy(isp->out_q);
        isp->out_q = NULL;
    }

    if (isp->out_pool != NULL) {
        mmal_port_pool_destroy(isp->output, isp->out_pool);
        isp->out_pool = NULL;
    }

    isp->input = NULL;
    isp->output = NULL;

    mmal_component_release(isp->component);
    isp->component = NULL;

    return;
}

// Restuff into output rather than return to pool is we can
static MMAL_BOOL_T isp_out_pool_cb(MMAL_POOL_T *pool, MMAL_BUFFER_HEADER_T *buffer, void *userdata)
{
    struct vout_isp_conf_s * const isp = userdata;
    VLC_UNUSED(pool);
    if (isp->output->is_enabled) {
        mmal_buffer_header_reset(buffer);
        if (mmal_port_send_buffer(isp->output, buffer) == MMAL_SUCCESS)
            return MMAL_FALSE;
    }
    return MMAL_TRUE;
}

static MMAL_STATUS_T isp_setup(vout_display_t * const vd, vout_display_sys_t * const vd_sys)
{
    struct vout_isp_conf_s * const isp = &vd_sys->isp;
    MMAL_STATUS_T err;

    if ((err = mmal_component_create(MMAL_COMPONENT_ISP_RESIZER, &isp->component)) != MMAL_SUCCESS) {
        msg_Err(vd, "Cannot create ISP component");
        return err;
    }
    isp->input = isp->component->input[0];
    isp->output = isp->component->output[0];

    isp->component->control->userdata = (void *)vd;
    if ((err = mmal_port_enable(isp->component->control, isp_control_port_cb)) != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to enable ISP control port");
        goto fail;
    }

    isp->input->userdata = (void *)vd;
    display_set_format(vd, isp->input->format, false);

    if ((err = port_parameter_set_bool(isp->input, MMAL_PARAMETER_ZERO_COPY, true)) != MMAL_SUCCESS)
        goto fail;

    if ((err = mmal_port_format_commit(isp->input)) != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to set ISP input format");
        goto fail;
    }

    isp->input->buffer_size = isp->input->buffer_size_recommended;
    isp->input->buffer_num = 30;

    if ((isp->in_pool = mmal_pool_create(isp->input->buffer_num, 0)) == NULL)
    {
        msg_Err(vd, "Failed to create input pool");
        goto fail;
    }

    if ((isp->out_q = mmal_queue_create()) == NULL)
    {
        err = MMAL_ENOMEM;
        goto fail;
    }

    display_set_format(vd, isp->output->format, true);

    if ((err = port_parameter_set_bool(isp->output, MMAL_PARAMETER_ZERO_COPY, true)) != MMAL_SUCCESS)
        goto fail;

    if ((err = mmal_port_format_commit(isp->output)) != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to set ISP input format");
        goto fail;
    }

    isp->output->buffer_size = isp->output->buffer_size_recommended;
    isp->output->buffer_num = 2;
    isp->output->userdata = (void *)vd;

    if ((isp->out_pool = mmal_port_pool_create(isp->output, isp->output->buffer_num, isp->output->buffer_size)) == NULL)
    {
        msg_Err(vd, "Failed to make ISP port pool");
        goto fail;
    }

    mmal_pool_callback_set(isp->out_pool, isp_out_pool_cb, isp);

    if ((err = isp_prepare(vd, isp)) != MMAL_SUCCESS)
        goto fail;

    return MMAL_SUCCESS;

fail:
    isp_close(vd, vd_sys);
    return err;
}

static MMAL_STATUS_T isp_check(vout_display_t * const vd, vout_display_sys_t * const vd_sys)
{
    struct vout_isp_conf_s *const isp = &vd_sys->isp;
    const bool has_isp = (isp->component != NULL);
    const bool wants_isp = false;

    if (has_isp == wants_isp)
    {
        // All OK - do nothing
    }
    else if (has_isp)
    {
        // ISP active but we don't want it
        isp_flush(isp);

        // Check we have everything back and then kill it
        if (mmal_queue_length(isp->out_pool->queue) == isp->output->buffer_num)
            isp_close(vd, vd_sys);
    }
    else
    {
        // ISP closed but we want it
        return isp_setup(vd, vd_sys);
    }

    return MMAL_SUCCESS;
}

/* TV service */
static void tvservice_cb(void *callback_data, uint32_t reason, uint32_t param1,
                uint32_t param2);
static void adjust_refresh_rate(vout_display_t *vd, const video_format_t *fmt);
static int set_latency_target(vout_display_t *vd, bool enable);

// Mmal
static void maintain_phase_sync(vout_display_t *vd);



static void vd_input_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
    VLC_UNUSED(port);

    mmal_buffer_header_release(buf);
}

static int query_resolution(vout_display_t *vd, const int display_id, unsigned *width, unsigned *height)
{
    TV_DISPLAY_STATE_T display_state = {0};
    int ret = 0;

    if (vc_tv_get_display_state_id(display_id, &display_state) == 0) {
        msg_Dbg(vd, "State=%#x", display_state.state);
        if (display_state.state & 0xFF) {
            msg_Dbg(vd, "HDMI: %dx%d", display_state.display.hdmi.width, display_state.display.hdmi.height);
            *width = display_state.display.hdmi.width;
            *height = display_state.display.hdmi.height;
        } else if (display_state.state & 0xFF00) {
            msg_Dbg(vd, "SDTV: %dx%d", display_state.display.sdtv.width, display_state.display.sdtv.height);
            *width = display_state.display.sdtv.width;
            *height = display_state.display.sdtv.height;
        } else {
            msg_Warn(vd, "Invalid display state %"PRIx32, display_state.state);
            ret = -1;
        }
    } else {
        msg_Warn(vd, "Failed to query display resolution");
        ret = -1;
    }

    return ret;
}

static MMAL_RECT_T
place_to_mmal_rect(const vout_display_place_t place)
{
    return (MMAL_RECT_T){
        .x      = place.x,
        .y      = place.y,
        .width  = place.width,
        .height = place.height
    };
}

static void
place_dest(vout_display_sys_t * const sys,
           const vout_display_cfg_t * const cfg, const video_format_t * fmt)
{
    video_format_t tfmt;

    // Fix SAR if unknown
    if (fmt->i_sar_den == 0 || fmt->i_sar_num == 0) {
        tfmt = *fmt;
        tfmt.i_sar_den = 1;
        tfmt.i_sar_num = 1;
        fmt = &tfmt;
    }

    // Ignore what VLC thinks might be going on with display size
    vout_display_cfg_t tcfg = *cfg;
    vout_display_place_t place;
    tcfg.display.width = sys->display_width;
    tcfg.display.height = sys->display_height;
    tcfg.is_display_filled = true;
    vout_display_PlacePicture(&place, fmt, &tcfg);

    sys->dest_rect = place_to_mmal_rect(place);
}



static int configure_display(vout_display_t *vd, const vout_display_cfg_t *cfg,
                const video_format_t *fmt)
{
    vout_display_sys_t * const sys = vd->sys;
    MMAL_DISPLAYREGION_T display_region;
    MMAL_STATUS_T status;

    if (!cfg && !fmt)
    {
        msg_Err(vd, "Missing cfg & fmt");
        return -EINVAL;
    }

    isp_check(vd, sys);

    if (fmt) {
        sys->input->format->es->video.par.num = fmt->i_sar_num;
        sys->input->format->es->video.par.den = fmt->i_sar_den;

        status = mmal_port_format_commit(sys->input);
        if (status != MMAL_SUCCESS) {
            msg_Err(vd, "Failed to commit format for input port %s (status=%"PRIx32" %s)",
                            sys->input->name, status, mmal_status_to_string(status));
            return -EINVAL;
        }
    } else {
        fmt = &vd->source;
    }

    if (!cfg)
        cfg = vd->cfg;

    place_dest(sys, cfg, fmt);

    display_region.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
    display_region.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
    display_region.fullscreen = MMAL_FALSE;
    display_src_rect(vd, &display_region.src_rect);
    display_region.dest_rect = sys->dest_rect;
    display_region.layer = sys->layer;
    display_region.alpha = 0xff | (1 << 29);
    display_region.set = MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_SRC_RECT |
            MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_ALPHA;
    status = mmal_port_parameter_set(sys->input, &display_region.hdr);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to set display region (status=%"PRIx32" %s)",
                        status, mmal_status_to_string(status));
        return -EINVAL;
    }

    sys->adjust_refresh_rate = var_InheritBool(vd, MMAL_ADJUST_REFRESHRATE_NAME);
    sys->native_interlaced = var_InheritBool(vd, MMAL_NATIVE_INTERLACED);
    if (sys->adjust_refresh_rate) {
        adjust_refresh_rate(vd, fmt);
        set_latency_target(vd, true);
    }

    return 0;
}

static void kill_pool(vout_display_sys_t * const sys)
{
    if (sys->pic_pool != NULL) {
        picture_pool_Release(sys->pic_pool);
        sys->pic_pool = NULL;
    }
}

static void vd_display(vout_display_t *vd, picture_t *p_pic)
{
    vout_display_sys_t * const sys = vd->sys;
    MMAL_STATUS_T err;

    if (!sys->input->is_enabled &&
        (err = mmal_port_enable(sys->input, vd_input_port_cb)) != MMAL_SUCCESS)
    {
        msg_Err(vd, "Input port enable failed");
        goto fail;
    }
    // Stuff into input
    // We assume the BH is already set up with values reflecting pic date etc.
    if (sys->copy_buf != NULL) {
        MMAL_BUFFER_HEADER_T *const buf = sys->copy_buf;
        sys->copy_buf = NULL;
        if (mmal_port_send_buffer(sys->input, buf) != MMAL_SUCCESS)
        {
            mmal_buffer_header_release(buf);
            msg_Err(vd, "Send copy buffer to render input failed");
            goto fail;
        }
    }
    else if (sys->isp.pending) {
        MMAL_BUFFER_HEADER_T *const buf = mmal_queue_wait(sys->isp.out_q);
        sys->isp.pending = false;
        if (mmal_port_send_buffer(sys->input, buf) != MMAL_SUCCESS)
        {
            mmal_buffer_header_release(buf);
            msg_Err(vd, "Send ISP buffer to render input failed");
            goto fail;
        }
    }
    else
    {
        MMAL_BUFFER_HEADER_T *const pic_buf = hw_mmal_pic_buf_replicated(p_pic, sys->pool);
        if (pic_buf == NULL)
        {
            msg_Err(vd, "Replicated buffer get fail");
            goto fail;
        }


        // If dimensions have chnaged then fix that
        if (hw_mmal_vlc_pic_to_mmal_fmt_update(sys->input->format, p_pic))
        {
            msg_Dbg(vd, "Reset port format");

            // HVS can deal with on-line dimension changes
            if (mmal_port_format_commit(sys->input) != MMAL_SUCCESS)
                msg_Warn(vd, "Input format commit failed");
        }

        if ((err = mmal_port_send_buffer(sys->input, pic_buf)) != MMAL_SUCCESS)
        {
            mmal_buffer_header_release(pic_buf);
            msg_Err(vd, "Send buffer to input failed");
            goto fail;
        }
    }

    {
        unsigned int sub_no = 0;
        MMAL_BUFFER_HEADER_T **psub_bufs2 = sys->subpic_bufs;
        const bool is_mmal_pic = hw_mmal_chroma_is_mmal(p_pic->format.i_chroma);

        for (sub_no = 0; sub_no != SUBS_MAX; ++sub_no) {
            int rv;
            MMAL_BUFFER_HEADER_T * const sub_buf = !is_mmal_pic ? NULL :
                hw_mmal_pic_sub_buf_get(p_pic, sub_no);

            if ((rv = hw_mmal_subpic_update(VLC_OBJECT(vd),
                                            sub_buf != NULL ? sub_buf : *psub_bufs2++,
                                            &sys->subs[sub_no].sub,
                                            &p_pic->format,
                                            &sys->dest_rect,
                                            p_pic->date)) == 0)
                break;
            else if (rv < 0)
                goto fail;
        }
    }

fail:
    for (unsigned int i = 0; i != SUBS_MAX && sys->subpic_bufs[i] != NULL; ++i) {
        mmal_buffer_header_release(sys->subpic_bufs[i]);
        sys->subpic_bufs[i] = NULL;
    }

    if (sys->next_phase_check == 0 && sys->adjust_refresh_rate)
        maintain_phase_sync(vd);
    sys->next_phase_check = (sys->next_phase_check + 1) % PHASE_CHECK_INTERVAL;
}

static int vd_control(vout_display_t *vd, int query, va_list args)
{
    vout_display_sys_t * const sys = vd->sys;
    int ret = VLC_EGENERIC;
    VLC_UNUSED(args);

    switch (query) {
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        {
            const vout_display_cfg_t *cfg = va_arg(args, const vout_display_cfg_t *);
            if (configure_display(vd, cfg, NULL) >= 0)
                ret = VLC_SUCCESS;
            break;
        }

        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
            if (configure_display(vd, NULL, &vd->source) >= 0)
                ret = VLC_SUCCESS;
            break;

        case VOUT_DISPLAY_RESET_PICTURES:
            msg_Warn(vd, "Reset Pictures");
            kill_pool(sys);
            vd->fmt = vd->source; // Take (nearly) whatever source wants to give us
            vd->fmt.i_chroma = req_chroma(&vd->fmt);  // Adjust chroma to something we can actually deal with
            ret = VLC_SUCCESS;
            break;

        case VOUT_DISPLAY_CHANGE_ZOOM:
            msg_Warn(vd, "Unsupported control query %d", query);
            ret = VLC_SUCCESS;
            break;

        default:
            msg_Warn(vd, "Unknown control query %d", query);
            break;
    }

    return ret;
}

static void vd_manage(vout_display_t *vd)
{
    vout_display_sys_t *sys = vd->sys;
    unsigned width, height;

    vlc_mutex_lock(&sys->manage_mutex);

    if (sys->need_configure_display) {
        if (query_resolution(vd, sys->display_id, &width, &height) >= 0) {
            sys->display_width = width;
            sys->display_height = height;
//            vout_window_ReportSize(vd->cfg->window, width, height);
        }

        sys->need_configure_display = false;
    }

    vlc_mutex_unlock(&sys->manage_mutex);
}


static int attach_subpics(vout_display_t * const vd, vout_display_sys_t * const sys,
                          subpicture_t * const subpicture)
{
    unsigned int n = 0;

    if (sys->vzc == NULL) {
        mmal_decoder_device_t *devsys = GetMMALDeviceOpaque(sys->dec_dev);
        if ((sys->vzc = hw_mmal_vzc_pool_new(devsys->is_cma)) == NULL)
        {
            msg_Err(vd, "Failed to allocate VZC");
            return VLC_ENOMEM;
        }
    }

    // Attempt to import the subpics
    for (subpicture_t * spic = subpicture; spic != NULL; spic = spic->p_next)
    {
        for (subpicture_region_t *sreg = spic->p_region; sreg != NULL; sreg = sreg->p_next) {
            picture_t *const src = sreg->p_picture;

            // At this point I think the subtitles are being placed in the
            // coord space of the cfg rectangle
            if ((sys->subpic_bufs[n] = hw_mmal_vzc_buf_from_pic(sys->vzc,
                src,
                (MMAL_RECT_T){.width = vd->cfg->display.width, .height=vd->cfg->display.height},
                sreg->i_x, sreg->i_y,
                sreg->i_alpha,
                n == 0)) == NULL)
            {
                msg_Err(vd, "Failed to allocate vzc buffer for subpic");
                return VLC_ENOMEM;
            }

            if (++n == SUBS_MAX)
                return VLC_SUCCESS;
        }
    }
    return VLC_SUCCESS;
}


static void vd_prepare(vout_display_t *vd, picture_t *p_pic,
                       subpicture_t *subpicture, vlc_tick_t date)
{
    VLC_UNUSED(date);
    vout_display_sys_t * const sys = vd->sys;

    vd_manage(vd);

    if (sys->force_config ||
        p_pic->format.i_frame_rate != sys->i_frame_rate ||
        p_pic->format.i_frame_rate_base != sys->i_frame_rate_base ||
        p_pic->b_progressive != sys->b_progressive ||
        p_pic->b_top_field_first != sys->b_top_field_first)
    {
        sys->force_config = false;
        sys->b_top_field_first = p_pic->b_top_field_first;
        sys->b_progressive = p_pic->b_progressive;
        sys->i_frame_rate = p_pic->format.i_frame_rate;
        sys->i_frame_rate_base = p_pic->format.i_frame_rate_base;
        configure_display(vd, NULL, &p_pic->format);
    }

    // Subpics can either turn up attached to the main pic or in the
    // subpic list here  - if they turn up here then process into temp
    // buffers
    if (subpicture != NULL) {
        attach_subpics(vd, sys, subpicture);
    }

    // *****
    if (want_copy(&vd->fmt)) {
        if (sys->copy_buf != NULL) {
            msg_Err(vd, "Copy buf not NULL");
            mmal_buffer_header_release(sys->copy_buf);
            sys->copy_buf = NULL;
        }

        MMAL_BUFFER_HEADER_T * const buf = mmal_queue_wait(sys->copy_pool->queue);
        // Copy 2d
        mmal_decoder_device_t *devsys = GetMMALDeviceOpaque(sys->dec_dev);
        hw_mmal_copy_pic_to_buf(buf->data, &buf->length, sys->input->format, p_pic,
                                devsys->is_cma);
        buf->flags = MMAL_BUFFER_HEADER_FLAG_FRAME_END;

        sys->copy_buf = buf;
    }

    if (isp_check(vd, sys) != MMAL_SUCCESS) {
        return;
    }
}


static void vd_control_port_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    vout_display_t *vd = (vout_display_t *)port->userdata;
    MMAL_STATUS_T status;

    if (buffer->cmd == MMAL_EVENT_ERROR) {
        status = *(uint32_t *)buffer->data;
        msg_Err(vd, "MMAL error %"PRIx32" \"%s\"", status, mmal_status_to_string(status));
    }

    mmal_buffer_header_release(buffer);
}

static void tvservice_cb(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2)
{
    VLC_UNUSED(reason);
    VLC_UNUSED(param1);
    VLC_UNUSED(param2);

    vout_display_t *vd = (vout_display_t *)callback_data;
    vout_display_sys_t *sys = vd->sys;

    vlc_mutex_lock(&sys->manage_mutex);
    sys->need_configure_display = true;
    vlc_mutex_unlock(&sys->manage_mutex);
}

static int set_latency_target(vout_display_t *vd, bool enable)
{
    vout_display_sys_t *sys = vd->sys;
    MMAL_STATUS_T status;

    MMAL_PARAMETER_AUDIO_LATENCY_TARGET_T latency_target = {
        .hdr = { MMAL_PARAMETER_AUDIO_LATENCY_TARGET, sizeof(latency_target) },
        .enable = enable ? MMAL_TRUE : MMAL_FALSE,
        .filter = 2,
        .target = 4000,
        .shift = 3,
        .speed_factor = -135,
        .inter_factor = 500,
        .adj_cap = 20
    };

    status = mmal_port_parameter_set(sys->input, &latency_target.hdr);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to configure latency target on input port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void adjust_refresh_rate(vout_display_t *vd, const video_format_t *fmt)
{
    vout_display_sys_t *sys = vd->sys;
    TV_DISPLAY_STATE_T display_state;
    TV_SUPPORTED_MODE_NEW_T supported_modes[VC_TV_MAX_MODE_IDS];
    char response[20]; /* answer is hvs_update_fields=%1d */
    int num_modes;
    double frame_rate = (double)fmt->i_frame_rate / fmt->i_frame_rate_base;
    int best_id = -1;
    double best_score, score;
    int i;

    vc_tv_get_display_state_id(sys->display_id, &display_state);
    if(display_state.display.hdmi.mode != HDMI_MODE_OFF) {
        num_modes = vc_tv_hdmi_get_supported_modes_new_id(sys->display_id, display_state.display.hdmi.group,
                        supported_modes, VC_TV_MAX_MODE_IDS, NULL, NULL);

        for (i = 0; i < num_modes; ++i) {
            TV_SUPPORTED_MODE_NEW_T *mode = &supported_modes[i];
            if (!sys->native_interlaced) {
                if (mode->width != display_state.display.hdmi.width ||
                                mode->height != display_state.display.hdmi.height ||
                                mode->scan_mode == HDMI_INTERLACED)
                    continue;
            } else {
                if (mode->width != vd->fmt.i_visible_width ||
                        mode->height != vd->fmt.i_visible_height)
                    continue;
                if (mode->scan_mode != sys->b_progressive ? HDMI_NONINTERLACED : HDMI_INTERLACED)
                    continue;
            }

            score = fmod(supported_modes[i].frame_rate, frame_rate);
            if((best_id < 0) || (score < best_score)) {
                best_id = i;
                best_score = score;
            }
        }

        if((best_id >= 0) && (display_state.display.hdmi.mode != supported_modes[best_id].code)) {
            msg_Info(vd, "Setting HDMI refresh rate to %"PRIu32,
                            supported_modes[best_id].frame_rate);
            vc_tv_hdmi_power_on_explicit_new_id(sys->display_id, HDMI_MODE_HDMI,
                            supported_modes[best_id].group,
                            supported_modes[best_id].code);
        }

        if (sys->native_interlaced &&
                supported_modes[best_id].scan_mode == HDMI_INTERLACED) {
            char hvs_mode = sys->b_top_field_first ? '1' : '2';
            if (vc_gencmd(response, sizeof(response), "hvs_update_fields %c",
                    hvs_mode) != 0 || response[18] != hvs_mode)
                msg_Warn(vd, "Could not set hvs field mode");
            else
                msg_Info(vd, "Configured hvs field mode for interlaced %s playback",
                        sys->b_top_field_first ? "tff" : "bff");
        }
    }
}

static void maintain_phase_sync(vout_display_t *vd)
{
    MMAL_PARAMETER_VIDEO_RENDER_STATS_T render_stats = {
        .hdr = { MMAL_PARAMETER_VIDEO_RENDER_STATS, sizeof(render_stats) },
    };
    int32_t frame_duration = CLOCK_FREQ /
        ((double)vd->sys->i_frame_rate /
        vd->sys->i_frame_rate_base);
    vout_display_sys_t *sys = vd->sys;
    int32_t phase_offset;
    MMAL_STATUS_T status;

    status = mmal_port_parameter_get(sys->input, &render_stats.hdr);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to read render stats on control port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
        return;
    }

    if (render_stats.valid) {
#ifndef NDEBUG
        msg_Dbg(vd, "render_stats: match: %u, period: %u ms, phase: %u ms, hvs: %u",
                render_stats.match, render_stats.period / 1000, render_stats.phase / 1000,
                render_stats.hvs_status);
#endif

        if (render_stats.phase > 0.1 * frame_duration &&
                render_stats.phase < 0.75 * frame_duration)
            return;

        phase_offset = frame_duration * PHASE_OFFSET_TARGET - render_stats.phase;
        if (phase_offset < 0)
            phase_offset += frame_duration;
        else
            phase_offset %= frame_duration;

        sys->phase_offset += phase_offset;
        sys->phase_offset %= frame_duration;
        msg_Dbg(vd, "Apply phase offset of %"PRId32" ms (total offset %"PRId32" ms)",
                phase_offset / 1000, sys->phase_offset / 1000);

        /* Reset the latency target, so that it does not get confused
         * by the jump in the offset */
        set_latency_target(vd, false);
        set_latency_target(vd, true);
    }
}

static void CloseMmalVout(vout_display_t * vd)
{
    vout_display_sys_t * const sys = vd->sys;
    char response[20]; /* answer is hvs_update_fields=%1d */

    kill_pool(sys);

    vc_tv_unregister_callback_full(tvservice_cb, vd);

    // Shouldn't be anything here - but just in case
    for (unsigned int i = 0; i != SUBS_MAX; ++i)
        if (sys->subpic_bufs[i] != NULL)
            mmal_buffer_header_release(sys->subpic_bufs[i]);

    for (unsigned int i = 0; i != SUBS_MAX; ++i) {
        vout_subpic_t * const sub = sys->subs + i;
        if (sub->component != NULL) {
            hw_mmal_subpic_close(&sub->sub);
            if (sub->component->control->is_enabled)
                mmal_port_disable(sub->component->control);
            if (sub->component->is_enabled)
                mmal_component_disable(sub->component);
            mmal_component_release(sub->component);
            sub->component = NULL;
        }
    }

    if (sys->input && sys->input->is_enabled)
        mmal_port_disable(sys->input);

    if (sys->component && sys->component->control->is_enabled)
        mmal_port_disable(sys->component->control);

    if (sys->copy_buf != NULL)
        mmal_buffer_header_release(sys->copy_buf);

    if (sys->input != NULL && sys->copy_pool != NULL)
        mmal_port_pool_destroy(sys->input, sys->copy_pool);

    if (sys->component && sys->component->is_enabled)
        mmal_component_disable(sys->component);

    if (sys->pool)
        mmal_pool_destroy(sys->pool);

    if (sys->component)
        mmal_component_release(sys->component);

    isp_close(vd, sys);

    hw_mmal_vzc_pool_release(sys->vzc);

    if (sys->native_interlaced) {
        if (vc_gencmd(response, sizeof(response), "hvs_update_fields 0") < 0 ||
                response[18] != '0')
            msg_Warn(vd, "Could not reset hvs field mode");
    }

    if (sys->dec_dev)
        vlc_decoder_device_Release(sys->dec_dev);

    free(sys);
}


static const struct {
    const char * name;
    int num;
} display_name_to_num[] = {
    {"auto",    -1},
    {"hdmi-1",  DISPMANX_ID_HDMI0},
    {"hdmi-2",  DISPMANX_ID_HDMI1},
    {NULL,      -2}
};

static int find_display_num(const char * name)
{
    unsigned int i;
    for (i = 0; display_name_to_num[i].name != NULL && strcasecmp(display_name_to_num[i].name, name) != 0; ++i)
        /* Loop */;
    return display_name_to_num[i].num;
}

static int OpenMmalVout(vout_display_t *vd, const vout_display_cfg_t *cfg,
                        video_format_t *fmtp, vlc_video_context *vctx)
{
    vout_display_sys_t *sys;
    MMAL_DISPLAYREGION_T display_region;
    MMAL_STATUS_T status;
    int ret = VLC_EGENERIC;
    // At the moment all copy is via I420
    const bool needs_copy = !hw_mmal_chroma_is_mmal(vd->fmt.i_chroma);
    const MMAL_FOURCC_T enc_in = needs_copy ? MMAL_ENCODING_I420 :
        vout_vlc_to_mmal_pic_fourcc(vd->fmt.i_chroma);

    sys = calloc(1, sizeof(struct vout_display_sys_t));
    if (!sys)
        return VLC_ENOMEM;
    vd->sys = sys;

    if (vctx)
    {
        sys->dec_dev = vlc_video_context_HoldDevice(vctx);
        if (sys->dec_dev && sys->dec_dev->type != VLC_DECODER_DEVICE_MMAL)
        {
            vlc_decoder_device_Release(sys->dec_dev);
            sys->dec_dev = NULL;
        }
    }

    if (sys->dec_dev == NULL)
        sys->dec_dev = vlc_decoder_device_Create(VLC_OBJECT(vd), cfg->window);
    if (sys->dec_dev == NULL || sys->dec_dev->type != VLC_DECODER_DEVICE_MMAL)
    {
        msg_Err(vd, "Missing decoder device");
        goto fail;
    }

    vlc_mutex_init(&sys->manage_mutex);

    vc_tv_register_callback(tvservice_cb, vd);

    sys->layer = var_InheritInteger(vd, MMAL_LAYER_NAME);

    {
        const char *display_name = var_InheritString(vd, MMAL_DISPLAY_NAME);
        int qt_num = -1;
        int display_id = find_display_num(display_name);
//        sys->display_id = display_id < 0 ? vc_tv_get_default_display_id() : display_id;
        sys->display_id = display_id >= 0 ? display_id : DISPMANX_ID_HDMI;
        if (display_id < -1)
            msg_Warn(vd, "Unknown display device: '%s'", display_name);
        else
            msg_Dbg(vd, "Display device: %s, qt=%d id=%d display=%d", display_name,
                    qt_num, display_id, sys->display_id);
    }

    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to create MMAL component %s (status=%"PRIx32" %s)",
                        MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, status, mmal_status_to_string(status));
        goto fail;
    }

    sys->component->control->userdata = (struct MMAL_PORT_USERDATA_T *)vd;
    status = mmal_port_enable(sys->component->control, vd_control_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to enable control port %s (status=%"PRIx32" %s)",
                        sys->component->control->name, status, mmal_status_to_string(status));
        goto fail;
    }

    sys->input = sys->component->input[0];
    sys->input->userdata = (struct MMAL_PORT_USERDATA_T *)vd;

    sys->input->format->encoding = enc_in;
    sys->input->format->encoding_variant = 0;
    sys->i_planes = 1;

    display_set_format(vd, sys->input->format, false);

    status = port_parameter_set_bool(sys->input, MMAL_PARAMETER_ZERO_COPY, true);
    if (status != MMAL_SUCCESS) {
       msg_Err(vd, "Failed to set zero copy on port %s (status=%"PRIx32" %s)",
                sys->input->name, status, mmal_status_to_string(status));
       goto fail;
    }

    status = mmal_port_format_commit(sys->input);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to commit format for input port %s (status=%"PRIx32" %s)",
                        sys->input->name, status, mmal_status_to_string(status));
        goto fail;
    }

    sys->input->buffer_size = sys->input->buffer_size_recommended;

    if (!needs_copy) {
        sys->input->buffer_num = 30;
    }
    else {
        sys->input->buffer_num = 2;
        if ((sys->copy_pool = mmal_port_pool_create(sys->input, 2, sys->input->buffer_size)) == NULL)
        {
            msg_Err(vd, "Cannot create copy pool");
            goto fail;
        }
    }

    if (query_resolution(vd, sys->display_id, &sys->display_width, &sys->display_height) < 0)
    {
        sys->display_width = vd->cfg->display.width;
        sys->display_height = vd->cfg->display.height;
    }

    place_dest(sys, vd->cfg, &vd->source);  // Sets sys->dest_rect

    display_region.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
    display_region.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
    display_region.display_num = sys->display_id;
    display_region.fullscreen = MMAL_FALSE;
    display_src_rect(vd, &display_region.src_rect);
    display_region.dest_rect = sys->dest_rect;
    display_region.layer = sys->layer;
    display_region.set =
        MMAL_DISPLAY_SET_NUM |
        MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_SRC_RECT |
        MMAL_DISPLAY_SET_DEST_RECT | MMAL_DISPLAY_SET_LAYER;
    status = mmal_port_parameter_set(sys->input, &display_region.hdr);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to set display region (status=%"PRIx32" %s)",
                        status, mmal_status_to_string(status));
        goto fail;
    }

    status = mmal_port_enable(sys->input, vd_input_port_cb);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to enable input port %s (status=%"PRIx32" %s)",
                sys->input->name, status, mmal_status_to_string(status));
        goto fail;
    }

    status = mmal_component_enable(sys->component);
    if (status != MMAL_SUCCESS) {
        msg_Err(vd, "Failed to enable component %s (status=%"PRIx32" %s)",
                sys->component->name, status, mmal_status_to_string(status));
        goto fail;
    }

    if ((sys->pool = mmal_pool_create(sys->input->buffer_num, 0)) == NULL)
    {
        msg_Err(vd, "Failed to create input pool");
        goto fail;
    }

    for (unsigned int i = 0; i != SUBS_MAX; ++i) {
        vout_subpic_t * const sub = sys->subs + i;
        if ((status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &sub->component)) != MMAL_SUCCESS)
        {
            msg_Dbg(vd, "Failed to create subpic component %d", i);
            goto fail;
        }
        sub->component->control->userdata = (struct MMAL_PORT_USERDATA_T *)vd;
        if ((status = mmal_port_enable(sub->component->control, vd_control_port_cb)) != MMAL_SUCCESS) {
            msg_Err(vd, "Failed to enable control port %s on sub %d (status=%"PRIx32" %s)",
                            sys->component->control->name, i, status, mmal_status_to_string(status));
            goto fail;
        }
        if ((status = hw_mmal_subpic_open(VLC_OBJECT(vd), &sub->sub, sub->component->input[0],
                                          sys->display_id, sys->layer + i + 1)) != MMAL_SUCCESS) {
            msg_Dbg(vd, "Failed to open subpic %d", i);
            goto fail;
        }
        if ((status = mmal_component_enable(sub->component)) != MMAL_SUCCESS)
        {
            msg_Dbg(vd, "Failed to enable subpic component %d", i);
            goto fail;
        }
    }

    // If we can't deal with it directly ask for I420
    fmtp->i_chroma = req_chroma(fmtp);

    vd->info = (vout_display_info_t){
        .subpicture_chromas = hw_mmal_vzc_subpicture_chromas
    };

    vd->prepare = vd_prepare;
    vd->display = vd_display;
    vd->control = vd_control;
    vd->close = CloseMmalVout;


    return VLC_SUCCESS;

fail:
    CloseMmalVout(vd);

    return ret;
}

