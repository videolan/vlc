/*****************************************************************************
 * intf_gnome.h: private Gnome interface description
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_gnome.h,v 1.6 2001/04/08 07:24:47 stef Exp $
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
 * interface modes
 *****************************************************************************/
#define FILE_MODE   0
#define NET_MODE    1
#define DVD_MODE    2
#define VCD_MODE    3

/*****************************************************************************
 * intf_sys_t: description and status of Gnome interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    /* special actions */
    boolean_t           b_popup_changed;                   /* display menu ? */
    boolean_t           b_window_changed;        /* window display toggled ? */
    boolean_t           b_playlist_changed;    /* playlist display toggled ? */
    boolean_t           b_slider_free;                      /* slider status */
    boolean_t           b_menus_update;        /* do we need to update menus */

    /* Windows and widgets */
    GtkWidget *         p_window;                             /* main window */
    GtkWidget *         p_popup;                               /* popup menu */
    GtkWidget *         p_playlist;                              /* playlist */
    GtkWidget *         p_modules;                         /* module manager */
    GtkWidget *         p_about;                             /* about window */
    GtkWidget *         p_fileopen;                      /* file open window */
    GtkWidget *         p_disc;                     /* disc selection window */
    GtkWidget *         p_network;                  /* network stream window */

    /* The slider */
    GtkAdjustment *     p_adj;                   /* slider adjustment object */
    float               f_adj_oldvalue;                    /* previous value */

    /* The window labels */
    GtkLabel *          p_label_date;
    GtkLabel *          p_label_status;
    GtkLabel *          p_label_title;
    GtkLabel *          p_label_chapter;

    /* input mode management */
    boolean_t           b_mode_changed;
    gint                i_intf_mode;      /* interface mode: file, net, disc */

    gint                i_part;                  /* current chapter */

    /* XXX: Ugly kludge, see intf_gnome.c */
    void             ( *pf_gtk_callback ) ( void );
    void             ( *pf_gdk_callback ) ( void );

} intf_sys_t;
