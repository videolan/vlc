/*****************************************************************************
 * dvd_ioctl.h: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: dvd_ioctl.h,v 1.7 2001/05/02 20:01:44 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

int ioctl_ReadCopyright     ( int, int, int * );
int ioctl_ReadKey           ( int, int *, u8 * );

int ioctl_ReportAgid        ( int, int * );
int ioctl_ReportChallenge   ( int, int *, u8 * );
int ioctl_ReportKey1        ( int, int *, u8 * );
int ioctl_ReportASF         ( int, int *, int * );
int ioctl_InvalidateAgid    ( int, int * );
int ioctl_SendChallenge     ( int, int *, u8 * );
int ioctl_SendKey2          ( int, int *, u8 * );

#ifdef SYS_BEOS

/* The generic packet command opcodes for CD/DVD Logical Units,
 * From Table 57 of the SFF8090 Ver. 3 (Mt. Fuji) draft standard. */
#define GPCMD_READ_DVD_STRUCTURE 0xad
#define GPCMD_REPORT_KEY         0xa4
#define GPCMD_SEND_KEY           0xa3

/* DVD struct types */
#define DVD_STRUCT_COPYRIGHT     0x01
#define DVD_STRUCT_DISCKEY       0x02

#endif

