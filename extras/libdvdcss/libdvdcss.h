/*****************************************************************************
 * private.h: private DVD reading library data
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: libdvdcss.h,v 1.1 2001/06/12 22:14:44 sam Exp $
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
 * Needed headers
 *****************************************************************************/
#include "css.h"

/*****************************************************************************
 * The libdvdcss structure
 *****************************************************************************/
struct dvdcss_s
{
    /* Status */
    int i_status;

    /* File descriptor */
    int i_fd;

    /* Decryption stuff */
    css_t     css;
    boolean_t b_encrypted;

    /* Error management */
    char     *psz_error;
    boolean_t b_errors;
    boolean_t b_debug;

};

/*****************************************************************************
 * Flags
 *****************************************************************************/
#define DVDCSS_STATUS_NONE     0
#define DVDCSS_STATUS_INIT     1 << 0
#define DVDCSS_STATUS_OPEN     1 << 1

/*****************************************************************************
 * Error management
 *****************************************************************************/
#define DVDCSS_ERROR( x... ) fprintf( stderr, "libdvdcss error: %s\n", ##x );
#define DVDCSS_DEBUG( x... ) fprintf( stderr, "libdvdcss debug: %s\n", ##x );

static __inline__ void _dvdcss_error( dvdcss_handle dvdcss, char *psz_string )
{
    if( dvdcss->b_errors )
    {
        DVDCSS_ERROR( psz_string );
    }

    dvdcss->psz_error = psz_string;
}

static __inline__ void _dvdcss_debug( dvdcss_handle dvdcss, char *psz_string )
{
    if( dvdcss->b_debug )
    {
        DVDCSS_DEBUG( psz_string );
    }
}


