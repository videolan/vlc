/*****************************************************************************
 * vpar_synchro.c : frame dropping routines
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vpar_synchro.c,v 1.73 2001/01/15 18:02:49 massiot Exp $
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
 * FIXME: write a few words about stream structure changes.
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
#include "defs.h"

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-dec.h"

#include "video.h"
#include "video_output.h"

#include "video_decoder.h"
#include "../video_decoder/vdec_idct.h"
#include "../video_decoder/vdec_motion.h"

#include "../video_decoder/vpar_blocks.h"
#include "../video_decoder/vpar_headers.h"
#include "../video_decoder/vpar_synchro.h"
#include "../video_decoder/video_parser.h"

#include "main.h"

/*
 * Local prototypes
 */
static int  SynchroType( void );

/* Error margins */
#define DELTA                   (int)(0.040*CLOCK_FREQ)

#define DEFAULT_NB_P            5
#define DEFAULT_NB_B            1

/*****************************************************************************
 * vpar_SynchroInit : You know what ?
 *****************************************************************************/
void vpar_SynchroInit( vpar_thread_t * p_vpar )
{
    p_vpar->synchro.i_type = SynchroType();
    p_vpar->synchro.i_start = p_vpar->synchro.i_end = 0;
    vlc_mutex_init( &p_vpar->synchro.fifo_lock );

    /* We use a fake stream pattern, which is often right. */
    p_vpar->synchro.i_n_p = p_vpar->synchro.i_eta_p = DEFAULT_NB_P;
    p_vpar->synchro.i_n_b = p_vpar->synchro.i_eta_b = DEFAULT_NB_B;
    memset( p_vpar->synchro.p_tau, 0, 4 * sizeof(mtime_t) );
    memset( p_vpar->synchro.pi_meaningful, 0, 4 * sizeof(unsigned int) );
    p_vpar->synchro.b_dropped_last = 0;
    p_vpar->synchro.current_pts = mdate() + DEFAULT_PTS_DELAY;
    p_vpar->synchro.backward_pts = p_vpar->synchro.next_period = 0;
#ifdef STATS
    p_vpar->synchro.i_trashed_pic = p_vpar->synchro.i_not_chosen_pic = 
        p_vpar->synchro.i_pic = 0;
#endif
}

/*****************************************************************************
 * vpar_SynchroChoose : Decide whether we will decode a picture or not
 *****************************************************************************/
boolean_t vpar_SynchroChoose( vpar_thread_t * p_vpar, int i_coding_type,
                              int i_structure )
{
    /* For clarity reasons, we separated the special synchros code from the
     * mathematical synchro */

    if( p_vpar->synchro.i_type != VPAR_SYNCHRO_DEFAULT )
    {
        switch( i_coding_type )
        {
        case I_CODING_TYPE:
            /* I, IP, IP+, IPB */
            if( p_vpar->synchro.i_type == VPAR_SYNCHRO_Iplus )
            {
                p_vpar->synchro.b_dropped_last = 1;
            }
            return( 1 );

        case P_CODING_TYPE:
            if( p_vpar->synchro.i_type == VPAR_SYNCHRO_I ) /* I */
            {
                return( 0 );
            }

            if( p_vpar->synchro.i_type == VPAR_SYNCHRO_Iplus ) /* I+ */
            {
                if( p_vpar->synchro.b_dropped_last )
                {
                    p_vpar->synchro.b_dropped_last = 0;
                    return( 1 );
                }
                else
                {
                    return( 0 );
                }
            }

            return( 1 ); /* IP, IP+, IPB */

        case B_CODING_TYPE:
            if( p_vpar->synchro.i_type <= VPAR_SYNCHRO_IP ) /* I, IP */
            {
                return( 0 );
            }
            else if( p_vpar->synchro.i_type == VPAR_SYNCHRO_IPB ) /* IPB */
            {
                return( 1 );
            }

            p_vpar->synchro.b_dropped_last ^= 1; /* IP+ */
            return( !p_vpar->synchro.b_dropped_last );
        }
        return( 0 ); /* never reached but gcc yells at me */
    }
    else
    {
#define TAU_PRIME( coding_type )    (p_vpar->synchro.p_tau[(coding_type)] \
                                 + (p_vpar->synchro.p_tau[(coding_type)] >> 1) \
                                            + tau_yuv)
#define S                           p_vpar->synchro
        /* VPAR_SYNCHRO_DEFAULT */
        mtime_t         now, pts, period, tau_yuv;
        boolean_t       b_decode = 0;
#ifdef DEBUG_VPAR
        char            p_date[MSTRTIME_MAX_SIZE];
#endif

        now = mdate();
        period = 1000000 / (p_vpar->sequence.i_frame_rate) * 1001;

        vlc_mutex_lock( &p_vpar->p_vout->change_lock );
        tau_yuv = p_vpar->p_vout->render_time;
        vlc_mutex_unlock( &p_vpar->p_vout->change_lock );

        vlc_mutex_lock( &p_vpar->synchro.fifo_lock );

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
            if( !b_decode )
                intf_WarnMsg( 3, "vpar synchro warning: trashing I" );
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

            if( (1 + S.i_n_p * (S.i_n_b + 1)) * period >
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

            if( (S.i_n_b + 1) * period > S.p_tau[P_CODING_TYPE] )
            {
                b_decode = (pts - now) > (TAU_PRIME(B_CODING_TYPE) + DELTA);
            }
            else
            {
                b_decode = 0;
            }
        }

        vlc_mutex_unlock( &p_vpar->synchro.fifo_lock );
#ifdef DEBUG_VPAR
        intf_DbgMsg("vpar synchro debug: %s picture scheduled for %s, %s (%lld)",
                    i_coding_type == B_CODING_TYPE ? "B" :
                    (i_coding_type == P_CODING_TYPE ? "P" : "I"),
                    mstrtime(p_date, pts), b_decode ? "decoding" : "trashed",
                    S.p_tau[i_coding_type]);
#endif
#ifdef STATS
        if( !b_decode )
        {
            S.i_not_chosen_pic++;
        }
#endif
        return( b_decode );
#undef S
#undef TAU_PRIME
    }
}

/*****************************************************************************
 * vpar_SynchroTrash : Update counters when we trash a picture
 *****************************************************************************/
void vpar_SynchroTrash( vpar_thread_t * p_vpar, int i_coding_type,
                        int i_structure )
{
#ifdef STATS
    p_vpar->synchro.i_trashed_pic++;
#endif
}

/*****************************************************************************
 * vpar_SynchroDecode : Update timers when we decide to decode a picture
 *****************************************************************************/
void vpar_SynchroDecode( vpar_thread_t * p_vpar, int i_coding_type,
                         int i_structure )
{
    vlc_mutex_lock( &p_vpar->synchro.fifo_lock );

    if( ((p_vpar->synchro.i_end + 1 - p_vpar->synchro.i_start)
            % MAX_DECODING_PIC) )
    {
        p_vpar->synchro.p_date_fifo[p_vpar->synchro.i_end] = mdate();
        p_vpar->synchro.pi_coding_types[p_vpar->synchro.i_end] = i_coding_type;

        FIFO_INCREMENT( i_end );
    }
    else
    {
        /* FIFO full, panic() */
        intf_ErrMsg("vpar error: synchro fifo full, estimations will be biased");
    }
    vlc_mutex_unlock( &p_vpar->synchro.fifo_lock );
}

/*****************************************************************************
 * vpar_SynchroEnd : Called when the image is totally decoded
 *****************************************************************************/
void vpar_SynchroEnd( vpar_thread_t * p_vpar, int i_garbage )
{
    mtime_t     tau;
    int         i_coding_type;

    vlc_mutex_lock( &p_vpar->synchro.fifo_lock );

    if (!i_garbage)
    {
        tau = mdate() - p_vpar->synchro.p_date_fifo[p_vpar->synchro.i_start];
        i_coding_type = p_vpar->synchro.pi_coding_types[p_vpar->synchro.i_start];

        /* Mean with average tau, to ensure stability. */
        p_vpar->synchro.p_tau[i_coding_type] =
            (p_vpar->synchro.pi_meaningful[i_coding_type]
             * p_vpar->synchro.p_tau[i_coding_type] + tau)
            / (p_vpar->synchro.pi_meaningful[i_coding_type] + 1);
        if( p_vpar->synchro.pi_meaningful[i_coding_type] < MAX_PIC_AVERAGE )
        {
            p_vpar->synchro.pi_meaningful[i_coding_type]++;
        }
#ifdef DEBUG_VPAR
        intf_DbgMsg("vpar synchro debug: finished decoding %s (%lld)",
                    i_coding_type == B_CODING_TYPE ? "B" :
                    (i_coding_type == P_CODING_TYPE ? "P" : "I"), tau);
#endif
    }

    FIFO_INCREMENT( i_start );

    vlc_mutex_unlock( &p_vpar->synchro.fifo_lock );
}

/*****************************************************************************
 * vpar_SynchroDate : When an image has been decoded, ask for its date
 *****************************************************************************/
mtime_t vpar_SynchroDate( vpar_thread_t * p_vpar )
{
    /* No need to lock, since PTS are only used by the video parser. */
    return( p_vpar->synchro.current_pts );
}

/*****************************************************************************
 * vpar_SynchroNewPicture: Update stream structure and PTS
 *****************************************************************************/
void vpar_SynchroNewPicture( vpar_thread_t * p_vpar, int i_coding_type,
                             int i_repeat_field )
{
    mtime_t         period = 1000000 / (p_vpar->sequence.i_frame_rate) * 1001;

    switch( i_coding_type )
    {
    case I_CODING_TYPE:
        if( p_vpar->synchro.i_eta_p
                && p_vpar->synchro.i_eta_p != p_vpar->synchro.i_n_p )
        {
            intf_WarnMsg( 1, "Stream periodicity changed from P[%d] to P[%d]",
                          p_vpar->synchro.i_n_p, p_vpar->synchro.i_eta_p );
            p_vpar->synchro.i_n_p = p_vpar->synchro.i_eta_p;
        }
        p_vpar->synchro.i_eta_p = p_vpar->synchro.i_eta_b = 0;
#ifdef STATS
        if( p_vpar->synchro.i_type == VPAR_SYNCHRO_DEFAULT )
        {
            intf_Msg( "vpar synchro stats: I(%lld) P(%lld)[%d] B(%lld)[%d] YUV(%lld) : trashed %d:%d/%d",
                  p_vpar->synchro.p_tau[I_CODING_TYPE],
                  p_vpar->synchro.p_tau[P_CODING_TYPE],
                  p_vpar->synchro.i_n_p,
                  p_vpar->synchro.p_tau[B_CODING_TYPE],
                  p_vpar->synchro.i_n_b,
                  p_vpar->p_vout->render_time,
                  p_vpar->synchro.i_not_chosen_pic,
                  p_vpar->synchro.i_trashed_pic -
                  p_vpar->synchro.i_not_chosen_pic,
                  p_vpar->synchro.i_pic );
            p_vpar->synchro.i_trashed_pic = p_vpar->synchro.i_not_chosen_pic
                = p_vpar->synchro.i_pic = 0;
        }
#endif
        break;
    case P_CODING_TYPE:
        p_vpar->synchro.i_eta_p++;
        if( p_vpar->synchro.i_eta_b
                && p_vpar->synchro.i_eta_b != p_vpar->synchro.i_n_b )
        {
            intf_WarnMsg( 1, "Stream periodicity changed from B[%d] to B[%d]",
                          p_vpar->synchro.i_n_b, p_vpar->synchro.i_eta_b );
            p_vpar->synchro.i_n_b = p_vpar->synchro.i_eta_b;
        }
        p_vpar->synchro.i_eta_b = 0;
        break;
    case B_CODING_TYPE:
        p_vpar->synchro.i_eta_b++;
        break;
    }

    p_vpar->synchro.current_pts += p_vpar->synchro.next_period;

    /* A video frame can be displayed 1, 2 or 3 times, according to
     * repeat_first_field, top_field_first, progressive_sequence and
     * progressive_frame. */
    p_vpar->synchro.next_period = i_repeat_field * (period >> 1);

#define PTS_THRESHOLD   (period >> 2)
    if( i_coding_type == B_CODING_TYPE )
    {
        if( p_vpar->sequence.next_pts )
        {
            if( p_vpar->sequence.next_pts - p_vpar->synchro.current_pts
                    > PTS_THRESHOLD
                 || p_vpar->synchro.current_pts - p_vpar->sequence.next_pts
                    > PTS_THRESHOLD )
            {
                intf_WarnMsg( 2,
                        "vpar synchro warning: pts != current_date (%lld)",
                        p_vpar->synchro.current_pts
                            - p_vpar->sequence.next_pts );
            }
            p_vpar->synchro.current_pts = p_vpar->sequence.next_pts;
            p_vpar->sequence.next_pts = 0;
        }
    }
    else
    {
        if( p_vpar->synchro.backward_pts )
        {
            if( p_vpar->sequence.next_dts && 
                (p_vpar->sequence.next_dts - p_vpar->synchro.backward_pts
                    > PTS_THRESHOLD
              || p_vpar->synchro.backward_pts - p_vpar->sequence.next_dts
                    > PTS_THRESHOLD) )
            {
                intf_WarnMsg( 2,
                        "vpar synchro warning: backward_pts != dts (%lld)",
                        p_vpar->synchro.backward_pts
                            - p_vpar->sequence.next_dts );
            }

            if( p_vpar->synchro.backward_pts - p_vpar->synchro.current_pts
                    > PTS_THRESHOLD
                 || p_vpar->synchro.current_pts - p_vpar->synchro.backward_pts
                    > PTS_THRESHOLD )
            {
                intf_WarnMsg( 2,
                   "vpar synchro warning: backward_pts != current_pts (%lld)",
                   p_vpar->synchro.current_pts - p_vpar->synchro.backward_pts );
            }
            p_vpar->synchro.current_pts = p_vpar->synchro.backward_pts;
            p_vpar->synchro.backward_pts = 0;
        }
        else if( p_vpar->sequence.next_dts )
        {
            if( p_vpar->sequence.next_dts - p_vpar->synchro.current_pts
                    > PTS_THRESHOLD
                 || p_vpar->synchro.current_pts - p_vpar->sequence.next_dts
                    > PTS_THRESHOLD )
            {
                intf_WarnMsg( 2,
                        "vpar synchro warning: dts != current_pts (%lld)",
                        p_vpar->synchro.current_pts
                            - p_vpar->sequence.next_dts );
            }
            /* By definition of a DTS. */
            p_vpar->synchro.current_pts = p_vpar->sequence.next_dts;
            p_vpar->sequence.next_dts = 0;
        }

        if( p_vpar->sequence.next_pts )
        {
            /* Store the PTS for the next time we have to date an I picture. */
            p_vpar->synchro.backward_pts = p_vpar->sequence.next_pts;
            p_vpar->sequence.next_pts = 0;
        }
    }
#undef PTS_THRESHOLD

#ifdef STATS
    p_vpar->synchro.i_pic++;
#endif
}

/*****************************************************************************
 * SynchroType: Get the user's synchro type
 *****************************************************************************
 * This function is called at initialization.
 *****************************************************************************/
static int SynchroType( void )
{
    char * psz_synchro = main_GetPszVariable( VPAR_SYNCHRO_VAR, NULL );

    if( psz_synchro == NULL )
    {
        return VPAR_SYNCHRO_DEFAULT;
    }

    switch( *psz_synchro++ )
    {
      case 'i':
      case 'I':
        switch( *psz_synchro++ )
        {
          case '\0':
            return VPAR_SYNCHRO_I;

          case '+':
            if( *psz_synchro ) return 0;
            return VPAR_SYNCHRO_Iplus;

          case 'p':
          case 'P':
            switch( *psz_synchro++ )
            {
              case '\0':
                return VPAR_SYNCHRO_IP;

              case '+':
                if( *psz_synchro ) return 0;
                return VPAR_SYNCHRO_IPplus;

              case 'b':
              case 'B':
                if( *psz_synchro ) return 0;
                return VPAR_SYNCHRO_IPB;

              default:
                return VPAR_SYNCHRO_DEFAULT;
                
            }

          default:
            return VPAR_SYNCHRO_DEFAULT;
        }
    }

    return VPAR_SYNCHRO_DEFAULT;
}

