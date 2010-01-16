/*****************************************************************************
 * fs.c: file system access plugin
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
 * Copyright © 2006-2007 Rémi Denis-Courmont
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan # org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "fs.h"
#include <vlc_plugin.h>

#define CACHING_TEXT N_("Caching value (ms)")
#define CACHING_LONGTEXT N_( \
    "Caching value for files, in milliseconds." )

#define NETWORK_CACHING_TEXT N_("Extra network caching value (ms)")
#define NETWORK_CACHING_LONGTEXT N_( \
    "Supplementary caching value for remote files, in milliseconds." )

#define RECURSIVE_TEXT N_("Subdirectory behavior")
#define RECURSIVE_LONGTEXT N_( \
        "Select whether subdirectories must be expanded.\n" \
        "none: subdirectories do not appear in the playlist.\n" \
        "collapse: subdirectories appear but are expanded on first play.\n" \
        "expand: all subdirectories are expanded.\n" )

static const char *const psz_recursive_list[] = { "none", "collapse", "expand" };
static const char *const psz_recursive_list_text[] = {
    N_("none"), N_("collapse"), N_("expand") };

#define IGNORE_TEXT N_("Ignored extensions")
#define IGNORE_LONGTEXT N_( \
        "Files with these extensions will not be added to playlist when " \
        "opening a directory.\n" \
        "This is useful if you add directories that contain playlist files " \
        "for instance. Use a comma-separated list of extensions." )

vlc_module_begin ()
    set_description( N_("File input") )
    set_shortname( N_("File") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_integer( "file-caching", DEFAULT_PTS_DELAY / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, true )
        change_safe()
    add_integer( "network-caching", 3 * DEFAULT_PTS_DELAY / 1000, NULL,
                 NETWORK_CACHING_TEXT, NETWORK_CACHING_LONGTEXT, true )
        change_safe()
    add_obsolete_string( "file-cat" )
    set_capability( "access", 50 )
    add_shortcut( "file" )
    add_shortcut( "fd" )
    add_shortcut( "stream" )
    set_callbacks( Open, Close )

    add_submodule()
    set_shortname( N_("Directory" ) )
    set_description( N_("Directory input") )
    set_capability( "access", 55 )
    add_string( "recursive", "expand" , NULL, RECURSIVE_TEXT,
                RECURSIVE_LONGTEXT, false )
      change_string_list( psz_recursive_list, psz_recursive_list_text, 0 )
    add_string( "ignore-filetypes", "m3u,db,nfo,ini,jpg,jpeg,ljpg,gif,png,pgm,pgmyuv,pbm,pam,tga,bmp,pnm,xpm,xcf,pcx,tif,tiff,lbm,sfv,txt,sub,idx,srt,cue,ssa",
                NULL, IGNORE_TEXT, IGNORE_LONGTEXT, false )
#ifndef HAVE_FDOPENDIR
    add_shortcut( "file" )
#endif
    add_shortcut( "directory" )
    add_shortcut( "dir" )
    set_callbacks( DirOpen, DirClose )
vlc_module_end ()
