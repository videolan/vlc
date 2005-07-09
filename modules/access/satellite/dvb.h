/*****************************************************************************
 * linux_dvb_tools.h : functions to control a DVB card under Linux
 *****************************************************************************
 * Copyright (C) 1998-2001 the VideoLAN team
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA    02111, USA.
 *****************************************************************************/


/*****************************************************************************
 * Devices location
 *****************************************************************************/
#define SEC      "/dev/ost/sec"
#define DMX      "/dev/ost/demux"
#define FRONTEND "/dev/ost/frontend"
#define DVR      "/dev/ost/dvr"


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int ioctl_SECControl( int, int , int , int , int );
int ioctl_SetQPSKFrontend ( int, int , int , int , int , int , int );
int ioctl_SetDMXFilter( int, int , int *, int );
int ioctl_UnsetDMXFilter( int );
int ioctl_SetBufferSize( int, size_t );
