/*****************************************************************************
 * dvdread.c : DvdRead input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: dvdread.c,v 1.6 2003/03/30 18:14:35 gbazin Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <vlc/vlc.h>

/*****************************************************************************
 * External prototypes
 *****************************************************************************/
int  E_(OpenDVD)   ( vlc_object_t * );
void E_(CloseDVD)  ( vlc_object_t * );

int  E_(InitDVD)   ( vlc_object_t * );
void E_(EndDVD)    ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("DVD input (using libdvdread)") );
    add_submodule();
        set_capability( "access", 110 );
        set_callbacks( E_(OpenDVD), E_(CloseDVD) );
    add_submodule();
        set_capability( "demux", 0 );
        set_callbacks( E_(InitDVD), E_(EndDVD) );
vlc_module_end();

