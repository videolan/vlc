/*****************************************************************************
 * netutils.h: various network functions
 * This header describes miscellanous utility functions shared between several
 * modules.
 *****************************************************************************
 * Copyright (C) 1999, 2000, 2001 VideoLAN
 * $Id: netutils.h,v 1.18 2001/12/30 07:09:54 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Henri Fallon <henri@videolan.org>
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

/* The channel without stream is 0 */
#define COMMON_CHANNEL  0

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
struct sockaddr_in;
int   network_BuildAddr       ( struct sockaddr_in *, char *, int ); 

#ifndef PLUGIN
int   network_ChannelJoin     ( int );
int   network_ChannelCreate   ( void );
#else
#   define network_ChannelCreate p_symbols->network_ChannelCreate
#   define network_ChannelJoin   p_symbols->network_ChannelJoin
#endif

