/*****************************************************************************
 * network.h: interface to communicate with network plug-ins
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: network.h,v 1.3 2002/07/20 18:01:41 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
 * network_socket_t: structure passed to a network plug-in to define the
 *                   kind of socket we want
 *****************************************************************************/
struct network_socket_t
{
    unsigned int i_type;

    char * psz_bind_addr;
    int i_bind_port;

    char * psz_server_addr;
    int i_server_port;

    /* Return values */
    int i_handle;
    size_t i_mtu;
};

/* Socket types */
#define NETWORK_UDP 1
#define NETWORK_TCP 2

