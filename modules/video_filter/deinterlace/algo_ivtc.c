/*****************************************************************************
 * algo_ivtc.c : IVTC (inverse telecine) algorithm for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2010-2011 the VideoLAN team
 * $Id$
 *
 * Author: Juha Jeronen <juha.jeronen@jyu.fi>
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
#   include "config.h"
#endif

#ifdef CAN_COMPILE_MMXEXT
#   include "mmx.h"
#endif

#include <stdint.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_cpu.h>
#include <vlc_picture.h>
#include <vlc_filter.h>

#include "deinterlace.h" /* filter_sys_t */
#include "helpers.h"

#include "algo_ivtc.h"

/*****************************************************************************
 * Local data
 *****************************************************************************/

/* Fasten your seatbelt - lots of IVTC constants follow... */

/**
 * IVTC filter modes.
 *
 * Hard telecine: burned into video stream.
 * Soft telecine: stream consists of progressive frames;
 *                telecining handled by stream flags.
 *
 * @see ivtc_sys_t
 * @see RenderIVTC()
 */
typedef enum { IVTC_MODE_DETECTING           = 0,
               IVTC_MODE_TELECINED_NTSC_HARD = 1,
               IVTC_MODE_TELECINED_NTSC_SOFT = 2 } ivtc_mode;

/**
 *  Field pair combinations from successive frames in the PCN stencil.
 *  T = top, B = bottom, P = previous, C = current, N = next
 *  These are used as array indices; hence the explicit numbering.
 */
typedef enum { FIELD_PAIR_TPBP = 0, FIELD_PAIR_TPBC = 1,
               FIELD_PAIR_TCBP = 2, FIELD_PAIR_TCBC = 3,
               FIELD_PAIR_TCBN = 4, FIELD_PAIR_TNBC = 5,
               FIELD_PAIR_TNBN = 6 } ivtc_field_pair;

/* Note: only valid ones count for NUM */
#define NUM_CADENCE_POS 9
/**
 * Cadence positions for the PCN stencil (PCN, Previous Current Next).
 *
 * Note that "dea" in both cadence tables and a pure progressive signal
 * are indistinguishable.
 *
 * Used as array indices except the -1.
 *
 * This is a combined raw position containing both i_cadence_pos
 * and telecine field dominance.
 * @see pi_detected_pos_to_cadence_pos
 * @see pi_detected_pos_to_tfd
 */
typedef enum { CADENCE_POS_INVALID     = -1,
               CADENCE_POS_PROGRESSIVE =  0,
               CADENCE_POS_TFF_ABC     =  1,
               CADENCE_POS_TFF_BCD     =  2,
               CADENCE_POS_TFF_CDE     =  3,
               CADENCE_POS_TFF_EAB     =  4,
               CADENCE_POS_BFF_ABC     =  5,
               CADENCE_POS_BFF_BCD     =  6,
               CADENCE_POS_BFF_CDE     =  7,
               CADENCE_POS_BFF_EAB     =  8 } ivtc_cadence_pos;
/* First and one-past-end for TFF-only and BFF-only raw positions. */
#define CADENCE_POS_TFF_FIRST 1
#define CADENCE_POS_TFF_END   5
#define CADENCE_POS_BFF_FIRST 5
#define CADENCE_POS_BFF_END   9

/**
 * For the "vektor" cadence detector algorithm.
 *
 * The algorithm produces a set of possible positions instead of a unique
 * position, until it locks on. The set is represented as a bitmask.
 *
 * The bitmask is stored in a word, and its layout is:
 * blank blank BFF_CARRY BFF4 BFF3 BFF2 BFF1 BFF0   (high byte)
 * blank blank TFF_CARRY TFF4 TFF3 TFF2 TFF1 TFF0   (low byte)
 *
 * This allows predicting the next position by left-shifting the previous
 * result by one bit, copying the CARRY bits to the respective zeroth position,
 * and ANDing with 0x1F1F.
 *
 * This table is indexed with a valid ivtc_cadence_pos.
 * @see ivtc_cadence_pos
 */
const int pi_detected_pos_to_bitmask[NUM_CADENCE_POS] = { 0x0808, /* prog. */
                                                          0x0001, /* TFF ABC */
                                                          0x0002, /* TFF BCD */
                                                          0x0004, /* TFF CDE */
                                                          0x0010, /* TFF EAB */
                                                          0x0100, /* BFF ABC */
                                                          0x0200, /* BFF BCD */
                                                          0x0400, /* BFF CDE */
                                                          0x1000, /* BFF EAB */
                                                        };
#define VEKTOR_CADENCE_POS_ALL 0x1F1F
#define VEKTOR_CADENCE_POS_TFF 0x00FF
#define VEKTOR_CADENCE_POS_BFF 0xFF00
#define VEKTOR_CADENCE_POS_TFF_HIGH 0x0010
#define VEKTOR_CADENCE_POS_TFF_LOW  0x0001
#define VEKTOR_CADENCE_POS_BFF_HIGH 0x1000
#define VEKTOR_CADENCE_POS_BFF_LOW  0x0100

/* Telecine field dominance */
typedef enum { TFD_INVALID = -1, TFD_TFF = 0, TFD_BFF = 1 } ivtc_tfd;

/**
 * Position detection table for the "scores" cadence detector algorithm.
 *
 * These are the (only) field pair combinations that should give progressive
 * frames. There are three for each position.
 *
 * First index: ivtc_cadence_pos
 */
static const ivtc_field_pair pi_best_field_pairs[NUM_CADENCE_POS][3] = {
    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBC, FIELD_PAIR_TNBN}, /* prog. */

    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBP, FIELD_PAIR_TNBC}, /* TFF ABC */
    {FIELD_PAIR_TCBP, FIELD_PAIR_TNBC, FIELD_PAIR_TNBN}, /* TFF BCD */
    {FIELD_PAIR_TCBP, FIELD_PAIR_TCBC, FIELD_PAIR_TNBN}, /* TFF CDE */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBC, FIELD_PAIR_TNBC}, /* TFF EAB */

    {FIELD_PAIR_TPBP, FIELD_PAIR_TPBC, FIELD_PAIR_TCBN}, /* BFF ABC */
    {FIELD_PAIR_TPBC, FIELD_PAIR_TCBN, FIELD_PAIR_TNBN}, /* BFF BCD */
    {FIELD_PAIR_TPBC, FIELD_PAIR_TCBC, FIELD_PAIR_TNBN}, /* BFF CDE */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBC, FIELD_PAIR_TCBN}, /* BFF EAB */
};

/**
 * Alternative position detection table for the "scores" cadence detector
 * algorithm.
 *
 * These field pair combinations should give only interlaced frames.
 * There are four for each position.
 *
 * First index: ivtc_cadence_pos
 *
 * Currently unused. During development it was tested that whether we detect
 * best or worst, the resulting detected cadence positions are identical
 * (neither strategy performs any different from the other).
 */
static const ivtc_field_pair pi_worst_field_pairs[NUM_CADENCE_POS][4] = {
    {FIELD_PAIR_TPBC, FIELD_PAIR_TCBP,
        FIELD_PAIR_TCBN, FIELD_PAIR_TNBC}, /* prog. */

    {FIELD_PAIR_TPBC, FIELD_PAIR_TCBC,
        FIELD_PAIR_TCBN, FIELD_PAIR_TNBN}, /* TFF ABC */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TPBC,
        FIELD_PAIR_TCBC, FIELD_PAIR_TCBN}, /* TFF BCD */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TPBC,
        FIELD_PAIR_TCBN, FIELD_PAIR_TNBC}, /* TFF CDE */
    {FIELD_PAIR_TPBC, FIELD_PAIR_TCBP,
        FIELD_PAIR_TCBN, FIELD_PAIR_TNBN}, /* TFF EAB */

    {FIELD_PAIR_TCBP, FIELD_PAIR_TCBC,
        FIELD_PAIR_TNBC, FIELD_PAIR_TNBN}, /* BFF ABC */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBP,
        FIELD_PAIR_TCBC, FIELD_PAIR_TNBC}, /* BFF BCD */
    {FIELD_PAIR_TPBP, FIELD_PAIR_TCBP,
        FIELD_PAIR_TNBC, FIELD_PAIR_TCBN}, /* BFF CDE */
    {FIELD_PAIR_TCBP, FIELD_PAIR_TPBC,
        FIELD_PAIR_TNBC, FIELD_PAIR_TNBN}, /* BFF EAB */
};

/**
 * Table for extracting the i_cadence_pos part of detected cadence position
 * (ivtc_cadence_pos).
 *
 * The counter goes from 0 to 4, where "abc" = 0, "bcd" = 1, ...
 *
 * @see ivtc_cadence_pos
 */
static const int pi_detected_pos_to_cadence_pos[NUM_CADENCE_POS] = {
    3, /* prog. */
    0, /* TFF ABC */
    1, /* TFF BCD */
    2, /* TFF CDE */
    4, /* TFF EAB */
    0, /* BFF ABC */
    1, /* BFF BCD */
    2, /* BFF CDE */
    4, /* BFF EAB */
};

/**
 * Table for extracting the telecine field dominance part of detected
 * cadence position (ivtc_cadence_pos).
 *
 * The position "dea" does not provide TFF/BFF information, because it is
 * indistinguishable from progressive.
 *
 * @see ivtc_cadence_pos
 */
static const int pi_detected_pos_to_tfd[NUM_CADENCE_POS] = {
    TFD_INVALID, /* prog. */
    TFD_TFF, /* TFF ABC */
    TFD_TFF, /* TFF BCD */
    TFD_TFF, /* TFF CDE */
    TFD_TFF, /* TFF EAB */
    TFD_BFF, /* BFF ABC */
    TFD_BFF, /* BFF BCD */
    TFD_BFF, /* BFF CDE */
    TFD_BFF, /* BFF EAB */
};

/* Valid telecine sequences (TFF and BFF). Indices: [TFD][i_cadence_pos] */
/* Currently unused and left here for documentation only.
   There is an easier way - just decode the i_cadence_pos part of the
   detected position using the pi_detected_pos_to_cadence_pos table,
   and check that it is successive mod 5. See IVTCCadenceAnalyze(). */
/*static const int pi_valid_cadences[2][5] = { {CADENCE_POS_TFF_ABC,
                                             CADENCE_POS_TFF_BCD,
                                             CADENCE_POS_TFF_CDE,
                                             CADENCE_POS_PROGRESSIVE,
                                             CADENCE_POS_TFF_EAB},

                                             {CADENCE_POS_BFF_ABC,
                                             CADENCE_POS_BFF_BCD,
                                             CADENCE_POS_BFF_CDE,
                                             CADENCE_POS_PROGRESSIVE,
                                             CADENCE_POS_BFF_EAB},
                                           };
*/

/**
 * Operations needed in film frame reconstruction.
 */
typedef enum { IVTC_OP_DROP_FRAME,
               IVTC_OP_COPY_N,
               IVTC_OP_COPY_C,
               IVTC_OP_COMPOSE_TNBC,
               IVTC_OP_COMPOSE_TCBN } ivtc_op;

/* Note: During hard IVTC, we must avoid COPY_C and do a compose instead.
   If we COPY_C, some subtitles will flicker badly, even if we use the
   cadence-based film frame reconstruction. Try the first scene in
   Kanon (2006) vol. 3 to see the problem.

   COPY_C can be used without problems when it is used consistently
   (not constantly mixed in with COPY_N and compose operations),
   for example in soft IVTC.
*/
/**
 * Operation table for film frame reconstruction depending on cadence position.
 * Indices: [TFD][i_cadence_pos]
 * @see pi_detected_pos_to_tfd
 * @see pi_detected_pos_to_cadence_pos
 */
static const ivtc_op pi_reconstruction_ops[2][5] = { /* TFF */
                                                     {IVTC_OP_COMPOSE_TNBC,
                                                      IVTC_OP_COPY_N,
                                                      IVTC_OP_COPY_N,
                                                      IVTC_OP_DROP_FRAME,
                                                      IVTC_OP_COMPOSE_TNBC},

                                                     /* BFF */
                                                     {IVTC_OP_COMPOSE_TCBN,
                                                      IVTC_OP_COPY_N,
                                                      IVTC_OP_COPY_N,
                                                      IVTC_OP_DROP_FRAME,
                                                      IVTC_OP_COMPOSE_TCBN},
                                                   };

/**
 * Timestamp mangling table.
 *
 * This is used in the 29.97 -> 23.976 fps conversion.
 *
 * Index: i_cadence_pos, 0..4.
 *
 * Valid values are nonnegative. The -1 corresponds to the dropped frame
 * and is never used, except for a debug assert.
 *
 * The unit of the values is 1/4 of frame duration.
 * See the function documentation of RenderIVTC() for an explanation.
 * @see ivtc_cadence_pos
 * @see pi_detected_pos_to_cadence_pos
 * @see pi_reconstruction_ops
 * @see RenderIVTC()
 */
static const int pi_timestamp_deltas[5] = { 1, 2, 3, -1, 0 };

/*****************************************************************************
 * Internal functions
 *****************************************************************************/

/**
 * Internal helper function for RenderIVTC(): performs initialization
 * at the start of a new frame.
 *
 * In practice, this slides detector histories.
 *
 * This function should only perform initialization that does NOT require
 * the input frame history buffer. This runs at every frame, including
 * the first two.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 */
static void IVTCFrameInit( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;

    /* Slide detector histories */
    for( int i = 1; i < IVTC_DETECTION_HISTORY_SIZE; i++ )
    {
        p_ivtc->pi_top_rep[i-1] = p_ivtc->pi_top_rep[i];
        p_ivtc->pi_bot_rep[i-1] = p_ivtc->pi_bot_rep[i];
        p_ivtc->pi_motion[i-1]  = p_ivtc->pi_motion[i];

        p_ivtc->pi_s_cadence_pos[i-1] = p_ivtc->pi_s_cadence_pos[i];
        p_ivtc->pb_s_reliable[i-1]    = p_ivtc->pb_s_reliable[i];
        p_ivtc->pi_v_cadence_pos[i-1] = p_ivtc->pi_v_cadence_pos[i];
        p_ivtc->pi_v_raw[i-1]         = p_ivtc->pi_v_raw[i];
        p_ivtc->pb_v_reliable[i-1]    = p_ivtc->pb_v_reliable[i];

        p_ivtc->pi_cadence_pos_history[i-1]
                                      = p_ivtc->pi_cadence_pos_history[i];

        p_ivtc->pb_all_progressives[i-1] = p_ivtc->pb_all_progressives[i];
    }
    /* The latest position has not been detected yet. */
    p_ivtc->pi_s_cadence_pos[IVTC_LATEST] = CADENCE_POS_INVALID;
    p_ivtc->pb_s_reliable[IVTC_LATEST]    = false;
    p_ivtc->pi_v_cadence_pos[IVTC_LATEST] = CADENCE_POS_INVALID;
    p_ivtc->pi_v_raw[IVTC_LATEST]         = VEKTOR_CADENCE_POS_ALL;
    p_ivtc->pb_v_reliable[IVTC_LATEST]    = false;
    p_ivtc->pi_cadence_pos_history[IVTC_LATEST] = CADENCE_POS_INVALID;
    p_ivtc->pi_top_rep[IVTC_LATEST] =  0;
    p_ivtc->pi_bot_rep[IVTC_LATEST] =  0;
    p_ivtc->pi_motion[IVTC_LATEST]  = -1;
    p_ivtc->pb_all_progressives[IVTC_LATEST] = false;

    /* Slide history of field pair interlace scores */
    p_ivtc->pi_scores[FIELD_PAIR_TPBP] = p_ivtc->pi_scores[FIELD_PAIR_TCBC];
    p_ivtc->pi_scores[FIELD_PAIR_TPBC] = p_ivtc->pi_scores[FIELD_PAIR_TCBN];
    p_ivtc->pi_scores[FIELD_PAIR_TCBP] = p_ivtc->pi_scores[FIELD_PAIR_TNBC];
    p_ivtc->pi_scores[FIELD_PAIR_TCBC] = p_ivtc->pi_scores[FIELD_PAIR_TNBN];
    /* These have not been detected yet */
    p_ivtc->pi_scores[FIELD_PAIR_TCBN] = 0;
    p_ivtc->pi_scores[FIELD_PAIR_TNBC] = 0;
    p_ivtc->pi_scores[FIELD_PAIR_TNBN] = 0;
}

/**
 * Internal helper function for RenderIVTC(): computes various raw detector
 * data at the start of a new frame.
 *
 * This function requires the input frame history buffer.
 * IVTCFrameInit() must have been called first.
 * Last two frames must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 * @see IVTCFrameInit()
 */
static void IVTCLowLevelDetect( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;
    picture_t *p_curr = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );
    assert( p_curr != NULL );

    /* Compute interlace scores for TNBN, TNBC and TCBN.
        Note that p_next contains TNBN. */
    p_ivtc->pi_scores[FIELD_PAIR_TNBN] = CalculateInterlaceScore( p_next,
                                                                  p_next );
    p_ivtc->pi_scores[FIELD_PAIR_TNBC] = CalculateInterlaceScore( p_next,
                                                                  p_curr );
    p_ivtc->pi_scores[FIELD_PAIR_TCBN] = CalculateInterlaceScore( p_curr,
                                                                  p_next );

    int i_top = 0, i_bot = 0;
    int i_motion = EstimateNumBlocksWithMotion(p_curr, p_next, &i_top, &i_bot);
    p_ivtc->pi_motion[IVTC_LATEST] = i_motion;

    /* If one field changes "clearly more" than the other, we know the
       less changed one is a likely duplicate.

       Threshold 1/2 is too low for some scenes (e.g. pan of the space junk
       at beginning of The Third ep. 1, right after the OP). Thus, we use 2/3,
       which seems to work.
    */
    p_ivtc->pi_top_rep[IVTC_LATEST] = (i_top <= 2*i_bot/3);
    p_ivtc->pi_bot_rep[IVTC_LATEST] = (i_bot <= 2*i_top/3);
}

/**
 * Internal helper function for RenderIVTC(): using raw detector data,
 * detect cadence position by an interlace scores based algorithm ("scores").
 *
 * IVTCFrameInit() and IVTCLowLevelDetect() must have been called first.
 * Last frame must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 * @see IVTCFrameInit()
 * @see IVTCLowLevelDetect()
 * @see IVTCCadenceDetectFinalize()
 */
static void IVTCCadenceDetectAlgoScores( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;
    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );

    /* Detect likely cadence position according to the tables,
       using the tabulated combinations of all 7 available interlace scores.
    */
    int pi_ivtc_scores[NUM_CADENCE_POS];
    for( int i = 0; i < NUM_CADENCE_POS; i++ )
        pi_ivtc_scores[i] = p_ivtc->pi_scores[ pi_best_field_pairs[i][0] ]
                          + p_ivtc->pi_scores[ pi_best_field_pairs[i][1] ]
                          + p_ivtc->pi_scores[ pi_best_field_pairs[i][2] ];
    /* Find minimum */
    int j = CADENCE_POS_PROGRESSIVE; /* valid regardless of TFD */
    int minscore = pi_ivtc_scores[j];
    /* A TFF (respectively BFF) stream may only have TFF (respectively BFF)
       telecine. Don't bother looking at the wrong table. */
    int imin = CADENCE_POS_TFF_FIRST; /* first TFF-only entry */
    int iend = CADENCE_POS_TFF_END;   /* one past last TFF-only entry */
    if( !p_next->b_top_field_first )
    {
        imin = CADENCE_POS_BFF_FIRST; /* first BFF-only entry */
        iend = CADENCE_POS_BFF_END;   /* one past last BFF-only entry */
    }
    for( int i = imin; i < iend; i++ )
    {
        if( pi_ivtc_scores[i] < minscore )
        {
            minscore = pi_ivtc_scores[i];
            j = i;
        }
    }

    /* Now "j" contains the most likely position according to the tables,
       accounting also for video TFF/BFF. */
    p_ivtc->pi_s_cadence_pos[IVTC_LATEST] = j;

    /* Estimate reliability of detector result.

       We do this by checking if the winner is an outlier at least
       to some extent. For anyone better versed in statistics,
       feel free to improve this.
    */

    /* Compute sample mean with the winner included and without.

       Sample mean is defined as mu = sum( x_i, i ) / N ,
       where N is the number of samples.
    */
    int mean = pi_ivtc_scores[CADENCE_POS_PROGRESSIVE];
    int mean_except_min = 0;
    if( j != CADENCE_POS_PROGRESSIVE )
        mean_except_min = pi_ivtc_scores[CADENCE_POS_PROGRESSIVE];
    for( int i = imin; i < iend; i++ )
    {
        mean += pi_ivtc_scores[i];
        if( i != j )
            mean_except_min += pi_ivtc_scores[i];
    }
    /* iend points one past end, but progressive counts as the +1. */
    mean /= (iend - imin + 1);
    mean_except_min /= (iend - imin);

    /* Check how much excluding the winner changes the mean. */
    double mean_ratio = (double)mean_except_min / (double)mean;

    /* Let's pretend that the detected position is a stochastic variable.
       Compute sample variance with the winner included and without.

       var = sum( (x_i - mu)^2, i ) / N ,

       where mu is the sample mean.

       Note that we really need int64_t; the numbers are pretty large.
    */
    int64_t diff = (int64_t)(pi_ivtc_scores[CADENCE_POS_PROGRESSIVE] - mean);
    int64_t var = diff*diff;
    int64_t var_except_min = 0;
    if( j != CADENCE_POS_PROGRESSIVE )
    {
        int64_t diff_exm = (int64_t)(pi_ivtc_scores[CADENCE_POS_PROGRESSIVE]
                                      - mean_except_min);
        var_except_min = diff_exm*diff_exm;
    }
    for( int i = imin; i < iend; i++ )
    {
        diff = (int64_t)(pi_ivtc_scores[i] - mean);
        var += (diff*diff);
        if( i != j )
        {
            int64_t diff_exm = (int64_t)(pi_ivtc_scores[i] - mean_except_min);
            var_except_min += (diff_exm*diff_exm);
        }
    }
    /* iend points one past end, but progressive counts as the +1. */
    var /= (uint64_t)(iend - imin + 1);
    var_except_min /= (uint64_t)(iend - imin);

    /* Extract cadence counter part of detected positions for the
       last two frames.

       Note that for the previous frame, we use the final detected cadence
       position, which was not necessarily produced by this algorithm.
       It is the result that was judged the most reliable.
    */
    int j_curr = p_ivtc->pi_cadence_pos_history[IVTC_LATEST-1];
    int pos_next = pi_detected_pos_to_cadence_pos[j];

    /* Be optimistic when unsure. We bias the detection toward accepting
       the next "correct" position, even if the variance check comes up bad.
    */
    bool b_expected = false;
    if( j_curr != CADENCE_POS_INVALID )
    {
        int pos_curr = pi_detected_pos_to_cadence_pos[j_curr];
        b_expected = (pos_next == (pos_curr + 1) % 5);
    }

    /* Use motion detect result as a final sanity check.
       If no motion, the result from this algorithm cannot be reliable.
    */
    int i_blocks_with_motion = p_ivtc->pi_motion[IVTC_LATEST];

    /* The numbers given here are empirical constants that have been tuned
       through trial and error. The test material used was NTSC anime DVDs.

        Easy-to-detect parts seem to give variance boosts of 40-70%, but
        hard-to-detect parts sometimes only 18%. Anything with a smaller boost
        in variance doesn't seem reliable for catching a new lock-on,

        Additionally, it seems that if the mean changes by less than 0.5%,
        the result is not reliable.

        Note that the numbers given are only valid for the pi_best_field_pairs
        detector strategy.

        For motion detection, the detector seems good enough so that
        we can threshold at zero.
    */
    bool b_result_reliable =
      ( i_blocks_with_motion > 0      &&
        mean_ratio           > 1.005  &&
        ( b_expected || ( (double)var > 1.17*(double)var_except_min ) )
      );
    p_ivtc->pb_s_reliable[IVTC_LATEST] = b_result_reliable;
}

/**
 * Internal helper function for RenderIVTC(): using raw detector data,
 * detect cadence position by a hard field repeat based algorithm ("vektor").
 *
 * This algorithm is inspired by the classic TVTime/Xine IVTC filter
 * by Billy Biggs (Vektor); hence the name. There are however some
 * differences between this and the TVTime/Xine filter.
 *
 * IVTCFrameInit() and IVTCLowLevelDetect() must have been called first.
 * Last frame must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 * @see IVTCFrameInit()
 * @see IVTCLowLevelDetect()
 * @see IVTCCadenceDetectFinalize()
 */
static void IVTCCadenceDetectAlgoVektor( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;

    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );

    /* This algorithm is based on detecting hard-repeated fields (by motion
       detection), and conservatively estimating what the seen repeats could
       mean for the cadence position.

       "Conservative" means that we do not rule out possibilities if repeats
       are *not* seen, but only *add* possibilities based on what repeats
       *are* seen. This is important. Otherwise full-frame repeats in the
       original film (8fps or 12fps animation is very common in anime),
       causing spurious field repeats, would mess up the detection.
       With this strategy, spurious repeats will only slow down the lock-on,
       and will not break an existing lock-on once acquired.

       Several possibilities are kept open until the sequence gives enough
       information to make a unique detection. When the sequence becomes
       inconsistent (e.g. bad cut), the detector resets itself.

       The main ideas taken from the TVTime/Xine algorithm are:
        1) Conservatively using information from detected field repeats,
        2) Cadence counting the earlier detection results and combining with
           the new detection result, and
        3) The observation that video TFF/BFF uniquely determines TFD.

       The main differences are
        1) Different motion detection (see EstimateNumBlocksWithMotion()).
           Vektor's original estimates the average top/bottom field diff
           over the last 3 frames, while ours uses a block-based approach
           for diffing and just compares the field diffs between "curr" and
           "next" against each other (see IVTCLowLevelDetect()).
           Both approaches are adaptive, but in a different way.
        2) The specific detection logic used is a bit different (see both
           codes for details; the original is in xine-lib, function
           determine_pulldown_offset_short_history_new() in pulldown.c;
           ours is obviously given below). I think the one given here
           is a bit simpler.

       Note that we don't have to worry about getting a detection in all cases.
       It's enough if we work reliably, say, 99% of the time, and the other 1%
       of the time just admit that we don't know the cadence position.
       (This mostly happens after a bad cut, when the new scene has
       "difficult" motion characteristics, such as repeated film frames.)
       Our frame composer is built to handle also cases where we have no
       reliable detection of the cadence position; see IVTCOutputOrDropFrame().
       More important is to never lock on incorrectly, as this would both
       generate interlacing artifacts where none existed, and cause motion
       to stutter (because duplicate frames would be shown and unique ones
       dropped).
    */

    /* Progressive requires no repeats, so it is always a possibility.
       Filtering will drop it out if we know that the current position
       cannot be "dea".
    */
    int detected = 0;
    detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_PROGRESSIVE ];

    /* Add in other possibilities depending on field repeats seen during the
       last three input frames (i.e. two transitions between input frames).
       See the "Dups." column in the cadence tables.
    */
    bool b_top_rep     = p_ivtc->pi_top_rep[IVTC_LATEST];
    bool b_bot_rep     = p_ivtc->pi_bot_rep[IVTC_LATEST];
    bool b_old_top_rep = p_ivtc->pi_top_rep[IVTC_LATEST-1];
    bool b_old_bot_rep = p_ivtc->pi_bot_rep[IVTC_LATEST-1];
    if( b_top_rep )
    {
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_TFF_EAB ];
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_BFF_BCD ];
    }
    if( b_old_top_rep )
    {
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_TFF_ABC ];
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_BFF_CDE ];
    }
    if( b_bot_rep )
    {
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_TFF_BCD ];
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_BFF_EAB ];
    }
    if( b_old_bot_rep )
    {
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_TFF_CDE ];
        detected |= pi_detected_pos_to_bitmask[ CADENCE_POS_BFF_ABC ];
    }

    /* A TFF stream may only have TFF telecine, and similarly for BFF.
       Discard the possibility we know to be incorrect for this stream.
       (The stream may flipflop between the possibilities if it contains
        soft-telecined sequences or lone field repeats, so we must keep
        detecting this for each incoming frame.)
    */
    bool b_tff = p_next->b_top_field_first;
    if( b_tff )
        detected &= VEKTOR_CADENCE_POS_TFF;
    else
        detected &= VEKTOR_CADENCE_POS_BFF;

    /* Predict possible next positions based on our last detection.
       Begin with a shift and carry. */
    int predicted = p_ivtc->pi_v_raw[IVTC_LATEST-1];
    bool b_wrap_tff = false;
    bool b_wrap_bff = false;
    if( predicted & VEKTOR_CADENCE_POS_TFF_HIGH )
        b_wrap_tff = true;
    if( predicted & VEKTOR_CADENCE_POS_BFF_HIGH )
        b_wrap_bff = true;
    /* bump to next position and keep only valid bits */
    predicted = (predicted << 1) & VEKTOR_CADENCE_POS_ALL;
    /* carry */
    if( b_wrap_tff )
        predicted |= VEKTOR_CADENCE_POS_TFF_LOW;
    if( b_wrap_bff )
        predicted |= VEKTOR_CADENCE_POS_BFF_LOW;

    /* Filter: narrow down possibilities based on previous detection,
       if consistent. If not consistent, reset the detector.
       Reset works better than just using the latest raw detection.
    */
    if( (detected & predicted) != 0 )
        detected = detected & predicted;
    else
        detected = VEKTOR_CADENCE_POS_ALL;

    /* We're done. Save result to our internal storage so we can use it
       for prediction at the next frame.

       Note that the outgoing frame check in IVTCOutputOrDropFrame()
       has a veto right, resetting our state if it determines that
       the cadence has become broken.
    */
    p_ivtc->pi_v_raw[IVTC_LATEST] = detected;

    /* See if the position has been detected uniquely.
       If so, we have acquired a lock-on. */
    ivtc_cadence_pos exact = CADENCE_POS_INVALID;
    if( detected != 0 )
    {
        for( int i = 0; i < NUM_CADENCE_POS; i++ )
        {
            /* Note that we must use "&" instead of just equality to catch
               the progressive case, and also not to trigger on an incomplete
               detection. */
            if( detected == (detected & pi_detected_pos_to_bitmask[i]) )
            {
                exact = i;
                break;
            }
        }
    }

    /* If the result was unique, now "exact" contains the detected
       cadence position (and otherwise CADENCE_POS_INVALID).

       In practice, if the result from this algorithm is unique,
       it is always reliable.
    */
    p_ivtc->pi_v_cadence_pos[IVTC_LATEST] =  exact;
    p_ivtc->pb_v_reliable[IVTC_LATEST]    = (exact != CADENCE_POS_INVALID);
}

/**
 * Internal helper function for RenderIVTC(): decide the final detected
 * cadence position for the current position of the PCN stencil,
 * using the results of the different cadence detection algorithms.
 *
 * Must be called after all IVTCCadenceDetectAlgo*() functions.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 * @see IVTCCadenceDetectAlgoScores()
 * @see IVTCCadenceDetectAlgoVektor()
 */
static void IVTCCadenceDetectFinalize( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;

    /* In practice "vektor" is more reliable than "scores", but it may
       take longer to lock on. Thus, we prefer "vektor" if its reliable bit
       is set, then "scores", and finally just give up.

       For progressive sequences, "vektor" outputs "3, -, 3, -, ...",
       because the repeated progressive position is an inconsistent prediction.
       In this case, "scores" fills in the blanks. (This particular task
       could also be done without another cadence detector, by just
       detecting the alternating pattern of "3" and no result.)
    */
    int pos = CADENCE_POS_INVALID;
    if( p_ivtc->pb_v_reliable[IVTC_LATEST] )
        pos = p_ivtc->pi_v_cadence_pos[IVTC_LATEST];
    else if( p_ivtc->pb_s_reliable[IVTC_LATEST] )
        pos = p_ivtc->pi_s_cadence_pos[IVTC_LATEST];
    p_ivtc->pi_cadence_pos_history[IVTC_LATEST] = pos;
}

/**
 * Internal helper function for RenderIVTC(): using stream flags,
 * detect soft telecine.
 *
 * This function is different from the other detectors; it may enter or exit
 * IVTC_MODE_TELECINED_NTSC_SOFT, if it detects that soft telecine has just
 * been entered or exited.
 *
 * Upon exit from soft telecine, the filter will resume operation in its
 * previous mode (which it had when soft telecine was entered).
 *
 * Last three frames must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 */
static void IVTCSoftTelecineDetect( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;
    picture_t *p_prev = p_sys->pp_history[0];
    picture_t *p_curr = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );
    assert( p_curr != NULL );
    assert( p_prev != NULL );

    /* Soft telecine can be detected from the flag pattern:
       nb_fields = 3,2,3,2,... and *video* TFF = true, false, false, true
       (TFF telecine) or false, true, true, false (BFF telecine).

       We don't particularly care which field goes first, because in soft TC
       we're working with progressive frames. And in any case, the video FDs
       of successive frames must match any field repeats in order for field
       renderers (such as traditional DVD player + CRT TV) to work correctly.
       Thus the video TFF/BFF flag provides no additional useful information
       for us on top of checking nb_fields.

       The only thing to *do* to soft telecine in an IVTC filter is to even
       out the outgoing PTS diffs to 2.5 fields each, so that we get
       a steady 24fps output. Thus, we can do this processing even if it turns
       out that we saw a lone field repeat (which are also sometimes used,
       such as in the Silent Mobius OP and in Sol Bianca). We can be aggressive
       and don't need to care about false positives - as long as we are equally
       aggressive about dropping out of soft telecine mode the moment a "2" is
       followed by another "2" and not a "3" as in soft TC.

       Finally, we conclude that the one-frame future buffer is enough for us
       to make soft TC decisions just in time for rendering the frame in the
       "current" position. The flag patterns given below constitute proof
       of this property.

       Soft telecine is relatively rare at least in anime, but it exists;
       e.g. Angel Links OP, Silent Mobius, and Stellvia of the Universe have
       sequences that are soft telecined. Stellvia, especially, alternates
       between soft and hard telecine all the time.
    */

    /* Valid stream flag patterns for soft telecine. There are three: */

    /* Entering soft telecine at frame curr, or running inside it already */
    bool b_soft_telecine_1 = (p_prev->i_nb_fields == 2) &&
                             (p_curr->i_nb_fields == 3) &&
                             (p_next->i_nb_fields == 2);
    /* Running inside soft telecine */
    bool b_soft_telecine_2 = (p_prev->i_nb_fields == 3) &&
                             (p_curr->i_nb_fields == 2) &&
                             (p_next->i_nb_fields == 3);
    /* Exiting soft telecine at frame curr (curr is the last frame
       that should be handled as soft TC) */
    bool b_soft_telecine_3 = (p_prev->i_nb_fields == 3) &&
                             (p_curr->i_nb_fields == 2) &&
                             (p_next->i_nb_fields == 2);

    /* Soft telecine is very clear-cut - the moment we see or do not see
       a valid flag pattern, we can change the filter mode.
    */
    if( b_soft_telecine_1 || b_soft_telecine_2 || b_soft_telecine_3 )
    {
        if( p_ivtc->i_mode != IVTC_MODE_TELECINED_NTSC_SOFT )
        {
            msg_Dbg( p_filter, "IVTC: 3:2 pulldown: NTSC soft telecine "\
                               "detected." );
            p_ivtc->i_old_mode = p_ivtc->i_mode;
        }

        /* Valid flag pattern seen, this frame is soft telecined */
        p_ivtc->i_mode = IVTC_MODE_TELECINED_NTSC_SOFT;

        /* Only used during IVTC'ing hard telecine. */
        p_ivtc->i_cadence_pos = CADENCE_POS_INVALID;
        p_ivtc->i_tfd         = TFD_INVALID;
    }
    /* Note: no flag pattern match now */
    else if( p_ivtc->i_mode == IVTC_MODE_TELECINED_NTSC_SOFT )
    {
        msg_Dbg( p_filter, "IVTC: 3:2 pulldown: NTSC soft telecine ended. "\
                           "Returning to previous mode." );

        /* No longer soft telecined, return filter to the mode it had earlier.
           This is needed to fix cases where we came in from hard telecine, and
           should go back, but can't catch a cadence in time before telecined
           frames slip through. Kickstarting back to hard IVTC, using the
           emergency frame composer until the cadence locks on again,
           fixes the problem. This happens a lot in Stellvia.
        */
        p_ivtc->i_mode = p_ivtc->i_old_mode;
        p_ivtc->i_cadence_pos = 0; /* Wild guess. The film frame reconstruction
                                      will start in emergency mode, and this
                                      will be filled in by the detector ASAP.*/
        /* I suppose video field dominance no longer flipflops. */
        p_ivtc->i_tfd = !p_next->b_top_field_first; /* tff  <=>  TFD == 0 */
    }
}

/**
 * Internal helper function for RenderIVTC(): using the history of detected
 * cadence positions, analyze the cadence and enter or exit
 * IVTC_MODE_TELECINED_NTSC_HARD when appropriate.
 *
 * This also updates b_sequence_valid.
 *
 * Last three frames must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 */
static void IVTCCadenceAnalyze( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;
    picture_t *p_prev = p_sys->pp_history[0];
    picture_t *p_curr = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );
    assert( p_curr != NULL );
    assert( p_prev != NULL );

    /* Determine which frames in the buffer qualify for analysis.

       Note that hard telecine always has nb_fields = 2 and
       video TFF = constant (i.e. the stream flags look no different from
       a true interlaced or true progressive stream). Basically, no one ever
       sets the progressive frame flag for the input frames d, e, and a -
       in practice they're all flagged as interlaced.

       A frame may qualify for hard TC analysis if it has no soft field repeat
       (i.e. it cannot be part of a soft telecine). The condition
       nb_fields == 2 must always match.

       Additionally, curr and next must have had motion with respect to the
       previous frame, to ensure that the different field combinations have
       produced unique pictures.

       Alternatively, if there was no motion, but the cadence position was
       reliably detected and it was the expected one, we qualify the frame
       for analysis (mainly, for TFD voting).

       We only proceed with the cadence analysis if all three frames
       in the buffer qualify.
    */

    /* Note that these are the final detected positions
       produced by IVTCCadenceDetectFinalize(). */
    int j_next = p_ivtc->pi_cadence_pos_history[IVTC_LATEST];
    int j_curr = p_ivtc->pi_cadence_pos_history[IVTC_LATEST-1];
    int j_prev = p_ivtc->pi_cadence_pos_history[IVTC_LATEST-2];

    bool b_expected = false;
    if( j_next != CADENCE_POS_INVALID  &&  j_curr != CADENCE_POS_INVALID )
    {
        int pos_next = pi_detected_pos_to_cadence_pos[j_next];
        int pos_curr = pi_detected_pos_to_cadence_pos[j_curr];
        b_expected = (pos_next == (pos_curr + 1) % 5);
    }
    bool b_old_expected  = false;
    if( j_curr != CADENCE_POS_INVALID  &&  j_prev != CADENCE_POS_INVALID )
    {
        int pos_curr = pi_detected_pos_to_cadence_pos[j_curr];
        int pos_prev = pi_detected_pos_to_cadence_pos[j_prev];
        b_old_expected = (pos_curr == (pos_prev + 1) % 5);
    }

    int i_motion     = p_ivtc->pi_motion[IVTC_LATEST];
    int i_old_motion = p_ivtc->pi_motion[IVTC_LATEST-1];

    bool b_prev_valid  = (p_prev->i_nb_fields == 2);
    bool b_curr_valid  = (p_curr->i_nb_fields == 2)  &&
                         (i_old_motion > 0  ||  b_old_expected);
    bool b_next_valid  = (p_next->i_nb_fields == 2)  &&
                         (i_motion > 0      ||  b_expected);
    bool b_no_invalids = (b_prev_valid && b_curr_valid && b_next_valid);

    /* Final sanity check: see that the detection history has been
       completely filled,  i.e. the latest three positions of the stencil
       have given a result from the cadence detector.
    */
    if( b_no_invalids )
    {
        for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE; ++i )
        {
            const int i_detected_pos = p_ivtc->pi_cadence_pos_history[i];
            if( i_detected_pos == CADENCE_POS_INVALID )
            {
                b_no_invalids = false;
                break;
            }
        }
    }

    /* If still ok, do the analysis. */
    p_ivtc->b_sequence_valid = false; /* needed in frame reconstruction */
    if( b_no_invalids )
    {
        /* Convert the history elements to cadence position and TFD. */
        int pi_tfd[IVTC_DETECTION_HISTORY_SIZE];
        int pi_pos[IVTC_DETECTION_HISTORY_SIZE];
        for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE; ++i )
        {
            const int i_detected_pos = p_ivtc->pi_cadence_pos_history[i];
            pi_pos[i] = pi_detected_pos_to_cadence_pos[i_detected_pos];
            pi_tfd[i] = pi_detected_pos_to_tfd[i_detected_pos];
        }

        /* See if the sequence is valid. The cadence positions must be
           successive mod 5. We can't say anything about TFF/BFF yet,
           because the progressive-looking position "dea" may be there.
           If the sequence otherwise looks valid, we handle that last
           by voting.

           We also test for a progressive signal here, so that we know
           when to exit IVTC_MODE_TELECINED_NTSC_HARD.
        */
        p_ivtc->b_sequence_valid = true;
        bool b_all_progressive = (pi_pos[0] == 3);
        int j = pi_pos[0];
        for( int i = 1; i < IVTC_DETECTION_HISTORY_SIZE; ++i )
        {
            if( pi_pos[i] != (++j % 5) )
                p_ivtc->b_sequence_valid = false;
            if( pi_pos[i] != 3 )
                b_all_progressive = false;
        }
        p_ivtc->pb_all_progressives[IVTC_LATEST] = b_all_progressive;

        if( p_ivtc->b_sequence_valid )
        {
            /* Determine TFF/BFF. */
            int i_vote_invalid = 0;
            int i_vote_tff     = 0;
            int i_vote_bff     = 0;
            for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE; ++i )
            {
                if( pi_tfd[i] == TFD_INVALID )
                    i_vote_invalid++;
                else if( pi_tfd[i] == TFD_TFF )
                    i_vote_tff++;
                else if( pi_tfd[i] == TFD_BFF )
                    i_vote_bff++;
            }

            /* With three entries, two votes for any one item are enough
               to decide this conclusively. */
            int i_telecine_field_dominance = TFD_INVALID;
            if( i_vote_tff >= 2)
                i_telecine_field_dominance = TFD_TFF;
            else if( i_vote_bff >= 2)
                i_telecine_field_dominance = TFD_BFF;
            /* In all other cases, "invalid" won or no winner.
               This means no NTSC telecine detected. */

            /* Lock on to the cadence if it was valid and TFF/BFF was found.

               Also, aggressively update the cadence counter from the
               lock-on data whenever we can. In practice this has been found
               to be a reliable strategy (if the cadence detectors are
               good enough).
            */
            if( i_telecine_field_dominance == TFD_TFF )
            {
                if( p_ivtc->i_mode != IVTC_MODE_TELECINED_NTSC_HARD )
                    msg_Dbg( p_filter, "IVTC: 3:2 pulldown: NTSC TFF "\
                                       "hard telecine detected." );
                p_ivtc->i_mode        = IVTC_MODE_TELECINED_NTSC_HARD;
                p_ivtc->i_cadence_pos = pi_pos[IVTC_LATEST];
                p_ivtc->i_tfd         = TFD_TFF;
            }
            else if( i_telecine_field_dominance == TFD_BFF )
            {
                if( p_ivtc->i_mode != IVTC_MODE_TELECINED_NTSC_HARD )
                    msg_Dbg( p_filter, "IVTC: 3:2 pulldown: NTSC BFF "\
                                       "hard telecine detected." );
                p_ivtc->i_mode        = IVTC_MODE_TELECINED_NTSC_HARD;
                p_ivtc->i_cadence_pos = pi_pos[IVTC_LATEST];
                p_ivtc->i_tfd         = TFD_BFF;
            }
        }
        /* No telecine... maybe a progressive signal? */
        else if( b_all_progressive )
        {
            /* It seems that in practice, three "3"s in a row can still be
               a fluke rather often. Four or five usually are not.
               This fixes the Stellvia OP. */

            bool b_really_all_progressive = true;
            for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE ; i++ )
            {
                if( p_ivtc->pb_all_progressives[i] == false )
                {
                    b_really_all_progressive = false;
                    break;
                }
            }

            /* If we still think the signal is progressive... */
            if( b_really_all_progressive )
            {
                /* ...exit film mode immediately. This does not break
                   soft TC handling, because for soft TC at least one
                   of the frames will not qualify (due to i_nb_fields == 3),
                   and in that case this analysis will not run.
                */
                if( p_ivtc->i_mode == IVTC_MODE_TELECINED_NTSC_HARD )
                    msg_Dbg( p_filter, "IVTC: 3:2 pulldown: progressive "\
                                       "signal detected." );
                p_ivtc->i_mode        = IVTC_MODE_DETECTING;
                p_ivtc->i_cadence_pos = CADENCE_POS_INVALID;
                p_ivtc->i_tfd         = TFD_INVALID;
            }
        }
        /* Final missing "else": no valid NTSC telecine sequence detected.

           Either there is no telecine, or the detector - although it produced
           results - had trouble finding it. In this case we do nothing,
           as it's not a good idea to act on unreliable data.

           Note that if we are already in IVTC_MODE_TELECINED_NTSC_HARD, this
           case means that we have lost the lock-on, but are still (probably)
           in a hard-telecined stream. This will start the emergency mode
           for film frame reconstruction. See IVTCOutputOrDropFrame().
        */
    }
}

/**
 * Internal helper function for RenderIVTC(): render or drop frame,
 * whichever needs to be done. This also sets the output frame PTS.
 *
 * Last two frames must be available in the history buffer.
 *
 * This is an internal function only used by RenderIVTC().
 * There is no need to call this function manually.
 *
 * @param p_filter The filter instance. Must be non-NULL.
 * @param[out] p_dst Frame will be rendered here. Must be non-NULL.
 * @return Whether a frame was constructed.
 * @retval true Yes, output frame is in p_dst.
 * @retval false No, this frame was dropped as part of normal IVTC operation.
 * @see RenderIVTC()
 */
static bool IVTCOutputOrDropFrame( filter_t *p_filter, picture_t *p_dst )
{
    assert( p_filter != NULL );
    assert( p_dst != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;
    mtime_t t_final = VLC_TS_INVALID; /* for custom timestamp mangling */

    picture_t *p_curr = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    assert( p_next != NULL );
    assert( p_curr != NULL );

    /* Perform IVTC if we're in film mode (either hard or soft telecine).

       Note that we don't necessarily have a lock-on, even if we are in
       IVTC_MODE_TELECINED_NTSC_HARD. We *may* be locked on, or alternatively,
       we have seen a valid cadence some time in the past, but lock-on has
       since been lost, and we have not seen a progressive signal after that.
       The latter case usually results from bad cuts, which interrupt
       the cadence.

       Lock-on state is given by p_ivtc->b_sequence_valid.
    */
    int i_result_score = -1;
    int op;
    if( p_ivtc->i_mode == IVTC_MODE_TELECINED_NTSC_HARD )
    {
        /* Decide what to do. The operation table is only enabled
           if the cadence seems reliable. Otherwise we use a backup strategy.
        */
        if( p_ivtc->b_sequence_valid )
        {
            assert( p_ivtc->i_cadence_pos != CADENCE_POS_INVALID );
            assert( p_ivtc->i_tfd != TFD_INVALID );

            /* Pick correct operation from the operation table. */
            op = pi_reconstruction_ops[p_ivtc->i_tfd][p_ivtc->i_cadence_pos];

            if( op == IVTC_OP_DROP_FRAME )
            {
                /* Bump cadence counter into the next expected position */
                p_ivtc->i_cadence_pos = (p_ivtc->i_cadence_pos + 1) % 5;

                /* Drop frame. We're done. */
                return false;
            }
            else
            {
                if( op == IVTC_OP_COPY_N )
                    i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TNBN];
                else if( op == IVTC_OP_COPY_C )
                    i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TCBC];
                else if( op == IVTC_OP_COMPOSE_TNBC )
                    i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TNBC];
                else if( op == IVTC_OP_COMPOSE_TCBN )
                    i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TCBN];

                /* Sanity check the result */

                /* Compute running mean of outgoing interlace score.
                   See below for history mechanism. */
                int i_avg = 0;
                for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE; i++)
                    i_avg += p_ivtc->pi_final_scores[i];
                i_avg /= IVTC_DETECTION_HISTORY_SIZE;

                /* Check if the score suddenly became "clearly larger".
                   Also, filter out spurious peaks at the low end. */
                if( i_result_score > 1000  &&  i_result_score > 2*i_avg )
                {
                    /* Sequence wasn't reliable after all; we'll use
                       the Transcode strategy for this frame. */
                    p_ivtc->b_sequence_valid = false;
                    msg_Dbg( p_filter, "Rejected cadence-based frame "\
                                       "construction: interlace score %d "\
                                       "(running average %d)",
                                       i_result_score, i_avg );

                    /* We also reset the detector used in the "vektor"
                       algorithm, as it depends on having a reliable previous
                       position. In practice, we continue using the Transcode
                       strategy until the cadence becomes locked on again.
                       (At that point, b_sequence_valid will become true again,
                       and we continue with this strategy.)
                    */
                    p_ivtc->pi_v_raw[IVTC_LATEST] = VEKTOR_CADENCE_POS_ALL;
                }
            }
        }

        /* Frame not dropped, and the cadence counter seems unreliable.

            Note that this is not an "else" to the previous case. This may
            begin with a valid sequence, and then the above logic decides
            that it wasn't valid after all.
        */
        if( !p_ivtc->b_sequence_valid )
        {
            /* In this case, we must proceed with no cadence information.
                We use a Transcode-like strategy.

                We check which field paired with TN or BN (accounting for
                the field dominance) gives the smallest interlace score,
                and declare that combination the resulting progressive frame.

                This strategy gives good results on average, but often fails
                in talking scenes in anime. Those can be handled more reliably
                with a locked-on cadence produced by the "vektor" algorithm.
            */

            int tnbn = p_ivtc->pi_scores[FIELD_PAIR_TNBN]; /* TFF and BFF */
            int tnbc = p_ivtc->pi_scores[FIELD_PAIR_TNBC]; /* TFF only */
            int tcbn = p_ivtc->pi_scores[FIELD_PAIR_TCBN]; /* BFF only */

            if( p_next->b_top_field_first )
            {
                if( tnbn <= tnbc )
                {
                    op = IVTC_OP_COPY_N;
                    i_result_score = tnbn;
                }
                else
                {
                    op = IVTC_OP_COMPOSE_TNBC;
                    i_result_score = tnbc;
                }
            }
            else
            {
                if( tnbn <= tcbn )
                {
                    op = IVTC_OP_COPY_N;
                    i_result_score = tnbn;
                }
                else
                {
                    op = IVTC_OP_COMPOSE_TCBN;
                    i_result_score = tcbn;
                }
            }
        }

        /* Mangle timestamps when locked on.

           "Current" is the frame that is being extracted now. Use its original
           timestamp as the base.

           Note that this way there will be no extra delay compared to the
           raw stream, even though we look one frame into the future.
        */
        if( p_ivtc->b_sequence_valid )
        {
            /* Convert 29.97 -> 23.976 fps. We get to this point only if we
               didn't drop the frame, so we always get a valid delta.
            */
            int i_timestamp_delta = pi_timestamp_deltas[p_ivtc->i_cadence_pos];
            assert( i_timestamp_delta >= 0 );

            /* FIXME: use field length as measured by Deinterlace()? */
            t_final = p_curr->date
                    + (p_next->date - p_curr->date)*i_timestamp_delta/4;
        }
        else /* Do not mangle timestamps (or drop frames, either) if cadence
                is not locked on. This causes one of five output frames - if
                all are reconstructed correctly - to be a duplicate, but in
                practice at least with anime (which is the kind of material
                that tends to have this problem) this is less noticeable than
                a sudden jump in the cadence. Especially, a consistently wrong
                lock-on will cause a very visible stutter, which we wish
                to avoid. */
        {
            t_final = p_curr->date;
        }

        /* Bump cadence counter into the next expected position. */
        p_ivtc->i_cadence_pos = (p_ivtc->i_cadence_pos + 1) % 5;
    }
    else if( p_ivtc->i_mode == IVTC_MODE_TELECINED_NTSC_SOFT )
    {
        /* Soft telecine. We have the progressive frames already;
           even out PTS diffs only. */

        /* Pass through the "current" frame. We must choose the frame "current"
           in order to be able to detect soft telecine before we have to output
           the frame. See IVTCSoftTelecineDetect(). Also, this allows
           us to peek at the next timestamp to calculate the duration of
           "current".
        */
        op = IVTC_OP_COPY_C;
        i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TCBC];

        /* Timestamp mangling for soft telecine: bump "threes" forward by
           0.5 field durations. This is more forgiving for the renderer
           than bumping the "twos" back (which would require to render
           them sooner),
        */
        if( p_curr->i_nb_fields == 3 )
        {
            /* Approximate field duration from the PTS difference. */
            /* FIXME: use field length as measured by Deinterlace()? */
            mtime_t i_half_field_dur = ( (p_next->date - p_curr->date)/3 ) / 2;
            t_final = p_curr->date + i_half_field_dur;
        }
        else /* Otherwise, use original PTS of the outgoing frame. */
        {
            t_final = p_curr->date;
        }
    }
    else /* Not film mode, timestamp mangling bypassed. */
    {
        op = IVTC_OP_COPY_N;
        i_result_score = p_ivtc->pi_scores[FIELD_PAIR_TNBN];

        /* Preserve original PTS (note that now, in principle,
                                  "next" is the outgoing frame) */
        t_final = p_next->date;
    }

    /* There is only one case where we should drop the frame,
       and it was already handled above. */
    assert( op != IVTC_OP_DROP_FRAME );

    /* Render into p_dst according to the final operation chosen. */
    if( op == IVTC_OP_COPY_N )
        picture_Copy( p_dst, p_next );
    else if( op == IVTC_OP_COPY_C )
        picture_Copy( p_dst, p_curr );
    else if( op == IVTC_OP_COMPOSE_TNBC )
        ComposeFrame( p_filter, p_dst, p_next, p_curr, CC_ALTLINE, false );
    else if( op == IVTC_OP_COMPOSE_TCBN )
        ComposeFrame( p_filter, p_dst, p_curr, p_next, CC_ALTLINE, false );

    /* Slide history of outgoing interlace scores. This must be done last,
       and only if the frame was not dropped, so we do it here.

       This is used during the reconstruction to get an idea of what is
       (in the temporally local sense) an acceptable interlace score
       for a correctly reconstructed frame. See above.
    */
    for( int i = 1; i < IVTC_DETECTION_HISTORY_SIZE; i++ )
        p_ivtc->pi_final_scores[i-1] = p_ivtc->pi_final_scores[i];
    p_ivtc->pi_final_scores[IVTC_LATEST] = i_result_score;

    /* Note that picture_Copy() copies the PTS, too. Apply timestamp mangling
       now, if any was needed.
    */
    if( t_final > VLC_TS_INVALID )
        p_dst->date = t_final;

    return true;
}

/*****************************************************************************
 * Public functions
 *****************************************************************************/

/* See function doc in header. */
int RenderIVTC( filter_t *p_filter, picture_t *p_dst )
{
    assert( p_filter != NULL );
    assert( p_dst != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc  = &p_sys->ivtc;

    picture_t *p_prev = p_sys->pp_history[0];
    picture_t *p_curr = p_sys->pp_history[1];
    picture_t *p_next = p_sys->pp_history[2];

    /* If the history mechanism has failed, we have nothing to do. */
    if( !p_next )
        return VLC_EGENERIC;

    /* Slide algorithm-specific histories */
    IVTCFrameInit( p_filter );

    /* Filter if we have all the pictures we need.
       Note that we always have p_next at this point. */
    if( p_prev && p_curr )
    {
        /* Update raw data for motion, field repeats, interlace scores... */
        IVTCLowLevelDetect( p_filter );

        /* Detect soft telecine.

           Enter/exit IVTC_MODE_TELECINED_NTSC_SOFT when needed.
        */
        IVTCSoftTelecineDetect( p_filter );

        /* Detect hard telecine.

           Enter/exit IVTC_MODE_TELECINED_NTSC_HARD when needed.

           If we happen to be running in IVTC_MODE_TELECINED_NTSC_SOFT,
           we nevertheless let the algorithms see for themselves that
           the stream is progressive. This doesn't break anything,
           and this way the full filter state gets updated at each frame.

           See the individual function docs for details.
        */
        IVTCCadenceDetectAlgoScores( p_filter );
        IVTCCadenceDetectAlgoVektor( p_filter );
        IVTCCadenceDetectFinalize( p_filter ); /* pick winner */
        IVTCCadenceAnalyze( p_filter ); /* update filter state */

        /* Now we can... */
        bool b_have_output_frame = IVTCOutputOrDropFrame( p_filter, p_dst );

        /* The next frame will get a custom timestamp, too. */
        p_sys->i_frame_offset = CUSTOM_PTS;

        if( b_have_output_frame )
            return VLC_SUCCESS;
        else
            return VLC_EGENERIC; /* Signal the caller not to expect a frame */
    }
    else if( !p_prev && !p_curr ) /* first frame */
    {
        /* Render the first frame as-is, so that a picture appears immediately.

           We will also do some init for the filter. This score will become
           TPBP by the time the actual filter starts. Note that the sliding of
           final scores only starts when the filter has started (third frame).
        */
        int i_score = CalculateInterlaceScore( p_next, p_next );
        p_ivtc->pi_scores[FIELD_PAIR_TNBN] = i_score;
        p_ivtc->pi_final_scores[0]         = i_score;

        picture_Copy( p_dst, p_next );
        return VLC_SUCCESS;
    }
    else /* second frame */
    {
        /* If the history sliding mechanism works correctly,
           the only remaining possibility is that: */
        assert( p_curr && !p_prev );

        /* We need three frames for the cadence detector to work, so we just
           do some init for the detector and pass the frame through.
           Passthrough for second frame, too, works better than drop
           for some still-image DVD menus.

           Now that we have two frames, we can run a full IVTCLowLevelDetect().

           The interlace scores from here will become TCBC, TCBP and TPBC
           when the filter starts. The score for the current TCBC has already
           been computed at the first frame, and slid into place at the start
           of this frame (by IVTCFrameInit()).
        */
        IVTCLowLevelDetect( p_filter );

        /* Note that the sliding mechanism for output scores only starts
           when the actual filter does.
        */
        p_ivtc->pi_final_scores[1] = p_ivtc->pi_scores[FIELD_PAIR_TNBN];

        /* At the next frame, the filter starts. The next frame will get
           a custom timestamp. */
        p_sys->i_frame_offset = CUSTOM_PTS;

        picture_Copy( p_dst, p_next );
        return VLC_SUCCESS;
    }
}

/* See function doc in header. */
void IVTCClearState( filter_t *p_filter )
{
    assert( p_filter != NULL );

    filter_sys_t *p_sys = p_filter->p_sys;
    ivtc_sys_t *p_ivtc = &p_sys->ivtc;

    p_ivtc->i_cadence_pos = CADENCE_POS_INVALID;
    p_ivtc->i_tfd         = TFD_INVALID;
    p_ivtc->b_sequence_valid = false;
    p_ivtc->i_mode     = IVTC_MODE_DETECTING;
    p_ivtc->i_old_mode = IVTC_MODE_DETECTING;
    for( int i = 0; i < IVTC_NUM_FIELD_PAIRS; i++ )
        p_ivtc->pi_scores[i] = 0;
    for( int i = 0; i < IVTC_DETECTION_HISTORY_SIZE; i++ )
    {
        p_ivtc->pi_cadence_pos_history[i] = CADENCE_POS_INVALID;

        p_ivtc->pi_s_cadence_pos[i] = CADENCE_POS_INVALID;
        p_ivtc->pb_s_reliable[i]    = false;
        p_ivtc->pi_v_cadence_pos[i] = CADENCE_POS_INVALID;
        p_ivtc->pb_v_reliable[i]    = false;

        p_ivtc->pi_v_raw[i]         = VEKTOR_CADENCE_POS_ALL;

        p_ivtc->pi_top_rep[i] =  0;
        p_ivtc->pi_bot_rep[i] =  0;
        p_ivtc->pi_motion[i]  = -1;

        p_ivtc->pb_all_progressives[i] = false;

        p_ivtc->pi_final_scores[i] = 0;
    }
}
