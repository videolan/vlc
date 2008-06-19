/*****************************************************************************
 * decoder_synchro.c : frame dropping routines
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_codec.h>
#include <vlc_codec_synchro.h>

/*
 * Local prototypes
 */

#define MAX_PIC_AVERAGE         8

struct decoder_synchro_t
{
    /* */
    decoder_t       *p_dec;

    /* */
    int             i_frame_rate;
    int             i_current_rate;
    bool      b_no_skip;
    bool      b_quiet;

    /* date of the beginning of the decoding of the current picture */
    mtime_t         decoding_start;

    /* stream properties */
    unsigned int    i_n_p, i_n_b;

    /* decoding values */
    mtime_t         p_tau[4];                  /* average decoding durations */
    unsigned int    pi_meaningful[4];            /* number of durations read */

    /* render_time filled by SynchroChoose() */
    int i_render_time;

    /* stream context */
    int             i_nb_ref;                /* Number of reference pictures */
    int             i_dec_nb_ref;      /* Number of reference pictures we'll *
                                        * have if we decode the current pic  */
    int             i_trash_nb_ref;    /* Number of reference pictures we'll *
                                        * have if we trash the current pic   */
    unsigned int    i_eta_p, i_eta_b;
    mtime_t         backward_pts, current_pts;
    int             i_current_period;   /* period to add to the next picture */
    int             i_backward_period;  /* period to add after the next
                                         * reference picture
                                         * (backward_period * period / 2) */

    /* statistics */
    unsigned int    i_trashed_pic, i_not_chosen_pic, i_pic;
};

/* Error margins */
#define DELTA                   (int)(0.075*CLOCK_FREQ)
#define MAX_VALID_TAU           (int)(0.3*CLOCK_FREQ)

#define DEFAULT_NB_P            5
#define DEFAULT_NB_B            1

/*****************************************************************************
 * decoder_SynchroInit : You know what ?
 *****************************************************************************/
decoder_synchro_t * decoder_SynchroInit( decoder_t *p_dec, int i_frame_rate )
{
    decoder_synchro_t * p_synchro = malloc( sizeof(*p_synchro) );
    if ( p_synchro == NULL )
        return NULL;
    memset( p_synchro, 0, sizeof(*p_synchro) );

    p_synchro->p_dec = p_dec;
    p_synchro->b_no_skip = !config_GetInt( p_dec, "skip-frames" );
    p_synchro->b_quiet = config_GetInt( p_dec, "quiet-synchro" );

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
 * decoder_SynchroRelease : You know what ?
 *****************************************************************************/
void decoder_SynchroRelease( decoder_synchro_t * p_synchro )
{
    free( p_synchro );
}

/*****************************************************************************
 * decoder_SynchroReset : Reset the reference picture counter
 *****************************************************************************/
void decoder_SynchroReset( decoder_synchro_t * p_synchro )
{
    p_synchro->i_nb_ref = 0;
    p_synchro->i_trash_nb_ref = p_synchro->i_dec_nb_ref = 0;
}

/*****************************************************************************
 * decoder_SynchroChoose : Decide whether we will decode a picture or not
 *****************************************************************************/
bool decoder_SynchroChoose( decoder_synchro_t * p_synchro, int i_coding_type,
                               int i_render_time, bool b_low_delay )
{
#define TAU_PRIME( coding_type )    (p_synchro->p_tau[(coding_type)] \
                                    + (p_synchro->p_tau[(coding_type)] >> 1) \
                                    + p_synchro->i_render_time)
#define S (*p_synchro)
    mtime_t         now, period;
    mtime_t         pts = 0;
    bool      b_decode = 0;

    if ( p_synchro->b_no_skip )
        return 1;

    now = mdate();
    period = 1000000 * 1001 / p_synchro->i_frame_rate
                     * p_synchro->i_current_rate / INPUT_RATE_DEFAULT;

    p_synchro->i_render_time = i_render_time;

    switch( i_coding_type )
    {
    case I_CODING_TYPE:
        if( b_low_delay )
        {
            pts = S.current_pts;
        }
        else if( S.backward_pts )
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
            msg_Warn( p_synchro->p_dec,
                      "synchro trashing I (%"PRId64")", pts - now );
        }
        break;

    case P_CODING_TYPE:
        if( b_low_delay )
        {
            pts = S.current_pts;
        }
        else if( S.backward_pts )
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
 * decoder_SynchroTrash : Update counters when we trash a picture
 *****************************************************************************/
void decoder_SynchroTrash( decoder_synchro_t * p_synchro )
{
    p_synchro->i_trashed_pic++;
    p_synchro->i_nb_ref = p_synchro->i_trash_nb_ref;
}

/*****************************************************************************
 * decoder_SynchroDecode : Update timers when we decide to decode a picture
 *****************************************************************************/
void decoder_SynchroDecode( decoder_synchro_t * p_synchro )
{
    p_synchro->decoding_start = mdate();
    p_synchro->i_nb_ref = p_synchro->i_dec_nb_ref;
}

/*****************************************************************************
 * decoder_SynchroEnd : Called when the image is totally decoded
 *****************************************************************************/
void decoder_SynchroEnd( decoder_synchro_t * p_synchro, int i_coding_type,
                      bool b_garbage )
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
 * decoder_SynchroDate : When an image has been decoded, ask for its date
 *****************************************************************************/
mtime_t decoder_SynchroDate( decoder_synchro_t * p_synchro )
{
    /* No need to lock, since PTS are only used by the video parser. */
    return p_synchro->current_pts;
}

/*****************************************************************************
 * decoder_SynchroNewPicture: Update stream structure and PTS
 *****************************************************************************/
void decoder_SynchroNewPicture( decoder_synchro_t * p_synchro, int i_coding_type,
                             int i_repeat_field, mtime_t next_pts,
                             mtime_t next_dts, int i_current_rate,
                             bool b_low_delay )
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
#if 0
            if( !p_synchro->b_quiet )
                msg_Dbg( p_synchro->p_dec,
                         "stream periodicity changed from P[%d] to P[%d]",
                         p_synchro->i_n_p, p_synchro->i_eta_p );
#endif
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
            msg_Dbg( p_synchro->p_dec, "I(%"PRId64") P(%"PRId64")[%d] B(%"PRId64")"
                  "[%d] YUV(%"PRId64") : trashed %d:%d/%d",
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
            if( !p_synchro->b_quiet && p_synchro->i_trashed_pic != 0 )
                msg_Dbg( p_synchro->p_dec, "decoded %d/%d pictures",
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
#if 0
            if( !p_synchro->b_quiet )
                msg_Dbg( p_synchro->p_dec,
                         "stream periodicity changed from B[%d] to B[%d]",
                         p_synchro->i_n_b, p_synchro->i_eta_b );
#endif
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
    if( i_coding_type == B_CODING_TYPE || b_low_delay )
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
                msg_Warn( p_synchro->p_dec, "decoder synchro warning: pts != "
                          "current_date (%"PRId64")",
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
                msg_Warn( p_synchro->p_dec, "backward_pts != dts (%"PRId64")",
                           next_dts
                               - p_synchro->backward_pts );
            }
            if( (p_synchro->backward_pts - p_synchro->current_pts
                    > PTS_THRESHOLD
                  || p_synchro->current_pts - p_synchro->backward_pts
                    > PTS_THRESHOLD) && !p_synchro->b_quiet )
            {
                msg_Warn( p_synchro->p_dec,
                          "backward_pts != current_pts (%"PRId64")",
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
                msg_Warn( p_synchro->p_dec, "dts != current_pts (%"PRId64")",
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
            msg_Warn( p_synchro->p_dec, "PTS << now (%"PRId64"), resetting",
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
