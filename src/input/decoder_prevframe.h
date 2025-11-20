/*****************************************************************************
 * decoder_prevframe.h: decoder previous frame helpers
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

 #ifndef DECODER_PREVFRAME_H
 #define DECODER_PREVFRAME_H

#include <limits.h>

#include <vlc_common.h>
#include <vlc_tick.h>
#include <vlc_atomic.h>
#include <vlc_threads.h>

/* Steps as x frames behind displayed pts */
#define DEC_PF_SEEK_STEPS_NONE INT_MAX
#define DEC_PF_SEEK_STEPS_INITIAL 1
#define DEC_PF_SEEK_STEPS_MAX 200

/* Guarded by external lock */
struct decoder_prevframe
{
    picture_t *pic;

    unsigned req_count;
    int seek_steps;
    bool flushing;
    bool failed;
};

/*
 * Init previous frame mode (it doesn't enable it)
 */
void
decoder_prevframe_Init(struct decoder_prevframe *pf);

/*
 * Flush lifo (when seeking)
 */
void
decoder_prevframe_Flush(struct decoder_prevframe *pf);

/*
 * Reset the previous frame mode (transition to normal playback)
 */
void
decoder_prevframe_Reset(struct decoder_prevframe *pf);

/*
 * Request a previous frame
 */
void
decoder_prevframe_Request(struct decoder_prevframe *pf, int *seek_steps);


static inline bool
decoder_prevframe_IsActive(struct decoder_prevframe *pf)
{
    return pf->req_count > 0;
}

/*
 * Add a picture to the lifo
 *
 * @param [inout] inout_pts pointer to the last displayed pts, updated when a
 * previous frame is found
 * @param [out] seek_steps pointer to seek request, a seek is requested if
 * different from DEC_PF_SEEK_STEPS_NONE
 */
picture_t *
decoder_prevframe_AddPic(struct decoder_prevframe *pf, picture_t *pic,
                         vlc_tick_t *inout_pts, int *seek_steps);

#endif /* DECODER_PREVFRAME_H */