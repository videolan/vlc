/*****************************************************************************
 * input_dvd.h: thread structure of the DVD plugin
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: input_dvd.h,v 1.17 2001/04/10 17:47:05 stef Exp $
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

/*****************************************************************************
 * thread_dvd_data_t: extension of input_thread_t for DVD specificity.
 *****************************************************************************/
typedef struct thread_dvd_data_s
{
    int                     i_fd;               // File descriptor of device
    boolean_t               b_encrypted;        // CSS encryption

    int                     i_block_once;       // Nb of block read once by 
                                                // readv

    /* Navigation information */
    int                     i_title;
    int                     i_vts_title;
    int                     i_program_chain;

    int                     i_chapter_nb;
    int                     i_chapter;

    int                     i_cell;         /* cell index in adress map */
    int                     i_prg_cell;     /* cell index in program map */

    int                     i_sector;
    int                     i_end_sector;   /* last sector of current cell */

    off_t                   i_title_start;
    off_t                   i_start;
    off_t                   i_size;

    /* Scrambling Information */
    struct css_s *          p_css;

    /* Structure that contains all information of the DVD */
    struct ifo_s *          p_ifo;

} thread_dvd_data_t;

/*****************************************************************************
 * Prototypes in dvd_css.c
 *****************************************************************************/
int   CSSTest             ( int );
int   CSSInit             ( struct css_s * );
int   CSSGetKey           ( struct css_s * );
int   CSSDescrambleSector ( u8 * , u8 * );

/*****************************************************************************
 * Prototypes in dvd_ifo.c
 *****************************************************************************/
int   IfoCreate   ( struct thread_dvd_data_s * );
int   IfoInit     ( struct ifo_s * );
int   IfoTitleSet ( struct ifo_s * );
void  IfoEnd      ( struct ifo_s * );


