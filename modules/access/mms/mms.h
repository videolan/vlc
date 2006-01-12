/*****************************************************************************
 * mms.h: MMS access plug-in
 *****************************************************************************
 * Copyright (C) 2001, 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#define MMS_PROTO_AUTO  0
#define MMS_PROTO_TCP   1
#define MMS_PROTO_UDP   2
#define MMS_PROTO_HTTP  3


/* mmst and mmsu */
int  E_( MMSTUOpen )  ( access_t * );
void E_( MMSTUClose ) ( access_t * );

/* mmsh */
int  E_( MMSHOpen )  ( access_t * );
void E_( MMSHClose ) ( access_t * );

#define FREE( p ) if( p ) { free( p ); (p) = NULL; }
