/*****************************************************************************
 * input_dvd.h: thread structure of the DVD plugin
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
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
    int                     i_read_once;        // NB of bytes read by DVDRead

    int                     i_chapter_nb;
    off_t                   i_start;
    off_t                   i_size;

    /* Scrambling Information */
    struct css_s            css;

    /* Structure that contains all information of the DVD */
    struct ifo_s            ifo;

} thread_dvd_data_t;

/*****************************************************************************
 * Prototypes in dvd_ifo.c
 *****************************************************************************/
struct ifo_s    IfoInit( int );
int             IfoReadVTS( struct ifo_s * );
void            IfoRead( struct ifo_s * );
void            IfoEnd( ifo_t * );

/*****************************************************************************
 * Prototypes in dvd_css.c
 *****************************************************************************/
int             CSSTest     ( int );
struct css_s    CSSInit     ( int );
int             CSSGetKey   ( struct css_s * );
int             CSSDescrambleSector( u8 * , u8 * );

