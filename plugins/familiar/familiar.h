/*****************************************************************************
 * familiar.h: private Gtk+ interface description
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: familiar.h,v 1.1 2002/07/22 13:49:42 jpsaman Exp $
 *
 * Authors: Jean-Paul Saman <jpsaman@wxs.nl>
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
 * Drag'n'drop stuff
 *****************************************************************************/
#define DROP_ACCEPT_TEXT_URI_LIST  0
#define DROP_ACCEPT_TEXT_PLAIN     1
#define DROP_ACCEPT_STRING         2

#define MAX_ATEXIT                 10

/*****************************************************************************
 * intf_sys_t: description and status of Gtk+ interface
 *****************************************************************************/
struct intf_sys_t
{
    /* special actions */
    vlc_bool_t          b_playing;
    vlc_bool_t          b_popup_changed;                   /* display menu ? */
    vlc_bool_t          b_window_changed;        /* window display toggled ? */
    vlc_bool_t          b_playlist_changed;    /* playlist display toggled ? */
    vlc_bool_t          b_slider_free;                      /* slider status */

    /* menus handlers */
    vlc_bool_t          b_program_update;   /* do we need to update programs
                                                                        menu */
    vlc_bool_t          b_title_update;  /* do we need to update title menus */
    vlc_bool_t          b_chapter_update;            /* do we need to update
                                                               chapter menus */
    vlc_bool_t          b_audio_update;  /* do we need to update audio menus */
    vlc_bool_t          b_spu_update;      /* do we need to update spu menus */

    /* windows and widgets */
    GtkWidget *         p_window;                             /* main window */
    GtkWidget *         p_popup;                               /* popup menu */
    GtkWidget *         p_playwin;                               /* playlist */
    GtkWidget *         p_modules;                         /* module manager */
    GtkWidget *         p_about;                             /* about window */
    GtkWidget *         p_open;                          /* multiopen window */
    GtkWidget *         p_jump;                               /* jump window */

    GtkTooltips *       p_tooltips;                              /* tooltips */

    /* The input thread */
    input_thread_t *    p_input;

    /* The slider */
    GtkFrame *          p_slider_frame;
    GtkAdjustment *     p_adj;                   /* slider adjustment object */
    float               f_adj_oldvalue;                    /* previous value */

    /* The messages window */
    GtkWidget *         p_messages;                       /* messages window */
    GtkText *           p_messages_text;                   /* messages frame */
    msg_subscription_t* p_sub;                  /* message bank subscription */

    /* Playlist management */
    int                 i_playing;                 /* playlist selected item */

    /* The window labels for DVD mode */
    GtkLabel *          p_label_title;
    GtkLabel *          p_label_chapter;
    gint                i_part;                           /* current chapter */

    /* XXX: Ugly kludge, see gtk.c */
    void             ( *pf_callback[MAX_ATEXIT] ) ( void );
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
gint GtkModeManage   ( intf_thread_t * p_intf );
void GtkDisplayDate  ( GtkAdjustment *p_adj );

/*****************************************************************************
 * Useful macro
 ****************************************************************************/
#define  GtkGetIntf( widget ) __GtkGetIntf( GTK_WIDGET( widget ) )
void * __GtkGetIntf( GtkWidget * );

