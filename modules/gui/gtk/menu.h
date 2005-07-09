/*****************************************************************************
 * gtk_menu.h: prototypes for menu functions
 *****************************************************************************
 * Copyright (C) 1999, 2000 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Stéphane Borel <stef@via.ecp.fr>
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

gint GtkSetupMenus( intf_thread_t * );

/*****************************************************************************
 * String sizes
 *****************************************************************************/
#define GTK_MENU_LABEL_SIZE 64

/*****************************************************************************
 * Convert user_data structures to title and chapter information
 *****************************************************************************/
#define DATA2TITLE( user_data )    ( (gint)((long)(user_data)) >> 16 )
#define DATA2CHAPTER( user_data )  ( (gint)((long)(user_data)) & 0xffff )
#define POS2DATA( title, chapter ) ( 0 + ( ((title) << 16) \
                                         | ((chapter) & 0xffff)) )

