/*****************************************************************************
 * vout_synchro.h: frame-dropping structures
 *****************************************************************************
 * Copyright (C) 1999-2005 VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Marc Dressler <polux@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
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

/*****************************************************************************
 * vout_synchro_t : timers for the video synchro
 *****************************************************************************/
#define MAX_PIC_AVERAGE         8

/* Read the discussion on top of vout_synchro.c for more information. */
struct vout_synchro_t
{
    VLC_COMMON_MEMBERS

    vout_thread_t * p_vout;
    int             i_frame_rate;
    int             i_current_rate;
    vlc_bool_t      b_no_skip;
    vlc_bool_t      b_quiet;

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

/* Pictures types */
#define I_CODING_TYPE           1
#define P_CODING_TYPE           2
#define B_CODING_TYPE           3
#define D_CODING_TYPE           4 /* MPEG-1 ONLY */
/* other values are reserved */

/* Structures */
#define TOP_FIELD               1
#define BOTTOM_FIELD            2
#define FRAME_STRUCTURE         3

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define vout_SynchroInit(a,b) __vout_SynchroInit(VLC_OBJECT(a),b)
VLC_EXPORT( vout_synchro_t *, __vout_SynchroInit, ( vlc_object_t *, int ) );
VLC_EXPORT( void, vout_SynchroRelease,        ( vout_synchro_t * ) );
VLC_EXPORT( void, vout_SynchroReset,          ( vout_synchro_t * ) );
VLC_EXPORT( vlc_bool_t, vout_SynchroChoose,   ( vout_synchro_t *, int, int, vlc_bool_t ) );
VLC_EXPORT( void, vout_SynchroTrash,          ( vout_synchro_t * ) );
VLC_EXPORT( void, vout_SynchroDecode,         ( vout_synchro_t * ) );
VLC_EXPORT( void, vout_SynchroEnd,            ( vout_synchro_t *, int, vlc_bool_t ) );
VLC_EXPORT( mtime_t, vout_SynchroDate,        ( vout_synchro_t * ) );
VLC_EXPORT( void, vout_SynchroNewPicture,     ( vout_synchro_t *, int, int, mtime_t, mtime_t, int, vlc_bool_t ) );

