/*****************************************************************************
 * vout_synchro.c : frame dropping routines
 *****************************************************************************
 * Copyright (C) 1999-2005 VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Samuel Hocevar <sam@via.ecp.fr>
 *          Jean-Marc Dressler <polux@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*
 * DISCUSSION : How to Write an efficient Frame-Dropping Algorithm
 * ==========
 *
 * This implementation is based on mathematical and statistical
 * developments. Older implementations used an enslavement, considering
 * that if we're late when reading an I picture, we will decode one frame
 * less. It had a tendancy to derive, and wasn't responsive enough, which
 * would have caused trouble with the stream control stuff.
 *
 * 1. Structure of a picture stream
 *    =============================
 * Between 2 I's, we have for instance :
 *    I   B   P   B   P   B   P   B   P   B   P   B   I
 *    t0  t1  t2  t3  t4  t5  t6  t7  t8  t9  t10 t11 t12
 * Please bear in mind that B's and IP's will be inverted when displaying
 * (decoding order != presentation order). Thus, t1 < t0.
 *
 * 2. Definitions
 *    ===========
 * t[0..12]     : Presentation timestamps of pictures 0..12.
 * t            : Current timestamp, at the moment of the decoding.
 * T            : Picture period, T = 1/frame_rate.
 * tau[I,P,B]   : Mean time to decode an [I,P,B] picture.
 * tauYUV       : Mean time to render a picture (given by the video_output).
 * tau´[I,P,B] = 2 * tau[I,P,B] + tauYUV
 *              : Mean time + typical difference (estimated to tau/2, that
 *                needs to be confirmed) + render time.
 * DELTA        : A given error margin.
 *
 * 3. General considerations
 *    ======================
 * We define three types of machines :
 *      14T > tauI : machines capable of decoding all I pictures
 *      2T > tauP  : machines capable of decoding all P pictures
 *      T > tauB   : machines capable of decoding all B pictures
 *
 * 4. Decoding of an I picture
 *    ========================
 * On fast machines, we decode all I's.
 * Otherwise :
 * We can decode an I picture if we simply have enough time to decode it
 * before displaying :
 *      t0 - t > tau´I + DELTA
 *
 * 5. Decoding of a P picture
 *    =======================
 * On fast machines, we decode all P's.
 * Otherwise :
 * First criterion : have time to decode it.
 *      t2 - t > tau´P + DELTA
 *
 * Second criterion : it shouldn't prevent us from displaying the forthcoming
 * I picture, which is more important.
 *      t12 - t > tau´P + tau´I + DELTA
 *
 * 6. Decoding of a B picture
 *    =======================
 * On fast machines, we decode all B's. Otherwise :
 *      t1 - t > tau´B + DELTA
 * Since the next displayed I or P is already decoded, we don't have to
 * worry about it.
 *
 * I hope you will have a pleasant flight and do not forget your life
 * jacket.
 *                                                  --Meuuh (2000-12-29)
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                                /* free() */
#include <string.h>                                    /* memcpy(), memset() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/input.h>

#include "vout_synchro.h"

/*
 * Local prototypes
 */

/* Error margins */
#define DELTA                   (int)(0.075*CLOCK_FREQ)
#define MAX_VALID_TAU           (int)(0.3*CLOCK_FREQ)

#define DEFAULT_NB_P            5
#define DEFAULT_NB_B            1

/*****************************************************************************
 * vout_SynchroInit : You know what ?
 *****************************************************************************/
vout_synchro_t * __vout_SynchroInit( vlc_object_t * p_object,
                                     int i_frame_rate )
{
    vout_synchro_t * p_synchro = vlc_object_create( p_object,
                                                  sizeof(vout_synchro_t) );
    if ( p_synchro == NULL )
    {
        msg_Err( p_object, "out of memory" );
        return NULL;
    }
    vlc_object_attach( p_synchro, p_object );

    p_synchro->b_no_skip = !config_GetInt( p_object, "skip-frames" );
    p_synchro->b_quiet = config_GetInt( p_object, "quiet-synchro" );

    /* We use a fake stream pattern, which is often right. */
    p_synchro->i_n_p = p_synchro->i_eta_p = DEFAULT_NB_P;
    p_synchro->i_n_b = p_synchro->i_eta_b = DEFAULT_NB_B;
    memset( p_synchro->p_tau, 0, 4 * sizeof(mtime_t) );
    memset( p_synchro->pi_meaningful, 0, 4 * sizeof(unsigned int) );
    p_synchro->i_nb_ref = 0;
    p_synchro->i_trash_nb_ref = p_synchro->i_dec_nb_ref = 0;
    p_synchro->current_pts = mdate() + DEFAULT_PTS_DELAY;
    p_synchro->backward_pts = 0;
    p_synchro->i_current_period = p_synchro->i_backward_period = 0;
    p_synchro->i_trashed_pic = p_synchro->i_not_chosen_pic =
        p_synchro->i_pic = 0;

    p_synchro->i_frame_rate = i_frame_rate;

    return p_synchro;
}

/*****************************************************************************
 * vout_SynchroRelease : You know what ?
 *****************************************************************************/
void vout_SynchroRelease( vout_synchro_t * p_synchro )
{
    vlc_object_detach( p_synchro );
    vlc_object_destroy( p_synchro );
}

/*****************************************************************************
 * vout_SynchroReset : Reset the reference picture counter
 *****************************************************************************/
void vout_SynchroReset( vout_synchro_t * p_synchro )
{
    p_synchro->i_nb_ref = 0;
    p_synchro->i_trash_nb_ref = p_synchro->i_dec_nb_ref = 0;
}

/*****************************************************************************
 * vout_SynchroChoose : Decide whether we will decode a picture or not
 *****************************************************************************/
vlc_bool_t vout_SynchroChoose( vout_synchro_t * p_synchro, int i_coding_type,
                               int i_render_time )
{
#define TAU_PRIME( coding_type )    (p_synchro->p_tau[(coding_type)] \
                                    + (p_synchro->p_tau[(coding_type)] >> 1) \
                                    + p_synchro->i_render_time)
#define S (*p_synchro)
    mtime_t         now, period;
    mtime_t         pts = 0;
    vlc_bool_t      b_decode = 0;

    if ( p_synchro->b_no_skip )
        return 1;

    now = mdate();
    period = 1000000 * 1001 / p_synchro->i_frame_rate
                     * p_synchro->i_current_rate / INPUT_RATE_DEFAULT;

    p_synchro->i_render_time = i_render_time;

    switch( i_coding_type )
    {
    case I_CODING_TYPE:
        if( S.backward_pts )
        {
            pts = S.backward_pts;
        }
        else
        {
            /* displaying order : B B P B B I
             *                      ^       ^
             *                      |       +- current picture
             *                      +- current PTS
             */
            pts = S.current_pts + period * (S.i_n_b + 2);
        }

        if( (1 + S.i_n_p * (S.i_n_b + 1)) * period >
                S.p_tau[I_CODING_TYPE] )
        {
            b_decode = 1;
        }
        else
        {
            b_decode = (pts - now) > (TAU_PRIME(I_CODING_TYPE) + DELTA);
        }
        if( !b_decode && !p_synchro->b_quiet )
        {
            msg_Warn( p_synchro,
                      "synchro trashing I ("I64Fd")", pts - now );
        }
        break;

    case P_CODING_TYPE:
        if( S.backward_pts )
        {
            pts = S.backward_pts;
        }
        else
        {
            pts = S.current_pts + period * (S.i_n_b + 1);
        }

        if( p_synchro->i_nb_ref < 1 )
        {
            b_decode = 0;
        }
        else if( (1 + S.i_n_p * (S.i_n_b + 1)) * period >
                S.p_tau[I_CODING_TYPE] )
        {
            if( (S.i_n_b + 1) * period > S.p_tau[P_CODING_TYPE] )
            {
                /* Security in case we're _really_ late */
                b_decode = (pts - now > 0);
            }
            else
            {
                b_decode = (pts - now) > (TAU_PRIME(P_CODING_TYPE) + DELTA);
                /* next I */
                b_decode &= (pts - now
                              + period
                          * ( (S.i_n_p - S.i_eta_p) * (1 + S.i_n_b) - 1 ))
                            > (TAU_PRIME(P_CODING_TYPE)
                                + TAU_PRIME(I_CODING_TYPE) + DELTA);
            }
        }
        else
        {
            b_decode = 0;
        }
        break;

    case B_CODING_TYPE:
        pts = S.current_pts;

        if( p_synchro->i_nb_ref < 2 )
        {
            b_decode = 0;
        }
        else if( (S.i_n_b + 1) * period > S.p_tau[P_CODING_TYPE] )
        {
            b_decode = (pts - now) > (TAU_PRIME(B_CODING_TYPE) + DELTA);
        }
        else
        {
            b_decode = 0;
        }
    }

    if( !b_decode )
    {
        S.i_not_chosen_pic++;
    }
    return( b_decode );
#undef S
#undef TAU_PRIME
}

/*****************************************************************************
 * vout_SynchroTrash : Update counters when we trash a picture
 *****************************************************************************/
void vout_SynchroTrash( vout_synchro_t * p_synchro )
{
    p_synchro->i_trashed_pic++;
    p_synchro->i_nb_ref = p_synchro->i_trash_nb_ref;
}

/*****************************************************************************
 * vout_SynchroDecode : Update timers when we decide to decode a picture
 *****************************************************************************/
void vout_SynchroDecode( vout_synchro_t * p_synchro )
{
    p_synchro->decoding_start = mdate();
    p_synchro->i_nb_ref = p_synchro->i_dec_nb_ref;
}

/*****************************************************************************
 * vout_SynchroEnd : Called when the image is totally decoded
 *****************************************************************************/
void vout_SynchroEnd( vout_synchro_t * p_synchro, int i_coding_type,
                      vlc_bool_t b_garbage )
{
    mtime_t     tau;

    if( !b_garbage )
    {
        tau = mdate() - p_synchro->decoding_start;

        /* If duration too high, something happened (pause ?), so don't
         * take it into account. */
        if( tau < 3 * p_synchro->p_tau[i_coding_type]
             || ( !p_synchro->pi_meaningful[i_coding_type]
                   && tau < MAX_VALID_TAU ) )
        {
            /* Mean with average tau, to ensure stability. */
            p_synchro->p_tau[i_coding_type] =
                (p_synchro->pi_meaningful[i_coding_type]
                 * p_synchro->p_tau[i_coding_type] + tau)
                / (p_synchro->pi_meaningful[i_coding_type] + 1);
            if( p_synchro->pi_meaningful[i_coding_type] < MAX_PIC_AVERAGE )
            {
                p_synchro->pi_meaningful[i_coding_type]++;
            }
        }
    }
}

/*****************************************************************************
 * vout_SynchroDate : When an image has been decoded, ask for its date
 *****************************************************************************/
mtime_t vout_SynchroDate( vout_synchro_t * p_synchro )
{
    /* No need to lock, since PTS are only used by the video parser. */
    return p_synchro->current_pts;
}

/*****************************************************************************
 * vout_SynchroNewPicture: Update stream structure and PTS
 *****************************************************************************/
void vout_SynchroNewPicture( vout_synchro_t * p_synchro, int i_coding_type,
                             int i_repeat_field, mtime_t next_pts,
                             mtime_t next_dts, int i_current_rate )
{
    mtime_t         period = 1000000 * 1001 / p_synchro->i_frame_rate
                              * i_current_rate / INPUT_RATE_DEFAULT;
#if 0
    mtime_t         now = mdate();
#endif
    p_synchro->i_current_rate = i_current_rate;

    switch( i_coding_type )
    {
    case I_CODING_TYPE:
        if( p_synchro->i_eta_p
             && p_synchro->i_eta_p != p_synchro->i_n_p )
        {
            if( !p_synchro->b_quiet )
                msg_Dbg( p_synchro,
                         "stream periodicity changed from P[%d] to P[%d]",
                         p_synchro->i_n_p, p_synchro->i_eta_p );
            p_synchro->i_n_p = p_synchro->i_eta_p;
        }
        p_synchro->i_eta_p = p_synchro->i_eta_b = 0;
        p_synchro->i_trash_nb_ref = 0;
        if( p_synchro->i_nb_ref < 2 )
            p_synchro->i_dec_nb_ref = p_synchro->i_nb_ref + 1;
        else
            p_synchro->i_dec_nb_ref = p_synchro->i_nb_ref;

#if 0
        if( !p_synchro->b_quiet )
            msg_Dbg( p_synchro, "I("I64Fd") P("I64Fd")[%d] B("I64Fd")"
                  "[%d] YUV("I64Fd") : trashed %d:%d/%d",
                  p_synchro->p_tau[I_CODING_TYPE],
                  p_synchro->p_tau[P_CODING_TYPE],
                  p_synchro->i_n_p,
                  p_synchro->p_tau[B_CODING_TYPE],
                  p_synchro->i_n_b,
                  p_synchro->i_render_time,
                  p_synchro->i_not_chosen_pic,
                  p_synchro->i_trashed_pic -
                  p_synchro->i_not_chosen_pic,
                  p_synchro->i_pic );
        p_synchro->i_trashed_pic = p_synchro->i_not_chosen_pic
            = p_synchro->i_pic = 0;
#else
        if( p_synchro->i_pic >= 100 )
        {
            if( !p_synchro->b_quiet )
                msg_Dbg( p_synchro, "decoded %d/%d pictures",
                         p_synchro->i_pic
                           - p_synchro->i_trashed_pic,
                         p_synchro->i_pic );
            p_synchro->i_trashed_pic = p_synchro->i_not_chosen_pic
                = p_synchro->i_pic = 0;
        }
#endif
        break;

    case P_CODING_TYPE:
        p_synchro->i_eta_p++;
        if( p_synchro->i_eta_b
             && p_synchro->i_eta_b != p_synchro->i_n_b )
        {
            if( !p_synchro->b_quiet )
                msg_Dbg( p_synchro,
                         "stream periodicity changed from B[%d] to B[%d]",
                         p_synchro->i_n_b, p_synchro->i_eta_b );
            p_synchro->i_n_b = p_synchro->i_eta_b;
        }
        p_synchro->i_eta_b = 0;
        p_synchro->i_dec_nb_ref = 2;
        p_synchro->i_trash_nb_ref = 0;
        break;

    case B_CODING_TYPE:
        p_synchro->i_eta_b++;
        p_synchro->i_dec_nb_ref = p_synchro->i_trash_nb_ref
            = p_synchro->i_nb_ref;
        break;
    }

    p_synchro->current_pts += p_synchro->i_current_period
                                        * (period >> 1);

#define PTS_THRESHOLD   (period >> 2)
    if( i_coding_type == B_CODING_TYPE )
    {
        /* A video frame can be displayed 1, 2 or 3 times, according to
         * repeat_first_field, top_field_first, progressive_sequence and
         * progressive_frame. */
        p_synchro->i_current_period = i_repeat_field;

        if( next_pts )
        {
            if( (next_pts - p_synchro->current_pts
                    > PTS_THRESHOLD
                  || p_synchro->current_pts - next_pts
                    > PTS_THRESHOLD) && !p_synchro->b_quiet )
            {
                msg_Warn( p_synchro, "vout synchro warning: pts != "
                          "current_date ("I64Fd")",
                          p_synchro->current_pts
                              - next_pts );
            }
            p_synchro->current_pts = next_pts;
        }
    }
    else
    {
        p_synchro->i_current_period = p_synchro->i_backward_period;
        p_synchro->i_backward_period = i_repeat_field;

        if( p_synchro->backward_pts )
        {
            if( next_dts &&
                (next_dts - p_synchro->backward_pts
                    > PTS_THRESHOLD
                  || p_synchro->backward_pts - next_dts
                    > PTS_THRESHOLD) && !p_synchro->b_quiet )
            {
                msg_Warn( p_synchro, "backward_pts != dts ("I64Fd")",
                           next_dts
                               - p_synchro->backward_pts );
            }
            if( (p_synchro->backward_pts - p_synchro->current_pts
                    > PTS_THRESHOLD
                  || p_synchro->current_pts - p_synchro->backward_pts
                    > PTS_THRESHOLD) && !p_synchro->b_quiet )
            {
                msg_Warn( p_synchro,
                          "backward_pts != current_pts ("I64Fd")",
                          p_synchro->current_pts
                              - p_synchro->backward_pts );
            }
            p_synchro->current_pts = p_synchro->backward_pts;
            p_synchro->backward_pts = 0;
        }
        else if( next_dts )
        {
            if( (next_dts - p_synchro->current_pts
                    > PTS_THRESHOLD
                  || p_synchro->current_pts - next_dts
                    > PTS_THRESHOLD) && !p_synchro->b_quiet )
            {
                msg_Warn( p_synchro, "dts != current_pts ("I64Fd")",
                          p_synchro->current_pts
                              - next_dts );
            }
            /* By definition of a DTS. */
            p_synchro->current_pts = next_dts;
            next_dts = 0;
        }

        if( next_pts )
        {
            /* Store the PTS for the next time we have to date an I picture. */
            p_synchro->backward_pts = next_pts;
            next_pts = 0;
        }
    }
#undef PTS_THRESHOLD

#if 0
    /* Removed for incompatibility with slow motion */
    if( p_synchro->current_pts + DEFAULT_PTS_DELAY < now )
    {
        /* We cannot be _that_ late, something must have happened, reinit
         * the dates. */
        if( !p_synchro->b_quiet )
            msg_Warn( p_synchro, "PTS << now ("I64Fd"), resetting",
                      now - p_synchro->current_pts - DEFAULT_PTS_DELAY );
        p_synchro->current_pts = now + DEFAULT_PTS_DELAY;
    }
    if( p_synchro->backward_pts
         && p_synchro->backward_pts + DEFAULT_PTS_DELAY < now )
    {
        /* The same. */
        p_synchro->backward_pts = 0;
    }
#endif

    p_synchro->i_pic++;
}
