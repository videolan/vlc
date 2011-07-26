/*****************************************************************************
 * ncurses.c : NCurses interface for vlc
 *****************************************************************************
 * Copyright © 2001-2011 the VideoLAN team
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

/* UTF8 locale is required */

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
#include <vlc_aout_intf.h>
#include <vlc_charset.h>
#include <vlc_input.h>
#include <vlc_es.h>
#include <vlc_playlist.h>
#include <vlc_meta.h>
#include <vlc_fs.h>
#include <vlc_url.h>

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
    BOX_LOG,
    BOX_PLAYLIST,
    BOX_SEARCH,
    BOX_OPEN,
    BOX_BROWSE,
    BOX_META,
    BOX_OBJECTS,
    BOX_STATS
};

static const char *box_title[] = {
    [BOX_NONE]      = "",
    [BOX_HELP]      = " Help ",
    [BOX_INFO]      = " Information ",
    [BOX_LOG]       = " Messages ",
    [BOX_PLAYLIST]  = " Playlist ",
    [BOX_SEARCH]    = " Playlist ",
    [BOX_OPEN]      = " Playlist ",
    [BOX_BROWSE]    = " Browse ",
    [BOX_META]      = " Meta-information ",
    [BOX_OBJECTS]   = " Objects ",
    [BOX_STATS]     = " Stats ",
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
    C_INFO,
    C_ERROR,
    C_WARNING,
    C_DEBUG,
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

    /* VLC messages, keep the order from highest priority to lowest */
    [C_INFO]        = { COLOR_BLACK,    COLOR_WHITE },
    [C_ERROR]       = { COLOR_RED,      COLOR_BLACK },
    [C_WARNING]     = { COLOR_YELLOW,   COLOR_BLACK },
    [C_DEBUG]       = { COLOR_WHITE,    COLOR_BLACK },

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
    bool            b_exit;

    int             i_box_type;
    int             i_box_y;            // start of box content
    int             i_box_height;
    int             i_box_lines_total;  // number of lines in the box
    int             i_box_start;        // first line of box displayed
    int             i_box_idx;          // selected line

    msg_subscription_t  *p_sub;         // message bank subscription
    msg_item_t          *msgs[50];      // ring buffer
    int                 i_msgs;
    vlc_mutex_t         msg_lock;

    /* Search Box context */
    char            psz_search_chain[20];
    char            *psz_old_search;
    int             i_before_search;

    /* Open Box Context */
    char            psz_open_chain[50];

    /* File Browser context */
    char            *psz_current_dir;
    int             i_dir_entries;
    struct dir_entry_t  **pp_dir_entries;
    bool            b_show_hidden_files;

    /* Playlist context */
    struct pl_item_t    **pp_plist;
    int             i_plist_entries;
    bool            b_need_update;
    vlc_mutex_t     pl_lock;
    bool            b_plidx_follow;
    playlist_item_t *p_node;        /* current node */

};

struct msg_cb_data_t
{
    intf_sys_t *p_sys;
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

    if (asprintf(&uri, "%s" DIR_SEP "%s", current_dir, entry) != -1)
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
        p_dir_entry->psz_path = psz_entry;
        INSERT_ELEM(p_sys->pp_dir_entries, p_sys->i_dir_entries,
             p_sys->i_dir_entries, p_dir_entry);
        continue;

next:
        free(psz_entry);
    }

    /* Sort */
    qsort(p_sys->pp_dir_entries, p_sys->i_dir_entries,
           sizeof(struct dir_entry_t*), &comp_dir_entries);

    closedir(p_current_dir);
}

/*****************************************************************************
 * Adjust index position after a change (list navigation or item switching)
 *****************************************************************************/
static void CheckIdx(intf_sys_t *p_sys)
{
    int lines = p_sys->i_box_lines_total;
    int height = LINES - p_sys->i_box_y - 2;
    if (height > lines - 1)
        height = lines - 1;

    /* make sure the new index is within the box */
    if (p_sys->i_box_idx <= 0)
    {
        p_sys->i_box_idx = 0;
        p_sys->i_box_start = 0;
    }
    else if (p_sys->i_box_idx >= lines - 1 && lines > 0)
    {
        p_sys->i_box_idx = lines - 1;
        p_sys->i_box_start = p_sys->i_box_idx - height;
    }

    /* Fix box start (1st line of the box displayed) */
    if (p_sys->i_box_idx < p_sys->i_box_start ||
        p_sys->i_box_idx > height + p_sys->i_box_start + 1)
    {
        p_sys->i_box_start = p_sys->i_box_idx - height/2;
        if (p_sys->i_box_start < 0)
            p_sys->i_box_start = 0;
    }
    else if (p_sys->i_box_idx == p_sys->i_box_start - 1)
        p_sys->i_box_start--;
    else if (p_sys->i_box_idx == height + p_sys->i_box_start + 1)
        p_sys->i_box_start++;
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
    PlaylistAddNode(p_sys, p_playlist->p_root_onelevel, "");
    PL_UNLOCK;
}

static int ItemChanged(vlc_object_t *p_this, const char *psz_variable,
                            vlc_value_t oval, vlc_value_t nval, void *param)
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_variable);
    VLC_UNUSED(oval); VLC_UNUSED(nval);

    intf_thread_t *p_intf   = (intf_thread_t *)param;
    intf_sys_t *p_sys       = p_intf->p_sys;

    vlc_mutex_lock(&p_sys->pl_lock);
    p_sys->b_need_update = true;
    vlc_mutex_unlock(&p_sys->pl_lock);

    return VLC_SUCCESS;
}

static int PlaylistChanged(vlc_object_t *p_this, const char *psz_variable,
                            vlc_value_t oval, vlc_value_t nval, void *param)
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_variable);
    VLC_UNUSED(oval); VLC_UNUSED(nval);
    intf_thread_t *p_intf   = (intf_thread_t *)param;
    intf_sys_t *p_sys       = p_intf->p_sys;
    playlist_item_t *p_node = playlist_CurrentPlayingItem(pl_Get(p_intf));

    vlc_mutex_lock(&p_sys->pl_lock);
    p_sys->b_need_update = true;
    p_sys->p_node = p_node ? p_node->p_parent : NULL;
    vlc_mutex_unlock(&p_sys->pl_lock);

    return VLC_SUCCESS;
}

/* Playlist suxx */
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
        p_sys->i_box_idx = p_sys->i_before_search;
        return;
    }

    i_item = SubSearchPlaylist(p_sys, psz_searchstring, i_first + 1,
                               p_sys->i_plist_entries);
    if (i_item < 0)
        i_item = SubSearchPlaylist(p_sys, psz_searchstring, 0, i_first);

    if (i_item > 0)
    {
        p_sys->i_box_idx = i_item;
        CheckIdx(p_sys);
    }
}

static inline bool IsIndex(intf_sys_t *p_sys, playlist_t *p_playlist, int i)
{
    playlist_item_t *p_item = p_sys->pp_plist[i]->p_item;
    playlist_item_t *p_played_item;

    PL_ASSERT_LOCKED;

    vlc_mutex_lock(&p_sys->pl_lock);
    if (p_item->i_children == 0 && p_item == p_sys->p_node) {
        vlc_mutex_unlock(&p_sys->pl_lock);
        return true;
    }
    vlc_mutex_unlock(&p_sys->pl_lock);

    p_played_item = playlist_CurrentPlayingItem(p_playlist);
    if (p_played_item && p_item->p_input && p_played_item->p_input)
        return p_item->p_input->i_id == p_played_item->p_input->i_id;

    return false;
}

static void FindIndex(intf_sys_t *p_sys, playlist_t *p_playlist)
{
    int plidx = p_sys->i_box_idx;
    int max = p_sys->i_plist_entries;

    PL_LOCK;

    if (!IsIndex(p_sys, p_playlist, plidx))
        for(int i = 0; i < max; i++)
            if (IsIndex(p_sys, p_playlist, i))
            {
                p_sys->i_box_idx = i;
                CheckIdx(p_sys);
                break;
            }

    PL_UNLOCK;

    p_sys->b_plidx_follow = true;
}

/****************************************************************************
 * Drawing
 ****************************************************************************/

static void start_color_and_pairs(intf_thread_t *p_intf)
{
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
}

static void DrawBox(int y, int h, bool b_color, const char *title)
{
    int i_len;
    int w = COLS;

    if (w <= 3 || h <= 0)
        return;

    if (b_color) color_set(C_BOX, NULL);

    if (!title) title = "";
    i_len = strlen(title);

    if (i_len > w - 2)
        i_len = w - 2;

    mvaddch(y, 0,    ACS_ULCORNER);
    mvhline(y, 1,  ACS_HLINE, (w-i_len-2)/2);
    mvprintw(y, 1+(w-i_len-2)/2, "%s", title);
    mvhline(y, (w-i_len)/2+i_len,  ACS_HLINE, w - 1 - ((w-i_len)/2+i_len));
    mvaddch(y, w-1,ACS_URCORNER);

    for(int i = 0; i < h; i++)
    {
        mvaddch(++y, 0,   ACS_VLINE);
        mvaddch(y, w-1, ACS_VLINE);
    }

    mvaddch(++y, 0,   ACS_LLCORNER);
    mvhline(y,   1,   ACS_HLINE, w - 2);
    mvaddch(y,   w-1, ACS_LRCORNER);
    if (b_color) color_set(C_DEFAULT, NULL);
}

static void DrawEmptyLine(int y, int x, int w)
{
    if (w <= 0) return;

    mvhline(y, x, ' ', w);
}

static void DrawLine(int y, int x, int w)
{
    if (w <= 0) return;

    attrset(A_REVERSE);
    mvhline(y, x, ' ', w);
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

static void MainBoxWrite(intf_sys_t *p_sys, int l, const char *p_fmt, ...)
{
    va_list     vl_args;
    char        *p_buf;
    bool        b_selected = l == p_sys->i_box_idx;

    if (l < p_sys->i_box_start || l - p_sys->i_box_start >= p_sys->i_box_height)
        return;

    va_start(vl_args, p_fmt);
    if (vasprintf(&p_buf, p_fmt, vl_args) == -1)
        return;
    va_end(vl_args);

    if (b_selected) attron(A_REVERSE);
    mvnprintw(p_sys->i_box_y + l - p_sys->i_box_start, 1, COLS - 2, "%s", p_buf);
    if (b_selected) attroff(A_REVERSE);

    free(p_buf);
}

static int SubDrawObject(intf_sys_t *p_sys, int l, vlc_object_t *p_obj, int i_level, const char *prefix)
{
    int x = 2 * i_level++;
    char *psz_name = vlc_object_get_name(p_obj);
    vlc_list_t *list;
    char space[x+3];
    memset(space, ' ', x);
    space[x] = '\0';

    if (psz_name)
    {
        MainBoxWrite(p_sys, l++, "%s%s%s \"%s\" (%p)", space, prefix,
                      p_obj->psz_object_type, psz_name, p_obj);
        free(psz_name);
    }
    else
        MainBoxWrite(p_sys, l++, "%s%s%s (%p)", space, prefix,
                      p_obj->psz_object_type, p_obj);

    list = vlc_list_children(p_obj);
    for(int i = 0; i < list->i_count ; i++)
    {
        l = SubDrawObject(p_sys, l, list->p_values[i].p_object, i_level,
            (i == list->i_count - 1) ? "`-" : "|-" );
    }
    vlc_list_release(list);
    return l;
}

static int DrawObjects(intf_thread_t *p_intf)
{
    return SubDrawObject(p_intf->p_sys, 0, VLC_OBJECT(p_intf->p_libvlc), 0, "");
}

static int DrawMeta(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    input_item_t *p_item;
    int l = 0;

    if (!p_input)
        return 0;

    p_item = input_GetItem(p_input);
    vlc_mutex_lock(&p_item->lock);
    for(int i=0; i<VLC_META_TYPE_COUNT; i++)
    {
        const char *psz_meta = vlc_meta_Get(p_item->p_meta, i);
        if (!psz_meta || !*psz_meta)
            continue;

        if (p_sys->b_color) color_set(C_CATEGORY, NULL);
        MainBoxWrite(p_sys, l++, "  [%s]", vlc_meta_TypeToLocalizedString(i));
        if (p_sys->b_color) color_set(C_DEFAULT, NULL);
        MainBoxWrite(p_sys, l++, "      %s", psz_meta);
    }
    vlc_mutex_unlock(&p_item->lock);

    return l;
}

static int DrawInfo(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    input_item_t *p_item;
    int l = 0;

    if (!p_input)
        return 0;

    p_item = input_GetItem(p_input);
    vlc_mutex_lock(&p_item->lock);
    for(int i = 0; i < p_item->i_categories; i++)
    {
        info_category_t *p_category = p_item->pp_categories[i];
        if (p_sys->b_color) color_set(C_CATEGORY, NULL);
        MainBoxWrite(p_sys, l++, _("  [%s]"), p_category->psz_name);
        if (p_sys->b_color) color_set(C_DEFAULT, NULL);
        for(int j = 0; j < p_category->i_infos; j++)
        {
            info_t *p_info = p_category->pp_infos[j];
            MainBoxWrite(p_sys, l++, _("      %s: %s"),
                         p_info->psz_name, p_info->psz_value);
        }
    }
    vlc_mutex_unlock(&p_item->lock);

    return l;
}

static int DrawStats(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    input_item_t *p_item;
    input_stats_t *p_stats;
    int l = 0, i_audio = 0, i_video = 0;

    if (!p_input)
        return 0;

    p_item = input_GetItem(p_input);
    assert(p_item);

    vlc_mutex_lock(&p_item->lock);
    p_stats = p_item->p_stats;
    vlc_mutex_lock(&p_stats->lock);

    for(int i = 0; i < p_item->i_es ; i++)
    {
        i_audio += (p_item->es[i]->i_cat == AUDIO_ES);
        i_video += (p_item->es[i]->i_cat == VIDEO_ES);
    }

    /* Input */
    if (p_sys->b_color) color_set(C_CATEGORY, NULL);
    MainBoxWrite(p_sys, l++, _("  [Incoming]"));
    if (p_sys->b_color) color_set(C_DEFAULT, NULL);
    MainBoxWrite(p_sys, l++, _("      input bytes read : %8.0f KiB"),
            (float)(p_stats->i_read_bytes)/1024);
    MainBoxWrite(p_sys, l++, _("      input bitrate    :   %6.0f kb/s"),
            p_stats->f_input_bitrate*8000);
    MainBoxWrite(p_sys, l++, _("      demux bytes read : %8.0f KiB"),
            (float)(p_stats->i_demux_read_bytes)/1024);
    MainBoxWrite(p_sys, l++, _("      demux bitrate    :   %6.0f kb/s"),
            p_stats->f_demux_bitrate*8000);

    /* Video */
    if (i_video)
    {
        if (p_sys->b_color) color_set(C_CATEGORY, NULL);
        MainBoxWrite(p_sys, l++, _("  [Video Decoding]"));
        if (p_sys->b_color) color_set(C_DEFAULT, NULL);
        MainBoxWrite(p_sys, l++, _("      video decoded    :    %"PRId64),
                p_stats->i_decoded_video);
        MainBoxWrite(p_sys, l++, _("      frames displayed :    %"PRId64),
                p_stats->i_displayed_pictures);
        MainBoxWrite(p_sys, l++, _("      frames lost      :    %"PRId64),
                p_stats->i_lost_pictures);
    }
    /* Audio*/
    if (i_audio)
    {
        if (p_sys->b_color) color_set(C_CATEGORY, NULL);
        MainBoxWrite(p_sys, l++, _("  [Audio Decoding]"));
        if (p_sys->b_color) color_set(C_DEFAULT, NULL);
        MainBoxWrite(p_sys, l++, _("      audio decoded    :    %"PRId64),
                p_stats->i_decoded_audio);
        MainBoxWrite(p_sys, l++, _("      buffers played   :    %"PRId64),
                p_stats->i_played_abuffers);
        MainBoxWrite(p_sys, l++, _("      buffers lost     :    %"PRId64),
                p_stats->i_lost_abuffers);
    }
    /* Sout */
    if (p_sys->b_color) color_set(C_CATEGORY, NULL);
    MainBoxWrite(p_sys, l++, _("  [Streaming]"));
    if (p_sys->b_color) color_set(C_DEFAULT, NULL);
    MainBoxWrite(p_sys, l++, _("      packets sent     :    %5i"), p_stats->i_sent_packets);
    MainBoxWrite(p_sys, l++, _("      bytes sent       : %8.0f KiB"),
            (float)(p_stats->i_sent_bytes)/1025);
    MainBoxWrite(p_sys, l++, _("      sending bitrate  :   %6.0f kb/s"),
            p_stats->f_send_bitrate*8000);
    if (p_sys->b_color) color_set(C_DEFAULT, NULL);

    vlc_mutex_unlock(&p_stats->lock);
    vlc_mutex_unlock(&p_item->lock);

    return l;
}

static int DrawHelp(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    int l = 0;

#define H(a) MainBoxWrite(p_sys, l++, a)

    if (p_sys->b_color) color_set(C_CATEGORY, NULL);
    H(_("[Display]"));
    if (p_sys->b_color) color_set(C_DEFAULT, NULL);
    H(_(" h,H                    Show/Hide help box"));
    H(_(" i                      Show/Hide info box"));
    H(_(" m                      Show/Hide metadata box"));
    H(_(" L                      Show/Hide messages box"));
    H(_(" P                      Show/Hide playlist box"));
    H(_(" B                      Show/Hide filebrowser"));
    H(_(" x                      Show/Hide objects box"));
    H(_(" S                      Show/Hide statistics box"));
    H(_(" Esc                    Close Add/Search entry"));
    H(_(" Ctrl-l                 Refresh the screen"));
    H("");

    if (p_sys->b_color) color_set(C_CATEGORY, NULL);
    H(_("[Global]"));
    if (p_sys->b_color) color_set(C_DEFAULT, NULL);
    H(_(" q, Q, Esc              Quit"));
    H(_(" s                      Stop"));
    H(_(" <space>                Pause/Play"));
    H(_(" f                      Toggle Fullscreen"));
    H(_(" n, p                   Next/Previous playlist item"));
    H(_(" [, ]                   Next/Previous title"));
    H(_(" <, >                   Next/Previous chapter"));
    /* xgettext: You can use ← and → characters */
    H(_(" <left>,<right>         Seek -/+ 1%%"));
    H(_(" a, z                   Volume Up/Down"));
    /* xgettext: You can use ↑ and ↓ characters */
    H(_(" <up>,<down>            Navigate through the box line by line"));
    /* xgettext: You can use ⇞ and ⇟ characters */
    H(_(" <pageup>,<pagedown>    Navigate through the box page by page"));
    /* xgettext: You can use ↖ and ↘ characters */
    H(_(" <start>,<end>          Navigate to start/end of box"));
    H("");

    if (p_sys->b_color) color_set(C_CATEGORY, NULL);
    H(_("[Playlist]"));
    if (p_sys->b_color) color_set(C_DEFAULT, NULL);
    H(_(" r                      Toggle Random playing"));
    H(_(" l                      Toggle Loop Playlist"));
    H(_(" R                      Toggle Repeat item"));
    H(_(" o                      Order Playlist by title"));
    H(_(" O                      Reverse order Playlist by title"));
    H(_(" g                      Go to the current playing item"));
    H(_(" /                      Look for an item"));
    H(_(" A                      Add an entry"));
    /* xgettext: You can use ⌫ character to translate <backspace> */
    H(_(" D, <backspace>, <del>  Delete an entry"));
    H(_(" e                      Eject (if stopped)"));
    H("");

    if (p_sys->b_color) color_set(C_CATEGORY, NULL);
    H(_("[Filebrowser]"));
    if (p_sys->b_color) color_set(C_DEFAULT, NULL);
    H(_(" <enter>                Add the selected file to the playlist"));
    H(_(" <space>                Add the selected directory to the playlist"));
    H(_(" .                      Show/Hide hidden files"));
    H("");

    if (p_sys->b_color) color_set(C_CATEGORY, NULL);
    H(_("[Player]"));
    if (p_sys->b_color) color_set(C_DEFAULT, NULL);
    /* xgettext: You can use ↑ and ↓ characters */
    H(_(" <up>,<down>            Seek +/-5%%"));

#undef H
    return l;
}

static int DrawBrowse(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;

    for(int i = 0; i < p_sys->i_dir_entries; i++)
    {
        struct dir_entry_t *p_dir_entry = p_sys->pp_dir_entries[i];
        char type = p_dir_entry->b_file ? ' ' : '+';

        if (p_sys->b_color)
            color_set(p_dir_entry->b_file ? C_DEFAULT : C_FOLDER, NULL);
        MainBoxWrite(p_sys, i, " %c %s", type, p_dir_entry->psz_path);
    }

    return p_sys->i_dir_entries;
}

static int DrawPlaylist(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = pl_Get(p_intf);

    vlc_mutex_lock(&p_sys->pl_lock);
    if (p_sys->b_need_update)
    {
        PlaylistRebuild(p_intf);
        p_sys->b_need_update = false;
    }
    vlc_mutex_unlock(&p_sys->pl_lock);

    if (p_sys->b_plidx_follow)
        FindIndex(p_sys, p_playlist);

    for(int i = 0; i < p_sys->i_plist_entries; i++)
    {
        char c;
        playlist_item_t *p_current_item;
        playlist_item_t *p_item = p_sys->pp_plist[i]->p_item;
        vlc_mutex_lock(&p_sys->pl_lock);
        playlist_item_t *p_node = p_sys->p_node;
        vlc_mutex_unlock(&p_sys->pl_lock);

        PL_LOCK;
        assert(p_item);
        p_current_item = playlist_CurrentPlayingItem(p_playlist);
        if ((p_node && p_item->p_input == p_node->p_input) ||
           (!p_node && p_current_item && p_item->p_input == p_current_item->p_input))
            c = '*';
        else if (p_item == p_node || p_current_item == p_item)
            c = '>';
        else
            c = ' ';
        PL_UNLOCK;

        if (p_sys->b_color) color_set(i%3 + C_PLAYLIST_1, NULL);
        MainBoxWrite(p_sys, i, "%c%s", c, p_sys->pp_plist[i]->psz_display);
        if (p_sys->b_color) color_set(C_DEFAULT, NULL);
    }

    return p_sys->i_plist_entries;
}

static int DrawMessages(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    int l = 0;
    int i;

    vlc_mutex_lock(&p_sys->msg_lock);
    i = p_sys->i_msgs;
    for(;;)
    {
        msg_item_t *msg = p_sys->msgs[i];
        if (msg)
        {
            if (p_sys->b_color)
                color_set(msg->i_type + C_INFO, NULL);
            MainBoxWrite(p_sys, l++, "[%s] %s", msg->psz_module, msg->psz_msg);
        }

        if (++i == sizeof p_sys->msgs / sizeof *p_sys->msgs)
            i = 0;

        if (i == p_sys->i_msgs) /* did we loop around the ring buffer ? */
            break;
    }

    vlc_mutex_unlock(&p_sys->msg_lock);
    if (p_sys->b_color)
        color_set(C_DEFAULT, NULL);
    return l;
}

static int DrawStatus(intf_thread_t *p_intf)
{
    intf_sys_t     *p_sys = p_intf->p_sys;
    input_thread_t *p_input = p_sys->p_input;
    playlist_t     *p_playlist = pl_Get(p_intf);
    static const char name[] = "VLC media player "PACKAGE_VERSION;
    const size_t name_len = sizeof name - 1; /* without \0 termination */
    int y = 0;
    const char *repeat, *loop, *random;


    /* Title */
    int padding = COLS - name_len; /* center title */
    if (padding < 0)
        padding = 0;

    attrset(A_REVERSE);
    if (p_sys->b_color) color_set(C_TITLE, NULL);
    DrawEmptyLine(y, 0, COLS);
    mvnprintw(y++, padding / 2, COLS, "%s", name);
    if (p_sys->b_color) color_set(C_STATUS, NULL);
    attroff(A_REVERSE);

    y++; /* leave a blank line */

    repeat = var_GetBool(p_playlist, "repeat") ? _("[Repeat] ") : "";
    random = var_GetBool(p_playlist, "random") ? _("[Random] ") : "";
    loop   = var_GetBool(p_playlist, "loop")   ? _("[Loop]")    : "";

    if (p_input && !p_input->b_dead)
    {
        vlc_value_t val;
        char *psz_path, *psz_uri;

        psz_uri = input_item_GetURI(input_GetItem(p_input));
        psz_path = make_path(psz_uri);

        mvnprintw(y++, 0, COLS, _(" Source   : %s"), psz_path?psz_path:psz_uri);
        free(psz_uri);
        free(psz_path);

        var_Get(p_input, "state", &val);
        switch(val.i_int)
        {
            static const char *input_state[] = {
                [PLAYING_S] = " State    : Playing %s%s%s",
                [OPENING_S] = " State    : Opening/Connecting %s%s%s",
                [PAUSE_S]   = " State    : Paused %s%s%s",
            };
            char buf1[MSTRTIME_MAX_SIZE];
            char buf2[MSTRTIME_MAX_SIZE];
            unsigned i_volume;

        case INIT_S:
        case END_S:
            y += 2;
            break;

        case PLAYING_S:
        case OPENING_S:
        case PAUSE_S:
            mvnprintw(y++, 0, COLS, _(input_state[val.i_int]),
                        repeat, random, loop);

        default:
            var_Get(p_input, "time", &val);
            secstotimestr(buf1, val.i_time / CLOCK_FREQ);
            var_Get(p_input, "length", &val);
            secstotimestr(buf2, val.i_time / CLOCK_FREQ);

            mvnprintw(y++, 0, COLS, _(" Position : %s/%s"), buf1, buf2);

            i_volume = aout_VolumeGet(p_playlist);
            mvnprintw(y++, 0, COLS, _(" Volume   : %u%%"),
                      i_volume*100/AOUT_VOLUME_DEFAULT);

            if (!var_Get(p_input, "title", &val))
            {
                int i_title_count = var_CountChoices(p_input, "title");
                if (i_title_count > 0)
                    mvnprintw(y++, 0, COLS, _(" Title    : %"PRId64"/%d"),
                               val.i_int, i_title_count);
            }

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
        mvnprintw(y++, 0, COLS, _(" Source: <no current item> "));
        mvnprintw(y++, 0, COLS, " %s%s%s", repeat, random, loop);
        mvnprintw(y++, 0, COLS, _(" [ h for help ]"));
        DrawEmptyLine(y++, 0, COLS);
    }

    if (p_sys->b_color) color_set(C_DEFAULT, NULL);
    DrawBox(y++, 1, p_sys->b_color, ""); /* position slider */
    DrawEmptyLine(y, 1, COLS-2);
    if (p_input)
        DrawLine(y, 1, (int)((COLS-2) * var_GetFloat(p_input, "position")));

    y += 2; /* skip slider and box */

    return y;
}

static void FillTextBox(intf_sys_t *p_sys)
{
    int width = COLS - 2;
    const char *title = p_sys->i_box_type == BOX_OPEN ? "Open: %s" : "Find: %s";
    char *chain = p_sys->i_box_type == BOX_OPEN ? p_sys->psz_open_chain :
                    p_sys->psz_old_search ?  p_sys->psz_old_search :
                     p_sys->psz_search_chain;

    DrawEmptyLine(7, 1, width);
    mvnprintw(7, 1, width, _(title), chain);
}

static void FillBox(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    static int (* const draw[]) (intf_thread_t *) = {
        [BOX_HELP]      = DrawHelp,
        [BOX_INFO]      = DrawInfo,
        [BOX_META]      = DrawMeta,
        [BOX_OBJECTS]   = DrawObjects,
        [BOX_STATS]     = DrawStats,
        [BOX_BROWSE]    = DrawBrowse,
        [BOX_PLAYLIST]  = DrawPlaylist,
        [BOX_SEARCH]    = DrawPlaylist,
        [BOX_OPEN]      = DrawPlaylist,
        [BOX_LOG]       = DrawMessages,
    };

    p_sys->i_box_lines_total = draw[p_sys->i_box_type](p_intf);

    if (p_sys->i_box_type == BOX_SEARCH || p_sys->i_box_type == BOX_OPEN)
        FillTextBox(p_sys);
}

static void Redraw(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys   = p_intf->p_sys;
    int         box     = p_sys->i_box_type;
    int         y       = DrawStatus(p_intf);

    p_sys->i_box_height = LINES - y - 2;
    DrawBox(y++, p_sys->i_box_height, p_sys->b_color, _(box_title[box]));

    p_sys->i_box_y = y;

    if (box != BOX_NONE)
    {
        FillBox(p_intf);

        if (p_sys->i_box_lines_total == 0)
            p_sys->i_box_start = 0;
        else if (p_sys->i_box_start > p_sys->i_box_lines_total - 1)
            p_sys->i_box_start = p_sys->i_box_lines_total - 1;
        y += __MIN(p_sys->i_box_lines_total - p_sys->i_box_start,
                   p_sys->i_box_height);
    }

    while (y < LINES - 1)
        DrawEmptyLine(y++, 1, COLS - 2);

    refresh();
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

static inline void RemoveLastUTF8Entity(char *psz, int len)
{
    while (len && ((psz[--len] & 0xc0) == 0x80))    /* UTF8 continuation byte */
        ;
    psz[len] = '\0';
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
    psz_device = psz_name ? GetDiscDevice(p_intf, psz_name) : NULL;

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
    p_sys->i_box_start = 0;
    p_sys->i_box_idx = 0;
}

static bool HandlePlaylistKey(intf_thread_t *p_intf, int key)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = pl_Get(p_intf);
    struct pl_item_t *p_pl_item;

    switch(key)
    {
    /* Playlist Settings */
    case 'r': var_ToggleBool(p_playlist, "random"); return true;
    case 'l': var_ToggleBool(p_playlist, "loop");   return true;
    case 'R': var_ToggleBool(p_playlist, "repeat"); return true;

    /* Playlist sort */
    case 'o':
    case 'O':
        playlist_RecursiveNodeSort(p_playlist, p_playlist->p_root_onelevel,
                                    SORT_TITLE_NODES_FIRST,
                                    (key == 'o')? ORDER_NORMAL : ORDER_REVERSE);
        vlc_mutex_lock(&p_sys->pl_lock);
        p_sys->b_need_update = true;
        vlc_mutex_unlock(&p_sys->pl_lock);
        return true;

    case 'g':
        FindIndex(p_sys, p_playlist);
        return true;

    /* Deletion */
    case 'D':
    case KEY_BACKSPACE:
    case 0x7f:
    case KEY_DC:
    {
        playlist_item_t *p_item;

        PL_LOCK;
        p_item = p_sys->pp_plist[p_sys->i_box_idx]->p_item;
        if (p_item->i_children == -1)
            playlist_DeleteFromInput(p_playlist, p_item->p_input, pl_Locked);
        else
            playlist_NodeDelete(p_playlist, p_item, true , false);
        PL_UNLOCK;
        vlc_mutex_lock(&p_sys->pl_lock);
        p_sys->b_need_update = true;
        vlc_mutex_unlock(&p_sys->pl_lock);
        return true;
    }

    case KEY_ENTER:
    case '\r':
    case '\n':
        if (!(p_pl_item = p_sys->pp_plist[p_sys->i_box_idx]))
            return false;

        if (p_pl_item->p_item->i_children)
        {
            playlist_item_t *p_item, *p_parent = p_pl_item->p_item;
            if (p_parent->i_children == -1)
            {
                p_item = p_parent;

                while (p_parent->p_parent)
                    p_parent = p_parent->p_parent;
            }
            else
            {
                vlc_mutex_lock(&p_sys->pl_lock);
                p_sys->p_node = p_parent;
                vlc_mutex_unlock(&p_sys->pl_lock);
                p_item = NULL;
            }

            playlist_Control(p_playlist, PLAYLIST_VIEWPLAY, pl_Unlocked,
                              p_parent, p_item);
        }
        else
        {   /* We only want to set the current node */
            playlist_Stop(p_playlist);
            vlc_mutex_lock(&p_sys->pl_lock);
            p_sys->p_node = p_pl_item->p_item;
            vlc_mutex_unlock(&p_sys->pl_lock);
        }

        p_sys->b_plidx_follow = true;
        return true;
    }

    return false;
}

static bool HandleBrowseKey(intf_thread_t *p_intf, int key)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    struct dir_entry_t *dir_entry;

    switch(key)
    {
    case '.':
        p_sys->b_show_hidden_files = !p_sys->b_show_hidden_files;
        ReadDir(p_intf);
        return true;

    case KEY_ENTER:
    case '\r':
    case '\n':
    case ' ':
        dir_entry = p_sys->pp_dir_entries[p_sys->i_box_idx];
        char *psz_path;
        if (asprintf(&psz_path, "%s" DIR_SEP "%s", p_sys->psz_current_dir,
                     dir_entry->psz_path) == -1)
            return true;

        if (!dir_entry->b_file && key != ' ')
        {
            free(p_sys->psz_current_dir);
            p_sys->psz_current_dir = psz_path;
            ReadDir(p_intf);

            p_sys->i_box_start = 0;
            p_sys->i_box_idx = 0;
            return true;
        }

        char *psz_uri = make_URI(psz_path, dir_entry->b_file ? "file"
                                                             : "directory");
        free(psz_path);
        if (psz_uri == NULL)
            return true;

        playlist_t *p_playlist = pl_Get(p_intf);
        vlc_mutex_lock(&p_sys->pl_lock);
        playlist_item_t *p_parent = p_sys->p_node;
        vlc_mutex_unlock(&p_sys->pl_lock);
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

        input_item_t *p_input = p_playlist->p_local_onelevel->p_input;
        playlist_Add(p_playlist, psz_uri, NULL, PLAYLIST_APPEND,
                      PLAYLIST_END, p_parent->p_input == p_input, false);

        BoxSwitch(p_sys, BOX_PLAYLIST);
        free(psz_uri);
        return true;
    }

    return false;
}

static void HandleEditBoxKey(intf_thread_t *p_intf, int key, int box)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    bool search = box == BOX_SEARCH;
    char *str = search ? p_sys->psz_search_chain: p_sys->psz_open_chain;
    size_t len = strlen(str);

    assert(box == BOX_SEARCH || box == BOX_OPEN);

    switch(key)
    {
    case 0x0c:  /* ^l */
    case KEY_CLEAR:     clear(); return;

    case KEY_ENTER:
    case '\r':
    case '\n':
        if (search)
        {
            if (len)
                p_sys->psz_old_search = strdup(p_sys->psz_search_chain);
            else if (p_sys->psz_old_search)
                SearchPlaylist(p_sys, p_sys->psz_old_search);
        }
        else if (len)
        {
            char *psz_uri = make_URI(p_sys->psz_open_chain, NULL);
            if (psz_uri == NULL)
            {
                p_sys->i_box_type = BOX_PLAYLIST;
                return;
            }

            playlist_t *p_playlist = pl_Get(p_intf);
            vlc_mutex_lock(&p_sys->pl_lock);
            playlist_item_t *p_parent = p_sys->p_node, *p_current;
            vlc_mutex_unlock(&p_sys->pl_lock);

            PL_LOCK;
            if (!p_parent)
            {
                p_current = playlist_CurrentPlayingItem(p_playlist);
                p_parent = p_current ? p_current->p_parent : NULL;
                if (!p_parent)
                    p_parent = p_playlist->p_local_onelevel;
            }

            while (p_parent->p_parent && p_parent->p_parent->p_parent)
                p_parent = p_parent->p_parent;
            PL_UNLOCK;

            playlist_Add(p_playlist, psz_uri, NULL,
                  PLAYLIST_APPEND|PLAYLIST_GO, PLAYLIST_END,
                  p_parent->p_input == p_playlist->p_local_onelevel->p_input,
                  false);

            free(psz_uri);
            p_sys->b_plidx_follow = true;
        }
        p_sys->i_box_type = BOX_PLAYLIST;
        return;

    case 0x1b: /* ESC */
        /* Alt+key combinations return 2 keys in the terminal keyboard:
         * ESC, and the 2nd key.
         * If some other key is available immediately (where immediately
         * means after getch() 1 second delay), that means that the
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
        if (getch() != ERR)
            return;

        if (search) p_sys->i_box_idx = p_sys->i_before_search;
        p_sys->i_box_type = BOX_PLAYLIST;
        return;

    case KEY_BACKSPACE:
    case 0x7f:
        RemoveLastUTF8Entity(str, len);
        break;

    default:
        if (len + 1 < (search ? sizeof p_sys->psz_search_chain
                              : sizeof p_sys->psz_open_chain))
        {
            str[len + 0] = (char) key;
            str[len + 1] = '\0';
        }
    }

    if (search)
    {
        free(p_sys->psz_old_search);
        p_sys->psz_old_search = NULL;
        SearchPlaylist(p_sys, str);
    }
}

static void InputNavigate(input_thread_t* p_input, const char *var)
{
    if (p_input)
        var_TriggerCallback(p_input, var);
}

static void HandleCommonKey(intf_thread_t *p_intf, int key)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = pl_Get(p_intf);
    switch(key)
    {
    case 0x1b:  /* ESC */
        if (getch() != ERR)
            return;

    case 'q':
    case 'Q':
    case KEY_EXIT:
        libvlc_Quit(p_intf->p_libvlc);
        p_sys->b_exit = true;           // terminate the main loop
        return;

    case 'h':
    case 'H': BoxSwitch(p_sys, BOX_HELP);       return;
    case 'i': BoxSwitch(p_sys, BOX_INFO);       return;
    case 'm': BoxSwitch(p_sys, BOX_META);       return;
    case 'L': BoxSwitch(p_sys, BOX_LOG);        return;
    case 'P': BoxSwitch(p_sys, BOX_PLAYLIST);   return;
    case 'B': BoxSwitch(p_sys, BOX_BROWSE);     return;
    case 'x': BoxSwitch(p_sys, BOX_OBJECTS);    return;
    case 'S': BoxSwitch(p_sys, BOX_STATS);      return;

    case '/': /* Search */
        p_sys->psz_search_chain[0] = '\0';
        p_sys->b_plidx_follow = false;
        if (p_sys->i_box_type == BOX_PLAYLIST)
        {
            p_sys->i_before_search = p_sys->i_box_idx;
            p_sys->i_box_type = BOX_SEARCH;
        }
        else
        {
            p_sys->i_before_search = 0;
            BoxSwitch(p_sys, BOX_SEARCH);
        }
        return;

    case 'A': /* Open */
        p_sys->psz_open_chain[0] = '\0';
        if (p_sys->i_box_type == BOX_PLAYLIST)
            p_sys->i_box_type = BOX_OPEN;
        else
            BoxSwitch(p_sys, BOX_OPEN);
        return;

    /* Navigation */
    case KEY_RIGHT: ChangePosition(p_intf, +0.01); return;
    case KEY_LEFT:  ChangePosition(p_intf, -0.01); return;

    /* Common control */
    case 'f':
        if (p_sys->p_input)
        {
            vout_thread_t *p_vout = input_GetVout(p_sys->p_input);
            if (p_vout)
            {
                bool fs = var_ToggleBool(p_playlist, "fullscreen");
                var_SetBool(p_vout, "fullscreen", fs);
                vlc_object_release(p_vout);
            }
        }
        return;

    case ' ': PlayPause(p_intf);            return;
    case 's': playlist_Stop(p_playlist);    return;
    case 'e': Eject(p_intf);                return;

    case '[': InputNavigate(p_sys->p_input, "prev-title");      return;
    case ']': InputNavigate(p_sys->p_input, "next-title");      return;
    case '<': InputNavigate(p_sys->p_input, "prev-chapter");    return;
    case '>': InputNavigate(p_sys->p_input, "next-chapter");    return;

    case 'p': playlist_Prev(p_playlist);            break;
    case 'n': playlist_Next(p_playlist);            break;
    case 'a': aout_VolumeUp(p_playlist, 1, NULL);   break;
    case 'z': aout_VolumeDown(p_playlist, 1, NULL); break;

    case 0x0c:  /* ^l */
    case KEY_CLEAR:
        break;

    default:
        return;
    }

    clear();
    return;
}

static bool HandleListKey(intf_thread_t *p_intf, int key)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    playlist_t *p_playlist = pl_Get(p_intf);

    switch(key)
    {
#ifdef __FreeBSD__
/* workaround for FreeBSD + xterm:
 * see http://www.nabble.com/curses-vs.-xterm-key-mismatch-t3574377.html */
    case KEY_SELECT:
#endif
    case KEY_END:  p_sys->i_box_idx = p_sys->i_box_lines_total - 1; break;
    case KEY_HOME: p_sys->i_box_idx = 0;                            break;
    case KEY_UP:   p_sys->i_box_idx--;                              break;
    case KEY_DOWN: p_sys->i_box_idx++;                              break;
    case KEY_PPAGE:p_sys->i_box_idx -= p_sys->i_box_height;         break;
    case KEY_NPAGE:p_sys->i_box_idx += p_sys->i_box_height;         break;
    default:
        return false;
    }

    CheckIdx(p_sys);

    if (p_sys->i_box_type == BOX_PLAYLIST)
    {
        PL_LOCK;
        p_sys->b_plidx_follow = IsIndex(p_sys, p_playlist, p_sys->i_box_idx);
        PL_UNLOCK;
    }

    return true;
}

static void HandleKey(intf_thread_t *p_intf)
{
    intf_sys_t *p_sys = p_intf->p_sys;
    int key = getch();
    int box = p_sys->i_box_type;

    if (key == -1)
        return;

    if (box == BOX_SEARCH || box == BOX_OPEN)
    {
        HandleEditBoxKey(p_intf, key, p_sys->i_box_type);
        return;
    }

    if (box == BOX_NONE)
        switch(key)
        {
#ifdef __FreeBSD__
        case KEY_SELECT:
#endif
        case KEY_END:   ChangePosition(p_intf, +.99);   return;
        case KEY_HOME:  ChangePosition(p_intf, -1.0);   return;
        case KEY_UP:    ChangePosition(p_intf, +0.05);  return;
        case KEY_DOWN:  ChangePosition(p_intf, -0.05);  return;
        default:        HandleCommonKey(p_intf, key);   return;
        }

    if (box == BOX_BROWSE   && HandleBrowseKey(p_intf, key))
        return;

    if (box == BOX_PLAYLIST && HandlePlaylistKey(p_intf, key))
        return;

    if (HandleListKey(p_intf, key))
        return;

    HandleCommonKey(p_intf, key);
}

/*
 *
 */

static void MsgCallback(msg_cb_data_t *data, const msg_item_t *msg)
{
    intf_sys_t *p_sys = data->p_sys;
    int canc = vlc_savecancel();

    vlc_mutex_lock(&p_sys->msg_lock);

    if (p_sys->msgs[p_sys->i_msgs])
        msg_Free(p_sys->msgs[p_sys->i_msgs]);
    p_sys->msgs[p_sys->i_msgs++] = msg_Copy(msg);

    if (p_sys->i_msgs == (sizeof p_sys->msgs / sizeof *p_sys->msgs))
        p_sys->i_msgs = 0;

    vlc_mutex_unlock(&p_sys->msg_lock);

    vlc_restorecancel(canc);
}

static inline void UpdateInput(intf_sys_t *p_sys, playlist_t *p_playlist)
{
    if (!p_sys->p_input)
    {
        p_sys->p_input = playlist_CurrentInput(p_playlist);
    }
    else if (p_sys->p_input->b_dead)
    {
        vlc_object_release(p_sys->p_input);
        p_sys->p_input = NULL;
    }
}

/*****************************************************************************
 * Run: ncurses thread
 *****************************************************************************/
static void Run(intf_thread_t *p_intf)
{
    intf_sys_t    *p_sys = p_intf->p_sys;
    playlist_t    *p_playlist = pl_Get(p_intf);

    int canc = vlc_savecancel();

    var_AddCallback(p_playlist, "intf-change", PlaylistChanged, p_intf);
    var_AddCallback(p_playlist, "item-change", ItemChanged, p_intf);
    var_AddCallback(p_playlist, "playlist-item-append", PlaylistChanged, p_intf);

    while (vlc_object_alive(p_intf) && !p_sys->b_exit)
    {
        UpdateInput(p_sys, p_playlist);
        Redraw(p_intf);
        HandleKey(p_intf);
    }

    var_DelCallback(p_playlist, "intf-change", PlaylistChanged, p_intf);
    var_DelCallback(p_playlist, "item-change", ItemChanged, p_intf);
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
    struct msg_cb_data_t *msg_cb_data;

    if (!p_sys)
        return VLC_ENOMEM;

    msg_cb_data = malloc(sizeof *msg_cb_data);
    if (!msg_cb_data)
    {
        free(p_sys);
        return VLC_ENOMEM;
    }

    msg_cb_data->p_sys = p_sys;
    vlc_mutex_init(&p_sys->msg_lock);
    vlc_mutex_init(&p_sys->pl_lock);
    p_sys->i_msgs = 0;
    memset(p_sys->msgs, 0, sizeof p_sys->msgs);
    p_sys->p_sub = msg_Subscribe(p_intf->p_libvlc, MsgCallback, msg_cb_data);
    msg_SubscriptionSetVerbosity(p_sys->p_sub,
            var_GetInteger(p_intf->p_libvlc, "verbose"));

    p_sys->i_box_type = BOX_PLAYLIST;
    p_sys->b_plidx_follow = true;
    p_sys->b_color = var_CreateGetBool(p_intf, "color");

    p_sys->psz_current_dir = var_CreateGetString(p_intf, "browse-dir");
    if (!p_sys->psz_current_dir || !*p_sys->psz_current_dir)
    {
        free(p_sys->psz_current_dir);
        p_sys->psz_current_dir = config_GetUserDir(VLC_HOME_DIR);
    }

    initscr();   /* Initialize the curses library */

    if (p_sys->b_color)
        start_color_and_pairs(p_intf);

    keypad(stdscr, TRUE);
    nonl();                 /* Don't do NL -> CR/NL */
    cbreak();               /* Take input chars one at a time */
    noecho();               /* Don't echo */
    curs_set(0);            /* Invisible cursor */
    timeout(1000);          /* blocking getch() */
    clear();

    /* Stop printing errors to the console */
    if(!freopen("/dev/null", "wb", stderr))
        msg_Err(p_intf, "Couldn't close stderr (%m)");

    ReadDir(p_intf);
    PlaylistRebuild(p_intf),

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

    endwin();   /* Close the ncurses interface */

    msg_Unsubscribe(p_sys->p_sub);
    vlc_mutex_destroy(&p_sys->msg_lock);
    vlc_mutex_destroy(&p_sys->pl_lock);
    for(unsigned i = 0; i < sizeof p_sys->msgs / sizeof *p_sys->msgs; i++)
        if (p_sys->msgs[i])
            msg_Free(p_sys->msgs[i]);

    free(p_sys);
}
