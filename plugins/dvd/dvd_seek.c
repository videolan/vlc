/* dvd_seek.c: functions to navigate through DVD.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_seek.c,v 1.7 2002/05/20 22:45:03 sam Exp $
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
#   include <dvdcss/dvdcss.h>
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "dvd.h"
#include "dvd_seek.h"
#include "dvd_ifo.h"

#define title \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_title_id-1].title
#define cell  p_dvd->p_ifo->vts.cell_inf

int CellIsInterleaved( thread_dvd_data_t * p_dvd )
{
    return title.p_cell_play[p_dvd->i_prg_cell].i_category & 0xf000;
}

int CellPrg2Map( thread_dvd_data_t * p_dvd )
{
    int     i_cell;

    i_cell = 0;

    if( i_cell >= cell.i_cell_nb )
    {
        return -1;
    }

    while( ( i_cell < cell.i_cell_nb ) &&
           ( ( title.p_cell_pos[p_dvd->i_prg_cell].i_vob_id !=
               cell.p_cell_map[i_cell].i_vob_id ) ||
             ( title.p_cell_pos[p_dvd->i_prg_cell].i_cell_id !=
               cell.p_cell_map[i_cell].i_cell_id ) ) )
    {
        i_cell++;
    }
    
    if( i_cell >= cell.i_cell_nb )
    {
        return -1;
    }

    return i_cell;    
}

int CellAngleOffset( thread_dvd_data_t * p_dvd, int i_prg_cell )
{
    int     i_cell_off;
    
    if( i_prg_cell >= title.i_cell_nb )
    {
        return 0;
    }
    
    /* basic handling of angles */
    switch( ( ( title.p_cell_play[i_prg_cell].i_category & 0xf000 )
                    >> 12 ) )
    {
        /* we enter a muli-angle section */
        case 0x5:
            i_cell_off = p_dvd->i_angle - 1;
            p_dvd->i_angle_cell = 0;
            break;
        /* we exit a multi-angle section */
        case 0x9:
        case 0xd:
            i_cell_off = p_dvd->i_angle_nb - p_dvd->i_angle;
            break;
        default:
            i_cell_off = 0;
    }

    return i_cell_off;
}

int CellFirstSector( thread_dvd_data_t * p_dvd )
{
    return __MAX( cell.p_cell_map[p_dvd->i_map_cell].i_first_sector,
                  title.p_cell_play[p_dvd->i_prg_cell].i_first_sector );
}
    
int CellLastSector( thread_dvd_data_t * p_dvd )
{
    return __MIN( cell.p_cell_map[p_dvd->i_map_cell].i_last_sector,
                  title.p_cell_play[p_dvd->i_prg_cell].i_last_sector );
}

int NextCellPrg( thread_dvd_data_t * p_dvd )
{
    int     i_cell = p_dvd->i_prg_cell;
    
    if( p_dvd->i_vts_lb > title.p_cell_play[i_cell].i_last_sector )
    {
        i_cell ++;
        i_cell += CellAngleOffset( p_dvd, i_cell );

        if( i_cell >= title.i_cell_nb )
        {
            return -1;
        }
    }
    
    return i_cell;
}

int Lb2CellPrg( thread_dvd_data_t * p_dvd )
{
    int     i_cell = 0;
    
    while( p_dvd->i_vts_lb > title.p_cell_play[i_cell].i_last_sector )
    {
        i_cell ++;
        i_cell += CellAngleOffset( p_dvd, i_cell );

        if( i_cell >= title.i_cell_nb )
        {
            return -1;
        }
    }
    
    return i_cell;
}

int Lb2CellMap( thread_dvd_data_t * p_dvd )
{
    int     i_cell = 0;
    
    while( p_dvd->i_vts_lb > cell.p_cell_map[i_cell].i_last_sector )
    {
        i_cell ++;

        if( i_cell >= cell.i_cell_nb )
        {
            return -1;
        }
    }
    
    return i_cell;
}

int LbMaxOnce( thread_dvd_data_t * p_dvd )
{
    int i_block_once = p_dvd->i_last_lb - p_dvd->i_vts_lb + 1;

    /* Get the position of the next cell if we're at cell end */
    if( i_block_once <= 0 )
    {
        p_dvd->i_map_cell++;
        p_dvd->i_angle_cell++;

        if( ( p_dvd->i_prg_cell = NextCellPrg( p_dvd ) ) < 0 )
        {
            /* EOF */
            return 0;
        }

        if( ( p_dvd->i_map_cell = CellPrg2Map( p_dvd ) ) < 0 )
        {
            return 0;
        }

        p_dvd->i_vts_lb   = CellFirstSector( p_dvd );
        p_dvd->i_last_lb  = CellLastSector( p_dvd );
        
        if( ( p_dvd->i_chapter = NextChapter( p_dvd ) ) < 0)
        {
            return 0;
        }

        /* Position the fd pointer on the right address */
        if( ( dvdcss_seek( p_dvd->dvdhandle,
                           p_dvd->i_vts_start + p_dvd->i_vts_lb,
                           DVDCSS_SEEK_MPEG ) ) < 0 )
        {
            intf_ErrMsg( "dvd error: %s",
                         dvdcss_error( p_dvd->dvdhandle ) );
            return 0;
        }

        i_block_once = p_dvd->i_last_lb - p_dvd->i_vts_lb + 1;
    }

    return i_block_once;
}


int CellPrg2Chapter( thread_dvd_data_t * p_dvd )
{
    int     i_chapter = 1;
    int     i_cell    = p_dvd->i_prg_cell;
    
    if( CellIsInterleaved( p_dvd ) )
    {
        i_cell -= (p_dvd->i_angle - 1);
    }
    
    while( title.chapter_map.pi_start_cell[i_chapter] <= i_cell+1 )
    {
        i_chapter ++;
        if( i_chapter >= p_dvd->i_chapter_nb )
        {
            return p_dvd->i_chapter_nb;
        }
    }

    return i_chapter;
}

int NextChapter( thread_dvd_data_t * p_dvd )
{
    int i_cell = p_dvd->i_prg_cell;
    
    if( CellIsInterleaved( p_dvd ) )
    {
        i_cell -= (p_dvd->i_angle - 1);
    }
    
    if( title.chapter_map.pi_start_cell[p_dvd->i_chapter] <= i_cell+1 )
    {
        p_dvd->i_chapter++;
        if( p_dvd->i_chapter > p_dvd->i_chapter_nb )
        {
            return -1;
        }
        p_dvd->b_new_chapter = 1;

        return p_dvd->i_chapter;
    }

    return p_dvd->i_chapter;
}



int DVDSetChapter( thread_dvd_data_t * p_dvd, int i_chapter )
{
    if( i_chapter <= 0 || i_chapter > p_dvd->i_chapter_nb )
    {
        i_chapter = 1;
    }
    
    if( p_dvd->i_chapter != i_chapter )
    {
        /* Find cell index in Program chain for current chapter */
        p_dvd->i_prg_cell = title.chapter_map.pi_start_cell[i_chapter-1] - 1;
        p_dvd->i_prg_cell += CellAngleOffset( p_dvd, p_dvd->i_prg_cell );
        if( i_chapter < p_dvd->i_chapter )
        {
            p_dvd->i_map_cell = 0;
        }
        p_dvd->i_map_cell = CellPrg2Map( p_dvd );
        p_dvd->i_vts_lb   = CellFirstSector( p_dvd );
        p_dvd->i_last_lb  = CellLastSector( p_dvd );

        /* Position the fd pointer on the right address */
        if( dvdcss_seek( p_dvd->dvdhandle,
                         p_dvd->i_vts_start + p_dvd->i_vts_lb,
                         DVDCSS_SEEK_MPEG ) < 0 )
        {
            intf_ErrMsg( "dvd error: %s", dvdcss_error( p_dvd->dvdhandle ) );
            return -1;
        }
        
        intf_WarnMsg( 4, "dvd info: chapter %d prg_cell %d map_cell %d",
                i_chapter, p_dvd->i_prg_cell, p_dvd->i_map_cell );
    }
    
    return i_chapter;
}


#undef cell
#undef title
