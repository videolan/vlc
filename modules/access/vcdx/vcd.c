/*****************************************************************************
 * vcd.c : VCD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000,2003 VideoLAN
 * $Id: vcd.c,v 1.11 2003/12/11 12:56:25 rocky Exp $
 *
 * Authors: Rocky Bernstein <rocky@panix.com> 
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
 * top-level module code - handles options, shortcuts, loads sub-modules.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
int  E_(Open)         ( vlc_object_t * );
void E_(Close)        ( vlc_object_t * );
int  E_(OpenIntf)     ( vlc_object_t * );
void E_(CloseIntf)    ( vlc_object_t * );
int  E_(InitVCD)      ( vlc_object_t * );
void E_(EndVCD)       ( vlc_object_t * );

int  E_(DebugCallback) ( vlc_object_t *p_this, const char *psz_name,
			 vlc_value_t oldval, vlc_value_t val, 
			 void *p_data );

/*****************************************************************************
 * Option help text
 *****************************************************************************/

#define DEBUG_LONGTEXT N_( \
    "This integer when viewed in binary is a debugging mask\n" \
    "meta info         1\n" \
    "event info        2\n" \
    "MRL               4\n" \
    "external call     8\n" \
    "all calls (10)   16\n" \
    "LSN       (20)   32\n" \
    "PBC       (40)   64\n" \
    "libcdio   (80)  128\n" \
    "seek-set (100)  256\n" \
    "seek-cur (200)  512\n" \
    "still    (400) 1024\n" \
    "vcdinfo  (800) 2048\n" )

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    add_usage_hint( N_("vcdx://[device-or-file][@{P,S,T}num]") );
    set_description( _("Video CD (VCD 1.0, 1.1, 2.0, SVCD, HQVCD) input") );
    set_capability( "access", 85 /* slightly higher than vcd */ );
    set_callbacks( E_(Open), E_(Close) );
    add_shortcut( "vcd" );
    add_shortcut( "vcdx" );

    /* Configuration options */
    add_category_hint( N_("VCDX"), NULL, VLC_TRUE );

    add_integer ( MODULE_STRING "-debug", 0, E_(DebugCallback), 
		  N_("set debug mask for additional debugging."),
                  DEBUG_LONGTEXT, VLC_TRUE );

    add_bool( MODULE_STRING "-PBC", 0, NULL,
	      N_("Use playback control?"),
	      N_("If VCD is authored with playback control, use it. "
		 "Otherwise we play by tracks."), 
	      VLC_TRUE );

#ifdef FIXED
    add_submodule();
        set_capability( "demux", 0 );
        set_callbacks( E_(InitVCD), E_(EndVCD) );
#endif

    add_submodule();
        set_capability( "interface", 0 );
        set_callbacks( E_(OpenIntf), E_(CloseIntf) );
vlc_module_end();

