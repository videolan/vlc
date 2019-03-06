/*****************************************************************************
 * ncurses.c : NCurses interface for vlc
 *****************************************************************************
 * Copyright © 2001-2011 the VideoLAN team
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

#define _XOPEN_SOURCE_EXTENDED 1

#include <assert.h>
#include <wchar.h>
#include <sys/stat.h>
#include <math.h>
#include <errno.h>

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>

#include <ncurses.h>

#include <vlc_interface.h>
#include <vlc_vout.h>
#include <vlc_charset.h>
#include <vlc_input.h>
#include <vlc_es.h>
#include <vlc_playlist_legacy.h>
#include <vlc_meta.h>
#include <vlc_fs.h>
#include <vlc_url.h>

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
    add_directory("browse-dir", NULL, BROWSE_TEXT, BROWSE_LONGTEXT)
vlc_module_end ()

#include "eject.c"

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
    BOX_STATS
};

static const char box_title[][19] = {
    [BOX_NONE]      = "",
    [BOX_HELP]      = " Help ",
    [BOX_INFO]      = " Information ",
    [BOX_LOG]       = " Messages ",
    [BOX_PLAYLIST]  = " Playlist ",
    [BOX_SEARCH]    = " Playlist ",
    [BOX_OPEN]      = " Playlist ",
    [BOX_BROWSE]    = " Browse ",
    [BOX_META]      = " Meta-information ",
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
    bool        file;
    char        *path;
};

struct pl_item_t
{
    input_item_t *item;
    char         *display;
};

struct intf_sys_t
{
    vlc_thread_t    thread;

    bool            color;

    /* rgb values for the color yellow */
    short           yellow_r;
    short           yellow_g;
    short           yellow_b;

    int             box_type;
    int             box_y;            // start of box content
    int             box_height;
    int             box_lines_total;  // number of lines in the box
    int             box_start;        // first line of box displayed
    int             box_idx;          // selected line

    struct
    {
        int              type;
        vlc_log_t       *item;
        char            *msg;
    } msgs[50];      // ring buffer
    int                 i_msgs;
    int                 verbosity;
    vlc_mutex_t         msg_lock;

    /* Search Box context */
    char            search_chain[20];

    /* Open Box Context */
    char            open_chain[50];

    /* File Browser context */
    char            *current_dir;
    int             n_dir_entries;
    struct dir_entry_t  **dir_entries;
    bool            show_hidden_files;

    /* Playlist context */
    struct pl_item_t    **plist;
    int             plist_entries;
    bool            need_update;
    bool            plidx_follow;
    input_item_t    *node;        /* current node */
};

/*****************************************************************************
 * Directories
 *****************************************************************************/

static void DirsDestroy(intf_sys_t *sys)
{
    while (sys->n_dir_entries) {
        struct dir_entry_t *dir_entry = sys->dir_entries[--sys->n_dir_entries];
        free(dir_entry->path);
        free(dir_entry);
    }
    free(sys->dir_entries);
    sys->dir_entries = NULL;
}

static int comdir_entries(const void *a, const void *b)
{
    struct dir_entry_t *dir_entry1 = *(struct dir_entry_t**)a;
    struct dir_entry_t *dir_entry2 = *(struct dir_entry_t**)b;

    if (dir_entry1->file == dir_entry2->file)
        return strcasecmp(dir_entry1->path, dir_entry2->path);

    return dir_entry1->file ? 1 : -1;
}

static bool IsFile(const char *current_dir, const char *entry)
{
    bool ret = true;
#ifdef S_ISDIR
    char *uri;
    if (asprintf(&uri, "%s" DIR_SEP "%s", current_dir, entry) != -1) {
        struct stat st;
        ret = vlc_stat(uri, &st) || !S_ISDIR(st.st_mode);
        free(uri);
    }
#endif
    return ret;
}

static void ReadDir(intf_thread_t *intf)
{
    intf_sys_t *sys = intf->p_sys;

    if (!sys->current_dir || !*sys->current_dir) {
        msg_Dbg(intf, "no current dir set");
        return;
    }

    DIR *current_dir = vlc_opendir(sys->current_dir);
    if (!current_dir) {
        msg_Warn(intf, "cannot open directory `%s' (%s)", sys->current_dir,
                 vlc_strerror_c(errno));
        return;
    }

    DirsDestroy(sys);

    const char *entry;
    while ((entry = vlc_readdir(current_dir))) {
        if (!sys->show_hidden_files && *entry == '.' && strcmp(entry, ".."))
            continue;

        struct dir_entry_t *dir_entry = malloc(sizeof *dir_entry);
        if (unlikely(dir_entry == NULL))
            continue;

        dir_entry->file = IsFile(sys->current_dir, entry);
        dir_entry->path = strdup(entry);
        if (unlikely(dir_entry->path == NULL))
        {
            free(dir_entry);
            continue;
        }
        TAB_APPEND(sys->n_dir_entries, sys->dir_entries, dir_entry);
        continue;
    }

    closedir(current_dir);

    if (sys->n_dir_entries > 0)
        qsort(sys->dir_entries, sys->n_dir_entries,
              sizeof(struct dir_entry_t*), &comdir_entries);
}

/*****************************************************************************
 * Adjust index position after a change (list navigation or item switching)
 *****************************************************************************/
static void CheckIdx(intf_sys_t *sys)
{
    int lines = sys->box_lines_total;
    int height = LINES - sys->box_y - 2;
    if (height > lines - 1)
        height = lines - 1;

    /* make sure the new index is within the box */
    if (sys->box_idx <= 0) {
        sys->box_idx = 0;
        sys->box_start = 0;
    } else if (sys->box_idx >= lines - 1 && lines > 0) {
        sys->box_idx = lines - 1;
        sys->box_start = sys->box_idx - height;
    }

    /* Fix box start (1st line of the box displayed) */
    if (sys->box_idx < sys->box_start ||
        sys->box_idx > height + sys->box_start + 1) {
        sys->box_start = sys->box_idx - height/2;
        if (sys->box_start < 0)
            sys->box_start = 0;
    } else if (sys->box_idx == sys->box_start - 1) {
        sys->box_start--;
    } else if (sys->box_idx == height + sys->box_start + 1) {
        sys->box_start++;
    }
}

/*****************************************************************************
 * Playlist
 *****************************************************************************/
static void PlaylistDestroy(intf_sys_t *sys)
{
    while (sys->plist_entries) {
        struct pl_item_t *p_pl_item = sys->plist[--sys->plist_entries];

        input_item_Release(p_pl_item->item);
        free(p_pl_item->display);
        free(p_pl_item);
    }
    free(sys->plist);
    sys->plist = NULL;
}

static bool PlaylistAddChild(intf_sys_t *sys, playlist_item_t *p_child,
                             const char *c, const char d)
{
    int ret;
    char *name = input_item_GetTitleFbName(p_child->p_input);
    struct pl_item_t *p_pl_item = malloc(sizeof *p_pl_item);

    if (!name || !p_pl_item)
        goto error;

    if (c && *c)
        ret = asprintf(&p_pl_item->display, "%s%c-%s", c, d, name);
    else
        ret = asprintf(&p_pl_item->display, " %s", name);
    if (ret == -1)
        goto error;

    free(name);
    p_pl_item->item = input_item_Hold(p_child->p_input);

    TAB_APPEND(sys->plist_entries, sys->plist, p_pl_item);

    return true;

error:
    free(name);
    free(p_pl_item);
    return false;
}

static void PlaylistAddNode(intf_sys_t *sys, playlist_item_t *node,
                            const char *c)
{
    for (int k = 0; k < node->i_children; k++) {
        bool last = k == node->i_children - 1;
        playlist_item_t *p_child = node->pp_children[k];
        if (!PlaylistAddChild(sys, p_child, c, last ? '`' : '|'))
            return;

        if (p_child->i_children <= 0)
            continue;

        if (*c) {
            char *tmp;
            if (asprintf(&tmp, "%s%c ", c, last ? ' ' : '|') == -1)
                return;
            PlaylistAddNode(sys, p_child, tmp);
            free(tmp);
        } else {
            PlaylistAddNode(sys, p_child, " ");
        }
    }
}

static void PlaylistRebuild(intf_thread_t *intf)
{
    intf_sys_t *sys = intf->p_sys;
    playlist_t *p_playlist = pl_Get(intf);

    PlaylistDestroy(sys);
    PlaylistAddNode(sys, &p_playlist->root, "");
}

static int ItemChanged(vlc_object_t *p_this, const char *variable,
                       vlc_value_t oval, vlc_value_t nval, void *param)
{
    playlist_t *playlist = (playlist_t *)p_this;
    intf_sys_t *sys = param;

    VLC_UNUSED(p_this); VLC_UNUSED(variable);
    VLC_UNUSED(oval); VLC_UNUSED(nval);

    playlist_Lock(playlist);
    sys->need_update = true;
    playlist_Unlock(playlist);

    return VLC_SUCCESS;
}

static int PlaylistChanged(vlc_object_t *p_this, const char *variable,
                            vlc_value_t oval, vlc_value_t nval, void *param)
{
    playlist_t *playlist = (playlist_t *)p_this;
    intf_sys_t *sys = param;
    playlist_item_t *node = playlist_CurrentPlayingItem(playlist);

    VLC_UNUSED(variable);
    VLC_UNUSED(oval); VLC_UNUSED(nval);

    sys->need_update = true;

    if (sys->node != NULL)
        input_item_Release(sys->node);
    sys->node = (node != NULL) ? input_item_Hold(node->p_input) : NULL;

    return VLC_SUCCESS;
}

/* Playlist suxx */
static int SubSearchPlaylist(intf_sys_t *sys, char *searchstring,
                              int i_start, int i_stop)
{
    for (int i = i_start + 1; i < i_stop; i++)
        if (strcasestr(sys->plist[i]->display, searchstring))
            return i;

    return -1;
}

static void SearchPlaylist(intf_sys_t *sys)
{
    char *str = sys->search_chain;
    int i_first = sys->box_idx;
    if (i_first < 0)
        i_first = 0;

    if (!str || !*str)
        return;

    int i_item = SubSearchPlaylist(sys, str, i_first + 1, sys->plist_entries);
    if (i_item < 0)
        i_item = SubSearchPlaylist(sys, str, 0, i_first);

    if (i_item > 0) {
        sys->box_idx = i_item;
        CheckIdx(sys);
    }
}

static inline bool IsIndex(intf_sys_t *sys, playlist_t *p_playlist, int i)
{
    PL_ASSERT_LOCKED;

    input_item_t *input = sys->plist[i]->item;
    playlist_item_t *item = playlist_ItemGetByInput(p_playlist, input);
    if (unlikely(item == NULL))
        return false;

    if (item->i_children == 0 && input == sys->node)
        return true;

    playlist_item_t *p_played_item = playlist_CurrentPlayingItem(p_playlist);
    if (p_played_item != NULL)
        return input == p_played_item->p_input;

    return false;
}

static void FindIndex(intf_sys_t *sys, playlist_t *p_playlist)
{
    int plidx = sys->box_idx;
    int max = sys->plist_entries;

    PL_LOCK;

    if (!IsIndex(sys, p_playlist, plidx))
        for (int i = 0; i < max; i++)
            if (IsIndex(sys, p_playlist, i)) {
                sys->box_idx = i;
                CheckIdx(sys);
                break;
            }

    PL_UNLOCK;

    sys->plidx_follow = true;
}

/****************************************************************************
 * Drawing
 ****************************************************************************/

static void start_color_and_pairs(intf_thread_t *intf)
{
    intf_sys_t *sys = intf->p_sys;

    if (!has_colors()) {
        sys->color = false;
        msg_Warn(intf, "Terminal doesn't support colors");
        return;
    }

    start_color();
    for (int i = C_DEFAULT + 1; i < C_MAX; i++)
        init_pair(i, color_pairs[i].f, color_pairs[i].b);

    /* untested, in all my terminals, !can_change_color() --funman */
    if (can_change_color()) {
        color_content(COLOR_YELLOW, &sys->yellow_r, &sys->yellow_g, &sys->yellow_b);
        init_color(COLOR_YELLOW, 960, 500, 0); /* YELLOW -> ORANGE */
    }
}

static void DrawBox(int y, int h, bool color, const char *title)
{
    int w = COLS;
    if (w <= 3 || h <= 0)
        return;

    if (color) color_set(C_BOX, NULL);

    if (!title) title = "";
    int len = strlen(title);

    if (len > w - 2)
        len = w - 2;

    mvaddch(y, 0,    ACS_ULCORNER);
    mvhline(y, 1,  ACS_HLINE, (w-len-2)/2);
    mvprintw(y, 1+(w-len-2)/2, "%s", title);
    mvhline(y, (w-len)/2+len,  ACS_HLINE, w - 1 - ((w-len)/2+len));
    mvaddch(y, w-1,ACS_URCORNER);

    for (int i = 0; i < h; i++) {
        mvaddch(++y, 0,   ACS_VLINE);
        mvaddch(y, w-1, ACS_VLINE);
    }

    mvaddch(++y, 0,   ACS_LLCORNER);
    mvhline(y,   1,   ACS_HLINE, w - 2);
    mvaddch(y,   w-1, ACS_LRCORNER);
    if (color) color_set(C_DEFAULT, NULL);
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
    int      len;

    if (w <= 0)
        return;

    va_start(vl_args, p_fmt);
    int i_ret = vasprintf(&p_buf, p_fmt, vl_args);
    va_end(vl_args);

    if (i_ret == -1)
        return;

    len = strlen(p_buf);

    wchar_t wide[len + 1];

    EnsureUTF8(p_buf);
    size_t i_char_len = mbstowcs(wide, p_buf, len);

    size_t i_width; /* number of columns */

    if (i_char_len == (size_t)-1) /* an invalid character was encountered */ {
        free(p_buf);
        return;
    }

    i_width = wcswidth(wide, i_char_len);
    if (i_width == (size_t)-1) {
        /* a non printable character was encountered */
        i_width = 0;
        for (unsigned i = 0 ; i < i_char_len ; i++) {
            int i_cwidth = wcwidth(wide[i]);
            if (i_cwidth != -1)
                i_width += i_cwidth;
        }
    }

    if (i_width <= (size_t)w) {
        mvprintw(y, x, "%s", p_buf);
        mvhline(y, x + i_width, ' ', w - i_width);
        free(p_buf);
        return;
    }

    int i_total_width = 0;
    int i = 0;
    while (i_total_width < w) {
        i_total_width += wcwidth(wide[i]);
        if (w > 7 && i_total_width >= w/2) {
            wide[i  ] = '.';
            wide[i+1] = '.';
            i_total_width -= wcwidth(wide[i]) - 2;
            if (i > 0) {
                /* we require this check only if at least one character
                 * 4 or more columns wide exists (which i doubt) */
                wide[i-1] = '.';
                i_total_width -= wcwidth(wide[i-1]) - 1;
            }

            /* find the widest string */
            int j, i_2nd_width = 0;
            for (j = i_char_len - 1; i_2nd_width < w - i_total_width; j--)
                i_2nd_width += wcwidth(wide[j]);

            /* we already have i_total_width columns filled, and we can't
             * have more than w columns */
            if (i_2nd_width > w - i_total_width)
                j++;

            wmemmove(&wide[i+2], &wide[j+1], i_char_len - j - 1);
            wide[i + 2 + i_char_len - j - 1] = '\0';
            break;
        }
        i++;
    }
    if (w <= 7) /* we don't add the '...' else we lose too much chars */
        wide[i] = '\0';

    size_t i_wlen = wcslen(wide) * 6 + 1; /* worst case */
    char ellipsized[i_wlen];
    wcstombs(ellipsized, wide, i_wlen);
    mvprintw(y, x, "%s", ellipsized);

    free(p_buf);
}

static void MainBoxWrite(intf_sys_t *sys, int l, const char *p_fmt, ...)
{
    va_list     vl_args;
    char        *p_buf;
    bool        b_selected = l == sys->box_idx;

    if (l < sys->box_start || l - sys->box_start >= sys->box_height)
        return;

    va_start(vl_args, p_fmt);
    int i_ret = vasprintf(&p_buf, p_fmt, vl_args);
    va_end(vl_args);
    if (i_ret == -1)
        return;

    if (b_selected) attron(A_REVERSE);
    mvnprintw(sys->box_y + l - sys->box_start, 1, COLS - 2, "%s", p_buf);
    if (b_selected) attroff(A_REVERSE);

    free(p_buf);
}

static int DrawMeta(intf_thread_t *intf, input_thread_t *p_input)
{
    intf_sys_t *sys = intf->p_sys;
    input_item_t *item;
    int l = 0;

    if (!p_input)
        return 0;

    item = input_GetItem(p_input);
    vlc_mutex_lock(&item->lock);
    for (int i=0; i<VLC_META_TYPE_COUNT; i++) {
        const char *meta = vlc_meta_Get(item->p_meta, i);
        if (!meta || !*meta)
            continue;

        if (sys->color) color_set(C_CATEGORY, NULL);
        MainBoxWrite(sys, l++, "  [%s]", vlc_meta_TypeToLocalizedString(i));
        if (sys->color) color_set(C_DEFAULT, NULL);
        MainBoxWrite(sys, l++, "      %s", meta);
    }
    vlc_mutex_unlock(&item->lock);

    return l;
}

static int DrawInfo(intf_thread_t *intf, input_thread_t *p_input)
{
    intf_sys_t *sys = intf->p_sys;
    input_item_t *item;
    int l = 0;

    if (!p_input)
        return 0;

    item = input_GetItem(p_input);
    vlc_mutex_lock(&item->lock);
    for (int i = 0; i < item->i_categories; i++) {
        info_category_t *p_category = item->pp_categories[i];
        info_t *p_info;

        if (sys->color) color_set(C_CATEGORY, NULL);
        MainBoxWrite(sys, l++, _("  [%s]"), p_category->psz_name);
        if (sys->color) color_set(C_DEFAULT, NULL);
        info_foreach(p_info, &p_category->infos)
            MainBoxWrite(sys, l++, _("      %s: %s"),
                         p_info->psz_name, p_info->psz_value);
    }
    vlc_mutex_unlock(&item->lock);

    return l;
}

static int DrawStats(intf_thread_t *intf, input_thread_t *p_input)
{
    intf_sys_t *sys = intf->p_sys;
    input_item_t *item;
    input_stats_t *p_stats;
    int l = 0, i_audio = 0, i_video = 0;

    if (!p_input)
        return 0;

    item = input_GetItem(p_input);
    assert(item);

    vlc_mutex_lock(&item->lock);
    p_stats = item->p_stats;

    for (int i = 0; i < item->i_es ; i++) {
        i_audio += (item->es[i]->i_cat == AUDIO_ES);
        i_video += (item->es[i]->i_cat == VIDEO_ES);
    }

    /* Input */
    if (sys->color) color_set(C_CATEGORY, NULL);
    MainBoxWrite(sys, l++, _("+-[Incoming]"));
    if (sys->color) color_set(C_DEFAULT, NULL);
    MainBoxWrite(sys, l++, _("| input bytes read : %8.0f KiB"),
            (float)(p_stats->i_read_bytes)/1024);
    MainBoxWrite(sys, l++, _("| input bitrate    :   %6.0f kb/s"),
            p_stats->f_input_bitrate*8000);
    MainBoxWrite(sys, l++, _("| demux bytes read : %8.0f KiB"),
            (float)(p_stats->i_demux_read_bytes)/1024);
    MainBoxWrite(sys, l++, _("| demux bitrate    :   %6.0f kb/s"),
            p_stats->f_demux_bitrate*8000);

    /* Video */
    if (i_video) {
        if (sys->color) color_set(C_CATEGORY, NULL);
        MainBoxWrite(sys, l++, _("+-[Video Decoding]"));
        if (sys->color) color_set(C_DEFAULT, NULL);
        MainBoxWrite(sys, l++, _("| video decoded    :    %5"PRIi64),
                p_stats->i_decoded_video);
        MainBoxWrite(sys, l++, _("| frames displayed :    %5"PRIi64),
                p_stats->i_displayed_pictures);
        MainBoxWrite(sys, l++, _("| frames lost      :    %5"PRIi64),
                p_stats->i_lost_pictures);
    }
    /* Audio*/
    if (i_audio) {
        if (sys->color) color_set(C_CATEGORY, NULL);
        MainBoxWrite(sys, l++, _("+-[Audio Decoding]"));
        if (sys->color) color_set(C_DEFAULT, NULL);
        MainBoxWrite(sys, l++, _("| audio decoded    :    %5"PRIi64),
                p_stats->i_decoded_audio);
        MainBoxWrite(sys, l++, _("| buffers played   :    %5"PRIi64),
                p_stats->i_played_abuffers);
        MainBoxWrite(sys, l++, _("| buffers lost     :    %5"PRIi64),
                p_stats->i_lost_abuffers);
    }
    if (sys->color) color_set(C_DEFAULT, NULL);

    vlc_mutex_unlock(&item->lock);

    return l;
}

static int DrawHelp(intf_thread_t *intf, input_thread_t *input)
{
    intf_sys_t *sys = intf->p_sys;
    int l = 0;

#define H(a) MainBoxWrite(sys, l++, a)

    if (sys->color) color_set(C_CATEGORY, NULL);
    H(_("[Display]"));
    if (sys->color) color_set(C_DEFAULT, NULL);
    H(_(" h,H                    Show/Hide help box"));
    H(_(" i                      Show/Hide info box"));
    H(_(" M                      Show/Hide metadata box"));
    H(_(" L                      Show/Hide messages box"));
    H(_(" P                      Show/Hide playlist box"));
    H(_(" B                      Show/Hide filebrowser"));
    H(_(" S                      Show/Hide statistics box"));
    H(_(" Esc                    Close Add/Search entry"));
    H(_(" Ctrl-l                 Refresh the screen"));
    H("");

    if (sys->color) color_set(C_CATEGORY, NULL);
    H(_("[Global]"));
    if (sys->color) color_set(C_DEFAULT, NULL);
    H(_(" q, Q, Esc              Quit"));
    H(_(" s                      Stop"));
    H(_(" <space>                Pause/Play"));
    H(_(" f                      Toggle Fullscreen"));
    H(_(" c                      Cycle through audio tracks"));
    H(_(" v                      Cycle through subtitles tracks"));
    H(_(" b                      Cycle through video tracks"));
    H(_(" n, p                   Next/Previous playlist item"));
    H(_(" [, ]                   Next/Previous title"));
    H(_(" <, >                   Next/Previous chapter"));
    /* xgettext: You can use ← and → characters */
    H(_(" <left>,<right>         Seek -/+ 1%%"));
    H(_(" a, z                   Volume Up/Down"));
    H(_(" m                      Mute"));
    /* xgettext: You can use ↑ and ↓ characters */
    H(_(" <up>,<down>            Navigate through the box line by line"));
    /* xgettext: You can use ⇞ and ⇟ characters */
    H(_(" <pageup>,<pagedown>    Navigate through the box page by page"));
    /* xgettext: You can use ↖ and ↘ characters */
    H(_(" <start>,<end>          Navigate to start/end of box"));
    H("");

    if (sys->color) color_set(C_CATEGORY, NULL);
    H(_("[Playlist]"));
    if (sys->color) color_set(C_DEFAULT, NULL);
    H(_(" r                      Toggle Random playing"));
    H(_(" l                      Toggle Loop Playlist"));
    H(_(" R                      Toggle Repeat item"));
    H(_(" o                      Order Playlist by title"));
    H(_(" O                      Reverse order Playlist by title"));
    H(_(" g                      Go to the current playing item"));
    H(_(" /                      Look for an item"));
    H(_(" ;                      Look for the next item"));
    H(_(" A                      Add an entry"));
    /* xgettext: You can use ⌫ character to translate <backspace> */
    H(_(" D, <backspace>, <del>  Delete an entry"));
    H(_(" e                      Eject (if stopped)"));
    H("");

    if (sys->color) color_set(C_CATEGORY, NULL);
    H(_("[Filebrowser]"));
    if (sys->color) color_set(C_DEFAULT, NULL);
    H(_(" <enter>                Add the selected file to the playlist"));
    H(_(" <space>                Add the selected directory to the playlist"));
    H(_(" .                      Show/Hide hidden files"));
    H("");

    if (sys->color) color_set(C_CATEGORY, NULL);
    H(_("[Player]"));
    if (sys->color) color_set(C_DEFAULT, NULL);
    /* xgettext: You can use ↑ and ↓ characters */
    H(_(" <up>,<down>            Seek +/-5%%"));

#undef H
    (void) input;
    return l;
}

static int DrawBrowse(intf_thread_t *intf, input_thread_t *input)
{
    intf_sys_t *sys = intf->p_sys;

    for (int i = 0; i < sys->n_dir_entries; i++) {
        struct dir_entry_t *dir_entry = sys->dir_entries[i];
        char type = dir_entry->file ? ' ' : '+';

        if (sys->color)
            color_set(dir_entry->file ? C_DEFAULT : C_FOLDER, NULL);
        MainBoxWrite(sys, i, " %c %s", type, dir_entry->path);
    }

    (void) input;
    return sys->n_dir_entries;
}

static int DrawPlaylist(intf_thread_t *intf, input_thread_t *input)
{
    intf_sys_t *sys = intf->p_sys;
    playlist_t *p_playlist = pl_Get(intf);

    PL_LOCK;
    if (sys->need_update) {
        PlaylistRebuild(intf);
        sys->need_update = false;
    }
    PL_UNLOCK;

    if (sys->plidx_follow)
        FindIndex(sys, p_playlist);

    for (int i = 0; i < sys->plist_entries; i++) {
        char c;
        playlist_item_t *current;
        input_item_t *item = sys->plist[i]->item;

        PL_LOCK;
        current = playlist_CurrentPlayingItem(p_playlist);

        if ((sys->node != NULL && item == sys->node) ||
            (sys->node == NULL && current != NULL && item == current->p_input))
            c = '*';
        else if (current != NULL && current->p_input == item)
            c = '>';
        else
            c = ' ';
        PL_UNLOCK;

        if (sys->color) color_set(i%3 + C_PLAYLIST_1, NULL);
        MainBoxWrite(sys, i, "%c%s", c, sys->plist[i]->display);
        if (sys->color) color_set(C_DEFAULT, NULL);
    }

    (void) input;
    return sys->plist_entries;
}

static int DrawMessages(intf_thread_t *intf, input_thread_t *input)
{
    intf_sys_t *sys = intf->p_sys;
    int l = 0;

    vlc_mutex_lock(&sys->msg_lock);
    int i = sys->i_msgs;
    for(;;) {
        vlc_log_t *msg = sys->msgs[i].item;
        if (msg) {
            if (sys->color)
                color_set(sys->msgs[i].type + C_INFO, NULL);
            MainBoxWrite(sys, l++, "[%s] %s", msg->psz_module, sys->msgs[i].msg);
        }

        if (++i == sizeof sys->msgs / sizeof *sys->msgs)
            i = 0;

        if (i == sys->i_msgs) /* did we loop around the ring buffer ? */
            break;
    }

    vlc_mutex_unlock(&sys->msg_lock);
    if (sys->color)
        color_set(C_DEFAULT, NULL);

    (void) input;
    return l;
}

static int DrawStatus(intf_thread_t *intf, input_thread_t *p_input)
{
    intf_sys_t     *sys = intf->p_sys;
    playlist_t     *p_playlist = pl_Get(intf);
    const char *name = _("VLC media player");
    const size_t name_len = strlen(name) + sizeof(PACKAGE_VERSION);
    int y = 0;
    const char *repeat, *loop, *random;


    /* Title */
    int padding = COLS - name_len; /* center title */
    if (padding < 0)
        padding = 0;

    attrset(A_REVERSE);
    if (sys->color) color_set(C_TITLE, NULL);
    DrawEmptyLine(y, 0, COLS);
    mvnprintw(y++, padding / 2, COLS, "%s %s", name, PACKAGE_VERSION);
    if (sys->color) color_set(C_STATUS, NULL);
    attroff(A_REVERSE);

    y++; /* leave a blank line */

    repeat = var_GetBool(p_playlist, "repeat") ? _("[Repeat]") : "";
    random = var_GetBool(p_playlist, "random") ? _("[Random]") : "";
    loop   = var_GetBool(p_playlist, "loop")   ? _("[Loop]")    : "";

    if (p_input) {
        vlc_value_t val;
        char *path, *uri;

        uri = input_item_GetURI(input_GetItem(p_input));
        path = vlc_uri2path(uri);

        mvnprintw(y++, 0, COLS, _(" Source   : %s"), path?path:uri);
        free(uri);
        free(path);

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
            float volume;

        case INIT_S:
        case END_S:
            y += 2;
            break;

        case PLAYING_S:
        case OPENING_S:
        case PAUSE_S:
            mvnprintw(y++, 0, COLS, _(input_state[val.i_int]),
                        repeat, random, loop);
            /* fall-through */

        default:
            secstotimestr(buf1, SEC_FROM_VLC_TICK(var_GetInteger(p_input, "time")));
            secstotimestr(buf2, SEC_FROM_VLC_TICK(var_GetInteger(p_input, "length")));

            mvnprintw(y++, 0, COLS, _(" Position : %s/%s"), buf1, buf2);

            volume = playlist_VolumeGet(p_playlist);
            int mute = playlist_MuteGet(p_playlist);
            mvnprintw(y++, 0, COLS,
                      mute ? _(" Volume   : Mute") :
                      volume >= 0.f ? _(" Volume   : %3ld%%") : _(" Volume   : ----"),
                      lroundf(volume * 100.f));

            if (!var_Get(p_input, "title", &val)) {
                int i_title_count = var_CountChoices(p_input, "title");
                if (i_title_count > 0)
                    mvnprintw(y++, 0, COLS, _(" Title    : %"PRId64"/%d"),
                               val.i_int, i_title_count);
            }

            if (!var_Get(p_input, "chapter", &val)) {
                int i_chapter_count = var_CountChoices(p_input, "chapter");
                if (i_chapter_count > 0) mvnprintw(y++, 0, COLS, _(" Chapter  : %"PRId64"/%d"),
                               val.i_int, i_chapter_count);
            }
        }
    } else {
        mvnprintw(y++, 0, COLS, _(" Source: <no current item>"));
        mvnprintw(y++, 0, COLS, " %s%s%s", repeat, random, loop);
        mvnprintw(y++, 0, COLS, _(" [ h for help ]"));
        DrawEmptyLine(y++, 0, COLS);
    }

    if (sys->color) color_set(C_DEFAULT, NULL);
    DrawBox(y++, 1, sys->color, ""); /* position slider */
    DrawEmptyLine(y, 1, COLS-2);
    if (p_input)
        DrawLine(y, 1, (int)((COLS-2) * var_GetFloat(p_input, "position")));

    y += 2; /* skip slider and box */

    return y;
}

static void FillTextBox(intf_sys_t *sys)
{
    int width = COLS - 2;

    DrawEmptyLine(7, 1, width);
    if (sys->box_type == BOX_OPEN)
        mvnprintw(7, 1, width, _("Open: %s"), sys->open_chain);
    else
        mvnprintw(7, 1, width, _("Find: %s"), sys->search_chain);
}

static void FillBox(intf_thread_t *intf, input_thread_t *input)
{
    intf_sys_t *sys = intf->p_sys;
    static int (* const draw[]) (intf_thread_t *, input_thread_t *) = {
        [BOX_HELP]      = DrawHelp,
        [BOX_INFO]      = DrawInfo,
        [BOX_META]      = DrawMeta,
        [BOX_STATS]     = DrawStats,
        [BOX_BROWSE]    = DrawBrowse,
        [BOX_PLAYLIST]  = DrawPlaylist,
        [BOX_SEARCH]    = DrawPlaylist,
        [BOX_OPEN]      = DrawPlaylist,
        [BOX_LOG]       = DrawMessages,
    };

    sys->box_lines_total = draw[sys->box_type](intf, input);

    if (sys->box_type == BOX_SEARCH || sys->box_type == BOX_OPEN)
        FillTextBox(sys);
}

static void Redraw(intf_thread_t *intf, input_thread_t *input)
{
    intf_sys_t *sys   = intf->p_sys;
    int         box   = sys->box_type;
    int         y     = DrawStatus(intf, input);

    sys->box_height = LINES - y - 2;
    DrawBox(y++, sys->box_height, sys->color, _(box_title[box]));

    sys->box_y = y;

    if (box != BOX_NONE) {
        FillBox(intf, input);

        if (sys->box_lines_total == 0)
            sys->box_start = 0;
        else if (sys->box_start > sys->box_lines_total - 1)
            sys->box_start = sys->box_lines_total - 1;
        y += __MIN(sys->box_lines_total - sys->box_start,
                   sys->box_height);
    }

    while (y < LINES - 1)
        DrawEmptyLine(y++, 1, COLS - 2);

    refresh();
}

static void ChangePosition(input_thread_t *p_input, float increment)
{
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

static char *GetDiscDevice(const char *name)
{
    static const struct { const char *s; size_t n; const char *v; } devs[] =
    {
        { "cdda://", 7, "cd-audio", },
        { "dvd://",  6, "dvd",      },
        { "vcd://",  6, "vcd",      },
    };
    char *device;

    for (unsigned i = 0; i < sizeof devs / sizeof *devs; i++) {
        size_t n = devs[i].n;
        if (!strncmp(name, devs[i].s, n)) {
            if (name[n] == '@' || name[n] == '\0')
                return config_GetPsz(devs[i].v);
            /* Omit the beginning MRL-selector characters */
            return strdup(name + n);
        }
    }

    device = strdup(name);

    if (device) /* Remove what we have after @ */
        device[strcspn(device, "@")] = '\0';

    return device;
}

static void Eject(intf_thread_t *intf, input_thread_t *p_input)
{
    char *device, *name;
    playlist_t * p_playlist = pl_Get(intf);

    /* If there's a stream playing, we aren't allowed to eject ! */
    if (p_input)
        return;

    PL_LOCK;

    if (!playlist_CurrentPlayingItem(p_playlist)) {
        PL_UNLOCK;
        return;
    }

    name = playlist_CurrentPlayingItem(p_playlist)->p_input->psz_name;
    device = name ? GetDiscDevice(name) : NULL;

    PL_UNLOCK;

    if (device) {
        intf_Eject(intf, device);
        free(device);
    }
}

static void PlayPause(intf_thread_t *intf, input_thread_t *p_input)
{
    if (p_input) {
        int64_t state = var_GetInteger( p_input, "state" );
        state = (state != PLAYING_S) ? PLAYING_S : PAUSE_S;
        var_SetInteger( p_input, "state", state );
    } else
        playlist_Play(pl_Get(intf));
}

static void AddItem(intf_thread_t *intf, const char *path)
{
    char *uri = vlc_path2uri(path, NULL);
    if (uri == NULL)
        return;

    input_item_t *item = input_item_New(uri, NULL);
    free(uri);
    if (unlikely(item == NULL))
        return;

    playlist_t *playlist = pl_Get(intf);
    playlist_item_t *node;

    playlist_Lock(playlist);
    node = playlist_CurrentPlayingItem(playlist);

    while (node != NULL) {
        if (node == playlist->p_playing)
            break;
        node = node->p_parent;
    }

    if (node == NULL)
        node = playlist->p_playing;

    playlist_NodeAddInput(playlist, item, node, PLAYLIST_END);
    playlist_Unlock(playlist);

    input_item_Release(item);
}

static inline void BoxSwitch(intf_sys_t *sys, int box)
{
    sys->box_type = (sys->box_type == box) ? BOX_NONE : box;
    sys->box_start = 0;
    sys->box_idx = 0;
}

static bool HandlePlaylistKey(intf_thread_t *intf, int key)
{
    intf_sys_t *sys = intf->p_sys;
    playlist_t *p_playlist = pl_Get(intf);

    switch(key)
    {
    /* Playlist Settings */
    case 'r': var_ToggleBool(p_playlist, "random"); return true;
    case 'l': var_ToggleBool(p_playlist, "loop");   return true;
    case 'R': var_ToggleBool(p_playlist, "repeat"); return true;

    /* Playlist sort */
    case 'o':
    case 'O':
        playlist_Lock(p_playlist);
        playlist_RecursiveNodeSort(p_playlist, &p_playlist->root,
                                   SORT_TITLE_NODES_FIRST,
                                   (key == 'o')? ORDER_NORMAL : ORDER_REVERSE);
        sys->need_update = true;
        playlist_Unlock(p_playlist);
        return true;

    case ';':
        SearchPlaylist(sys);
        return true;

    case 'g':
        FindIndex(sys, p_playlist);
        return true;

    /* Deletion */
    case 'D':
    case KEY_BACKSPACE:
    case 0x7f:
    case KEY_DC:
    {
        input_item_t *input = sys->plist[sys->box_idx]->item;
        playlist_item_t *item;

        PL_LOCK;
        item = playlist_ItemGetByInput(p_playlist, input);
        playlist_NodeDelete(p_playlist, item);

        if (sys->box_idx >= sys->box_lines_total - 1)
            sys->box_idx = sys->box_lines_total - 2;
        sys->need_update = true;
        PL_UNLOCK;
        return true;
    }

    case KEY_ENTER:
    case '\r':
    case '\n':
    {
        struct pl_item_t *p_pl_item = sys->plist[sys->box_idx];
        if (p_pl_item == NULL)
            return false;

        playlist_item_t *item;

        playlist_Lock(p_playlist);
        item = playlist_ItemGetByInput(p_playlist, p_pl_item->item);

        if (item->i_children) {
            playlist_item_t *parent = item;

            if (item->i_children == -1) {
                while (parent->p_parent != NULL)
                    parent = parent->p_parent;
            } else {
                if (sys->node != NULL)
                    input_item_Release(sys->node);
                sys->node = parent->p_input ? input_item_Hold(parent->p_input)
                                            : NULL;
                item = NULL;
            }

            playlist_ViewPlay(p_playlist, parent, item);
        } else {   /* We only want to set the current node */
            playlist_Control(p_playlist, PLAYLIST_STOP, true);
            if (sys->node != NULL)
                input_item_Release(sys->node);
            sys->node = p_pl_item->item ? input_item_Hold(p_pl_item->item)
                                        : NULL;
        }
        playlist_Unlock(p_playlist);

        sys->plidx_follow = true;
        return true;
    }
    }

    return false;
}

static bool HandleBrowseKey(intf_thread_t *intf, int key)
{
    intf_sys_t *sys = intf->p_sys;
    struct dir_entry_t *dir_entry;

    switch(key)
    {
    case '.':
        sys->show_hidden_files = !sys->show_hidden_files;
        ReadDir(intf);
        return true;

    case KEY_ENTER:
    case '\r':
    case '\n':
    case ' ':
        dir_entry = sys->dir_entries[sys->box_idx];
        char *path;
        if (asprintf(&path, "%s" DIR_SEP "%s", sys->current_dir,
                     dir_entry->path) == -1)
            return true;

        if (!dir_entry->file && key != ' ') {
            free(sys->current_dir);
            sys->current_dir = path;
            ReadDir(intf);

            sys->box_start = 0;
            sys->box_idx = 0;
            return true;
        }

        AddItem(intf, path);
        free(path);
        BoxSwitch(sys, BOX_PLAYLIST);
        return true;
    }

    return false;
}

static void OpenSelection(intf_thread_t *intf)
{
    intf_sys_t *sys = intf->p_sys;

    AddItem(intf, sys->open_chain);
    sys->plidx_follow = true;
}

static void HandleEditBoxKey(intf_thread_t *intf, int key, int box)
{
    intf_sys_t *sys = intf->p_sys;
    bool search = box == BOX_SEARCH;
    char *str = search ? sys->search_chain: sys->open_chain;
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
            SearchPlaylist(sys);
        else
            OpenSelection(intf);

        sys->box_type = BOX_PLAYLIST;
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
        if (getch() == ERR)
            sys->box_type = BOX_PLAYLIST;
        return;

    case KEY_BACKSPACE:
    case 0x7f:
        RemoveLastUTF8Entity(str, len);
        break;

    default:
        if (len + 1 < (search ? sizeof sys->search_chain
                              : sizeof sys->open_chain)) {
            str[len + 0] = key;
            str[len + 1] = '\0';
        }
    }

    if (search)
        SearchPlaylist(sys);
}

static void InputNavigate(input_thread_t* p_input, const char *var)
{
    if (p_input)
        var_TriggerCallback(p_input, var);
}

static void CycleESTrack(input_thread_t *input, const char *var)
{
    if (!input)
        return;

    vlc_value_t *list;
    size_t count;

    if (var_Change(input, var, VLC_VAR_GETCHOICES,
                   &count, &list, (char ***)NULL) < 0)
        return;

    int64_t current = var_GetInteger(input, var);

    size_t i;
    for (i = 0; i < count; i++)
        if (list[i].i_int == current)
            break;

    if (++i >= count)
        i = 0;
    var_SetInteger(input, var, list[i].i_int);
    free(list);
}

static void HandleCommonKey(intf_thread_t *intf, input_thread_t *input,
                            int key)
{
    intf_sys_t *sys = intf->p_sys;
    playlist_t *p_playlist = pl_Get(intf);
    switch(key)
    {
    case 0x1b:  /* ESC */
        /* See comment in HandleEditBoxKey() */
        if (getch() != ERR)
            return;
        /* fall through */
    case 'q':
    case 'Q':
    case KEY_EXIT:
        libvlc_Quit(vlc_object_instance(intf));
        return;

    case 'h':
    case 'H': BoxSwitch(sys, BOX_HELP);       return;
    case 'i': BoxSwitch(sys, BOX_INFO);       return;
    case 'M': BoxSwitch(sys, BOX_META);       return;
    case 'L': BoxSwitch(sys, BOX_LOG);        return;
    case 'P': BoxSwitch(sys, BOX_PLAYLIST);   return;
    case 'B': BoxSwitch(sys, BOX_BROWSE);     return;
    case 'S': BoxSwitch(sys, BOX_STATS);      return;

    case '/': /* Search */
        sys->plidx_follow = false;
        BoxSwitch(sys, BOX_SEARCH);
        return;

    case 'A': /* Open */
        sys->open_chain[0] = '\0';
        BoxSwitch(sys, BOX_OPEN);
        return;

    /* Navigation */
    case KEY_RIGHT: ChangePosition(input, +0.01); return;
    case KEY_LEFT:  ChangePosition(input, -0.01); return;

    /* Common control */
    case 'f':
        if (input) {
            vout_thread_t *p_vout = input_GetVout(input);
            if (p_vout) {
                bool fs = var_ToggleBool(p_playlist, "fullscreen");
                var_SetBool(p_vout, "fullscreen", fs);
                vout_Release(p_vout);
            }
        }
        return;

    case ' ': PlayPause(intf, input);       return;
    case 's': playlist_Stop(p_playlist);    return;
    case 'e': Eject(intf, input);           return;

    case '[': InputNavigate(input, "prev-title");      return;
    case ']': InputNavigate(input, "next-title");      return;
    case '<': InputNavigate(input, "prev-chapter");    return;
    case '>': InputNavigate(input, "next-chapter");    return;

    case 'p': playlist_Prev(p_playlist);            break;
    case 'n': playlist_Next(p_playlist);            break;
    case 'a': playlist_VolumeUp(p_playlist, 1, NULL);   break;
    case 'z': playlist_VolumeDown(p_playlist, 1, NULL); break;
    case 'm': playlist_MuteToggle(p_playlist); break;

    case 'c': CycleESTrack(input, "audio-es"); break;
    case 'v': CycleESTrack(input, "spu-es");   break;
    case 'b': CycleESTrack(input, "video-es"); break;

    case 0x0c:  /* ^l */
    case KEY_CLEAR:
        break;

    default:
        return;
    }

    clear();
    return;
}

static bool HandleListKey(intf_thread_t *intf, int key)
{
    intf_sys_t *sys = intf->p_sys;
    playlist_t *p_playlist = pl_Get(intf);

    switch(key)
    {
#ifdef __FreeBSD__
/* workaround for FreeBSD + xterm:
 * see http://www.nabble.com/curses-vs.-xterm-key-mismatch-t3574377.html */
    case KEY_SELECT:
#endif
    case KEY_END:  sys->box_idx = sys->box_lines_total - 1; break;
    case KEY_HOME: sys->box_idx = 0;                            break;
    case KEY_UP:   sys->box_idx--;                              break;
    case KEY_DOWN: sys->box_idx++;                              break;
    case KEY_PPAGE:sys->box_idx -= sys->box_height;         break;
    case KEY_NPAGE:sys->box_idx += sys->box_height;         break;
    default:
        return false;
    }

    CheckIdx(sys);

    if (sys->box_type == BOX_PLAYLIST) {
        PL_LOCK;
        sys->plidx_follow = IsIndex(sys, p_playlist, sys->box_idx);
        PL_UNLOCK;
    }

    return true;
}

static void HandleKey(intf_thread_t *intf, input_thread_t *input)
{
    intf_sys_t *sys = intf->p_sys;
    int key = getch();
    int box = sys->box_type;

    if (key == -1)
        return;

    if (box == BOX_SEARCH || box == BOX_OPEN) {
        HandleEditBoxKey(intf, key, sys->box_type);
        return;
    }

    if (box == BOX_NONE)
        switch(key)
        {
#ifdef __FreeBSD__
        case KEY_SELECT:
#endif
        case KEY_END:   ChangePosition(input, +.99);       return;
        case KEY_HOME:  ChangePosition(input, -1.0);       return;
        case KEY_UP:    ChangePosition(input, +0.05);      return;
        case KEY_DOWN:  ChangePosition(input, -0.05);      return;
        default:        HandleCommonKey(intf, input, key); return;
        }

    if (box == BOX_BROWSE   && HandleBrowseKey(intf, key))
        return;

    if (box == BOX_PLAYLIST && HandlePlaylistKey(intf, key))
        return;

    if (HandleListKey(intf, key))
        return;

    HandleCommonKey(intf, input, key);
}

/*
 *
 */
static vlc_log_t *msg_Copy (const vlc_log_t *msg)
{
    vlc_log_t *copy = (vlc_log_t *)xmalloc (sizeof (*copy));
    copy->i_object_id = msg->i_object_id;
    copy->psz_object_type = msg->psz_object_type;
    copy->psz_module = strdup (msg->psz_module);
    copy->psz_header = msg->psz_header ? strdup (msg->psz_header) : NULL;
    return copy;
}

static void msg_Free (vlc_log_t *msg)
{
    free ((char *)msg->psz_module);
    free ((char *)msg->psz_header);
    free (msg);
}

static void MsgCallback(void *data, int type, const vlc_log_t *msg,
                        const char *format, va_list ap)
{
    intf_sys_t *sys = data;
    char *text;

    if (sys->verbosity < 0
     || sys->verbosity < (type - VLC_MSG_ERR)
     || vasprintf(&text, format, ap) == -1)
        return;

    vlc_mutex_lock(&sys->msg_lock);

    sys->msgs[sys->i_msgs].type = type;
    if (sys->msgs[sys->i_msgs].item != NULL)
        msg_Free(sys->msgs[sys->i_msgs].item);
    sys->msgs[sys->i_msgs].item = msg_Copy(msg);
    free(sys->msgs[sys->i_msgs].msg);
    sys->msgs[sys->i_msgs].msg = text;

    if (++sys->i_msgs == (sizeof sys->msgs / sizeof *sys->msgs))
        sys->i_msgs = 0;

    vlc_mutex_unlock(&sys->msg_lock);
}

static const struct vlc_logger_operations log_ops = { MsgCallback, NULL };

/*****************************************************************************
 * Run: ncurses thread
 *****************************************************************************/
static void *Run(void *data)
{
    intf_thread_t *intf = data;
    playlist_t    *p_playlist = pl_Get(intf);

    for (;;) {
        vlc_testcancel();

        int canc = vlc_savecancel();
        input_thread_t *input = playlist_CurrentInput(p_playlist);

        Redraw(intf, input);
        HandleKey(intf, input);
        if (input)
            input_Release(input);
        vlc_restorecancel(canc);
    }
    vlc_assert_unreachable();
}

/*****************************************************************************
 * Open: initialize and create window
 *****************************************************************************/
static int Open(vlc_object_t *p_this)
{
    intf_thread_t *intf = (intf_thread_t *)p_this;
    intf_sys_t    *sys  = intf->p_sys = calloc(1, sizeof(intf_sys_t));
    playlist_t    *p_playlist = pl_Get(intf);

    if (!sys)
        return VLC_ENOMEM;

    vlc_mutex_init(&sys->msg_lock);

    sys->verbosity = var_InheritInteger(intf, "verbose");
    vlc_LogSet(vlc_object_instance(intf), &log_ops, sys);

    sys->box_type = BOX_PLAYLIST;
    sys->plidx_follow = true;
    sys->color = var_CreateGetBool(intf, "color");

    sys->current_dir = var_CreateGetNonEmptyString(intf, "browse-dir");
    if (!sys->current_dir)
        sys->current_dir = config_GetUserDir(VLC_HOME_DIR);

    initscr();   /* Initialize the curses library */

    if (sys->color)
        start_color_and_pairs(intf);

    keypad(stdscr, TRUE);
    nonl();                 /* Don't do NL -> CR/NL */
    cbreak();               /* Take input chars one at a time */
    noecho();               /* Don't echo */
    curs_set(0);            /* Invisible cursor */
    timeout(1000);          /* blocking getch() */
    clear();

    /* Stop printing errors to the console */
    if (!freopen("/dev/null", "wb", stderr))
        msg_Err(intf, "Couldn't close stderr (%s)", vlc_strerror_c(errno));

    ReadDir(intf);
    PL_LOCK;
    PlaylistRebuild(intf),
    PL_UNLOCK;

    var_AddCallback(p_playlist, "item-change", ItemChanged, sys);
    var_AddCallback(p_playlist, "playlist-item-append", PlaylistChanged, sys);

    if (vlc_clone(&sys->thread, Run, intf, VLC_THREAD_PRIORITY_LOW))
        abort(); /* TODO */

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface window
 *****************************************************************************/
static void Close(vlc_object_t *p_this)
{
    intf_thread_t *intf = (intf_thread_t *)p_this;
    intf_sys_t *sys = intf->p_sys;
    playlist_t *playlist = pl_Get(intf);

    vlc_cancel(sys->thread);
    vlc_join(sys->thread, NULL);

    var_DelCallback(playlist, "playlist-item-append", PlaylistChanged, sys);
    var_DelCallback(playlist, "item-change", ItemChanged, sys);

    PlaylistDestroy(sys);
    DirsDestroy(sys);

    free(sys->current_dir);

    if (can_change_color())
        /* Restore yellow to its original color */
        init_color(COLOR_YELLOW, sys->yellow_r, sys->yellow_g, sys->yellow_b);

    endwin();   /* Close the ncurses interface */

    vlc_LogSet(vlc_object_instance(p_this), NULL, NULL);
    vlc_mutex_destroy(&sys->msg_lock);
    for(unsigned i = 0; i < sizeof sys->msgs / sizeof *sys->msgs; i++) {
        if (sys->msgs[i].item)
            msg_Free(sys->msgs[i].item);
        free(sys->msgs[i].msg);
    }
    free(sys);
}
