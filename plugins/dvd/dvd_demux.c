/* dvd_demux.c: DVD demux functions.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_demux.c,v 1.2 2002/03/06 12:26:35 stef Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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
 * Preamble
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "debug.h"

/* how many packets DVDDemux will read in each loop */
#define DVD_READ_ONCE 64

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* called from outside */
static int  DVDRewind       ( struct input_thread_s * );
static int  DVDDemux        ( struct input_thread_s * );
static int  DVDInit         ( struct input_thread_s * );
static void DVDEnd          ( struct input_thread_s * );

void DVDLaunchDecoders( input_thread_t * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( demux_getfunctions)( function_list_t * p_function_list )
{
#define demux p_function_list->functions.demux
    demux.pf_init             = DVDInit;
    demux.pf_end              = DVDEnd;
    demux.pf_demux            = DVDDemux;
    demux.pf_rewind           = DVDRewind;
#undef demux
}

/*
 * Data demux functions
 */

/*****************************************************************************
 * DVDInit: initializes DVD structures
 *****************************************************************************/
static int DVDInit( input_thread_t * p_input )
{
    vlc_mutex_lock( &p_input->stream.stream_lock );
    
    DVDLaunchDecoders( p_input );
    
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

/*****************************************************************************
 * DVDEnd: frees unused data
 *****************************************************************************/
static void DVDEnd( input_thread_t * p_input )
{
}

/*****************************************************************************
 * DVDDemux
 *****************************************************************************/
static int DVDDemux( input_thread_t * p_input )
{
    int                 i;
    data_packet_t *     p_data;

    /* Read headers to compute payload length */
    for( i = 0 ; i < DVD_READ_ONCE ; i++ )
    {
        input_ReadPS( p_input, &p_data );

        input_DemuxPS( p_input, p_data );
    }
    
    return i;
}

/*****************************************************************************
 * DVDRewind : reads a stream backward
 *****************************************************************************/
static int DVDRewind( input_thread_t * p_input )
{
    return( -1 );
}
