/*****************************************************************************
 * display.h: Gtk+ tools for main interface.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN (Centrale RÃ©seaux) and its contributors
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

/*****************************************************************************
 * Prototypes
 *****************************************************************************/

gint E_(GtkModeManage)      ( intf_thread_t * p_intf );
void E_(GtkDisplayDate)     ( GtkAdjustment *p_adj );
int  E_(GtkHideTooltips)    ( vlc_object_t *, const char *,
                              vlc_value_t, vlc_value_t, void * );
int  GtkHideToolbarText     ( vlc_object_t *, const char *,
                              vlc_value_t, vlc_value_t, void * );

