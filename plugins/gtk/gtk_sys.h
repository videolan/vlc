/*****************************************************************************
 * gtk_sys.h: private Gtk+ interface description
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: gtk_sys.h,v 1.2 2001/03/04 03:12:00 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * drag'n'drop stuff
 *****************************************************************************/
#define DROP_ACCEPT_TEXT_URI_LIST  0
#define DROP_ACCEPT_TEXT_PLAIN     1

/*****************************************************************************
 * intf_sys_t: description and status of Gtk+ interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    /* special actions */
    vlc_mutex_t         change_lock;                      /* the change lock */

    boolean_t           b_popup_changed;                   /* display menu ? */
    boolean_t           b_window_changed;        /* window display toggled ? */
    boolean_t           b_playlist_changed;    /* playlist display toggled ? */
    boolean_t           b_scale_isfree;       /* user isn't dragging scale ? */

    /* intf_Manage callback timeout */
    int                 i_timeout;

    /* windows and widgets */
    GtkWidget *         p_window;                             /* main window */
    GtkWidget *         p_popup;                               /* popup menu */
    GtkWidget *         p_playlist;                              /* playlist */
    GtkWidget *         p_modules;                         /* module manager */
    GtkWidget *         p_about;                             /* about window */
    GtkWidget *         p_fileopen;                      /* file open window */
    GtkWidget *         p_disc;                     /* disc selection window */

    /* XXX: ugly kludge */
    void             ( *pf_gtk_callback ) ( void );
    void             ( *pf_gdk_callback ) ( void );

} intf_sys_t;

