/* dvd_seek.c: functions to navigate through DVD.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_seek.c,v 1.1 2002/03/06 01:20:56 stef Exp $
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#ifdef GOD_DAMN_DMCA
#   include "dummy_dvdcss.h"
#else
#   include <videolan/dvdcss.h>
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "dvd.h"
#include "dvd_seek.h"
#include "dvd_ifo.h"

#include "debug.h"

#define title \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_title_id-1].title
#define cell  p_dvd->p_ifo->vts.cell_inf

/*****************************************************************************
 * DVDFindCell: adjust the title cell index with the program cell
 *****************************************************************************/
int DVDFindCell( thread_dvd_data_t * p_dvd )
{
    int                 i_cell;
    int                 i_index;

    i_cell = p_dvd->i_cell;
    i_index = p_dvd->i_prg_cell;

    if( i_cell >= cell.i_cell_nb )
    {
        return -1;
    }

    while( ( ( title.p_cell_pos[i_index].i_vob_id !=
                   cell.p_cell_map[i_cell].i_vob_id ) ||
      ( title.p_cell_pos[i_index].i_cell_id !=
                   cell.p_cell_map[i_cell].i_cell_id ) ) &&
           ( i_cell < cell.i_cell_nb - 1 ) )
    {
        i_cell++;
    }

/*
intf_WarnMsg( 12, "FindCell: i_cell %d i_index %d found %d nb %d",
                    p_dvd->i_cell,
                    p_dvd->i_prg_cell,
                    i_cell,
                    cell.i_cell_nb );
*/

    p_dvd->i_cell = i_cell;

    return 0;    
}

#undef cell

/*****************************************************************************
 * DVDFindSector: find cell index in adress map from index in
 * information table program map and give corresponding sectors.
 *****************************************************************************/
int DVDFindSector( thread_dvd_data_t * p_dvd )
{

    if( p_dvd->i_sector > title.p_cell_play[p_dvd->i_prg_cell].i_end_sector )
    {
        p_dvd->i_prg_cell++;

        if( DVDChooseAngle( p_dvd ) < 0 )
        {
            return -1;
        }
    }

    if( DVDFindCell( p_dvd ) < 0 )
    {
        intf_ErrMsg( "dvd error: can't find sector" );
        return -1;
    }
    
    /* Find start and end sectors of new cell */
#if 1
    p_dvd->i_sector = __MAX(
         p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_start_sector,
         title.p_cell_play[p_dvd->i_prg_cell].i_start_sector );
    p_dvd->i_end_sector = __MIN(
         p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_end_sector,
         title.p_cell_play[p_dvd->i_prg_cell].i_end_sector );
#else
    p_dvd->i_sector = title.p_cell_play[p_dvd->i_prg_cell].i_start_sector;
    p_dvd->i_end_sector = title.p_cell_play[p_dvd->i_prg_cell].i_end_sector;
#endif

/*
    intf_WarnMsg( 12, "cell: %d sector1: 0x%x end1: 0x%x\n"
                   "index: %d sector2: 0x%x end2: 0x%x\n"
                   "category: 0x%x ilvu end: 0x%x vobu start 0x%x", 
        p_dvd->i_cell,
        p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_start_sector,
        p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_end_sector,
        p_dvd->i_prg_cell,
        title.p_cell_play[p_dvd->i_prg_cell].i_start_sector,
        title.p_cell_play[p_dvd->i_prg_cell].i_end_sector,
        title.p_cell_play[p_dvd->i_prg_cell].i_category, 
        title.p_cell_play[p_dvd->i_prg_cell].i_first_ilvu_vobu_esector,
        title.p_cell_play[p_dvd->i_prg_cell].i_last_vobu_start_sector );
*/

    return 0;
}

/*****************************************************************************
 * DVDChapterSelect: find the cell corresponding to requested chapter
 *****************************************************************************/
int DVDChapterSelect( thread_dvd_data_t * p_dvd, int i_chapter )
{

    /* Find cell index in Program chain for current chapter */
    p_dvd->i_prg_cell = title.chapter_map.pi_start_cell[i_chapter-1] - 1;
    p_dvd->i_cell = 0;
    p_dvd->i_sector = 0;

    DVDChooseAngle( p_dvd );

    /* Search for cell_index in cell adress_table and initialize
     * start sector */
    if( DVDFindSector( p_dvd ) < 0 )
    {
        intf_ErrMsg( "dvd error: can't select chapter" );
        return -1;
    }

    /* start is : beginning of vts vobs + offset to vob x */
    p_dvd->i_start = p_dvd->i_title_start + p_dvd->i_sector;

    /* Position the fd pointer on the right address */
    if( ( p_dvd->i_start = dvdcss_seek( p_dvd->dvdhandle,
                                        p_dvd->i_start,
                                        DVDCSS_SEEK_MPEG ) ) < 0 )
    {
        intf_ErrMsg( "dvd error: %s", dvdcss_error( p_dvd->dvdhandle ) );
        return -1;
    }

    p_dvd->i_chapter = i_chapter;
    return 0;
}

/*****************************************************************************
 * DVDChooseAngle: select the cell corresponding to the selected angle
 *****************************************************************************/
int DVDChooseAngle( thread_dvd_data_t * p_dvd )
{
    /* basic handling of angles */
    switch( ( ( title.p_cell_play[p_dvd->i_prg_cell].i_category & 0xf000 )
                    >> 12 ) )
    {
        /* we enter a muli-angle section */
        case 0x5:
            p_dvd->i_prg_cell += p_dvd->i_angle - 1;
            p_dvd->i_angle_cell = 0;
            break;
        /* we exit a multi-angle section */
        case 0x9:
        case 0xd:
            p_dvd->i_prg_cell += p_dvd->i_angle_nb - p_dvd->i_angle;
            break;
    }

    return 0;
}

#undef title
