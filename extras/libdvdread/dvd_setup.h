/**
 * dvd_setup.h: setup functions header.
 */

/* Copyright (C) 2001 VideoLAN
 * $Id: dvd_setup.h,v 1.2 2002/01/23 03:56:51 stef Exp $
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
 */

/**
 * The libdvdcss structure.
 */
struct dvdcss_s
{
    /* File descriptor */
    int i_fd;
};

#ifdef GOD_DAMN_DMCA
/**
 * Defines and flags.
 */
#  define DVDCSS_NOFLAGS         0
#  define DVDCSS_READ_DECRYPT    (1 << 0)
#  define DVDCSS_SEEK_MPEG       (1 << 0)
#  define DVDCSS_SEEK_KEY        (1 << 1)

#else
#  include <videolan/dvdcss.h>
#endif

typedef struct dvdcss_s* dvd_handle;

/**
 * Pointers which will be filled either with dummy functions or
 * with the dlopen()ed ones.
 */
dvd_handle (*pf_dvd_open)  ( char * );
int        (*pf_dvd_close) ( dvd_handle );
int        (*pf_dvd_seek)  ( dvd_handle, int, int );
int        (*pf_dvd_read)  ( dvd_handle, void *, int, int );
int        (*pf_dvd_readv) ( dvd_handle, void *, int, int );
char *     (*pf_dvd_error) ( dvd_handle );

/**
 * Setup function accessed by dvd_reader.c
 */
void DVDSetupRead( void );
