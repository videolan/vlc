/*****************************************************************************
 * vpar_synchro.h : video parser blocks management
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: vpar_synchro.h,v 1.1 2000/12/21 17:19:52 massiot Exp $
 *
 * Author: Christophe Massiot <massiot@via.ecp.fr>
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
 * Requires:
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "threads.h"
 *  "input.h"
 *  "video.h"
 *  "video_output.h"
 *  "decoder_fifo.h"
 *  "video_fifo.h"
 *****************************************************************************/

/*****************************************************************************
 * video_synchro_t and video_synchro_tab_s : timers for the video synchro
 *****************************************************************************/
#define MAX_DECODING_PIC        16
#define MAX_PIC_AVERAGE         8

/* Read the discussion on top of vpar_synchro.c for more information. */
typedef struct video_synchro_s
{
    /* synchro algorithm */
    int             i_type;

    /* fifo containing decoding dates */
    mtime_t         p_date_fifo[MAX_DECODING_PIC];
    int             pi_coding_types[MAX_DECODING_PIC];
    unsigned int    i_start, i_end;
    vlc_mutex_t     fifo_lock;

    /* stream properties */
    unsigned int    i_n_p, i_n_b;

    /* decoding values */
    mtime_t         p_tau[4];                  /* average decoding durations */
    unsigned int    pi_meaningful[4];            /* number of durations read */
    /* and p_vout->render_time (read with p_vout->change_lock) */

    /* stream context */
    unsigned int    i_eta_p, i_eta_b;
    boolean_t       b_dropped_last;            /* for special synchros below */
    mtime_t         backward_pts, current_pts;

#ifdef STATS
    unsigned int    i_trashed_pic;
#endif
} video_synchro_t;

#define FIFO_INCREMENT( i_counter )                                         \
    p_vpar->synchro.i_counter =                                             \
        (p_vpar->synchro.i_counter + 1) % MAX_DECODING_PIC;

/* Synchro algorithms */
#define VPAR_SYNCHRO_DEFAULT    0
#define VPAR_SYNCHRO_I          1
#define VPAR_SYNCHRO_Iplus      2
#define VPAR_SYNCHRO_IP         3
#define VPAR_SYNCHRO_IPplus     4
#define VPAR_SYNCHRO_IPB        5

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void vpar_SynchroInit           ( struct vpar_thread_s * p_vpar );
boolean_t vpar_SynchroChoose    ( struct vpar_thread_s * p_vpar,
                                  int i_coding_type, int i_structure );
void vpar_SynchroTrash          ( struct vpar_thread_s * p_vpar,
                                  int i_coding_type, int i_structure );
void vpar_SynchroDecode         ( struct vpar_thread_s * p_vpar,
                                  int i_coding_type, int i_structure );
void vpar_SynchroEnd            ( struct vpar_thread_s * p_vpar, int i_garbage );
mtime_t vpar_SynchroDate        ( struct vpar_thread_s * p_vpar );
