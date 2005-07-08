/*****************************************************************************
 * cdda.c : CD digital audio input module for vlc using libcdio
 *****************************************************************************
 * Copyright (C) 2000, 2003, 2004, 2005 VideoLAN (Centrale Réseaux) and its contributors
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
 * Preamble
 *****************************************************************************/

#include "callback.h"
#include "access.h"
#include <cdio/version.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/*****************************************************************************
 * Option help text
 *****************************************************************************/

#if LIBCDIO_VERSION_NUM >= 72
static char *psz_paranoia_list[] = { "none", "overlap", "full" };
static char *psz_paranoia_list_text[] = { N_("none"), N_("overlap"),
					  N_("full") };
#endif

#define DEBUG_LONGTEXT N_( \
    "This integer when viewed in binary is a debugging mask\n" \
    "meta info          1\n" \
    "events             2\n" \
    "MRL                4\n" \
    "external call      8\n" \
    "all calls (0x10)  16\n" \
    "LSN       (0x20)  32\n" \
    "seek      (0x40)  64\n" \
    "libcdio   (0x80) 128\n" \
    "libcddb  (0x100) 256\n" )

#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for CDDA streams. This " \
    "value should be set in millisecond units." )

#define BLOCKS_PER_READ_LONGTEXT N_( \
    "Allows you to specify how many CD blocks to get on a single CD read. " \
    "Generally on newer/faster CDs, this increases throughput at the " \
    "expense of a little more memory usage and initial delay. SCSI-MMC " \
    "limitations generally don't allow for more than 25 blocks per access.")

#define CDDB_TITLE_FMT_LONGTEXT N_( \
"Format used in the GUI Playlist Title. Similar to the Unix date \n" \
"Format specifiers that start with a percent sign. Specifiers are: \n" \
"   %a : The artist (for the album)\n" \
"   %A : The album information\n" \
"   %C : Category\n" \
"   %e : The extended data (for a track)\n" \
"   %I : CDDB disk ID\n" \
"   %G : Genre\n" \
"   %M : The current MRL\n" \
"   %m : The CD-DA Media Catalog Number (MCN)\n" \
"   %n : The number of tracks on the CD\n" \
"   %p : The artist/performer/composer in the track\n" \
"   %T : The track number\n" \
"   %s : Number of seconds in this track\n" \
"   %S : Number of seconds in the CD\n" \
"   %t : The track title or MRL if no title\n" \
"   %Y : The year 19xx or 20xx\n" \
"   %% : a % \n")

#define TITLE_FMT_LONGTEXT N_( \
"Format used in the GUI Playlist Title. Similar to the Unix date \n" \
"Format specifiers that start with a percent sign. Specifiers are: \n" \
"   %M : The current MRL\n" \
"   %m : The CD-DA Media Catalog Number (MCN)\n" \
"   %n : The number of tracks on the CD\n" \
"   %T : The track number\n" \
"   %s : Number of seconds in this track\n" \
"   %S : Number of seconds in the CD\n" \
"   %t : The track title or MRL if no title\n" \
"   %% : a % \n")

#define PARANOIA_TEXT N_("Enable CD paranoia?")
#define PARANOIA_LONGTEXT N_( \
        "Select whether to use CD Paranoia for jitter/error correction.\n" \
        "none: no paranoia - fastest.\n" \
        "overlap: do only overlap detection - not generally recommended.\n" \
        "full: complete jitter and error correction detection - slowest.\n" )

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin();
    add_usage_hint( N_("cddax://[device-or-file][@[T]track]") );
    set_description( _("Compact Disc Digital Audio (CD-DA) input") );
    set_capability( "access2", 10 /* compare with priority of cdda */ );
    set_shortname( N_("Audio Compact Disc"));
    set_callbacks( CDDAOpen, CDDAClose );
    add_shortcut( "cddax" );
    add_shortcut( "cd" );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    /* Configuration options */
    add_integer ( MODULE_STRING "-debug", 0, CDDADebugCB,
                  N_("If nonzero, this gives additional debug information."),
                  DEBUG_LONGTEXT, VLC_TRUE );

    add_integer( MODULE_STRING "-caching",
                 DEFAULT_PTS_DELAY / MILLISECONDS_PER_SEC, NULL,
                 N_("Caching value in microseconds"),
                 CACHING_LONGTEXT, VLC_TRUE );

    add_integer( MODULE_STRING "-blocks-per-read",
                 DEFAULT_BLOCKS_PER_READ, CDDABlocksPerReadCB,
                 N_("Number of blocks per CD read"),
                 BLOCKS_PER_READ_LONGTEXT, VLC_TRUE );

    add_string( MODULE_STRING "-title-format",
                "Track %T. %t", NULL,
                N_("Format to use in playlist \"title\" field when no CDDB"),
                TITLE_FMT_LONGTEXT, VLC_TRUE );

#if LIBCDIO_VERSION_NUM >= 73
    add_bool( MODULE_STRING "-analog-output", VLC_FALSE, NULL,
              N_("Use CD audio controls and output?"),
              N_("If set, audio controls and audio jack output are used"),
              VLC_FALSE );
#endif

    add_bool( MODULE_STRING "-cdtext-enabled", VLC_TRUE, CDTextEnabledCB,
              N_("Do CD-Text lookups?"),
              N_("If set, get CD-Text information"),
              VLC_FALSE );

    add_bool( MODULE_STRING "-navigation-mode", VLC_TRUE, 
#if FIXED
	      CDDANavModeCB,
#else
	      NULL,
#endif
              N_("Use Navigation-style playback?"),
              N_("If set, tracks are navigated via Navagation rather than "
		 "a playlist entries"),
              VLC_FALSE );

#if LIBCDIO_VERSION_NUM >= 72
      add_string( MODULE_STRING "-paranoia", NULL, NULL,
		PARANOIA_TEXT,
		PARANOIA_LONGTEXT,
		VLC_FALSE );
      change_string_list( psz_paranoia_list, psz_paranoia_list_text, 0 );
#endif /* LIBCDIO_VERSION_NUM >= 72 */

#ifdef HAVE_LIBCDDB
    set_section( N_("CDDB" ), 0 );
    add_string( MODULE_STRING "-cddb-title-format",
                "Track %T. %t - %p %A", NULL,
                N_("Format to use in playlist \"title\" field when using CDDB"),
                CDDB_TITLE_FMT_LONGTEXT, VLC_TRUE );

    add_bool( MODULE_STRING "-cddb-enabled", VLC_TRUE, CDDBEnabledCB,
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

    add_bool( MODULE_STRING "-cddb-enable-cache", VLC_TRUE, NULL,
              N_("Cache CDDB lookups?"),
              N_("If set cache CDDB information about this CD"),
              VLC_FALSE );

    add_bool( MODULE_STRING "-cddb-httpd", VLC_FALSE, NULL,
              N_("Contact CDDB via the HTTP protocol?"),
              N_("If set, the CDDB server gets information via the CDDB HTTP "
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

    add_bool( MODULE_STRING "-cdtext-prefer", VLC_TRUE, CDTextPreferCB,
              N_("Prefer CD-Text info to CDDB info?"),
              N_("If set, CD-Text information will be preferred "
		 "to CDDB information when both are available"),
              VLC_FALSE );
#endif /*HAVE_LIBCDDB*/

vlc_module_end();
