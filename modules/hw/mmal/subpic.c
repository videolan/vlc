/*****************************************************************************
 * subpic.c:
 *****************************************************************************
 * Copyright © 2018-2020 John Cox
 *
 * Authors: John Cox <jc@kynesim.co.uk>
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
#include <vlc_codec.h>

#include <interface/mmal/mmal.h>

#include "mmal_picture.h"
#include "subpic.h"


static bool cmp_rect(const MMAL_RECT_T * const a, const MMAL_RECT_T * const b)
{
    return a->x == b->x && a->y == b->y && a->width == b->width && a->height == b->height;
}

void hw_mmal_subpic_flush(subpic_reg_stash_t * const sub)
{
    if (sub->port != NULL && sub->port->is_enabled)
        mmal_port_disable(sub->port);
    sub->seq = 0;
}

void hw_mmal_subpic_close(subpic_reg_stash_t * const spe)
{
    hw_mmal_subpic_flush(spe);

    if (spe->pool != NULL)
        mmal_pool_destroy(spe->pool);

    // Zap to avoid any accidental reuse
    *spe = (subpic_reg_stash_t){NULL};
}

MMAL_STATUS_T hw_mmal_subpic_open(vlc_object_t * const obj, subpic_reg_stash_t * const spe, MMAL_PORT_T * const port,
                                  const int display_id, const unsigned int layer)
{
    MMAL_STATUS_T err;

    // Start by zapping all to zero
    *spe = (subpic_reg_stash_t){NULL};

    if ((err = port_parameter_set_bool(port, MMAL_PARAMETER_ZERO_COPY, true)) != MMAL_SUCCESS)
    {
        msg_Err(obj, "Failed to set sub port zero copy");
        return err;
    }

    if ((spe->pool = mmal_pool_create(30, 0)) == NULL)
    {
        msg_Err(obj, "Failed to create sub pool");
        return MMAL_ENOMEM;
    }

    port->userdata = (void *)obj;
    spe->port = port;
    spe->display_id = display_id;
    spe->layer = layer;

    return MMAL_SUCCESS;
}

static void conv_subpic_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf)
{
    VLC_UNUSED(port);

    mmal_buffer_header_release(buf);  // Will extract & release pic in pool callback
}

static MMAL_STATUS_T port_send_replicated(MMAL_PORT_T * const port, MMAL_POOL_T * const rep_pool,
                                          MMAL_BUFFER_HEADER_T * const src_buf,
                                          const uint64_t seq)
{
    MMAL_STATUS_T err;
    MMAL_BUFFER_HEADER_T *const rep_buf = mmal_queue_wait(rep_pool->queue);

    if (rep_buf == NULL)
        return MMAL_ENOSPC;

    if ((err = mmal_buffer_header_replicate(rep_buf, src_buf)) != MMAL_SUCCESS)
        return err;

    rep_buf->pts = seq;

    if ((err = mmal_port_send_buffer(port, rep_buf)) != MMAL_SUCCESS)
    {
        mmal_buffer_header_release(rep_buf);
        return err;
    }

    return MMAL_SUCCESS;
}

int hw_mmal_subpic_update(vlc_object_t * const obj,
    MMAL_BUFFER_HEADER_T * const sub_buf,
    subpic_reg_stash_t * const spe,
    const video_format_t * const fmt,
    const MMAL_RECT_T * const scale_out,
    const uint64_t pts)
{
    MMAL_STATUS_T err;

    if (sub_buf == NULL)
    {
        if (spe->port->is_enabled && spe->seq != 0)
        {
            MMAL_BUFFER_HEADER_T *const buf = mmal_queue_wait(spe->pool->queue);

            if (buf == NULL) {
                msg_Err(obj, "Buffer get for subpic failed");
                return -1;
            }
            buf->cmd = 0;
            buf->data = NULL;
            buf->alloc_size = 0;
            buf->offset = 0;
            buf->flags = MMAL_BUFFER_HEADER_FLAG_FRAME_END;
            buf->pts = pts;
            buf->dts = MMAL_TIME_UNKNOWN;
            buf->user_data = NULL;

            if ((err = mmal_port_send_buffer(spe->port, buf)) != MMAL_SUCCESS)
            {
                msg_Err(obj, "Send buffer to subput failed");
                mmal_buffer_header_release(buf);
                return -1;
            }

            spe->seq = 0;
        }
    }
    else
    {
        const unsigned int seq = hw_mmal_vzc_buf_seq(sub_buf);
        bool needs_update = (spe->seq != seq);

        hw_mmal_vzc_buf_scale_dest_rect(sub_buf, scale_out);

        if (hw_mmal_vzc_buf_set_format(sub_buf, spe->port->format))
        {
            MMAL_DISPLAYREGION_T * const dreg = hw_mmal_vzc_buf_region(sub_buf);
            MMAL_VIDEO_FORMAT_T *const v_fmt = &spe->port->format->es->video;

            v_fmt->frame_rate.den = fmt->i_frame_rate_base;
            v_fmt->frame_rate.num = fmt->i_frame_rate;
            v_fmt->par.den        = fmt->i_sar_den;
            v_fmt->par.num        = fmt->i_sar_num;
            v_fmt->color_space = MMAL_COLOR_SPACE_UNKNOWN;

            if (needs_update || dreg->alpha != spe->alpha || !cmp_rect(&dreg->dest_rect, &spe->dest_rect)) {

                spe->alpha = dreg->alpha;
                spe->dest_rect = dreg->dest_rect;
                needs_update = true;

                if (spe->display_id >= 0)
                {
                    dreg->display_num = spe->display_id;
                    dreg->set |= MMAL_DISPLAY_SET_NUM;
                }
                dreg->layer = spe->layer;
                dreg->set |= MMAL_DISPLAY_SET_LAYER;

                if ((err = mmal_port_parameter_set(spe->port, &dreg->hdr)) != MMAL_SUCCESS)
                {
                    msg_Err(obj, "Set display region on subput failed");
                    return -1;
                }

                if ((err = mmal_port_format_commit(spe->port)) != MMAL_SUCCESS)
                {
                    msg_Dbg(obj, "Subpic commit fail: %d", err);
                    return -1;
                }
            }
        }

        if (!spe->port->is_enabled)
        {
            spe->port->buffer_num = 30;
            spe->port->buffer_size = spe->port->buffer_size_recommended;  // Not used but shuts up the error checking

            if ((err = mmal_port_enable(spe->port, conv_subpic_cb)) != MMAL_SUCCESS)
            {
                msg_Dbg(obj, "Subpic enable fail: %d", err);
                return -1;
            }
        }

        if (needs_update)
        {
            if ((err = port_send_replicated(spe->port, spe->pool, sub_buf, pts)) != MMAL_SUCCESS)
            {
                msg_Err(obj, "Send buffer to subput failed");
                return -1;
            }

            spe->seq = seq;
        }
    }
    return 1;
}



