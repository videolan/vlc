/*****************************************************************************
 * network.h : Network part of the gtk-familiar plugin.
 *****************************************************************************
 * Copyright (C) 2000, 2001 VideoLAN
 * $Id: network.h,v 1.1 2003/03/13 15:50:17 marcari Exp $
 *
 * Authors: Marc Ariberti <marcari@videolan.org>
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

void
on_network_udp_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_network_udp_port_changed            (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_network_multicast_toggled           (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_network_multicast_port_changed      (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_network_multicast_address_changed   (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_network_http_toggled                (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_network_ftp_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_network_mms_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
FamiliarNetworkGo                      (GtkButton       *button,
                                        gpointer         user_data);
