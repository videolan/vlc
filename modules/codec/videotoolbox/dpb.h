/*****************************************************************************
 * dpb.h: decoder picture output pacing
 *****************************************************************************
 * Copyright © 2015-2023 VideoLabs, VideoLAN and VLC authors
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
#ifndef VIDEOTOOLBOX_DPB_H
#define VIDEOTOOLBOX_DPB_H

#define DPB_MAX_PICS 16

#include <vlc_common.h>
#include <vlc_tick.h>
#include <vlc_picture.h>

typedef struct frame_info_t frame_info_t;

struct frame_info_t
{
    picture_t *p_picture;
    int i_poc;
    int i_foc;
    vlc_tick_t pts;
    vlc_tick_t dts;
    unsigned field_rate_num;
    unsigned field_rate_den;
    bool b_flush;
    bool b_eos;
    bool b_keyframe;
    bool b_leading;
    bool b_field;
    bool b_progressive;
    bool b_top_field_first;
    bool b_output_needed;
    bool b_no_output_of_prior_pics;
    bool b_no_rasl_output;
    uint8_t i_num_ts;
    uint8_t i_max_pics_buffering;
    uint8_t i_max_num_reorder;
    uint8_t i_max_latency_pics;
    uint8_t i_latency;
    unsigned i_length;
    frame_info_t *p_next;
};

struct dpb_s
{
    frame_info_t *p_entries;
    uint8_t i_size; /* number of virtual buffers used (depends on i_fields_per_buffer) */
    uint8_t i_stored_fields;
    uint8_t i_need_output_size;
    uint8_t i_max_pics;
    uint8_t i_fields_per_buffer; /* stores 2 fields or 1 field/frame per buffer */
    bool b_strict_reorder;
    bool b_invalid_pic_reorder_max;
    bool b_poc_based_reorder;
    void (*pf_release)(picture_t *);
};

void InsertIntoDPB(struct dpb_s *, frame_info_t *);

void RemoveDPBSlot(struct dpb_s *, frame_info_t **);

picture_t * EmptyDPB(struct dpb_s *, date_t *);

picture_t * DPBOutputFrame(struct dpb_s *, date_t *, frame_info_t *);

picture_t * DPBOutputAndRemoval(struct dpb_s *, date_t *, const frame_info_t *);

#endif // VIDEOTOOLBOX_DPB_H
