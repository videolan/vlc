/*****************************************************************************
 * fs.c: file system access plugin
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 * Copyright © 2006-2007 Rémi Denis-Courmont
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Rémi Denis-Courmont <rem # videolan # org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "fs.h"
#include <vlc_plugin.h>

#define RECURSIVE_TEXT N_("Subdirectory behavior")
#define RECURSIVE_LONGTEXT N_( \
        "Select whether subdirectories must be expanded.\n" \
        "none: subdirectories do not appear in the playlist.\n" \
        "collapse: subdirectories appear but are expanded on first play.\n" \
        "expand: all subdirectories are expanded.\n" )

static const char *const psz_recursive_list[] = { "none", "collapse", "expand" };
static const char *const psz_recursive_list_text[] = {
    N_("None"), N_("Collapse"), N_("Expand") };

#define IGNORE_TEXT N_("Ignored extensions")
#define IGNORE_LONGTEXT N_( \
        "Files with these extensions will not be added to playlist when " \
        "opening a directory.\n" \
        "This is useful if you add directories that contain playlist files " \
        "for instance. Use a comma-separated list of extensions." )

static const char *const psz_sort_list[] = { "collate", "version", "none" };
static const char *const psz_sort_list_text[] = {
    N_("Sort alphabetically according to the current language's collation rules."),
    N_("Sort items in a natural order (for example: 1.ogg 2.ogg 10.ogg). This method does not take the current language's collation rules into account."),
    N_("Do not sort the items.") };

#define SORT_TEXT N_("Directory sort order")
#define SORT_LONGTEXT N_( \
    "Define the sort algorithm used when adding items from a directory." )

vlc_module_begin ()
    set_description( N_("File input") )
    set_shortname( N_("File") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_obsolete_string( "file-cat" )
    set_capability( "access", 50 )
    add_shortcut( "file", "fd", "stream" )
    set_callbacks( FileOpen, FileClose )

    add_submodule()
    set_section( N_("Directory" ), NULL )
    set_capability( "access", 55 )
    add_string( "recursive", "expand" , RECURSIVE_TEXT,
                RECURSIVE_LONGTEXT, false )
      change_string_list( psz_recursive_list, psz_recursive_list_text )
    add_string( "ignore-filetypes", "m3u,db,nfo,ini,jpg,jpeg,ljpg,gif,png,pgm,pgmyuv,pbm,pam,tga,bmp,pnm,xpm,xcf,pcx,tif,tiff,lbm,sfv,txt,sub,idx,srt,cue,ssa",
                IGNORE_TEXT, IGNORE_LONGTEXT, false )
    add_string( "directory-sort", "collate", SORT_TEXT, SORT_LONGTEXT, false )
      change_string_list( psz_sort_list, psz_sort_list_text )
#ifndef HAVE_FDOPENDIR
    add_shortcut( "file", "directory", "dir" )
#else
    add_shortcut( "directory", "dir" )
#endif
    set_callbacks( DirOpen, DirClose )
vlc_module_end ()
