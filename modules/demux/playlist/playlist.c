/*****************************************************************************
 * playlist.c :  Playlist import module
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_url.h>

#if defined( _WIN32 ) || defined( __OS2__ )
# include <ctype.h>                          /* isalpha */
#endif
#include <assert.h>

#include "playlist.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define SHOW_ADULT_TEXT N_( "Show shoutcast adult content" )
#define SHOW_ADULT_LONGTEXT N_( "Show NC17 rated video streams when " \
                "using shoutcast video playlists." )

#define SKIP_ADS_TEXT N_( "Skip ads" )
#define SKIP_ADS_LONGTEXT N_( "Use playlist options usually used to prevent " \
    "ads skipping to detect ads and prevent adding them to the playlist." )

static const char *const psz_recursive_list[] = { "none", "collapse", "expand" };
static const char *const psz_recursive_list_text[] = {
    N_("None"), N_("Collapse"), N_("Expand"), N_("Expand distant files") };

#define RECURSIVE_TEXT N_("Subdirectory behavior")
#define RECURSIVE_LONGTEXT N_( \
        "Select whether subdirectories must be expanded.\n" \
        "none: subdirectories do not appear in the playlist.\n" \
        "collapse: subdirectories appear but are expanded on first play.\n" \
        "expand: all subdirectories are expanded.\n" )

static const char *const psz_sort_list[] = { "collate", "version", "none" };
static const char *const psz_sort_list_text[] = {
    N_("Sort alphabetically according to the current language's collation rules."),
    N_("Sort items in a natural order (for example: 1.ogg 2.ogg 10.ogg). This method does not take the current language's collation rules into account."),
    N_("Do not sort the items.") };

#define SORT_TEXT N_("Directory sort order")
#define SORT_LONGTEXT N_( \
    "Define the sort algorithm used when adding items from a directory." )

#define IGNORE_TEXT N_("Ignored extensions")
#define IGNORE_LONGTEXT N_( \
        "Files with these extensions will not be added to playlist when " \
        "opening a directory.\n" \
        "This is useful if you add directories that contain playlist files " \
        "for instance. Use a comma-separated list of extensions." )

#define SHOW_HIDDENFILES_TEXT N_("Show hidden files")
#define SHOW_HIDDENFILES_LONGTEXT N_( \
        "Ignore files starting with '.'" )

vlc_module_begin ()
    add_shortcut( "playlist" )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    add_obsolete_integer( "parent-item" ) /* removed since 1.1.0 */

    add_bool( "playlist-skip-ads", true,
              SKIP_ADS_TEXT, SKIP_ADS_LONGTEXT, false )

    set_shortname( N_("Playlist") )
    set_description( N_("Playlist") )
    add_submodule ()
        set_description( N_("M3U playlist import") )
        add_shortcut( "playlist", "m3u", "m3u8", "m3u-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_M3U, Close_M3U )
    add_submodule ()
        set_description( N_("RAM playlist import") )
        add_shortcut( "playlist", "ram-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_RAM, NULL )
    add_submodule ()
        set_description( N_("PLS playlist import") )
        add_shortcut( "playlist", "pls-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_PLS, NULL )
    add_submodule ()
        set_description( N_("B4S playlist import") )
        add_shortcut( "playlist", "b4s-open", "shout-b4s" )
        set_capability( "demux", 10 )
        set_callbacks( Import_B4S, NULL )
    add_submodule ()
        set_description( N_("DVB playlist import") )
        add_shortcut( "playlist", "dvb-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_DVB, NULL )
    add_submodule ()
        set_description( N_("Podcast parser") )
        add_shortcut( "playlist", "podcast" )
        set_capability( "demux", 10 )
        set_callbacks( Import_podcast, NULL )
    add_submodule ()
        set_description( N_("XSPF playlist import") )
        add_shortcut( "playlist", "xspf-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_xspf, Close_xspf )
    add_submodule ()
        set_description( N_("New winamp 5.2 shoutcast import") )
        add_shortcut( "playlist", "shout-winamp" )
        set_capability( "demux", 10 )
        set_callbacks( Import_Shoutcast, NULL )
        add_bool( "shoutcast-show-adult", false,
                   SHOW_ADULT_TEXT, SHOW_ADULT_LONGTEXT, false )
    add_submodule ()
        set_description( N_("ASX playlist import") )
        add_shortcut( "playlist", "asx-open" )
        set_capability( "demux", 10 )
        set_callbacks( Import_ASX, NULL )
    add_submodule ()
        set_description( N_("Kasenna MediaBase parser") )
        add_shortcut( "playlist", "sgimb" )
        set_capability( "demux", 10 )
        set_callbacks( Import_SGIMB, Close_SGIMB )
    add_submodule ()
        set_description( N_("QuickTime Media Link importer") )
        add_shortcut( "playlist", "qtl" )
        set_capability( "demux", 10 )
        set_callbacks( Import_QTL, NULL )
    add_submodule ()
        set_description( N_("Google Video Playlist importer") )
        add_shortcut( "playlist", "gvp" )
        set_capability( "demux", 10 )
        set_callbacks( Import_GVP, NULL )
    add_submodule ()
        set_description( N_("Dummy IFO demux") )
        add_shortcut( "playlist" )
        set_capability( "demux", 12 )
        set_callbacks( Import_IFO, NULL )
    add_submodule ()
        set_description( N_("iTunes Music Library importer") )
        add_shortcut( "playlist", "itml" )
        set_capability( "demux", 10 )
        set_callbacks( Import_iTML, Close_iTML )
    add_submodule ()
        set_description( N_("WPL playlist import") )
        add_shortcut( "playlist", "wpl" )
        set_capability( "demux", 10 )
        set_callbacks( Import_WPL, Close_WPL )
    add_submodule ()
        set_description( N_("Directory import") )
        add_shortcut( "playlist", "directory" )
        set_capability( "demux", 10 )
        set_callbacks( Import_Dir, Close_Dir )
        add_string( "recursive", "collapse" , RECURSIVE_TEXT,
                    RECURSIVE_LONGTEXT, false )
          change_string_list( psz_recursive_list, psz_recursive_list_text )
        add_string( "ignore-filetypes", "m3u,db,nfo,ini,jpg,jpeg,ljpg,gif,png,pgm,pgmyuv,pbm,pam,tga,bmp,pnm,xpm,xcf,pcx,tif,tiff,lbm,sfv,txt,sub,idx,srt,cue,ssa",
                    IGNORE_TEXT, IGNORE_LONGTEXT, false )
        add_string( "directory-sort", "collate", SORT_TEXT, SORT_LONGTEXT, false )
          change_string_list( psz_sort_list, psz_sort_list_text )
        add_bool( "show-hiddenfiles", false,
                   SHOW_HIDDENFILES_TEXT, SHOW_HIDDENFILES_LONGTEXT, false )
vlc_module_end ()

int Control(demux_t *demux, int query, va_list args)
{
    (void) demux;
    switch( query )
    {
        case DEMUX_IS_PLAYLIST:
        {
            bool *pb_bool = (bool*)va_arg( args, bool * );
            *pb_bool = true;
            return VLC_SUCCESS;
        }
        case DEMUX_GET_META:
        {
            return VLC_SUCCESS;
        }
        case DEMUX_HAS_UNSUPPORTED_META:
        {
            *(va_arg( args, bool * )) = false;
            return VLC_SUCCESS;
        }
    }
    return VLC_EGENERIC;
}

input_item_t * GetCurrentItem(demux_t *p_demux)
{
    input_item_t *p_current_input = input_GetItem( p_demux->p_input );
    vlc_gc_incref(p_current_input);
    return p_current_input;
}

/**
 * Computes the base URL.
 *
 * Rebuilds the base URL for the playlist.
 */
char *FindPrefix(demux_t *p_demux)
{
    char *url;

    if (unlikely(asprintf(&url, "%s://%s", p_demux->psz_access,
                          p_demux->psz_location) == -1))
        url = NULL;
    return url;
}

/**
 * Resolves a playlist location.
 *
 * Resolves a resource location within the playlist relative to the playlist
 * base URL.
 */
char *ProcessMRL(const char *str, const char *base)
{
    if (str == NULL)
        return NULL;

#if (DIR_SEP_CHAR == '\\')
    /* UNC path prefix? */
    if (strncmp(str, "\\\\", 2) == 0
    /* Drive letter prefix? */
     || (isalpha((unsigned char)str[0]) && str[1] == ':'))
        /* Assume this an absolute file path - usually true */
        return vlc_path2uri(str, NULL);
    /* TODO: drive-relative path: if (str[0] == '\\') */
#endif

#ifdef HAVE_OPEN_MEMSTREAM
    /* The base URL is always an URL: it is the URL of the playlist.
     *
     * However it is not always known if the input string is a valid URL, a
     * broken URL or a local file path. As a rule, if it looks like a valid
     * URL, it must be treated as such, since most playlist formats use URLs.
     *
     * There are a few corner cases file paths that look like an URL but whose
     * URL representation does not match, notably when they contain a
     * percentage sign, a colon, a hash or a question mark. Luckily, they are
     * rather exceptional (and can be encoded as URL to make the playlist
     * work properly).
     *
     * If the input is not a valid URL, then we try to fix it up. It works in
     * all cases for URLs with incorrectly encoded segments, such as URLs with
     * white spaces or non-ASCII Unicode code points. It also works in most
     * cases where the input is a Unix-style file path, but not all.
     * It fails miserably if the playlist character encoding is misdetected.
     */
    char *rel = vlc_uri_fixup(str);
    if (rel != NULL)
        str = rel;

    char *abs = vlc_uri_resolve(base, str);
    free(rel);
    return abs;
#else
    const char *split = strrchr(base, '/');
    char *abs;

    assert(split != NULL);

    if (asprintf(&abs, "%.*s/%s", (int)(split - base), base, str) == -1)
        abs = NULL;
    return abs;
#endif
}

/**
 * Checks stream Content-Type against a known one
 */
bool CheckContentType( stream_t * p_stream, const char * psz_ctype )
{
    char *psz_check = stream_ContentType( p_stream );
    if( !psz_check ) return false;

    int i_len = strlen( psz_check );
    if ( i_len == 0 )
    {
        free( psz_check );
        return false;
    }

    /* check for Content-Type: foo-type; charset=... */
    const char * psz_sep = strchr( psz_check, ';' );
    if ( psz_sep )
        i_len = __MIN( i_len, psz_sep - psz_check );

    int i_res = strncasecmp( psz_check, psz_ctype, i_len );
    free( psz_check );

    return ( i_res == 0 ) ? true : false;
}
