/*****************************************************************************
 * familiar.h: private Gtk+ interface description
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: familiar.h,v 1.6.2.2 2002/10/02 19:58:45 jpsaman Exp $
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

#define MAX_ATEXIT                 10

/*****************************************************************************
 * intf_sys_t: description and status of Gtk+ interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    /* windows and widgets */
    GtkWidget *         p_window;                             /* main window */
    GtkNotebook *       p_notebook;
    GtkProgressBar *    p_progess;
    GtkCList    *       p_clist;
//    GHashTable *        config_hash_table;
    boolean_t           b_autoplayfile;

    /* XXX: Ugly kludge, see gtk.c */
    void             ( *pf_callback[MAX_ATEXIT] ) ( void );
} intf_sys_t;

/*****************************************************************************
 * Useful macro
 ****************************************************************************/
#define  GtkGetIntf( widget ) __GtkGetIntf( GTK_WIDGET( widget ) )
void * __GtkGetIntf( GtkWidget * );


