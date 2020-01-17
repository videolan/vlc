/*****************************************************************************
 * mmal_picture.h: Shared header for MMAL pictures
 *****************************************************************************
 * Copyright © 2014 jusst technologies GmbH
 *
 * Authors: Julian Scheel <julian@jusst.de>
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

#ifndef VLC_MMAL_MMAL_PICTURE_H_
#define VLC_MMAL_MMAL_PICTURE_H_

#include <vlc_common.h>
#include <vlc_codec.h>
#include <interface/mmal/mmal.h>

#include "mmal_cma.h"

/* Think twice before changing this. Incorrect values cause havoc. */
#define NUM_ACTUAL_OPAQUE_BUFFERS 30


MMAL_FOURCC_T vlc_to_mmal_video_fourcc(const video_frame_format_t * const vf_vlc);
MMAL_FOURCC_T vlc_to_mmal_color_space(const video_color_space_t vlc_cs);
void hw_mmal_vlc_fmt_to_mmal_fmt(MMAL_ES_FORMAT_T *const es_fmt, const video_frame_format_t * const vf_vlc);
// Returns true if fmt_changed
// frame_rate ignored for compare, but is set if something else is updated
bool hw_mmal_vlc_pic_to_mmal_fmt_update(MMAL_ES_FORMAT_T *const es_fmt, const picture_t * const pic);

// Copy pic contents into an existing buffer
int hw_mmal_copy_pic_to_buf(void * const buf_data, uint32_t * const pLength,
                            const MMAL_ES_FORMAT_T * const fmt, const picture_t * const pic,
                            bool is_cma);

//----------------------------------------------------------------------------

struct mmal_port_pool_ref_s;
typedef struct mmal_port_pool_ref_s hw_mmal_port_pool_ref_t;

void hw_mmal_port_pool_ref_release(hw_mmal_port_pool_ref_t * const ppr, const bool in_cb);
bool hw_mmal_port_pool_ref_recycle(hw_mmal_port_pool_ref_t * const ppr, MMAL_BUFFER_HEADER_T * const buf);
MMAL_STATUS_T hw_mmal_port_pool_ref_fill(hw_mmal_port_pool_ref_t * const ppr);
MMAL_STATUS_T hw_mmal_opaque_output(vlc_object_t * const obj,
                                    hw_mmal_port_pool_ref_t ** pppr,
                                    MMAL_PORT_T * const port,
                                    const unsigned int extra_buffers, MMAL_PORT_BH_CB_T callback);

MMAL_BUFFER_HEADER_T * hw_mmal_pic_sub_buf_get(picture_t * const pic, const unsigned int n);

static inline bool hw_mmal_chroma_is_mmal(const vlc_fourcc_t chroma)
{
    return
        chroma == VLC_CODEC_MMAL_OPAQUE;
}

picture_context_t * hw_mmal_gen_context(
    MMAL_BUFFER_HEADER_T * buf, hw_mmal_port_pool_ref_t * const ppr);

int hw_mmal_get_gpu_mem(void);


static inline MMAL_STATUS_T port_parameter_set_uint32(MMAL_PORT_T * port, uint32_t id, uint32_t val)
{
    const MMAL_PARAMETER_UINT32_T param = {
        .hdr = {.id = id, .size = sizeof(MMAL_PARAMETER_UINT32_T)},
        .value = val
    };
    return mmal_port_parameter_set(port, &param.hdr);
}

static inline MMAL_STATUS_T port_parameter_set_bool(MMAL_PORT_T * const port, const uint32_t id, const bool val)
{
    const MMAL_PARAMETER_BOOLEAN_T param = {
        .hdr = {.id = id, .size = sizeof(MMAL_PARAMETER_BOOLEAN_T)},
        .enable = val
    };
    return mmal_port_parameter_set(port, &param.hdr);
}

static inline void buf_to_pic_copy_props(picture_t * const pic, const MMAL_BUFFER_HEADER_T * const buf)
{
    // Contrary to docn the interlace & tff flags turn up in the header flags rather than the
    // video specific flags (which appear to be currently unused).
    pic->b_progressive = (buf->flags & MMAL_BUFFER_HEADER_VIDEO_FLAG_INTERLACED) == 0;
    pic->b_top_field_first = (buf->flags & MMAL_BUFFER_HEADER_VIDEO_FLAG_TOP_FIELD_FIRST) != 0;

    pic->date = buf->pts != MMAL_TIME_UNKNOWN ? buf->pts :
        buf->dts != MMAL_TIME_UNKNOWN ? buf->dts :
            VLC_TICK_INVALID;
}

MMAL_BUFFER_HEADER_T * hw_mmal_pic_buf_copied(const picture_t *const pic,
                                              MMAL_POOL_T * const rep_pool,
                                              MMAL_PORT_T * const port,
                                              cma_buf_pool_t * const cbp,
                                              bool is_cma);

MMAL_BUFFER_HEADER_T * hw_mmal_pic_buf_replicated(const picture_t *const pic, MMAL_POOL_T * const rep_pool);

//----------------------------------------------------------------------------

// At the moment we cope with any mono-planar RGBA thing
// We could cope with many other things but they currently don't occur
extern const vlc_fourcc_t hw_mmal_vzc_subpicture_chromas[];

bool hw_mmal_vzc_buf_set_format(MMAL_BUFFER_HEADER_T * const buf, MMAL_ES_FORMAT_T * const es_fmt);
MMAL_DISPLAYREGION_T * hw_mmal_vzc_buf_region(MMAL_BUFFER_HEADER_T * const buf);
void hw_mmal_vzc_buf_scale_dest_rect(MMAL_BUFFER_HEADER_T * const buf, const MMAL_RECT_T * const scale_rect);
unsigned int hw_mmal_vzc_buf_seq(MMAL_BUFFER_HEADER_T * const buf);

//----------------------------------------------------------------------------

struct vzc_pool_ctl_s;
typedef struct vzc_pool_ctl_s vzc_pool_ctl_t;

MMAL_BUFFER_HEADER_T * hw_mmal_vzc_buf_from_pic(vzc_pool_ctl_t * const pc, picture_t * const pic,
                                                const MMAL_RECT_T dst_pic_rect,
                                                const int x_offset, const int y_offset,
                                                const unsigned int alpha, const bool is_first);

void hw_mmal_vzc_pool_flush(vzc_pool_ctl_t * const pc);
void hw_mmal_vzc_pool_release(vzc_pool_ctl_t * const pc);
void hw_mmal_vzc_pool_ref(vzc_pool_ctl_t * const pc);
vzc_pool_ctl_t * hw_mmal_vzc_pool_new(bool is_cma);


//----------------------------------------------------------------------------

#define NUM_DECODER_BUFFER_HEADERS 30

bool rpi_is_model_pi4(void);


#define MMAL_COMPONENT_DEFAULT_RESIZER "vc.ril.resize"
#define MMAL_COMPONENT_ISP_RESIZER     "vc.ril.isp"
#define MMAL_COMPONENT_HVS             "vc.ril.hvs"

//----------------------------------------------------------------------------

typedef struct
{
    bool is_cma; // legacy MMAL mode otherwise
} mmal_decoder_device_t;

static inline mmal_decoder_device_t *GetMMALDeviceOpaque(vlc_decoder_device *dec_dev)
{
    if (!dec_dev || dec_dev->type != VLC_DECODER_DEVICE_MMAL)
        return NULL;
    return dec_dev->opaque;
}

#endif
