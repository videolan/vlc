/*****************************************************************************
 * intf_gnome.h: Gnome interface
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
typedef struct intf_sys_s
{
    /* X11 generic properties */
    Display *           p_display;                    /* X11 display pointer */
    int                 i_screen;                              /* X11 screen */
    Atom                wm_protocols;
    Atom                wm_delete_window;

    /* Main window properties */
    Window              window;                               /* main window */
    GC                  gc;               /* graphic context for main window */
    int                 i_width;                     /* width of main window */
    int                 i_height;                   /* height of main window */
    Colormap            colormap;               /* colormap used (8bpp only) */

    /* Screen saver properties */
    int                 i_ss_count;              /* enabling/disabling count */
    int                 i_ss_timeout;                             /* timeout */
    int                 i_ss_interval;           /* interval between changes */
    int                 i_ss_blanking;                      /* blanking mode */
    int                 i_ss_exposure;                      /* exposure mode */

    /* Mouse pointer properties */
    boolean_t           b_mouse;         /* is the mouse pointer displayed ? */

    /* Gnome part properties */
    gnome_thread_t *    p_gnome;

} intf_sys_t;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  GnomeCreateWindow             ( intf_thread_t *p_intf );
static void GnomeDestroyWindow            ( intf_thread_t *p_intf );
static void GnomeManageInterface          ( intf_thread_t *p_intf );
static gint GnomeManageMain               ( gpointer p_data );
static void GnomeManageWindow             ( intf_thread_t *p_intf );
static void GnomeEnableScreenSaver        ( intf_thread_t *p_intf );
static void GnomeDisableScreenSaver       ( intf_thread_t *p_intf );
static void GnomeTogglePointer            ( intf_thread_t *p_intf );

