/*****************************************************************************
 * vpar_synchro.h : video parser blocks management
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Jean-Marc Dressler <polux@via.ecp.fr>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#define SAM_SYNCHRO
//#define POLUX_SYNCHRO
//#define MEUUH_SYNCHRO

/*****************************************************************************
 * video_synchro_t and video_synchro_tab_s : timers for the video synchro
 *****************************************************************************/
#ifdef SAM_SYNCHRO
typedef struct video_synchro_s
{
    /* synchro algorithm */
    int          i_type;

    /* fifo containing decoding dates */
    mtime_t      i_date_fifo[16];
    unsigned int i_start;
    unsigned int i_stop;

    /* mean decoding time */
    mtime_t i_delay;
    mtime_t i_theorical_delay;

    /* dates */
    mtime_t i_last_pts;                   /* pts of the last displayed image */
    mtime_t i_last_seen_I_pts;              /* date of the last I we decoded */
    mtime_t i_last_kept_I_pts;            /* pts of last non-dropped I image */

    /* P images since the last I */
    unsigned int i_P_seen;
    unsigned int i_P_kept;
    /* B images since the last I */
    unsigned int i_B_seen;
    unsigned int i_B_kept;

    /* can we display pictures ? */
    boolean_t     b_all_I;
    boolean_t     b_all_P;
    int           displayable_p;
    boolean_t     b_all_B;
    int           displayable_b;
    boolean_t     b_dropped_last_B;

} video_synchro_t;

#define FIFO_INCREMENT( i_counter ) \
    p_vpar->synchro.i_counter = (p_vpar->synchro.i_counter + 1) & 0xf;

#define VPAR_SYNCHRO_DEFAULT   0
#define VPAR_SYNCHRO_I         1
#define VPAR_SYNCHRO_IP        2
#define VPAR_SYNCHRO_IPplus    3
#define VPAR_SYNCHRO_IPB       4

#endif

#ifdef MEUUH_SYNCHRO
typedef struct video_synchro_s
{
    int         kludge_level, kludge_p, kludge_b, kludge_nbp, kludge_nbb;
    int         kludge_nbframes;
    mtime_t     kludge_date, kludge_prevdate;
    int         i_coding_type;
} video_synchro_t;

#define SYNC_TOLERATE   ((int)(0.010*CLOCK_FREQ))                   /* 10 ms */
#define SYNC_DELAY      ((int)(0.500*CLOCK_FREQ))                  /* 500 ms */
#endif

#ifdef POLUX_SYNCHRO

#define SYNC_AVERAGE_COUNT 10

typedef struct video_synchro_s
{
    /* Date Section */

    /* Dates needed to compute the date of the current frame
     * We also use the stream frame rate (sequence.i_frame_rate) */
    mtime_t     i_current_frame_date;
    mtime_t     i_backward_frame_date;

    /* Frame Trashing Section */

    int         i_b_nb, i_p_nb;   /* number of decoded P and B between two I */
    float       r_b_average, r_p_average;
    int         i_b_count, i_p_count, i_i_count;
    int         i_b_trasher;                /* used for brensenham algorithm */

} video_synchro_t;

#endif

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
boolean_t vpar_SynchroChoose    ( struct vpar_thread_s * p_vpar,
                                  int i_coding_type, int i_structure );
void vpar_SynchroTrash          ( struct vpar_thread_s * p_vpar,
                                  int i_coding_type, int i_structure );
void vpar_SynchroDecode         ( struct vpar_thread_s * p_vpar,
                                  int i_coding_type, int i_structure );
void vpar_SynchroEnd            ( struct vpar_thread_s * p_vpar );
mtime_t vpar_SynchroDate        ( struct vpar_thread_s * p_vpar );

#ifndef SAM_SYNCHRO
void vpar_SynchroKludge         ( struct vpar_thread_s *, mtime_t );
#endif
