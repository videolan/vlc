/*****************************************************************************
 * decoder_prevframe.c: decoder previous frame helpers
 *****************************************************************************
 * Copyright (C) 2025 VLC authors, VideoLAN and Videolabs SAS
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

#include <vlc_picture.h>
#include "decoder_prevframe.h"

void
decoder_prevframe_Init(struct decoder_prevframe *pf)
{
    pf->pic = NULL;
    pf->req_count = 0;
    pf->failed = false;
    pf->flushing = false;
    pf->seek_steps = DEC_PF_SEEK_STEPS_INITIAL;
}

void
decoder_prevframe_Flush(struct decoder_prevframe *pf)
{
    if (pf->pic != NULL)
    {
        picture_Release(pf->pic);
        pf->pic = NULL;
    }
    pf->flushing = false;
    pf->failed = false;
}

void
decoder_prevframe_Reset(struct decoder_prevframe *pf)
{
    decoder_prevframe_Flush(pf);
    pf->seek_steps = DEC_PF_SEEK_STEPS_INITIAL;
    pf->req_count = 0;
}

void
decoder_prevframe_Request(struct decoder_prevframe *pf, int *seek_steps)
{
    if (pf->req_count == 0)
    {
        *seek_steps = pf->seek_steps;
        pf->flushing = true;
    }
    else
        *seek_steps = DEC_PF_SEEK_STEPS_NONE;
    pf->req_count++;
}

picture_t *
decoder_prevframe_AddPic(struct decoder_prevframe *pf, picture_t *pic,
                         vlc_tick_t *inout_pts, int *seek_steps)
{
    vlc_tick_t pts = *inout_pts;
    *seek_steps = DEC_PF_SEEK_STEPS_NONE;

    if (pf->flushing)
    {
        if (pic != NULL)
            picture_Release(pic);
        return NULL;
    }

    if (pic != NULL && pts <= pic->date && pf->pic != NULL)
    {
        /* Reached the previous frame */
        picture_t *resume_pic = pic;

        pic = pf->pic;
        pf->req_count--;
        pf->pic = NULL;

        *inout_pts = pic->date;

        /* Update seek request if needed */
        if (pf->req_count > 0)
        {
            *seek_steps = pf->seek_steps;
            pf->flushing = true;
        }

        assert(pic->p_next == NULL);
        /* Store the first pic to use if normal playback is resumed */
        pic->p_next = resume_pic;

        return pic;
    }

    if (pic == NULL || pic->date >= pts)
    {
        if (pf->pic == NULL && !pf->failed && pf->req_count > 0)
        {
            /* We need to seek again, and further */
            pf->seek_steps += DEC_PF_SEEK_STEPS_INITIAL * 2;
            pf->failed = true;
            pf->flushing = true;
            *seek_steps = pf->seek_steps;
        }

        if (pic != NULL)
            picture_Release(pic);
        return NULL;
    }

    if (pf->pic != NULL)
        picture_Release(pf->pic);
    /* Store the pic as the potential previous-frame (we know it only from
     * the next pic date) */
    pf->pic = pic;

    return NULL;
}