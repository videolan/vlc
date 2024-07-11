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
            if(p_info->b_output_needed)
                dpb->i_need_output_size++;
            assert(dpb->i_need_output_size <= dpb->i_stored_fields);
            break;
        }
    }
#ifdef DPB_DEBUG
    for (frame_info_t *p_in=dpb->p_entries; p_in; p_in = p_in->p_next)
        printf(" %d(%cl%d)", p_in->i_foc, p_in->b_output_needed ? 'N' : '_', p_in->i_latency);
    printf("\n");
#endif
}

void RemoveDPBSlot(struct dpb_s *dpb, frame_info_t **pp_info)
{
    assert(dpb->i_stored_fields >= dpb->i_need_output_size);
    frame_info_t *p_info = *pp_info;
    if(!p_info)
        return;
    dpb->i_stored_fields -= (p_info->b_field ? 1 : 2);
    if(dpb->i_fields_per_buffer == 2)
        dpb->i_size = (dpb->i_stored_fields + 1)/2;
    else
        dpb->i_size--;

    if(p_info->b_output_needed)
    {
        assert(dpb->i_need_output_size);
        dpb->i_need_output_size--;
    }
    if(p_info->p_picture) /* release picture */
        dpb->pf_release(p_info->p_picture);
    *pp_info = p_info->p_next;
    free(p_info);
}

static void ReduceDPBSize(struct dpb_s *dpb)
{
    RemoveDPBSlot(dpb, &dpb->p_entries);
}

picture_t * DPBOutputFrame(struct dpb_s *dpb, date_t *ptsdate, frame_info_t *p_info)
{
    if(!p_info)
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
        p_info->p_picture = NULL; /* detach picture from DPB storage */
    }

    if(p_info->b_output_needed) /* Tag storage as not used for output */
    {
        assert(dpb->i_need_output_size);
        dpb->i_need_output_size--;
        p_info->b_output_needed = false;
    }
    assert(dpb->i_need_output_size <= dpb->i_stored_fields);

    return p_output;
}

static picture_t * BumpDPB(struct dpb_s *dpb, date_t *ptsdate, const frame_info_t *p_info)
{
    picture_t *p_output = NULL;
    picture_t **pp_output_next = &p_output;

    for(;dpb->i_stored_fields;)
    {
        bool b_output = false;

        if(p_info->b_flush && dpb->i_stored_fields > 0)
            b_output = true;
        else if(dpb->i_size >= p_info->i_max_pics_buffering)
            b_output = true;
        else if(p_info->i_max_num_reorder > 0 &&
                 dpb->i_need_output_size > p_info->i_max_num_reorder)
            b_output = true;
        else if(p_info->i_max_latency_pics > 0 &&
                 dpb->i_need_output_size > 0 )
        {
            for(const frame_info_t *p = dpb->p_entries; p; p = p->p_next)
            {
                if(p->b_output_needed &&
                   p->i_latency >= p_info->i_max_latency_pics)
                {
                    b_output = true;
                    break;
                }
            }
        }

        if(!b_output)
            break;

        *pp_output_next = DPBOutputFrame(dpb, ptsdate, dpb->p_entries);
        if(*pp_output_next)
            pp_output_next = &((*pp_output_next)->p_next);

        ReduceDPBSize(dpb);
    };

    return p_output;
}

picture_t * EmptyDPB(struct dpb_s *dpb, date_t *ptsdate)
{
    picture_t *output = NULL;
    picture_t **pp_output_next = &output;
    while(dpb->i_size > 0)
    {
        *pp_output_next = DPBOutputFrame(dpb, ptsdate, dpb->p_entries);
        if(*pp_output_next)
            pp_output_next = &((*pp_output_next)->p_next);
        ReduceDPBSize(dpb);
    }
    return output;
}

picture_t * DPBOutputAndRemoval(struct dpb_s *dpb, date_t *ptsdate,
                                const frame_info_t *p_info)
{
    picture_t *output = NULL;

    /* C 5.2.3 Additional Bumping */
    if(p_info->b_output_needed)
    {
        for(frame_info_t *p = dpb->p_entries; p; p = p->p_next)
            if(p->b_output_needed && p->i_foc > p_info->i_foc)
                p->i_latency++;
    }

    /* IRAP picture with NoRaslOutputFlag */
    if(p_info->b_no_rasl_output && p_info->b_keyframe)
    {
        if(p_info->b_no_output_of_prior_pics)
        {
            /* discard everything */
            output = EmptyDPB(dpb, ptsdate);
        }
        else
        {
            /* clean-up only non-ref && non-needed */
            for(frame_info_t **pp_next = &dpb->p_entries; *pp_next;)
            {
                if(!(*pp_next)->b_output_needed)
                    RemoveDPBSlot(dpb, pp_next);
                else
                    pp_next = &((*pp_next)->p_next);
            }
        }

        while(output)
        {
            picture_t *next = output->p_next;
            output->p_next = NULL;
            dpb->pf_release(output);
            output = next;
        }

        /* Output remaining C.5.2.2 2. al.2 */
        output = EmptyDPB(dpb, ptsdate);
    }
    else /* Regular bump process */
    {
        output = BumpDPB(dpb, ptsdate, p_info);
    }

    return output;
}
