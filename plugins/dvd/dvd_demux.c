/* dvd_demux.c: DVD demux functions.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_demux.c,v 1.7 2002/06/01 12:31:58 sam Exp $
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

#include <vlc/vlc.h>
#include <vlc/input.h>

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

/* how many packets DVDDemux will read in each loop */
#define DVD_READ_ONCE 64

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* called from outside */
static int  DVDRewind  ( input_thread_t * );
static int  DVDDemux   ( input_thread_t * );
static int  DVDInit    ( input_thread_t * );
static void DVDEnd     ( input_thread_t * );

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
    if( p_input->stream.i_method != INPUT_METHOD_DVD )
    {
        return -1;
    }

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
    data_packet_t *     p_data;
    ssize_t             i_result;
    int                 i;

    /* Read headers to compute payload length */
    for( i = 0 ; i < DVD_READ_ONCE ; i++ )
    {
        i_result = input_ReadPS( p_input, &p_data );

        if( i_result < 0 )
        {
            return i_result;
        }
        else if( i_result == 0 )
        {
            return i;
        }

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
