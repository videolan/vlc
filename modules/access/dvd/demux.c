/* demux.c: DVD demux functions.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: demux.c,v 1.2 2002/08/07 00:29:36 sam Exp $
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

#include "../../demux/mpeg/system.h"

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
 * Private structure
 *****************************************************************************/
struct demux_sys_t
{
    module_t *   p_module;
    mpeg_demux_t mpeg;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  DVDDemux   ( input_thread_t * );

void DVDLaunchDecoders( input_thread_t * );

/*****************************************************************************
 * DVDInit: initialize DVD structures
 *****************************************************************************/
int E_(DVDInit) ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t *   p_demux;

    if( p_input->stream.i_method != INPUT_METHOD_DVD )
    {
        return -1;
    }

    p_demux = p_input->p_demux_data = malloc( sizeof(demux_sys_t ) );
    if( p_demux == NULL )
    {
        return -1;
    }

    p_input->p_private = (void*)&p_demux->mpeg;
    p_demux->p_module = module_Need( p_input, "mpeg-system", NULL );
    if( p_demux->p_module == NULL )
    {
        free( p_input->p_demux_data );
        return -1;
    }

    p_input->pf_demux = DVDDemux;
    p_input->pf_rewind = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    
    DVDLaunchDecoders( p_input );
    
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

/*****************************************************************************
 * DVDEnd: free DVD structures
 *****************************************************************************/
void E_(DVDEnd) ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;

    module_Unneed( p_input, p_input->p_demux_data->p_module );
    free( p_input->p_demux_data );
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
        i_result = p_input->p_demux_data->mpeg.pf_read_ps( p_input, &p_data );

        if( i_result < 0 )
        {
            return i_result;
        }
        else if( i_result == 0 )
        {
            return i;
        }

        p_input->p_demux_data->mpeg.pf_demux_ps( p_input, p_data );
    }
    
    return i;
}
