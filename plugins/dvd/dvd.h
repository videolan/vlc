/*****************************************************************************
 * dvd.h: thread structure of the DVD plugin
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd.h,v 1.2 2002/03/08 22:58:12 stef Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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
 * Preamble
 *****************************************************************************/
/* Logical block size for DVD-VIDEO */
#define DVD_LB_SIZE 2048
#define LB2OFF(x) ((off_t)(x) * (off_t)(DVD_LB_SIZE))
#define OFF2LB(x) ((x) >> 11)

/*****************************************************************************
 * thread_dvd_data_t: extension of input_thread_t for DVD specificity.
 *****************************************************************************/
typedef struct thread_dvd_data_s
{
    dvdcss_handle           dvdhandle;                   /* libdvdcss handle */

    int                     i_audio_nb;
    int                     i_spu_nb;

    /* Navigation information */
    int                     i_title;
    int                     i_title_id;

    int                     i_chapter_nb;
    int                     i_chapter;
    boolean_t               b_new_chapter;

    int                     i_angle_nb;
    int                     i_angle;

    int                     i_map_cell;   /* cell index in adress map */
    int                     i_prg_cell;   /* cell index in program map */
    int                     i_angle_cell; /* cell index in the current angle */

    int                     i_vts_start;  /* offset to beginning of vts */
    int                     i_vts_lb;     /* sector in vts */
    int                     i_end_lb;     /* last sector of current cell */

    int                     i_size;

    /* Structure that contains all information of the DVD */
    struct ifo_s *          p_ifo;

} thread_dvd_data_t;

