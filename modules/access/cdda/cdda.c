/*****************************************************************************
 * cddax.c : CD digital audio input module for vlc using libcdio
 *****************************************************************************
 * Copyright (C) 2000,2003 VideoLAN
 * $Id: cdda.c,v 1.2 2003/11/26 03:35:26 rocky Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Rocky Bernstein <rocky@panix.com> 
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

#include <vlc/vlc.h>


/*****************************************************************************
 * prototypes
 *****************************************************************************/
int  E_(Open)         ( vlc_object_t * );
void E_(Close)        ( vlc_object_t * );

int  E_(OpenIntf)     ( vlc_object_t * );
void E_(CloseIntf)    ( vlc_object_t * );
int  E_(DemuxOpen)    ( vlc_object_t * p_this);
void E_(DemuxClose)   ( vlc_object_t * p_this);

int  E_(DebugCallback)       ( vlc_object_t *p_this, const char *psz_name,
			       vlc_value_t oldval, vlc_value_t val, 
			       void *p_data );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/*****************************************************************************
 * Option help text
 *****************************************************************************/

#define DEBUG_TEXT N_("set debug mask for additional debugging.")
#define DEBUG_LONGTEXT N_( \
    "This integer when viewed in binary is a debugging mask\n" \
    "MRL             1\n" \
    "events          2\n" \
    "external call   4\n" \
    "all calls       8\n" \
    "LSN      (10)  16\n" \
    "libcdio  (20)  32\n" \
    "seeks    (40)  64\n" )

#define DEV_TEXT N_("CD-ROM device name")
#define DEV_LONGTEXT N_( \
    "Specify the name of the CD-ROM device that will be used by default. " \
    "If you don't specify anything, we'll scan for a suitable CD-ROM device.")

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for cdda streams. This " \
    "value should be set in millisecond units." )

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    add_usage_hint( N_("cddax://[device-or-file][@num]") );
    set_description( _("Compact Disc Digital Audio (CD-DA) input") );
    set_capability( "access", 75 /* slightly higher than cdda */ );
    set_callbacks( E_(Open), E_(Close) );
    add_shortcut( "cdda" );
    add_shortcut( "cddax" );

    /* Configuration options */
    add_category_hint( N_("CDX"), NULL, VLC_TRUE );

    add_integer ( MODULE_STRING "-debug", 0, E_(DebugCallback), DEBUG_TEXT, 
                  DEBUG_LONGTEXT, VLC_TRUE );

    add_integer( MODULE_STRING "-caching", 
		 DEFAULT_PTS_DELAY / 1000, NULL, 
		 CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );

    add_string( MODULE_STRING "-device", "", NULL, DEV_TEXT, 
                DEV_LONGTEXT, VLC_TRUE );

    add_submodule();
        set_description( _("CD Audio demux") );
        set_capability( "demux", 0 );
        set_callbacks( E_(DemuxOpen), E_(DemuxClose) );
        add_shortcut( "cdda" );

    add_submodule();
        set_capability( "interface", 0 );
        set_callbacks( E_(OpenIntf), E_(CloseIntf) );

vlc_module_end();
