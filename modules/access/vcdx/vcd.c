/*****************************************************************************
 * vcd.c : VCD input module for vlc
 *****************************************************************************
 * Copyright (C) 2000, 2003, 2004, 2005 VideoLAN
 * $Id$
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
int  VCDOpen       ( vlc_object_t * );
void VCDClose      ( vlc_object_t * );
int  VCDOpenIntf   ( vlc_object_t * );
void VCDCloseIntf  ( vlc_object_t * );
int  E_(VCDInit)   ( vlc_object_t * );
void E_(VCDEnd)    ( vlc_object_t * );

int  E_(DebugCallback) ( vlc_object_t *p_this, const char *psz_name,
                         vlc_value_t oldval, vlc_value_t val,
                         void *p_data );

int  E_(BlocksPerReadCallback) ( vlc_object_t *p_this, const char *psz_name,
				 vlc_value_t oldval, vlc_value_t val,
				 void *p_data );

/*****************************************************************************
 * Option help text
 *****************************************************************************/

#define DEBUG_LONGTEXT \
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
    "vcdinfo  (800) 2048\n"

#define VCD_TITLE_FMT_LONGTEXT \
"Format used in the GUI Playlist Title. Similar to the Unix date \n" \
"Format specifiers that start with a percent sign. Specifiers are: \n" \
"   %A : The album information\n" \
"   %C : The VCD volume count - the number of CDs in the collection\n" \
"   %c : The VCD volume num - the number of the CD in the collection.\n" \
"   %F : The VCD Format, e.g. VCD 1.0, VCD 1.1, VCD 2.0, or SVCD\n" \
"   %I : The current entry/segment/playback type, e.g. ENTRY, TRACK, SEGMENT...\n" \
"   %L : The playlist ID prefixed with \" LID\" if it exists\n" \
"   %N : The current number of the %I - a decimal number\n" \
"   %P : The publisher ID\n" \
"   %p : The preparer ID\n" \
"   %S : If we are in a segment (menu), the kind of segment\n" \
"   %T : The MPEG track number (starts at 1)\n" \
"   %V : The volume set ID\n" \
"   %v : The volume ID\n" \
"       A number between 1 and the volume count.\n" \
"   %% : a % \n"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    add_usage_hint( N_("vcdx://[device-or-file][@{P,S,T}num]") );
    set_description( _("Video CD (VCD 1.0, 1.1, 2.0, SVCD, HQVCD) input") );
    set_capability( "access2", 55 /* slightly lower than vcd */ );
    set_shortname( N_("(Super) Video CD"));
    set_callbacks( VCDOpen, VCDClose );
    add_shortcut( "vcdx" );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    /* Configuration options */
    add_integer ( MODULE_STRING "-debug", 0, NULL,
                  N_("If nonzero, this gives additional debug information."),
                  DEBUG_LONGTEXT, VLC_TRUE );

    add_integer ( MODULE_STRING "-blocks-per-read", 20, 
		  NULL,
                  N_("Number of CD blocks to get in a single read."),
                  N_("Number of CD blocks to get in a single read."),
		  VLC_TRUE );

    add_bool( MODULE_STRING "-PBC", 0, NULL,
              N_("Use playback control?"),
              N_("If VCD is authored with playback control, use it. "
                 "Otherwise we play by tracks."),
              VLC_FALSE );

    add_bool( MODULE_STRING "-track-length", VLC_TRUE, 
	      NULL,
              N_("Use track length as maximum unit in seek?"),
              N_("If set, the length of the seek bar is the track rather than "
		 "the length of an entry"),
              VLC_FALSE );

    add_bool( MODULE_STRING "-extended-info", 0, NULL,
              N_("Show extended VCD info?"),
              N_("Show the maximum about of information under Stream and "
                 "Media Info. Shows for example playback control navigation."),
              VLC_FALSE );

    add_string( MODULE_STRING "-author-format",
                "%v - %F disc %c of %C",
                NULL,
                N_("Format to use in playlist \"author\""),
                VCD_TITLE_FMT_LONGTEXT, VLC_TRUE );

    add_string( MODULE_STRING "-title-format",
                "%I %N %L%S - %M %A %v - disc %c of %C %F",
                NULL,
                N_("Format to use in playlist \"title\" field"),
                VCD_TITLE_FMT_LONGTEXT, VLC_FALSE );

vlc_module_end();

