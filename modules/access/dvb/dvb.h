/*****************************************************************************
 * dvb.h : functions to control a DVB card under Linux with v4l2
 *****************************************************************************
 * Copyright (C) 1998-2003 VideoLAN
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <saman@natlab.research.philips.com>
 *          Christopher Ross <ross@natlab.research.philips.com>
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
/*
#define DMX      "/dev/dvb/adapter1/demux0"
#define FRONTEND "/dev/dvb/adapter1/frontend0"
#define DVR      "/dev/dvb/adapter1/dvr0"
*/
#define DMX      "/dev/dvb/adapter%d/demux%d"
#define FRONTEND "/dev/dvb/adapter%d/frontend%d"
#define DVR      "/dev/dvb/adapter%d/dvr%d"

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int ioctl_FrontendControl( int freq, int pol, int lnb_slof, int diseqc, unsigned int u_adapter, unsigned int u_device );
int ioctl_SetQPSKFrontend ( struct dvb_frontend_parameters fep, int b_polarisation, unsigned int u_adapter, unsigned int u_device );
int ioctl_SetDMXFilter( int i_pid, int *pi_fd, int i_type, unsigned int u_adapter, unsigned int u_device );
int ioctl_UnsetDMXFilter( int );
int ioctl_InfoFrontend(struct dvb_frontend_info *info, unsigned int u_adapter, unsigned int u_device );
