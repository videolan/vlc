/*****************************************************************************
 * dpb.c: decoder picture output pacing
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dpb.h"

#include <assert.h>

//#define DPB_DEBUG

void InsertIntoDPB(struct dpb_s *dpb, frame_info_t *p_info)
{
    frame_info_t **pp_lead_in = &dpb->p_entries;

    for ( ;; pp_lead_in = & ((*pp_lead_in)->p_next))
    {
        bool b_insert;
        if (*pp_lead_in == NULL)
            b_insert = true;
        else if (dpb->b_poc_based_reorder)
            b_insert = ((*pp_lead_in)->i_foc > p_info->i_foc);
        else
            b_insert = ((*pp_lead_in)->pts >= p_info->pts);

        if (b_insert)
        {
            p_info->p_next = *pp_lead_in;
            *pp_lead_in = p_info;
            dpb->i_stored_fields += (p_info->b_field ? 1 : 2);
            if(dpb->i_fields_per_buffer == 2)
                dpb->i_size = (dpb->i_stored_fields + 1)/2;
            else
                dpb->i_size++;
            break;
        }
    }
#ifdef DPB_DEBUG
    for (frame_info_t *p_in=dpb->p_entries; p_in; p_in = p_in->p_next)
        printf(" %d", p_in->i_foc);
    printf("\n");
#endif
}

picture_t * OutputNextFrameFromDPB(struct dpb_s *dpb, date_t *ptsdate)
{
    frame_info_t *p_info = dpb->p_entries;
    if (p_info == NULL)
        return NULL;

    /* Asynchronous fallback time init */
    if(date_Get(ptsdate) == VLC_TICK_INVALID)
    {
        date_Set(ptsdate, p_info->pts != VLC_TICK_INVALID ?
                              p_info->pts : p_info->dts );
    }

    /* Compute time from output if missing */
    if (p_info->pts == VLC_TICK_INVALID)
        p_info->pts = date_Get(ptsdate);
    else
        date_Set(ptsdate, p_info->pts);

    /* Update frame rate (used on interpolation) */
    if(p_info->field_rate_num != ptsdate->i_divider_num ||
        p_info->field_rate_den != ptsdate->i_divider_den)
    {
        /* no date_Change due to possible invalid num */
        date_Init(ptsdate, p_info->field_rate_num,
                  p_info->field_rate_den);
        date_Set(ptsdate, p_info->pts);
    }

    /* Set next picture time, in case it is missing */
    if (p_info->i_length)
        date_Set(ptsdate, p_info->pts + p_info->i_length);
    else
        date_Increment(ptsdate, p_info->i_num_ts);

    /* Extract attached field to output */
    picture_t *p_output = p_info->p_picture;
    if( p_info->p_picture ) /* Can have no picture attached to entry on error */
    {
        if( p_info->p_picture->date == VLC_TICK_INVALID )
            p_info->p_picture->date = p_info->pts;
    }

    dpb->i_stored_fields -= (p_info->b_field ? 1 : 2);
    if(dpb->i_fields_per_buffer == 2)
        dpb->i_size = (dpb->i_stored_fields + 1)/2;
    else
        dpb->i_size--;

    dpb->p_entries = p_info->p_next;
    free(p_info);

    return p_output;
}
