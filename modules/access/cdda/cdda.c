/*****************************************************************************
 * cddax.c : CD digital audio input module for vlc using libcdio
 *****************************************************************************
 * Copyright (C) 2000,2003 VideoLAN
 * $Id: cdda.c,v 1.7 2003/12/01 03:37:23 rocky Exp $
 *
 * Authors: Rocky Bernstein <rocky@panix.com> 
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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

#define DEBUG_LONGTEXT N_( \
    "This integer when viewed in binary is a debugging mask\n" \
    "meta info        1\n" \
    "events           2\n" \
    "MRL              4\n" \
    "external call    8\n" \
    "all calls (10)  16\n" \
    "LSN       (20)  32\n" \
    "seek      (40)  64\n" \
    "libcdio   (80) 128\n" \
    "libcddb  (100) 256\n" )

#define DEV_LONGTEXT N_( \
    "Specify the name of the CD-ROM device that will be used by default. " \
    "If you don't specify anything, we'll scan for a suitable CD-ROM device.")

#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for cdda streams. This " \
    "value should be set in millisecond units." )

#define TITLE_FMT_LONGTEXT N_( \
"Format used in the GUI Playlist Title. Similar to the Unix date \n" \
"Format specifiers that start with a percent sign. Specifiers are: \n" \
"   %a : The artist **\n" \
"   %A : The album information **\n" \
"   %C : Category **\n" \
"   %I : CDDB disk ID\n" \
"   %G : Genre **\n" \
"   %M : The current MRL\n" \
"   %m : The CD-DA Media Catalog Number (MCN)\n" \
"   %n : The number of tracks on the CD\n" \
"   %p : The artist/performer/composer in the track **\n" \
"   %T : The track number\n" \
"   %s : Number of seconds in this track \n" \
"   %t : The title **\n" \
"   %Y : The year 19xx or 20xx **\n" \
"   %% : a % \n" \
"\n\n ** Only available if CDDB is enabled")

#ifdef HAVE_LIBCDDB
#define DEFAULT_TITLE_FORMAT "Track %T. %t - %p",
#else 
#define DEFAULT_TITLE_FORMAT "%T %M",
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    add_usage_hint( N_("cddax://[device-or-file][@[T]num]") );
    set_description( _("Compact Disc Digital Audio (CD-DA) input") );
    set_capability( "access", 75 /* slightly higher than cdda */ );
    set_callbacks( E_(Open), E_(Close) );
    add_shortcut( "cdda" );
    add_shortcut( "cddax" );

    /* Configuration options */
    add_category_hint( N_("CDX"), NULL, VLC_TRUE );

    add_integer ( MODULE_STRING "-debug", 0, E_(DebugCallback), 
		  N_("set debug mask for additional debugging."),
                  DEBUG_LONGTEXT, VLC_TRUE );

    add_integer( MODULE_STRING "-caching", 
		 DEFAULT_PTS_DELAY / 1000, NULL, 
		 N_("Caching value in ms"), 
		 CACHING_LONGTEXT, VLC_TRUE );

    add_string( MODULE_STRING "-device", "", NULL, 
		N_("CD-ROM device name"),
                DEV_LONGTEXT, VLC_FALSE );

    add_string( MODULE_STRING "-title-format", DEFAULT_TITLE_FORMAT, NULL, 
		N_("Format to use in playlist 'title' field"),
                TITLE_FMT_LONGTEXT, VLC_TRUE );

#ifdef HAVE_LIBCDDB
    add_bool( MODULE_STRING "-cddb-enabled", 1, NULL,
	      N_("Do CDDB lookups?"),
	      N_("If set, lookup CD-DA track information using the CDDB "
		 "protocol"),
	      VLC_FALSE );

    add_string( MODULE_STRING "-cddb-server", "freedb.freedb.org", NULL, 
		N_("CDDB server"), 
		N_( "Contact this CDDB server look up CD-DA information"),
		 VLC_TRUE );

    add_integer( MODULE_STRING "-cddb-port", 8880, NULL, 
		 N_("CDDB server port"), 
		 N_("CDDB server uses this port number to communicate on"), 
		 VLC_TRUE );

    add_string( MODULE_STRING "-cddb-email", "me@home", NULL, 
		N_("email address reported to CDDB server"), 
		N_("email address reported to CDDB server"), 
		 VLC_TRUE );

    add_bool( MODULE_STRING "-cddb-enable-cache", 1, NULL,
	      N_("Cache CDDB lookups?"),
	      N_("If set cache CDDB information about this CD"),
	      VLC_FALSE );

    add_bool( MODULE_STRING "-cddb-httpd", 0, NULL,
	      N_("Contact CDDB via the HTTP protocol?"),
	      N_("If set, the CDDB server get information via the CDDB HTTP "
		 "protocol"),
	      VLC_TRUE );

    add_integer( MODULE_STRING "-cddb-timeout", 10, NULL, 
		 N_("CDDB server timeout"), 
		 N_("Time (in seconds) to wait for a response from the "
		    "CDDB server"), 
		 VLC_FALSE );

    add_string( MODULE_STRING "-cddb-cachedir", "~/.cddbslave", NULL, 
		N_("Directory to cache CDDB requests"), 
		N_("Directory to cache CDDB requests"), 
		 VLC_TRUE );

#endif

    add_submodule();
        set_description( _("CD Audio demux") );
        set_capability( "demux", 0 );
        set_callbacks( E_(DemuxOpen), E_(DemuxClose) );
        add_shortcut( "cdda" );

    add_submodule();
        set_capability( "interface", 0 );
        set_callbacks( E_(OpenIntf), E_(CloseIntf) );

vlc_module_end();
