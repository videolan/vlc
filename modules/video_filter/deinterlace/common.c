/*****************************************************************************
 * common.c : Common helper function for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2000-2017 VLC authors and VideoLAN
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Christophe Massiot <massiot@via.ecp.fr>
 *         Laurent Aimar <fenrir@videolan.org>
 *         Juha Jeronen <juha.jeronen@jyu.fi>
 *         Steve Lhomme <robux4@gmail.com>
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

#include "common.h"

void InitDeinterlacingContext( struct deinterlace_ctx *p_context )
{
    p_context->settings.b_double_rate = false;
    p_context->settings.b_half_height = false;
    p_context->settings.b_use_frame_history = false;
    p_context->settings.b_custom_pts = false;

    p_context->meta[0].pi_date = VLC_TICK_INVALID;
    p_context->meta[0].pi_nb_fields = 2;
    p_context->meta[0].pb_top_field_first = true;
    for( int i = 1; i < METADATA_SIZE; i++ )
        p_context->meta[i] = p_context->meta[i-1];

    p_context->i_frame_offset = 0; /* start with default value (first-ever frame
                                  cannot have offset) */
    for( int i = 0; i < HISTORY_SIZE; i++ )
        p_context->pp_history[i] = NULL;
}

void FlushDeinterlacing(struct deinterlace_ctx *p_context)
{
    p_context->meta[0].pi_date = VLC_TICK_INVALID;
    p_context->meta[0].pi_nb_fields = 2;
    p_context->meta[0].pb_top_field_first = true;
    for( int i = 1; i < METADATA_SIZE; i++ )
        p_context->meta[i] = p_context->meta[i-1];

    p_context->i_frame_offset = 0; /* reset to default value (first frame after
                                      flush cannot have offset) */
    for( int i = 0; i < HISTORY_SIZE; i++ )
    {
        if( p_context->pp_history[i] )
            picture_Release( p_context->pp_history[i] );
        p_context->pp_history[i] = NULL;
    }
}

vlc_tick_t GetFieldDuration(const struct deinterlace_ctx *p_context,
                         const video_format_t *fmt, const picture_t *p_pic )
{
    vlc_tick_t i_field_dur = 0;

    /* Calculate one field duration. */
    int i = 0;
    int iend = METADATA_SIZE-1;
    /* Find oldest valid logged date.
       The current input frame doesn't count. */
    for( ; i < iend; i++ )
        if( p_context->meta[i].pi_date != VLC_TICK_INVALID )
            break;
    if( i < iend )
    {
        /* Count how many fields the valid history entries
           (except the new frame) represent. */
        int i_fields_total = 0;
        for( int j = i ; j < iend; j++ )
            i_fields_total += p_context->meta[j].pi_nb_fields;
        /* One field took this long. */
        i_field_dur = (p_pic->date - p_context->meta[i].pi_date) / i_fields_total;
    }
    else if (fmt->i_frame_rate)
        i_field_dur = vlc_tick_from_samples( fmt->i_frame_rate_base, fmt->i_frame_rate);

    /* Note that we default to field duration 0 if it could not be
       determined. This behaves the same as the old code - leaving the
       extra output frame dates the same as p_pic->date if the last cached
       date was not valid.
    */
    return i_field_dur;
}

void GetDeinterlacingOutput( const struct deinterlace_ctx *p_context,
                             video_format_t *p_dst, const video_format_t *p_src )
{
    *p_dst = *p_src;

    if( p_context->settings.b_half_height )
    {
        p_dst->i_height /= 2;
        p_dst->i_visible_height /= 2;
        p_dst->i_y_offset /= 2;
        p_dst->i_sar_den *= 2;
    }

    if( p_context->settings.b_double_rate )
    {
        p_dst->i_frame_rate *= 2;
    }
}

#define CUSTOM_PTS -1

picture_t *DoDeinterlacing( filter_t *p_filter,
                            struct deinterlace_ctx *p_context, picture_t *p_pic )
{
    picture_t *p_dst[DEINTERLACE_DST_SIZE];
    int i_double_rate_alloc_end;
    /* Remember the frame offset that we should use for this frame.
       The value in p_sys will be updated to reflect the correct value
       for the *next* frame when we call the renderer. */
    int i_frame_offset;
    int i_meta_idx;

    bool b_top_field_first;

    /* Request output picture */
    p_dst[0] = AllocPicture( p_filter );
    if( p_dst[0] == NULL )
    {
        picture_Release( p_pic );
        return NULL;
    }
    picture_CopyProperties( p_dst[0], p_pic );

    /* Any unused p_dst pointers must be NULL, because they are used to
       check how many output frames we have. */
    for( int i = 1; i < DEINTERLACE_DST_SIZE; ++i )
        p_dst[i] = NULL;

    /* Update the input frame history, if the currently active algorithm
       needs it. */
    if( p_context->settings.b_use_frame_history )
    {
        /* Keep reference for the picture */
        picture_t *p_dup = picture_Hold( p_pic );

        /* Slide the history */
        if( p_context->pp_history[0] )
            picture_Release( p_context->pp_history[0] );
        for( int i = 1; i < HISTORY_SIZE; i++ )
            p_context->pp_history[i-1] = p_context->pp_history[i];
        p_context->pp_history[HISTORY_SIZE-1] = p_dup;
    }

    /* Slide the metadata history. */
    for( int i = 1; i < METADATA_SIZE; i++ )
        p_context->meta[i-1] = p_context->meta[i];
    /* The last element corresponds to the current input frame. */
    p_context->meta[METADATA_SIZE-1].pi_date            = p_pic->date;
    p_context->meta[METADATA_SIZE-1].pi_nb_fields       = p_pic->i_nb_fields;
    p_context->meta[METADATA_SIZE-1].pb_top_field_first = p_pic->b_top_field_first;

    /* Remember the frame offset that we should use for this frame.
       The value in p_sys will be updated to reflect the correct value
       for the *next* frame when we call the renderer. */
    i_frame_offset = p_context->i_frame_offset;
    i_meta_idx     = (METADATA_SIZE-1) - i_frame_offset;

    int i_nb_fields;

    /* These correspond to the current *outgoing* frame. */
    if( i_frame_offset != CUSTOM_PTS )
    {
        /* Pick the correct values from the history. */
        b_top_field_first = p_context->meta[i_meta_idx].pb_top_field_first;
        i_nb_fields       = p_context->meta[i_meta_idx].pi_nb_fields;
    }
    else
    {
        /* Framerate doublers must not request CUSTOM_PTS, as they need the
           original field timings, and need Deinterlace() to allocate the
           correct number of output frames. */
        assert( !p_context->settings.b_double_rate );

        /* NOTE: i_nb_fields is only used for framerate doublers, so it is
                 unused in this case. b_top_field_first is only passed to the
                 algorithm. We assume that algorithms that request CUSTOM_PTS
                 will, if necessary, extract the TFF/BFF information themselves.
        */
        b_top_field_first = p_pic->b_top_field_first; /* this is not guaranteed
                                                         to be meaningful */
        i_nb_fields       = p_pic->i_nb_fields;       /* unused */
    }

    /* For framerate doublers, determine field duration and allocate
       output frames. */
    i_double_rate_alloc_end = 0; /* One past last for allocated output
                                        frames in p_dst[]. Used only for
                                        framerate doublers. Will be inited
                                        below. Declared here because the
                                        PTS logic needs the result. */
    if( p_context->settings.b_double_rate )
    {
        i_double_rate_alloc_end = i_nb_fields;
        if( i_nb_fields > DEINTERLACE_DST_SIZE )
        {
            /* Note that the effective buffer size depends also on the constant
               private_picture in vout_wrapper.c, since that determines the
               maximum number of output pictures AllocPicture() will
               successfully allocate for one input frame.
            */
            msg_Err( p_filter, "Framerate doubler: output buffer too small; "\
                               "fields = %d, buffer size = %d. Dropping the "\
                               "remaining fields.",
                               i_nb_fields, DEINTERLACE_DST_SIZE );
            i_double_rate_alloc_end = DEINTERLACE_DST_SIZE;
        }

        /* Allocate output frames. */
        for( int i = 1; i < i_double_rate_alloc_end ; ++i )
        {
            p_dst[i-1]->p_next =
            p_dst[i]           = AllocPicture( p_filter );
            if( p_dst[i] )
            {
                picture_CopyProperties( p_dst[i], p_pic );
            }
            else
            {
                msg_Err( p_filter, "Framerate doubler: could not allocate "\
                                   "output frame %d", i+1 );
                i_double_rate_alloc_end = i; /* Inform the PTS logic about the
                                                correct end position. */
                break; /* If this happens, the rest of the allocations
                          aren't likely to work, either... */
            }
        }
        /* Now we have allocated *up to* the correct number of frames;
           normally, exactly the correct number. Upon alloc failure,
           we may have succeeded in allocating *some* output frames,
           but fewer than were desired. In such a case, as many will
           be rendered as were successfully allocated.

           Note that now p_dst[i] != NULL
           for 0 <= i < i_double_rate_alloc_end. */
    }
    assert( p_context->settings.b_double_rate  ||  p_dst[1] == NULL );
    assert( i_nb_fields > 2  ||  p_dst[2] == NULL );

    /* Render */
    if ( !p_context->settings.b_double_rate )
    {
        if ( p_context->pf_render_single_pic( p_filter, p_dst[0], p_pic ) )
            goto drop;
    }
    else
    {
        /* Note: RenderIVTC will automatically drop the duplicate frames
                 produced by IVTC. This is part of normal operation. */
        if ( p_context->pf_render_ordered( p_filter, p_dst[0], p_pic,
                                           0, !b_top_field_first ) )
            goto drop;
        if ( p_dst[1] )
            p_context->pf_render_ordered( p_filter, p_dst[1], p_pic,
                                          1, b_top_field_first );
        if ( p_dst[2] )
            p_context->pf_render_ordered( p_filter, p_dst[2], p_pic,
                                          2, !b_top_field_first );
    }

    if ( p_context->settings.b_custom_pts )
    {
        assert(p_context->settings.b_use_frame_history);
        if( p_context->pp_history[0] && p_context->pp_history[1] )
        {
            /* The next frame will get a custom timestamp, too. */
            p_context->i_frame_offset = CUSTOM_PTS;
        }
        else if( !p_context->pp_history[0] && !p_context->pp_history[1] ) /* first frame */
        {
        }
        else /* second frame */
        {
            /* At the next frame, the filter starts. The next frame will get
               a custom timestamp. */
            p_context->i_frame_offset = CUSTOM_PTS;
        }
    }

    /* Set output timestamps, if the algorithm didn't request CUSTOM_PTS
       for this frame. */
    assert( i_frame_offset <= METADATA_SIZE ||
            i_frame_offset == CUSTOM_PTS );
    if( i_frame_offset != CUSTOM_PTS )
    {
        vlc_tick_t i_base_pts = p_context->meta[i_meta_idx].pi_date;

        /* Note: in the usual case (i_frame_offset = 0  and
                 b_double_rate = false), this effectively does nothing.
                 This is needed to correct the timestamp
                 when i_frame_offset > 0. */
        p_dst[0]->date = i_base_pts;

        if( p_context->settings.b_double_rate )
        {
            vlc_tick_t i_field_dur = GetFieldDuration( p_context, &p_filter->fmt_out.video, p_pic );
            /* Processing all actually allocated output frames. */
            for( int i = 1; i < i_double_rate_alloc_end; ++i )
            {
                /* XXX it's not really good especially for the first picture, but
                 * I don't think that delaying by one frame is worth it */
                if( i_base_pts != VLC_TICK_INVALID )
                    p_dst[i]->date = i_base_pts + i * i_field_dur;
                else
                    p_dst[i]->date = VLC_TICK_INVALID;
            }
        }
    }

    for( int i = 0; i < DEINTERLACE_DST_SIZE; ++i )
    {
        if( p_dst[i] )
        {
            p_dst[i]->b_progressive = true;
            p_dst[i]->i_nb_fields = 2;
        }
    }

    picture_Release( p_pic );
    return p_dst[0];

drop:
    picture_Release( p_dst[0] );
    for( int i = 1; i < DEINTERLACE_DST_SIZE; ++i )
    {
        if( p_dst[i] )
            picture_Release( p_dst[i] );
    }
#ifndef NDEBUG
    picture_Release( p_pic );
    return NULL;
#else
    return p_pic;
#endif
}
