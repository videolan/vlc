/*****************************************************************************
 * intf_gtk.h: private Gtk+ interface description
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: intf_gtk.h,v 1.6 2001/05/23 23:08:20 stef Exp $
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
 * Drag'n'drop stuff
 *****************************************************************************/
#define DROP_ACCEPT_TEXT_URI_LIST  0
#define DROP_ACCEPT_TEXT_PLAIN     1

/*****************************************************************************
 * String sizes
 *****************************************************************************/
#define GTK_MENU_LABEL_SIZE 64

/*****************************************************************************
 * Convert user_data structures to title and chapter information
 *****************************************************************************/
#define DATA2TITLE( user_data )    ( (gint)(user_data) >> 16 )
#define DATA2CHAPTER( user_data )  ( (gint)(user_data) & 0xffff )
#define POS2DATA( title, chapter ) ( ((title) << 16) | ((chapter) & 0xffff) )

/*****************************************************************************
 * Useful inline function
 ****************************************************************************/
static __inline__ intf_thread_t * GetIntf( GtkWidget *item, char * psz_parent )
{
    return( gtk_object_get_data( GTK_OBJECT( lookup_widget(item, psz_parent) ),
                                                     "p_intf" ) );
}

/*****************************************************************************
 * intf_sys_t: description and status of Gtk+ interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    /* special actions */
    boolean_t           b_popup_changed;                   /* display menu ? */
    boolean_t           b_window_changed;        /* window display toggled ? */
    boolean_t           b_playlist_changed;    /* playlist display toggled ? */
    boolean_t           b_slider_free;                      /* slider status */

    /* menus handlers */
    boolean_t           b_title_update;  /* do we need to update title menus */
    boolean_t           b_chapter_update;            /* do we need to update
                                                               chapter menus */
    boolean_t           b_angle_update;  /* do we need to update angle menus */
    boolean_t           b_audio_update;  /* do we need to update audio menus */
    boolean_t           b_spu_update;      /* do we need to update spu menus */

    /* windows and widgets */
    GtkWidget *         p_window;                             /* main window */
    GtkWidget *         p_popup;                               /* popup menu */
    GtkWidget *         p_playlist;                              /* playlist */
    GtkWidget *         p_modules;                         /* module manager */
    GtkWidget *         p_about;                             /* about window */
    GtkWidget *         p_fileopen;                      /* file open window */
    GtkWidget *         p_disc;                     /* disc selection window */
    GtkWidget *         p_network;                  /* network stream window */
    GtkWidget *         p_preferences;                 /* preferences window */
    GtkWidget *         p_jump;                               /* jump window */

    /* The slider */
    GtkFrame *          p_slider_frame;
    GtkAdjustment *     p_adj;                   /* slider adjustment object */
    float               f_adj_oldvalue;                    /* previous value */

    /* Playlist management */
    int                 i_playing;                 /* playlist selected item */

    /* The window labels for DVD mode */
    GtkLabel *          p_label_title;
    GtkLabel *          p_label_chapter;
    gint                i_part;                           /* current chapter */

    /* XXX: Ugly kludge, see intf_gnome.c */
    void             ( *pf_gtk_callback ) ( void );
    void             ( *pf_gdk_callback ) ( void );

} intf_sys_t;

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

gint GtkModeManage   ( intf_thread_t * p_intf );
void GtkDisplayDate  ( GtkAdjustment *p_adj );


