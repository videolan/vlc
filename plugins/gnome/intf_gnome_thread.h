/*****************************************************************************
 * intf_gnome_thread.h: Gnome thread
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * intf_sys_t: description and status of Gnome interface
 *****************************************************************************/
typedef struct gnome_thread_s
{
    vlc_thread_t        thread_id;                /* id for thread functions */
    boolean_t           b_die;                                 /* `die' flag */
    boolean_t           b_error;                             /* `error' flag */

    /* special actions */
    vlc_mutex_t         change_lock;                      /* the change lock */

    boolean_t           b_activity_changed;       /* vout activity toggled ? */
    boolean_t           b_activity;                         /* vout activity */

    boolean_t           b_popup_changed;                   /* display menu ? */

    boolean_t           b_window_changed;        /* window display toggled ? */
    boolean_t           b_window;                        /* display window ? */

    boolean_t           b_playlist_changed;    /* playlist display toggled ? */
    boolean_t           b_playlist;                    /* display playlist ? */

    /* windows and widgets */
    GtkWidget *         p_window;                             /* main window */
    GtkWidget *         p_popup;                               /* popup menu */
    GtkWidget *         p_playlist;                              /* playlist */
    GtkWidget *         p_about;                             /* about window */

} gnome_thread_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
void        GnomeThread              ( gnome_thread_t *p_gnome );

