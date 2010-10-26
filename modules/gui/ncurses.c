/*****************************************************************************
 * ncurses.c : NCurses interface for vlc
 *****************************************************************************
 * Copyright © 2001-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Yoann Peronneau <yoann@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Rafaël Carré <funman@videolanorg>
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

/*
 * Note that when we use wide characters (and link with libncursesw),
 * we assume that an UTF8 locale is used (or compatible, such as ASCII).
 * Other characters encodings are not supported.
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#define _XOPEN_SOURCE_EXTENDED 1
#include <wchar.h>

#include <ncurses.h>

#include <vlc_interface.h>
#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_charset.h>
#include <vlc_input.h>
#include <vlc_es.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_fs.h>

#include <assert.h>

#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open           (vlc_object_t *);
static void Close          (vlc_object_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define BROWSE_TEXT N_("Filebrowser starting point")
#define BROWSE_LONGTEXT N_(\
    "This option allows you to specify the directory the ncurses filebrowser " \
    "will show you initially.")

vlc_module_begin ()
    set_shortname("Ncurses")
    set_description(N_("Ncurses interface"))
    set_capability("interface", 10)
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_MAIN)
    set_callbacks(Open, Close)
    add_shortcut("curses")
    add_directory("browse-dir", NULL, BROWSE_TEXT, BROWSE_LONGTEXT, false)
vlc_module_end ()

/*****************************************************************************
 * intf_sys_t: description and status of ncurses interface
 *****************************************************************************/
enum
{
    BOX_NONE,
    BOX_HELP,
    BOX_INFO,
//  BOX_LOG,
    BOX_PLAYLIST,
    BOX_SEARCH,
    BOX_OPEN,
    BOX_BROWSE,
    BOX_META,
    BOX_OBJECTS,
    BOX_STATS
};

enum
{
    C_DEFAULT = 0,
    C_TITLE,
    C_PLAYLIST_1,
    C_PLAYLIST_2,
    C_PLAYLIST_3,
    C_BOX,
    C_STATUS,
#if 0
    C_INFO,
    C_ERROR,
    C_WARNING,
    C_DEBUG,
#endif
    C_CATEGORY,
    C_FOLDER,
    /* XXX: new elements here ! */

    C_MAX
};

/* Available colors: BLACK RED GREEN YELLOW BLUE MAGENTA CYAN WHITE */
static const struct { short f; short b; } color_pairs[] =
{
    /* element */       /* foreground*/ /* background*/
    [C_TITLE]       = { COLOR_YELLOW,   COLOR_BLACK },

    /* jamaican playlist, for rastafari sisters & brothers! */
    [C_PLAYLIST_1]  = { COLOR_GREEN,    COLOR_BLACK },
    [C_PLAYLIST_2]  = { COLOR_YELLOW,   COLOR_BLACK },
    [C_PLAYLIST_3]  = { COLOR_RED,      COLOR_BLACK },

    /* used in DrawBox() */
    [C_BOX]         = { COLOR_CYAN,     COLOR_BLACK },
    /* Source: State, Position, Volume, Chapters, etc...*/
    [C_STATUS]      = { COLOR_BLUE,     COLOR_BLACK },

#if 0
    /* VLC messages, keep the order from highest priority to lowest */
    [C_INFO]        = { COLOR_BLACK,    COLOR_WHITE },
    [C_ERROR]       = { COLOR_RED,      COLOR_BLACK },
    [C_WARNING]     = { COLOR_YELLOW,   COLOR_BLACK },
    [C_DEBUG]       = { COLOR_WHITE,    COLOR_BLACK },
#endif
    /* Category title: help, info, metadata */
    [C_CATEGORY]    = { COLOR_MAGENTA,  COLOR_BLACK },
    /* Folder (BOX_BROWSE) */
    [C_FOLDER]      = { COLOR_RED,      COLOR_BLACK },
};

struct dir_entry_t
{
    bool        b_file;
    char        *psz_path;
};

struct pl_item_t
{
    playlist_item_t *p_item;
    char            *psz_display;
};

struct intf_sys_t
{
    input_thread_t *p_input;

    bool            b_color;
    bool            b_color_started;

    WINDOW          *w;

    int             i_box_type;
    int             i_box_y;
    int             i_box_lines;
    int             i_box_lines_total;
    int             i_box_start;

    int             i_box_plidx;    /* Playlist index */
    int             b_box_plidx_follow;
    int             i_box_bidx;     /* browser index */

    playlist_item_t *p_node;        /* current node */

//  msg_subscription_t* p_sub;                  /* message bank subscription */

    char            psz_search_chain[20];
    char            *psz_old_search;
    int             i_before_search;

    char            psz_open_chain[50];

    char            *psz_current_dir;
    int             i_dir_entries;
    struct dir_entry_t  **pp_dir_entries;
    bool            b_show_hidden_files;

    bool            category_view;
    struct pl_item_t    **pp_plist;
    int             i_plist_entries;
    bool            b_need_update;              /* for playlist view         */
};

/*****************************************************************************
 * Directories
 *****************************************************************************/

static void DirsDestroy(intf_sys_t *p_sys)
{
    while (p_sys->i_dir_entries)
    {
        struct dir_entry_t *p_dir_entry;
        p_dir_entry = p_sys->pp_dir_entries[--p_sys->i_dir_entries];
        free(p_dir_entry->psz_path);
        free(p_dir_entry);
    }
    free(p_sys->pp_dir_entries);
    p_sys->pp_dir_entries = NULL;
}

static int comp_dir_entries(const void *pp_dir_entry1, const void *pp_dir_entry2)
{
    struct dir_entry_t *p_dir_entry1 = *(struct dir_entry_t**)pp_dir_entry1;
    struct dir_entry_t *p_dir_entry2 = *(struct dir_entry_t**)pp_dir_entry2;

    if (p_dir_entry1->b_file == p_dir_entry2->b_file)
        return strcasecmp(p_dir_entry1->psz_path, p_dir_entry2->psz_path);

    return p_dir_entry1->b_file ? 1 : -1;
}

static bool IsFile(const char *current_dir, const char *entry)
{
    bool ret = true;
#ifdef S_ISDIR
    char *uri;
    struct stat st;

    if (asprintf(&uri, "%s/%s", current_dir, entry) != -1)
    {
        ret = vlc_stat(uri, &st) || !S_ISDIR(st.st_mode);
        free(uri);
    }
#endif
    return ret;
}

static void ReadDir(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    DIR *p_current_dir;

    if (!p_sys->psz_current_dir || !*p_sys->psz_current_dir)
    {
        msg_Dbg(p_intf, "no current dir set");
        return;
    }

    char *psz_entry;

    /* Open the dir */
    p_current_dir = vlc_opendir(p_sys->psz_current_dir);

    if (!p_current_dir)
    {
        /* something went bad, get out of here ! */
        msg_Warn(p_intf, "cannot open directory `%s' (%m)",
                  p_sys->psz_current_dir);
        return;
    }

    /* Clean the old shit */
    DirsDestroy(p_sys);

    /* while we still have entries in the directory */
    while ((psz_entry = vlc_readdir(p_current_dir)))
    {
        struct dir_entry_t *p_dir_entry;

        if (!p_sys->b_show_hidden_files)
            if (*psz_entry == '.' && strcmp(psz_entry, ".."))
                goto next;

        if (!(p_dir_entry = malloc(sizeof *p_dir_entry)))
            goto next;

        p_dir_entry->b_file = IsFile(p_sys->psz_current_dir, psz_entry);
        p_dir_entry->psz_path = strdup(psz_entry);
        INSERT_ELEM(p_sys->pp_dir_entries, p_sys->i_dir_entries,
             p_sys->i_dir_entries, p_dir_entry);

next:
        free(psz_entry);
    }

    /* Sort */
    qsort(p_sys->pp_dir_entries, p_sys->i_dir_entries,
           sizeof(struct dir_entry_t*), &comp_dir_entries);

    closedir(p_current_dir);
}

/*****************************************************************************
 * Playlist
 *****************************************************************************/

static void PlaylistDestroy(intf_sys_t *p_sys)
{
    while (p_sys->i_plist_entries)
    {
        struct pl_item_t *p_pl_item = p_sys->pp_plist[--p_sys->i_plist_entries];
        free(p_pl_item->psz_display);
        free(p_pl_item);
    }
    free(p_sys->pp_plist);
    p_sys->pp_plist = NULL;
}

static inline playlist_item_t *PlaylistGetRoot(intf_thread_t *p_intf)
{
    playlist_t *p_playlist = pl_Get(p_intf);
    return p_intf->p_sys->category_view ?
        p_playlist->p_root_category :
        p_playlist->p_root_onelevel;
}

static bool PlaylistAddChild(intf_sys_t *p_sys, playlist_item_t *p_child,
                             const char *c, const char d)
{
    int ret;
    char *psz_name = input_item_GetTitleFbName(p_child->p_input);
    struct pl_item_t *p_pl_item = malloc(sizeof *p_pl_item);

    if (!psz_name || !p_pl_item)
        goto error;

    p_pl_item->p_item = p_child;

    if (c && *c)
        ret = asprintf(&p_pl_item->psz_display, "%s%c-%s", c, d, psz_name);
    else
        ret = asprintf(&p_pl_item->psz_display, " %s", psz_name);

    free(psz_name);
    psz_name = NULL;

    if (ret == -1)
        goto error;

    INSERT_ELEM(p_sys->pp_plist, p_sys->i_plist_entries,
                 p_sys->i_plist_entries, p_pl_item);

    return true;

error:
    free(psz_name);
    free(p_pl_item);
    return false;
}

static void PlaylistAddNode(intf_sys_t *p_sys, playlist_item_t *p_node,
                            const char *c)
{
    for(int k = 0; k < p_node->i_children; k++)
    {
        playlist_item_t *p_child = p_node->pp_children[k];
        char d = k == p_node->i_children - 1 ? '`' : '|';
        if(!PlaylistAddChild(p_sys, p_child, c, d))
            return;

        if (p_child->i_children <= 0)
            continue;

        if (*c)
        {
            char *psz_tmp;
            if (asprintf(&psz_tmp, "%s%c ", c,
                     k == p_node->i_children - 1 ? ' ' : '|') == -1)
                return;
            PlaylistAddNode(p_sys, p_child, psz_tmp);
            free(psz_tmp);
        }
        else
            PlaylistAddNode(p_sys, p_child, " ");
    }
}

static void PlaylistRebuild(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = pl_Get(p_intf);

    PL_LOCK;

    PlaylistDestroy(p_sys);
    PlaylistAddNode(p_sys, PlaylistGetRoot(p_intf), "");
    p_sys->b_need_update = false;

    PL_UNLOCK;
}

static int PlaylistChanged(vlc_object_t *p_this, const char *psz_variable,
                            vlc_value_t oval, vlc_value_t nval, void *param)
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_variable);
    VLC_UNUSED(oval); VLC_UNUSED(nval);

    intf_thread_t *p_intf   = (intf_thread_t *)param;
    intf_sys_t *p_sys       = p_intf->p_sys;
    playlist_item_t *p_node = playlist_CurrentPlayingItem(pl_Get(p_intf));

    p_sys->b_need_update = true;
    p_sys->p_node = p_node ? p_node->p_parent : NULL;

    return VLC_SUCCESS;
}

/* Playlist suxx */
/* This function have to be called with the playlist locked */
static inline bool PlaylistIsPlaying(playlist_t *p_playlist,
                                     playlist_item_t *p_item)
{
    playlist_item_t *p_played_item = playlist_CurrentPlayingItem(p_playlist);
    return p_item                && p_played_item
        && p_item->p_input       && p_played_item->p_input
        && p_item->p_input->i_id == p_played_item->p_input->i_id;
}

static int SubSearchPlaylist(intf_sys_t *p_sys, char *psz_searchstring,
                              int i_start, int i_stop)
{
    for(int i = i_start + 1; i < i_stop; i++)
        if (strcasestr(p_sys->pp_plist[i]->psz_display, psz_searchstring))
            return i;

    return -1;
}

static void SearchPlaylist(intf_sys_t *p_sys, char *psz_searchstring)
{
    int i_item, i_first = p_sys->i_before_search;

    if (i_first < 0)
        i_first = 0;

    if (!psz_searchstring || !*psz_searchstring)
    {
        p_sys->i_box_plidx = p_sys->i_before_search;
        return;
    }

    i_item = SubSearchPlaylist(p_sys, psz_searchstring, i_first + 1,
                               p_sys->i_plist_entries);
    if (i_item < 0)
        i_item = SubSearchPlaylist(p_sys, psz_searchstring, 0, i_first);

    if (i_item > 0)
        p_sys->i_box_plidx = i_item;
}

static inline bool IsIndex(intf_sys_t *p_sys, playlist_t *p_playlist, int i)
{
    playlist_item_t *p_item = p_sys->pp_plist[i]->p_item;
    return (p_item->i_children == 0 && p_item == p_sys->p_node) ||
            PlaylistIsPlaying(p_playlist, p_item);
}

static void FindIndex(intf_sys_t *p_sys, playlist_t *p_playlist, bool locked)
{
    int plidx = p_sys->i_box_plidx;
    if (!locked)
        PL_LOCK;

    if (plidx < 0 || plidx >= p_sys->i_plist_entries ||
        !IsIndex(p_sys, p_playlist, plidx))
    {
        for(int i = 0; i < p_sys->i_plist_entries; i++)
            if (IsIndex(p_sys, p_playlist, i))
            {
                p_sys->i_box_plidx = i;
                break;
            }
    }

    if (!locked)
        PL_UNLOCK;
}

/****************************************************************************
 * Drawing
 ****************************************************************************/

static void start_color_and_pairs(intf_thread_t *p_intf)
{
    if (p_intf->p_sys->b_color_started)
        return;

    p_intf->p_sys->b_color_started = true;

    if (!has_colors())
    {
        p_intf->p_sys->b_color = false;
        msg_Warn(p_intf, "Terminal doesn't support colors");
        return;
    }

    start_color();
    for(int i = C_DEFAULT + 1; i < C_MAX; i++)
        init_pair(i, color_pairs[i].f, color_pairs[i].b);

    /* untested, in all my terminals, !can_change_color() --funman */
    if (can_change_color())
        init_color(COLOR_YELLOW, 960, 500, 0); /* YELLOW -> ORANGE */

    p_intf->p_sys->b_color_started = true;
}

static void DrawBox(WINDOW *win, int y, int x, int h, int w, const char *title, bool b_color)
{
    int i_len;

    if (w <= 3 || h <= 2)
        return;

    if (b_color)
        wcolor_set(win, C_BOX, NULL);
    if (!title) title = "";
    i_len = strlen(title);

    if (i_len > w - 2)
        i_len = w - 2;

    mvwaddch(win, y, x,    ACS_ULCORNER);
    mvwhline(win, y, x+1,  ACS_HLINE, (w-i_len-2)/2);
    mvwprintw(win,y, x+1+(w-i_len-2)/2, "%s", title);
    mvwhline(win, y, x+(w-i_len)/2+i_len,  ACS_HLINE, w - 1 - ((w-i_len)/2+i_len));
    mvwaddch(win, y, x+w-1,ACS_URCORNER);

    for(int i = 0; i < h-2; i++)
    {
        mvwaddch(win, y+i+1, x,     ACS_VLINE);
        mvwaddch(win, y+i+1, x+w-1, ACS_VLINE);
    }

    mvwaddch(win, y+h-1, x,     ACS_LLCORNER);
    mvwhline(win, y+h-1, x+1,   ACS_HLINE, w - 2);
    mvwaddch(win, y+h-1, x+w-1, ACS_LRCORNER);
    if (b_color)
        wcolor_set(win, C_DEFAULT, NULL);
}

static void DrawEmptyLine(WINDOW *win, int y, int x, int w)
{
    if (w <= 0) return;

    mvwhline(win, y, x, ' ', w);
}

static void DrawLine(WINDOW *win, int y, int x, int w)
{
    if (w <= 0) return;

    attrset(A_REVERSE);
    mvwhline(win, y, x, ' ', w);
    attroff(A_REVERSE);
}

static void mvnprintw(int y, int x, int w, const char *p_fmt, ...)
{
    va_list  vl_args;
    char    *p_buf;
    int      i_len;

    if (w <= 0)
        return;

    va_start(vl_args, p_fmt);
    if (vasprintf(&p_buf, p_fmt, vl_args) == -1)
        return;
    va_end(vl_args);

    i_len = strlen(p_buf);

    wchar_t psz_wide[i_len + 1];

    EnsureUTF8(p_buf);
    size_t i_char_len = mbstowcs(psz_wide, p_buf, i_len);

    size_t i_width; /* number of columns */

    if (i_char_len == (size_t)-1) /* an invalid character was encountered */
    {
        free(p_buf);
        return;
    }

    i_width = wcswidth(psz_wide, i_char_len);
    if (i_width == (size_t)-1)
    {
        /* a non printable character was encountered */
        i_width = 0;
        for(unsigned i = 0 ; i < i_char_len ; i++)
        {
            int i_cwidth = wcwidth(psz_wide[i]);
            if (i_cwidth != -1)
                i_width += i_cwidth;
        }
    }

    if (i_width <= (size_t)w)
    {
        mvprintw(y, x, "%s", p_buf);
        mvhline(y, x + i_width, ' ', w - i_width);
        free(p_buf);
        return;
    }

    int i_total_width = 0;
    int i = 0;
    while (i_total_width < w)
    {
        i_total_width += wcwidth(psz_wide[i]);
        if (w > 7 && i_total_width >= w/2)
        {
            psz_wide[i  ] = '.';
            psz_wide[i+1] = '.';
            i_total_width -= wcwidth(psz_wide[i]) - 2;
            if (i > 0)
            {
                /* we require this check only if at least one character
                 * 4 or more columns wide exists (which i doubt) */
                psz_wide[i-1] = '.';
                i_total_width -= wcwidth(psz_wide[i-1]) - 1;
            }

            /* find the widest string */
            int j, i_2nd_width = 0;
            for(j = i_char_len - 1; i_2nd_width < w - i_total_width; j--)
                i_2nd_width += wcwidth(psz_wide[j]);

            /* we already have i_total_width columns filled, and we can't
             * have more than w columns */
            if (i_2nd_width > w - i_total_width)
                j++;

            wmemmove(&psz_wide[i+2], &psz_wide[j+1], i_char_len - j - 1);
            psz_wide[i + 2 + i_char_len - j - 1] = '\0';
            break;
        }
        i++;
    }
    if (w <= 7) /* we don't add the '...' else we lose too much chars */
        psz_wide[i] = '\0';

    size_t i_wlen = wcslen(psz_wide) * 6 + 1; /* worst case */
    char psz_ellipsized[i_wlen];
    wcstombs(psz_ellipsized, psz_wide, i_wlen);
    mvprintw(y, x, "%s", psz_ellipsized);

    free(p_buf);
}

static void MainBoxWrite(intf_thread_t *p_intf, int l, int x, const char *p_fmt, ...)
{
    intf_sys_t  *p_sys = p_intf->p_sys;
    va_list      vl_args;
    char        *p_buf;

    if (l < p_sys->i_box_start || l - p_sys->i_box_start >= p_sys->i_box_lines)
        return;

    va_start(vl_args, p_fmt);
    if (vasprintf(&p_buf, p_fmt, vl_args) == -1)
        return;
    va_end(vl_args);

    mvnprintw(p_sys->i_box_y + l - p_sys->i_box_start, x, COLS - x - 1, "%s", p_buf);
    free(p_buf);
}

static void DumpObject(intf_thread_t *p_intf, int *l, vlc_object_t *p_obj, int i_level)
{
    char *psz_name = vlc_object_get_name(p_obj);
    if (psz_name)
    {
        MainBoxWrite(p_intf, (*l)++, 1 + 2 * i_level, "%s \"%s\" (%p)",
                p_obj->psz_object_type, psz_name, p_obj);
        free(psz_name);
    }
    else
        MainBoxWrite(p_intf, (*l)++, 1 + 2 * i_level, "%s (%o)",
                p_obj->psz_object_type, p_obj);

    vlc_list_t *list = vlc_list_children(p_obj);
    for(int i = 0; i < list->i_count ; i++)
    {
        MainBoxWrite(p_intf, *l, 1 + 2 * i_level,
            i == list->i_count - 1 ? "`-" : "|-");
        DumpObject(p_intf, l, list->p_values[i].p_object, i_level + 1);
    }
    vlc_list_release(list);
}

static void Redraw(intf_thread_t *p_intf, time_t *t_last_refresh)
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    playlist_t     *p_playlist = pl_Get(p_intf);
    int y = 0;
    int h;
    int y_end;

    /* Title */
    attrset(A_REVERSE);
    int i_len = sizeof "VLC media player "PACKAGE_VERSION - 1;
    int mid = (COLS - i_len) / 2;
    if (mid < 0)
        mid = 0;
    int i_size = (COLS > i_len + 1) ? COLS : i_len + 1;
    char psz_title[i_size];
    memset(psz_title, ' ', mid);
    if (p_sys->b_color)
        wcolor_set(p_sys->w, C_TITLE, NULL);
    strlcpy(&psz_title[mid], "VLC media player "PACKAGE_VERSION, i_size);
    mvnprintw(y, 0, COLS, "%s", psz_title);
    attroff(A_REVERSE);
    y += 2;

    if (p_sys->b_color)
        wcolor_set(p_sys->w, C_STATUS, NULL);

    /* Infos */
    char *psz_state;
    if (asprintf(&psz_state, "%s%s%s",
            var_GetBool(p_playlist, "repeat") ? _("[Repeat] ") : "",
            var_GetBool(p_playlist, "random") ? _("[Random] ") : "",
            var_GetBool(p_playlist, "loop") ? _("[Loop]") : "") == -1)
        psz_state = NULL;

    if (p_input && !p_input->b_dead)
    {
        char buf1[MSTRTIME_MAX_SIZE];
        char buf2[MSTRTIME_MAX_SIZE];
        vlc_value_t val;

        /* Source */
        char *psz_uri = input_item_GetURI(input_GetItem(p_input));
        mvnprintw(y++, 0, COLS, _(" Source   : %s"), psz_uri);
        free(psz_uri);

        /* State */
        var_Get(p_input, "state", &val);
        if (val.i_int == PLAYING_S)
            mvnprintw(y++, 0, COLS, _(" State    : Playing %s"), psz_state);
        else if (val.i_int == OPENING_S)
            mvnprintw(y++, 0, COLS, _(" State    : Opening/Connecting %s"), psz_state);
        else if (val.i_int == PAUSE_S)
            mvnprintw(y++, 0, COLS, _(" State    : Paused %s"), psz_state);

        if (val.i_int == INIT_S || val.i_int == END_S)
            y += 2;
        else
        {
            audio_volume_t i_volume;

            /* Position */
            var_Get(p_input, "time", &val);
            secstotimestr(buf1, val.i_time / CLOCK_FREQ);

            var_Get(p_input, "length", &val);
            secstotimestr(buf2, val.i_time / CLOCK_FREQ);

            mvnprintw(y++, 0, COLS, _(" Position : %s/%s"), buf1, buf2);

            /* Volume */
            aout_VolumeGet(p_playlist, &i_volume);
            mvnprintw(y++, 0, COLS, _(" Volume   : %i%%"), i_volume*200/AOUT_VOLUME_MAX);

            /* Title */
            if (!var_Get(p_input, "title", &val))
            {
                int i_title_count = var_CountChoices(p_input, "title");
                if (i_title_count > 0)
                    mvnprintw(y++, 0, COLS, _(" Title    : %"PRId64"/%d"),
                               val.i_int, i_title_count);
            }

            /* Chapter */
            if (!var_Get(p_input, "chapter", &val))
            {
                int i_chapter_count = var_CountChoices(p_input, "chapter");
                if (i_chapter_count > 0)
                    mvnprintw(y++, 0, COLS, _(" Chapter  : %"PRId64"/%d"),
                               val.i_int, i_chapter_count);
            }
        }
    }
    else
    {
        mvnprintw(y++, 0, COLS, _(" Source: <no current item> %s"), psz_state);
        DrawEmptyLine(p_sys->w, y++, 0, COLS);
        mvnprintw(y++, 0, COLS, _(" [ h for help ]"));
        DrawEmptyLine(p_sys->w, y++, 0, COLS);
    }
    free(psz_state);
    if (p_sys->b_color)
        wcolor_set(p_sys->w, C_DEFAULT, NULL);

    DrawBox(p_sys->w, y, 0, 3, COLS, "", p_sys->b_color);
    DrawEmptyLine(p_sys->w, y+1, 1, COLS-2);

    if (p_input && var_GetInteger(p_input, "state") == PLAYING_S)
    {
        float pos = var_GetFloat(p_input, "position");
        DrawLine(p_sys->w, y+1, 1, (int)(pos * (COLS-2)));
    }

    y += 3;

    p_sys->i_box_y = y + 1;
    p_sys->i_box_lines = LINES - y - 2;

    h = LINES - y;
    y_end = y + h - 1;

    if (p_sys->i_box_type == BOX_HELP)
    {
        /* Help box */
        int l = 0;
        DrawBox(p_sys->w, y++, 0, h, COLS, _(" Help "), p_sys->b_color);

        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_CATEGORY, NULL);
        MainBoxWrite(p_intf, l++, 1, _("[Display]"));
        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_DEFAULT, NULL);
        MainBoxWrite(p_intf, l++, 1, _("     h,H         Show/Hide help box"));
        MainBoxWrite(p_intf, l++, 1, _("     i           Show/Hide info box"));
        MainBoxWrite(p_intf, l++, 1, _("     m           Show/Hide metadata box"));
//      MainBoxWrite(p_intf, l++, 1, _("     L           Show/Hide messages box"));
        MainBoxWrite(p_intf, l++, 1, _("     P           Show/Hide playlist box"));
        MainBoxWrite(p_intf, l++, 1, _("     B           Show/Hide filebrowser"));
        MainBoxWrite(p_intf, l++, 1, _("     x           Show/Hide objects box"));
        MainBoxWrite(p_intf, l++, 1, _("     S           Show/Hide statistics box"));
        MainBoxWrite(p_intf, l++, 1, _("     c           Switch color on/off"));
        MainBoxWrite(p_intf, l++, 1, _("     Esc         Close Add/Search entry"));
        MainBoxWrite(p_intf, l++, 1, "");

        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_CATEGORY, NULL);
        MainBoxWrite(p_intf, l++, 1, _("[Global]"));
        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_DEFAULT, NULL);
        MainBoxWrite(p_intf, l++, 1, _("     q, Q, Esc   Quit"));
        MainBoxWrite(p_intf, l++, 1, _("     s           Stop"));
        MainBoxWrite(p_intf, l++, 1, _("     <space>     Pause/Play"));
        MainBoxWrite(p_intf, l++, 1, _("     f           Toggle Fullscreen"));
        MainBoxWrite(p_intf, l++, 1, _("     n, p        Next/Previous playlist item"));
        MainBoxWrite(p_intf, l++, 1, _("     [, ]        Next/Previous title"));
        MainBoxWrite(p_intf, l++, 1, _("     <, >        Next/Previous chapter"));
        MainBoxWrite(p_intf, l++, 1, _("     <right>     Seek +1%%"));
        MainBoxWrite(p_intf, l++, 1, _("     <left>      Seek -1%%"));
        MainBoxWrite(p_intf, l++, 1, _("     a           Volume Up"));
        MainBoxWrite(p_intf, l++, 1, _("     z           Volume Down"));
        MainBoxWrite(p_intf, l++, 1, "");

        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_CATEGORY, NULL);
        MainBoxWrite(p_intf, l++, 1, _("[Playlist]"));
        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_DEFAULT, NULL);
        MainBoxWrite(p_intf, l++, 1, _("     r           Toggle Random playing"));
        MainBoxWrite(p_intf, l++, 1, _("     l           Toggle Loop Playlist"));
        MainBoxWrite(p_intf, l++, 1, _("     R           Toggle Repeat item"));
        MainBoxWrite(p_intf, l++, 1, _("     o           Order Playlist by title"));
        MainBoxWrite(p_intf, l++, 1, _("     O           Reverse order Playlist by title"));
        MainBoxWrite(p_intf, l++, 1, _("     g           Go to the current playing item"));
        MainBoxWrite(p_intf, l++, 1, _("     /           Look for an item"));
        MainBoxWrite(p_intf, l++, 1, _("     A           Add an entry"));
        MainBoxWrite(p_intf, l++, 1, _("     D, <del>    Delete an entry"));
        MainBoxWrite(p_intf, l++, 1, _("     <backspace> Delete an entry"));
        MainBoxWrite(p_intf, l++, 1, _("     e           Eject (if stopped)"));
        MainBoxWrite(p_intf, l++, 1, "");

        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_CATEGORY, NULL);
        MainBoxWrite(p_intf, l++, 1, _("[Filebrowser]"));
        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_DEFAULT, NULL);
        MainBoxWrite(p_intf, l++, 1, _("     <enter>     Add the selected file to the playlist"));
        MainBoxWrite(p_intf, l++, 1, _("     <space>     Add the selected directory to the playlist"));
        MainBoxWrite(p_intf, l++, 1, _("     .           Show/Hide hidden files"));
        MainBoxWrite(p_intf, l++, 1, "");

        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_CATEGORY, NULL);
        MainBoxWrite(p_intf, l++, 1, _("[Boxes]"));
        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_DEFAULT, NULL);
        MainBoxWrite(p_intf, l++, 1, _("     <up>,<down>     Navigate through the box line by line"));
        MainBoxWrite(p_intf, l++, 1, _("     <pgup>,<pgdown> Navigate through the box page by page"));
        MainBoxWrite(p_intf, l++, 1, "");

        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_CATEGORY, NULL);
        MainBoxWrite(p_intf, l++, 1, _("[Player]"));
        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_DEFAULT, NULL);
        MainBoxWrite(p_intf, l++, 1, _("     <up>,<down>     Seek +/-5%%"));
        MainBoxWrite(p_intf, l++, 1, "");

        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_CATEGORY, NULL);
        MainBoxWrite(p_intf, l++, 1, _("[Miscellaneous]"));
        if (p_sys->b_color)
            wcolor_set(p_sys->w, C_DEFAULT, NULL);
        MainBoxWrite(p_intf, l++, 1, _("     Ctrl-l          Refresh the screen"));

        p_sys->i_box_lines_total = l;
        if (p_sys->i_box_start >= p_sys->i_box_lines_total)
            p_sys->i_box_start = p_sys->i_box_lines_total - 1;

        if (l - p_sys->i_box_start < p_sys->i_box_lines)
            y += l - p_sys->i_box_start;
        else
            y += p_sys->i_box_lines;
    }
    else if (p_sys->i_box_type == BOX_INFO)
    {
        /* Info box */
        int l = 0;
        DrawBox(p_sys->w, y++, 0, h, COLS, _(" Information "), p_sys->b_color);

        if (p_input)
        {
            int i,j;
            vlc_mutex_lock(&input_GetItem(p_input)->lock);
            for(i = 0; i < input_GetItem(p_input)->i_categories; i++)
            {
                info_category_t *p_category = input_GetItem(p_input)->pp_categories[i];
                if (y >= y_end) break;
                if (p_sys->b_color)
                    wcolor_set(p_sys->w, C_CATEGORY, NULL);
                MainBoxWrite(p_intf, l++, 1, _("  [%s]"), p_category->psz_name);
                if (p_sys->b_color)
                    wcolor_set(p_sys->w, C_DEFAULT, NULL);
                for(j = 0; j < p_category->i_infos; j++)
                {
                    info_t *p_info = p_category->pp_infos[j];
                    if (y >= y_end) break;
                    MainBoxWrite(p_intf, l++, 1, _("      %s: %s"), p_info->psz_name, p_info->psz_value);
                }
            }
            vlc_mutex_unlock(&input_GetItem(p_input)->lock);
        }
        else
            MainBoxWrite(p_intf, l++, 1, _("No item currently playing"));

        p_sys->i_box_lines_total = l;
        if (p_sys->i_box_start >= p_sys->i_box_lines_total)
            p_sys->i_box_start = p_sys->i_box_lines_total - 1;

        if (l - p_sys->i_box_start < p_sys->i_box_lines)
            y += l - p_sys->i_box_start;
        else
            y += p_sys->i_box_lines;
    }
    else if (p_sys->i_box_type == BOX_META)
    {
        /* Meta data box */
        int l = 0;

        DrawBox(p_sys->w, y++, 0, h, COLS, _("Meta-information"),
                 p_sys->b_color);

        if (p_input)
        {
            input_item_t *p_item = input_GetItem(p_input);
            vlc_mutex_lock(&p_item->lock);
            for(int i=0; i<VLC_META_TYPE_COUNT; i++)
            {
                const char *psz_meta = vlc_meta_Get(p_item->p_meta, i);
                if (!psz_meta || !*psz_meta)
                    continue;

                if (p_sys->b_color) wcolor_set(p_sys->w, C_CATEGORY, NULL);
                MainBoxWrite(p_intf, l++, 1, "  [%s]",
                             vlc_meta_TypeToLocalizedString(i));
                if (p_sys->b_color) wcolor_set(p_sys->w, C_DEFAULT, NULL);
                MainBoxWrite(p_intf, l++, 1, "      %s", psz_meta);
            }
            vlc_mutex_unlock(&p_item->lock);
        }
        else
            MainBoxWrite(p_intf, l++, 1, _("No item currently playing"));

        p_sys->i_box_lines_total = l;
        if (p_sys->i_box_start >= p_sys->i_box_lines_total)
            p_sys->i_box_start = p_sys->i_box_lines_total - 1;

        y += __MIN(l - p_sys->i_box_start, p_sys->i_box_lines);
    }
#if 0 /* Deprecated API */
    else if (p_sys->i_box_type == BOX_LOG)
    {
        int i_line = 0;
        int i_stop;
        int i_start;

        DrawBox(p_sys->w, y++, 0, h, COLS, _(" Logs "), p_sys->b_color);


        i_start = p_intf->p_sys->p_sub->i_start;

        vlc_mutex_lock(p_intf->p_sys->p_sub->p_lock);
        i_stop = *p_intf->p_sys->p_sub->pi_stop;
        vlc_mutex_unlock(p_intf->p_sys->p_sub->p_lock);

        for(;;)
        {
            static const char *ppsz_type[4] = { "", "error", "warning", "debug" };
            if (i_line >= h - 2)
            {
                break;
            }
            i_stop--;
            i_line++;
            if (i_stop < 0) i_stop += VLC_MSG_QSIZE;
            if (i_stop == i_start)
            {
                break;
            }
            if (p_sys->b_color)
                wcolor_set(p_sys->w,
                    p_sys->p_sub->p_msg[i_stop].i_type + C_INFO,
                    NULL);
            mvnprintw(y + h-2-i_line, 1, COLS - 2, "   [%s] %s",
                      ppsz_type[p_sys->p_sub->p_msg[i_stop].i_type],
                      p_sys->p_sub->p_msg[i_stop].psz_msg);
            if (p_sys->b_color)
                wcolor_set(p_sys->w, C_DEFAULT, NULL);
        }

        vlc_mutex_lock(p_intf->p_sys->p_sub->p_lock);
        p_intf->p_sys->p_sub->i_start = i_stop;
        vlc_mutex_unlock(p_intf->p_sys->p_sub->p_lock);
        y = y_end;
    }
#endif
    else if (p_sys->i_box_type == BOX_BROWSE)
    {
        /* Filebrowser box */
        int        i_start, i_stop;
        int        i_item;
        DrawBox(p_sys->w, y++, 0, h, COLS, _(" Browse "), p_sys->b_color);

        if (p_sys->i_box_bidx >= p_sys->i_dir_entries) p_sys->i_box_plidx = p_sys->i_dir_entries - 1;
        if (p_sys->i_box_bidx < 0) p_sys->i_box_bidx = 0;

        if (p_sys->i_box_bidx < (h - 2)/2)
        {
            i_start = 0;
            i_stop = h - 2;
        }
        else if (p_sys->i_dir_entries - p_sys->i_box_bidx > (h - 2)/2)
        {
            i_start = p_sys->i_box_bidx - (h - 2)/2;
            i_stop = i_start + h - 2;
        }
        else
        {
            i_stop = p_sys->i_dir_entries;
            i_start = p_sys->i_dir_entries - (h - 2);
        }
        if (i_start < 0)
            i_start = 0;
        if (i_stop > p_sys->i_dir_entries)
            i_stop = p_sys->i_dir_entries;

        for(i_item = i_start; i_item < i_stop; i_item++)
        {
            bool b_selected = (p_sys->i_box_bidx == i_item);

            if (y >= y_end) break;
            if (b_selected)
                attrset(A_REVERSE);
            if (p_sys->b_color && !p_sys->pp_dir_entries[i_item]->b_file)
                wcolor_set(p_sys->w, C_FOLDER, NULL);
            mvnprintw(y++, 1, COLS - 2, " %c %s", p_sys->pp_dir_entries[i_item]->b_file == true ? ' ' : '+',
                            p_sys->pp_dir_entries[i_item]->psz_path);
            if (p_sys->b_color && !p_sys->pp_dir_entries[i_item]->b_file)
                wcolor_set(p_sys->w, C_DEFAULT, NULL);

            if (b_selected)
                attroff(A_REVERSE);
        }

    }
    else if (p_sys->i_box_type == BOX_OBJECTS)
    {
        int l = 0;
        DrawBox(p_sys->w, y++, 0, h, COLS, _(" Objects "), p_sys->b_color);
        DumpObject(p_intf, &l, VLC_OBJECT(p_intf->p_libvlc), 0);

        p_sys->i_box_lines_total = l;
        if (p_sys->i_box_start >= p_sys->i_box_lines_total)
            p_sys->i_box_start = p_sys->i_box_lines_total - 1;

        if (l - p_sys->i_box_start < p_sys->i_box_lines)
            y += l - p_sys->i_box_start;
        else
            y += p_sys->i_box_lines;
    }
    else if (p_sys->i_box_type == BOX_STATS)
    {
        DrawBox(p_sys->w, y++, 0, h, COLS, _(" Stats "), p_sys->b_color);

        if (p_input)
        {
            input_item_t *p_item = input_GetItem(p_input);
            assert(p_item);
            vlc_mutex_lock(&p_item->lock);
            vlc_mutex_lock(&p_item->p_stats->lock);

            int i_audio = 0;
            int i_video = 0;
            int i;

            if (!p_item->i_es)
                i_video = i_audio = 1;
            else
                for(i = 0; i < p_item->i_es ; i++)
                {
                    i_audio += (p_item->es[i]->i_cat == AUDIO_ES);
                    i_video += (p_item->es[i]->i_cat == VIDEO_ES);
                }

            int l = 0;

#define SHOW_ACS(x,c) \
    if (l >= p_sys->i_box_start && l - p_sys->i_box_start < p_sys->i_box_lines) \
        mvaddch(p_sys->i_box_y - p_sys->i_box_start + l, x, c)

            /* Input */
            if (p_sys->b_color) wcolor_set(p_sys->w, C_CATEGORY, NULL);
            MainBoxWrite(p_intf, l, 1, _("+-[Incoming]"));
            SHOW_ACS(1, ACS_ULCORNER);  SHOW_ACS(2, ACS_HLINE); l++;
            if (p_sys->b_color) wcolor_set(p_sys->w, C_DEFAULT, NULL);
            MainBoxWrite(p_intf, l, 1, _("| input bytes read : %8.0f KiB"),
                    (float)(p_item->p_stats->i_read_bytes)/1024);
            SHOW_ACS(1, ACS_VLINE); l++;
            MainBoxWrite(p_intf, l, 1, _("| input bitrate    :   %6.0f kb/s"),
                    (float)(p_item->p_stats->f_input_bitrate)*8000);
            MainBoxWrite(p_intf, l, 1, _("| demux bytes read : %8.0f KiB"),
                    (float)(p_item->p_stats->i_demux_read_bytes)/1024);
            SHOW_ACS(1, ACS_VLINE); l++;
            MainBoxWrite(p_intf, l, 1, _("| demux bitrate    :   %6.0f kb/s"),
                    (float)(p_item->p_stats->f_demux_bitrate)*8000);
            SHOW_ACS(1, ACS_VLINE); l++;
            DrawEmptyLine(p_sys->w, p_sys->i_box_y + l - p_sys->i_box_start, 1, COLS - 2);
            SHOW_ACS(1, ACS_VLINE); l++;

            /* Video */
            if (i_video)
            {
                if (p_sys->b_color) wcolor_set(p_sys->w, C_CATEGORY, NULL);
                MainBoxWrite(p_intf, l, 1, _("+-[Video Decoding]"));
                SHOW_ACS(1, ACS_LTEE);  SHOW_ACS(2, ACS_HLINE); l++;
                if (p_sys->b_color) wcolor_set(p_sys->w, C_DEFAULT, NULL);
                MainBoxWrite(p_intf, l, 1, _("| video decoded    :    %"PRId64),
                        p_item->p_stats->i_decoded_video);
                SHOW_ACS(1, ACS_VLINE); l++;
                MainBoxWrite(p_intf, l, 1, _("| frames displayed :    %"PRId64),
                        p_item->p_stats->i_displayed_pictures);
                SHOW_ACS(1, ACS_VLINE); l++;
                MainBoxWrite(p_intf, l, 1, _("| frames lost      :    %"PRId64),
                        p_item->p_stats->i_lost_pictures);
                SHOW_ACS(1, ACS_VLINE); l++;
                DrawEmptyLine(p_sys->w, p_sys->i_box_y + l - p_sys->i_box_start, 1, COLS - 2);
                SHOW_ACS(1, ACS_VLINE); l++;
            }
            /* Audio*/
            if (i_audio)
            {
                if (p_sys->b_color) wcolor_set(p_sys->w, C_CATEGORY, NULL);
                MainBoxWrite(p_intf, l, 1, _("+-[Audio Decoding]"));
                SHOW_ACS(1, ACS_LTEE);  SHOW_ACS(2, ACS_HLINE); l++;
                if (p_sys->b_color) wcolor_set(p_sys->w, C_DEFAULT, NULL);
                MainBoxWrite(p_intf, l, 1, _("| audio decoded    :    %"PRId64),
                        p_item->p_stats->i_decoded_audio);
                SHOW_ACS(1, ACS_VLINE); l++;
                MainBoxWrite(p_intf, l, 1, _("| buffers played   :    %"PRId64),
                        p_item->p_stats->i_played_abuffers);
                SHOW_ACS(1, ACS_VLINE); l++;
                MainBoxWrite(p_intf, l, 1, _("| buffers lost     :    %"PRId64),
                        p_item->p_stats->i_lost_abuffers);
                SHOW_ACS(1, ACS_VLINE); l++;
                DrawEmptyLine(p_sys->w, p_sys->i_box_y + l - p_sys->i_box_start, 1, COLS - 2);
                SHOW_ACS(1, ACS_VLINE); l++;
            }
            /* Sout */
            if (p_sys->b_color) wcolor_set(p_sys->w, C_CATEGORY, NULL);
            MainBoxWrite(p_intf, l, 1, _("+-[Streaming]"));
            SHOW_ACS(1, ACS_LTEE);  SHOW_ACS(2, ACS_HLINE); l++;
            if (p_sys->b_color) wcolor_set(p_sys->w, C_DEFAULT, NULL);
            MainBoxWrite(p_intf, l, 1, _("| packets sent     :    %5i"), p_item->p_stats->i_sent_packets);
            SHOW_ACS(1, ACS_VLINE); l++;
            MainBoxWrite(p_intf, l, 1, _("| bytes sent       : %8.0f KiB"),
                    (float)(p_item->p_stats->i_sent_bytes)/1024);
            SHOW_ACS(1, ACS_VLINE); l++;
            MainBoxWrite(p_intf, l, 1, _("\\ sending bitrate  :   %6.0f kb/s"),
                    (float)(p_item->p_stats->f_send_bitrate*8)*1000);
            SHOW_ACS(1, ACS_LLCORNER); l++;
            if (p_sys->b_color) wcolor_set(p_sys->w, C_DEFAULT, NULL);

#undef SHOW_ACS

            p_sys->i_box_lines_total = l;
            if (p_sys->i_box_start >= p_sys->i_box_lines_total)
                p_sys->i_box_start = p_sys->i_box_lines_total - 1;

            if (l - p_sys->i_box_start < p_sys->i_box_lines)
                y += l - p_sys->i_box_start;
            else
                y += p_sys->i_box_lines;

            vlc_mutex_unlock(&p_item->p_stats->lock);
            vlc_mutex_unlock(&p_item->lock);

        }
    }
    else if (p_sys->i_box_type == BOX_PLAYLIST ||
               p_sys->i_box_type == BOX_SEARCH ||
               p_sys->i_box_type == BOX_OPEN )
    {
        /* Playlist box */
        int        i_start, i_stop, i_max = p_sys->i_plist_entries;
        int        i_item;
        char       *psz_title;

        if (p_sys->category_view)
            psz_title = strdup(_(" Playlist (By category) "));
        else
            psz_title = strdup(_(" Playlist (All, one level) "));

        DrawBox(p_sys->w, y++, 0, h, COLS, psz_title, p_sys->b_color);
        free(psz_title);

        if (p_sys->b_need_update || !p_sys->pp_plist)
            PlaylistRebuild(p_intf);
        if (p_sys->b_box_plidx_follow)
            FindIndex(p_sys, p_playlist, false);

        if (p_sys->i_box_plidx < 0) p_sys->i_box_plidx = 0;
        if (p_sys->i_box_plidx >= i_max) p_sys->i_box_plidx = i_max - 1;

        if (p_sys->i_box_plidx < (h - 2)/2)
        {
            i_start = 0;
            i_stop = h - 2;
        }
        else if (i_max - p_sys->i_box_plidx > (h - 2)/2)
        {
            i_start = p_sys->i_box_plidx - (h - 2)/2;
            i_stop = i_start + h - 2;
        }
        else
        {
            i_stop = i_max;
            i_start = i_max - (h - 2);
        }
        if (i_start < 0)
            i_start = 0;
        if (i_stop > i_max)
            i_stop = i_max;

        for(i_item = i_start; i_item < i_stop; i_item++)
        {
            bool b_selected = (p_sys->i_box_plidx == i_item);
            playlist_item_t *p_item = p_sys->pp_plist[i_item]->p_item;
            playlist_item_t *p_node = p_sys->p_node;
            int c = ' ';
            input_thread_t *p_input2 = playlist_CurrentInput(p_playlist);

            PL_LOCK;
            assert(p_item);
            playlist_item_t *p_current_playing_item = playlist_CurrentPlayingItem(p_playlist);
            if ((p_node && p_item->p_input == p_node->p_input) ||
                        (!p_node && p_input2 && p_current_playing_item &&
                          p_item->p_input == p_current_playing_item->p_input))
                c = '*';
            else if (p_item == p_node || (p_item != p_node &&
                        PlaylistIsPlaying(p_playlist, p_item)))
                c = '>';
            PL_UNLOCK;

            if (p_input2)
                vlc_object_release(p_input2);

            if (y >= y_end) break;
            if (b_selected)
                attrset(A_REVERSE);
            if (p_sys->b_color)
                wcolor_set(p_sys->w, i_item % 3 + C_PLAYLIST_1, NULL);
            mvnprintw(y++, 1, COLS - 2, "%c%s", c,
                       p_sys->pp_plist[i_item]->psz_display);
            if (p_sys->b_color)
                wcolor_set(p_sys->w, C_DEFAULT, NULL);
            if (b_selected)
                attroff(A_REVERSE);
        }

    }
    else
        y++;

    if (p_sys->i_box_type == BOX_SEARCH)
    {
        DrawEmptyLine(p_sys->w, 7, 1, COLS-2);
        mvnprintw(7, 1, COLS-2, _("Find: %s"), *p_sys->psz_search_chain ?
                  p_sys->psz_search_chain : p_sys->psz_old_search);
    }
    if (p_sys->i_box_type == BOX_OPEN)
    {
        DrawEmptyLine(p_sys->w, 7, 1, COLS-2);
        mvnprintw(7, 1, COLS-2, _("Open: %s"), p_sys->psz_open_chain);
    }

    while (y < y_end)
        DrawEmptyLine(p_sys->w, y++, 1, COLS - 2);

    refresh();

    *t_last_refresh = time(0);
}

static void ChangePosition(intf_thread_t *p_intf, float increment)
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    float pos;

    if (!p_input || var_GetInteger(p_input, "state") != PLAYING_S)
        return;

    pos = var_GetFloat(p_input, "position") + increment;

    if (pos > 0.99) pos = 0.99;
    if (pos < 0.0)  pos = 0.0;

    var_SetFloat(p_input, "position", pos);
}

static inline int RemoveLastUTF8Entity(char *psz, int len)
{
    while (len && ((psz[--len] & 0xc0) == 0x80));
                       /* UTF8 continuation byte */

    psz[len] = '\0';
    return len;
}

static char *GetDiscDevice(intf_thread_t *p_intf, const char *name)
{
    static const struct { const char *s; size_t n; const char *v; } devs[] =
    {
        { "cdda://", 7, "cd-audio", },
        { "dvd://",  6, "dvd",      },
        { "vcd://",  6, "vcd",      },
    };
    char *device;

    for (unsigned i = 0; i < sizeof devs / sizeof *devs; i++)
    {
        size_t n = devs[i].n;
        if (!strncmp(name, devs[i].s, n))
            switch(name[n])
            {
            case '\0':
            case '@':
                return config_GetPsz(p_intf, devs[i].v);
            default:
                /* Omit the beginning MRL-selector characters */
                return strdup(name + n);
            }
    }

    device = strdup(name);

    if (device) /* Remove what we have after @ */
        device[strcspn(device, "@")] = '\0';

    return device;
}

static void Eject(intf_thread_t *p_intf)
{
    char *psz_device, *psz_name;
    playlist_t * p_playlist = pl_Get(p_intf);

    /* If there's a stream playing, we aren't allowed to eject ! */
    if (p_intf->p_sys->p_input)
        return;

    PL_LOCK;

    if (!playlist_CurrentPlayingItem(p_playlist))
    {
        PL_UNLOCK;
        return;
    }

    psz_name = playlist_CurrentPlayingItem(p_playlist)->p_input->psz_name;

    if (psz_name)
        psz_device = GetDiscDevice(p_intf, psz_name);

    PL_UNLOCK;

    if (psz_device)
    {
        intf_Eject(p_intf, psz_device);
        free(psz_device);
    }
}

static void PlayPause(intf_thread_t *p_intf)
{
    input_thread_t *p_input = p_intf->p_sys->p_input;

    if (p_input)
    {
        int64_t state = var_GetInteger( p_input, "state" );
        state = (state != PLAYING_S) ? PLAYING_S : PAUSE_S;
        var_SetInteger( p_input, "state", state );
    }
    else
        playlist_Play(pl_Get(p_intf));
}

static inline void BoxSwitch(intf_sys_t *p_sys, int box)
{
    p_sys->i_box_type = (p_sys->i_box_type == box) ? BOX_NONE : box;
}

static int HandleKey(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = pl_Get(p_intf);
    int i_key = wgetch(p_sys->w);

    if (i_key == -1)
        return 0;

    if (p_sys->i_box_type == BOX_PLAYLIST)
    {
        int b_ret = true;
        bool b_box_plidx_follow = false;

        switch(i_key)
        {
        /* Playlist Settings */
        case 'r':
            var_ToggleBool(p_playlist, "random");
            return 1;
        case 'l':
            var_ToggleBool(p_playlist, "loop");
            return 1;
        case 'R':
            var_ToggleBool(p_playlist, "repeat");
            return 1;

        /* Playlist sort */
        case 'o':
            playlist_RecursiveNodeSort(p_playlist, PlaylistGetRoot(p_intf),
                                        SORT_TITLE_NODES_FIRST, ORDER_NORMAL);
            p_sys->b_need_update = true;
            return 1;
        case 'O':
            playlist_RecursiveNodeSort(p_playlist, PlaylistGetRoot(p_intf),
                                        SORT_TITLE_NODES_FIRST, ORDER_REVERSE);
            p_sys->b_need_update = true;
            return 1;

        /* Playlist view */
        case 'v':
            p_sys->category_view = !p_sys->category_view;
            PlaylistRebuild(p_intf);
            return 1;

        /* Playlist navigation */
        case 'g':
            FindIndex(p_sys, p_playlist, false);
            break;
        case KEY_HOME:
            p_sys->i_box_plidx = 0;
            break;
#ifdef __FreeBSD__
/* workaround for FreeBSD + xterm:
* see http://www.nabble.com/curses-vs.-xterm-key-mismatch-t3574377.html */
        case KEY_SELECT:
#endif
        case KEY_END:
            p_sys->i_box_plidx = p_playlist->items.i_size - 1;
            break;
        case KEY_UP:
            p_sys->i_box_plidx--;
            break;
        case KEY_DOWN:
            p_sys->i_box_plidx++;
            break;
        case KEY_PPAGE:
            p_sys->i_box_plidx -= p_sys->i_box_lines;
            break;
        case KEY_NPAGE:
            p_sys->i_box_plidx += p_sys->i_box_lines;
            break;
        case 'D':
        case KEY_BACKSPACE:
        case 0x7f:
        case KEY_DC:
        {
            playlist_item_t *p_item;

            PL_LOCK;
            p_item = p_sys->pp_plist[p_sys->i_box_plidx]->p_item;
            if (p_item->i_children == -1)
                playlist_DeleteFromInput(p_playlist, p_item->p_input, pl_Locked);
            else
                playlist_NodeDelete(p_playlist, p_item, true , false);
            PL_UNLOCK;
            PlaylistRebuild(p_intf);
            break;
        }

        case KEY_ENTER:
        case '\r':
        case '\n':
            if (!p_sys->pp_plist[p_sys->i_box_plidx])
            {
                b_ret = false;
                break;
            }
            if (p_sys->pp_plist[p_sys->i_box_plidx]->p_item->i_children == -1)
            {
                playlist_item_t *p_item, *p_parent;
                p_item = p_parent = p_sys->pp_plist[p_sys->i_box_plidx]->p_item;

                if (!p_parent)
                    p_parent = p_playlist->p_root_onelevel;
                while (p_parent->p_parent)
                    p_parent = p_parent->p_parent;
                playlist_Control(p_playlist, PLAYLIST_VIEWPLAY, pl_Unlocked,
                                  p_parent, p_item);
            }
            else if (!p_sys->pp_plist[p_sys->i_box_plidx]->p_item->i_children)
            {   /* We only want to set the current node */
                playlist_Stop(p_playlist);
                p_sys->p_node = p_sys->pp_plist[p_sys->i_box_plidx]->p_item;
            }
            else
            {
                p_sys->p_node = p_sys->pp_plist[p_sys->i_box_plidx]->p_item;
                playlist_Control(p_playlist, PLAYLIST_VIEWPLAY, pl_Unlocked,
                    p_sys->pp_plist[p_sys->i_box_plidx]->p_item, NULL);
            }
            b_box_plidx_follow = true;
            break;
        default:
            b_ret = false;
            break;
        }

        if (b_ret)
        {
            int i_max = p_sys->i_plist_entries;
            if (p_sys->i_box_plidx >= i_max) p_sys->i_box_plidx = i_max - 1;
            if (p_sys->i_box_plidx < 0) p_sys->i_box_plidx = 0;

            PL_LOCK;
            if (PlaylistIsPlaying(p_playlist,
                                   p_sys->pp_plist[p_sys->i_box_plidx]->p_item))
                b_box_plidx_follow = true;
            PL_UNLOCK;
            p_sys->b_box_plidx_follow = b_box_plidx_follow;
            return 1;
        }
    }
    else if (p_sys->i_box_type == BOX_BROWSE)
    {
        bool b_ret = true;
        /* Browser navigation */
        switch(i_key)
        {
        case KEY_HOME:
            p_sys->i_box_bidx = 0;
            break;
#ifdef __FreeBSD__
        case KEY_SELECT:
#endif
        case KEY_END:
            p_sys->i_box_bidx = p_sys->i_dir_entries - 1;
            break;
        case KEY_UP:
            p_sys->i_box_bidx--;
            break;
        case KEY_DOWN:
            p_sys->i_box_bidx++;
            break;
        case KEY_PPAGE:
            p_sys->i_box_bidx -= p_sys->i_box_lines;
            break;
        case KEY_NPAGE:
            p_sys->i_box_bidx += p_sys->i_box_lines;
            break;
        case '.': /* Toggle show hidden files */
            p_sys->b_show_hidden_files = !p_sys->b_show_hidden_files;
            ReadDir(p_intf);
            break;

        case KEY_ENTER:
        case '\r':
        case '\n':
        case ' ':
            if (p_sys->pp_dir_entries[p_sys->i_box_bidx]->b_file || i_key == ' ')
            {
                char* psz_uri;
                if (asprintf(&psz_uri, "%s://%s/%s",
                    p_sys->pp_dir_entries[p_sys->i_box_bidx]->b_file ?
                        "file" : "directory",
                    p_sys->psz_current_dir,
                    p_sys->pp_dir_entries[p_sys->i_box_bidx]->psz_path
                   ) == -1)
                {
                    psz_uri = NULL;
                }

                playlist_item_t *p_parent = p_sys->p_node;
                if (!p_parent)
                {
                    playlist_item_t *p_item;
                    PL_LOCK;
                    p_item = playlist_CurrentPlayingItem(p_playlist);
                    p_parent = p_item ? p_item->p_parent : NULL;
                    PL_UNLOCK;
                    if (!p_parent)
                        p_parent = p_playlist->p_local_onelevel;
                }

                while (p_parent->p_parent && p_parent->p_parent->p_parent)
                    p_parent = p_parent->p_parent;

                playlist_Add(p_playlist, psz_uri, NULL, PLAYLIST_APPEND,
                              PLAYLIST_END,
                              p_parent->p_input ==
                                p_playlist->p_local_onelevel->p_input
                              , false);

                p_sys->i_box_type = BOX_PLAYLIST;
                free(psz_uri);
            }
            else
            {
                if (asprintf(&(p_sys->psz_current_dir), "%s/%s", p_sys->psz_current_dir,
                              p_sys->pp_dir_entries[p_sys->i_box_bidx]->psz_path) != -1)
                    ReadDir(p_intf);
            }
            break;
        default:
            b_ret = false;
            break;
        }
        if (b_ret)
        {
            if (p_sys->i_box_bidx >= p_sys->i_dir_entries) p_sys->i_box_bidx = p_sys->i_dir_entries - 1;
            if (p_sys->i_box_bidx < 0) p_sys->i_box_bidx = 0;
            return 1;
        }
    }
    else if (p_sys->i_box_type == BOX_HELP || p_sys->i_box_type == BOX_INFO ||
             p_sys->i_box_type == BOX_META || p_sys->i_box_type == BOX_STATS ||
             p_sys->i_box_type == BOX_OBJECTS)
    {
        switch(i_key)
        {
        case KEY_HOME:
            p_sys->i_box_start = 0;
            return 1;
#ifdef __FreeBSD__
        case KEY_SELECT:
#endif
        case KEY_END:
            p_sys->i_box_start = p_sys->i_box_lines_total - 1;
            return 1;
        case KEY_UP:
            if (p_sys->i_box_start > 0) p_sys->i_box_start--;
            return 1;
        case KEY_DOWN:
            if (p_sys->i_box_start < p_sys->i_box_lines_total - 1)
                p_sys->i_box_start++;
            return 1;
        case KEY_PPAGE:
            p_sys->i_box_start -= p_sys->i_box_lines;
            if (p_sys->i_box_start < 0) p_sys->i_box_start = 0;
            return 1;
        case KEY_NPAGE:
            p_sys->i_box_start += p_sys->i_box_lines;
            if (p_sys->i_box_start >= p_sys->i_box_lines_total)
                p_sys->i_box_start = p_sys->i_box_lines_total - 1;
            return 1;
        default:
            break;
        }
    }
    else if (p_sys->i_box_type == BOX_NONE)
    {
        switch(i_key)
        {
        case KEY_HOME:
            ChangePosition(p_intf, -1.0);
            return 1;
#ifdef __FreeBSD__
        case KEY_SELECT:
#endif
        case KEY_END:
            ChangePosition(p_intf, .99);
            return 1;
        case KEY_UP:
            ChangePosition(p_intf, 0.05);
            return 1;
        case KEY_DOWN:
            ChangePosition(p_intf, -0.05);
            return 1;

        default:
            break;
        }
    }
    else if (p_sys->i_box_type == BOX_SEARCH)
    {
        size_t i_chain_len = strlen(p_sys->psz_search_chain);
        switch(i_key)
        {
        case KEY_CLEAR:
        case 0x0c:      /* ^l */
            clear();
            return 1;
        case KEY_ENTER:
        case '\r':
        case '\n':
            if (i_chain_len)
                p_sys->psz_old_search = strdup(p_sys->psz_search_chain);
            else if (p_sys->psz_old_search)
                SearchPlaylist(p_sys, p_sys->psz_old_search);
            p_sys->i_box_type = BOX_PLAYLIST;
            return 1;
        case 0x1b: /* ESC */
            /* Alt+key combinations return 2 keys in the terminal keyboard:
             * ESC, and the 2nd key.
             * If some other key is available immediately (where immediately
             * means after wgetch() 1 second delay), that means that the
             * ESC key was not pressed.
             *
             * man 3X curs_getch says:
             *
             * Use of the escape key by a programmer for a single
             * character function is discouraged, as it will cause a delay
             * of up to one second while the keypad code looks for a
             * following function-key sequence.
             *
             */
            if (wgetch(p_sys->w) != ERR)
                return 0;
            p_sys->i_box_plidx = p_sys->i_before_search;
            p_sys->i_box_type = BOX_PLAYLIST;
            return 1;
        case KEY_BACKSPACE:
        case 0x7f:
            RemoveLastUTF8Entity(p_sys->psz_search_chain, i_chain_len);
            break;
        default:
            if (i_chain_len + 1 < sizeof p_sys->psz_search_chain)
            {
                p_sys->psz_search_chain[i_chain_len] = (char) i_key;
                p_sys->psz_search_chain[i_chain_len + 1] = '\0';
            }
        }
        free(p_sys->psz_old_search);
        p_sys->psz_old_search = NULL;
        SearchPlaylist(p_sys, p_sys->psz_search_chain);
        return 1;
    }
    else if (p_sys->i_box_type == BOX_OPEN)
    {
        size_t i_chain_len = strlen(p_sys->psz_open_chain);

        switch(i_key)
        {
        case KEY_CLEAR:
        case 0x0c:          /* ^l */
            clear();
            return 1;
        case KEY_ENTER:
        case '\r':
        case '\n':
            if (i_chain_len)
            {
                playlist_item_t *p_parent = p_sys->p_node;

                PL_LOCK;
                if (!p_parent)
                p_parent = playlist_CurrentPlayingItem(p_playlist) ? playlist_CurrentPlayingItem(p_playlist)->p_parent : NULL;
                if (!p_parent)
                    p_parent = p_playlist->p_local_onelevel;

                while (p_parent->p_parent && p_parent->p_parent->p_parent)
                    p_parent = p_parent->p_parent;
                PL_UNLOCK;

                playlist_Add(p_playlist, p_sys->psz_open_chain, NULL,
                              PLAYLIST_APPEND|PLAYLIST_GO, PLAYLIST_END,
                              p_parent->p_input ==
                                p_playlist->p_local_onelevel->p_input
                              , false);

                p_sys->b_box_plidx_follow = true;
            }
            p_sys->i_box_type = BOX_PLAYLIST;
            return 1;
        case 0x1b:  /* ESC */
            if (wgetch(p_sys->w) != ERR)
                return 0;
            p_sys->i_box_type = BOX_PLAYLIST;
            return 1;
        case KEY_BACKSPACE:
        case 0x7f:
            RemoveLastUTF8Entity(p_sys->psz_open_chain, i_chain_len);
            return 1;
        default:
            if (i_chain_len + 1 < sizeof p_sys->psz_open_chain)
            {
                p_sys->psz_open_chain[i_chain_len] = (char) i_key;
                p_sys->psz_open_chain[i_chain_len + 1] = '\0';
            }
        }
        return 1;
    }


    /* Common keys */
    switch(i_key)
    {
    case 0x1b:  /* ESC */
        if (wgetch(p_sys->w) != ERR)
            return 0;

    case 'q':
    case 'Q':
    case KEY_EXIT:
        libvlc_Quit(p_intf->p_libvlc);
        return 0;

    /* Box switching */
    case 'i':
        BoxSwitch(p_sys, BOX_INFO);
        p_sys->i_box_lines_total = 0;
        break;
    case 'm':
        BoxSwitch(p_sys, BOX_META);
        p_sys->i_box_lines_total = 0;
        break;
#if 0
    case 'L':
        BoxSwitch(p_sys, BOX_LOG)
        break;
#endif
    case 'P':
        BoxSwitch(p_sys, BOX_PLAYLIST);
        break;
    case 'B':
        BoxSwitch(p_sys, BOX_BROWSE);
        break;
    case 'x':
        BoxSwitch(p_sys, BOX_OBJECTS);
        break;
    case 'S':
        BoxSwitch(p_sys, BOX_STATS);
        break;
    case 'c':
        if ((p_sys->b_color = !p_sys->b_color))
            start_color_and_pairs(p_intf);
        break;
    case 'h':
    case 'H':
        BoxSwitch(p_sys, BOX_HELP);
        p_sys->i_box_lines_total = 0;
        break;
    case '/':
        if (p_sys->i_box_type != BOX_SEARCH && p_sys->psz_search_chain)
        {
            p_sys->psz_search_chain[0] = '\0';
            p_sys->b_box_plidx_follow = false;
            p_sys->i_before_search = p_sys->i_box_plidx;
            p_sys->i_box_type = BOX_SEARCH;
        }
        break;
    case 'A': /* Open */
        if (p_sys->i_box_type != BOX_OPEN)
        {
            p_sys->psz_open_chain[0] = '\0';
            p_sys->i_box_type = BOX_OPEN;
        }
        break;

    /* Navigation */
    case KEY_RIGHT:
        ChangePosition(p_intf, 0.01);
        break;

    case KEY_LEFT:
        ChangePosition(p_intf, -0.01);
        break;

    /* Common control */
    case 'f':
        if (p_intf->p_sys->p_input)
        {
            vout_thread_t *p_vout = input_GetVout(p_intf->p_sys->p_input);
            if (p_vout)
            {
                bool fs = var_ToggleBool(p_playlist, "fullscreen");
                var_SetBool(p_vout, "fullscreen", fs);
                vlc_object_release(p_vout);
            }
        }
        return 0;

    case ' ':
        PlayPause(p_intf);
        break;

    case 's':
        playlist_Stop(p_playlist);
        break;

    case 'e':
        Eject(p_intf);
        break;

    case '[':
        if (p_sys->p_input)
            var_TriggerCallback(p_sys->p_input, "prev-title");
        break;

    case ']':
        if (p_sys->p_input)
            var_TriggerCallback(p_sys->p_input, "next-title");
        break;

    case '<':
        if (p_sys->p_input)
            var_TriggerCallback(p_sys->p_input, "prev-chapter");
        break;

    case '>':
        if (p_sys->p_input)
            var_TriggerCallback(p_sys->p_input, "next-chapter");
        break;

    case 'p':
        playlist_Prev(p_playlist);
        clear();
        break;

    case 'n':
        playlist_Next(p_playlist);
        clear();
        break;

    case 'a':
        aout_VolumeUp(p_playlist, 1, NULL);
        clear();
        break;

    case 'z':
        aout_VolumeDown(p_playlist, 1, NULL);
        clear();
        break;

    /*
     * ^l should clear and redraw the screen
     */
    case KEY_CLEAR:
    case 0x0c:          /* ^l */
        clear();
        break;

    default:
        return 0;
    }

    return 1;
}

/*****************************************************************************
 * Run: ncurses thread
 *****************************************************************************/
static void Run(intf_thread_t *p_intf)
{
    intf_sys_t    *p_sys = p_intf->p_sys;
    playlist_t    *p_playlist = pl_Get(p_intf);
    bool           force_redraw = false;

    time_t t_last_refresh;
    int canc = vlc_savecancel();

    PlaylistRebuild(p_intf);
    Redraw(p_intf, &t_last_refresh);

    var_AddCallback(p_playlist, "intf-change", PlaylistChanged, p_intf);
    var_AddCallback(p_playlist, "playlist-item-append", PlaylistChanged, p_intf);

    while (vlc_object_alive(p_intf))
    {
        msleep(INTF_IDLE_SLEEP);

        /* Update the input */
        if (!p_sys->p_input)
        {
            p_sys->p_input = playlist_CurrentInput(p_playlist);
            force_redraw = true;
        }
        else if (p_sys->p_input->b_dead)
        {
            vlc_object_release(p_sys->p_input);
            p_sys->p_input = NULL;
        }

        PL_LOCK;
        if (p_sys->b_box_plidx_follow && playlist_CurrentPlayingItem(p_playlist))
            FindIndex(p_sys, p_playlist, true);

        PL_UNLOCK;

        while (HandleKey(p_intf))
            Redraw(p_intf, &t_last_refresh);

        if (force_redraw)
        {
            clear();
            Redraw(p_intf, &t_last_refresh);
            force_redraw = false;
        }

        if ((time(0) - t_last_refresh) >= 1)
            Redraw(p_intf, &t_last_refresh);
    }
    var_DelCallback(p_playlist, "intf-change", PlaylistChanged, p_intf);
    var_DelCallback(p_playlist, "playlist-item-append", PlaylistChanged, p_intf);
    vlc_restorecancel(canc);
}

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t    *p_sys  = p_intf->p_sys = calloc(1, sizeof(intf_sys_t));
    if (!p_sys)
        return VLC_ENOMEM;

    p_sys->i_box_type = BOX_PLAYLIST;
    p_sys->b_box_plidx_follow = true;
//  p_sys->p_sub = msg_Subscribe(p_intf);
    p_sys->b_color = var_CreateGetBool(p_intf, "color");

    p_sys->category_view = true; //FIXME: switching back & forth is broken

    p_sys->psz_current_dir = var_CreateGetString(p_intf, "browse-dir");
    if (!p_sys->psz_current_dir || !*p_sys->psz_current_dir)
    {
        free(p_sys->psz_current_dir);
        p_sys->psz_current_dir = config_GetUserDir(VLC_HOME_DIR);
    }

    p_sys->w = initscr();   /* Initialize the curses library */

    if (p_sys->b_color)
        start_color_and_pairs(p_intf);

    keypad(p_sys->w, TRUE); /* Don't do NL -> CR/NL */
    nonl();                 /* Take input chars one at a time */
    cbreak();               /* Don't echo */
    noecho();               /* Invisible cursor */
    curs_set(0);            /* Non blocking wgetch() */
    wtimeout(p_sys->w, 0);
    clear();

    /* Stop printing errors to the console */
    freopen("/dev/null", "wb", stderr);

    ReadDir(p_intf);

    p_intf->pf_run = Run;
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    intf_sys_t *p_sys = ((intf_thread_t*)p_this)->p_sys;

    PlaylistDestroy(p_sys);
    DirsDestroy(p_sys);

    free(p_sys->psz_current_dir);
    free(p_sys->psz_old_search);

    if (p_sys->p_input)
        vlc_object_release(p_sys->p_input);

    /* Close the ncurses interface */
    endwin();

//  msg_Unsubscribe(p_intf, p_sys->p_sub);

    free(p_sys);
}
