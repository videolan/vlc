/*****************************************************************************
 * input_dvdread.h: thread structure of the DVD plugin
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: input_dvdread.h,v 1.6 2002/03/04 01:53:56 stef Exp $
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

/* dvdread includes */
#include "dvd_reader.h"
#include "ifo_types.h"
#include "ifo_read.h"
#include "dvd_udf.h"
#include "nav_read.h"
#include "nav_print.h"

/* Logical block size for DVD-VIDEO */
#define LB2OFF(x) ((off_t)(x) * (off_t)(DVD_VIDEO_LB_LEN))
#define OFF2LB(x) ((x) >> 11)

/*****************************************************************************
 * thread_dvd_data_t: extension of input_thread_t for DVD specificity.
 *****************************************************************************/
typedef struct thread_dvd_data_s
{
    dvd_reader_t *          p_dvdread;
    dvd_file_t *            p_title;

    ifo_handle_t *          p_vmg_file;
    ifo_handle_t *          p_vts_file;
            
    int                     i_title;
    int                     i_chapter;
    int                     i_angle;
    int                     i_angle_nb;

    tt_srpt_t *             p_tt_srpt;
    pgc_t *                 p_cur_pgc;

    dsi_t                   dsi_pack;

    int                     i_ttn;
    
    unsigned int            i_pack_len;
    unsigned int            i_cur_block;
    unsigned int            i_next_vobu;
    unsigned int            i_end_block;

    int                     i_cur_cell;
    int                     i_next_cell;
    boolean_t               b_eoc;
} thread_dvd_data_t;

