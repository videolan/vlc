/*****************************************************************************
 * dummy.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: dummy.c,v 1.6 2003/03/03 14:21:08 gbazin Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include "codecs.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int     Open   ( vlc_object_t * );
static void    Close  ( vlc_object_t * );

static int Capability(sout_instance_t *, int, void *, void * );
static int AddStream( sout_instance_t *, sout_input_t * );
static int DelStream( sout_instance_t *, sout_input_t * );
static int Mux      ( sout_instance_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Dummy muxer") );
    set_capability( "sout mux", 5 );
    add_shortcut( "dummy" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;

    msg_Info( p_sout, "Open" );

    p_sout->pf_mux_capacity  = Capability;
    p_sout->pf_mux_addstream = AddStream;
    p_sout->pf_mux_delstream = DelStream;
    p_sout->pf_mux           = Mux;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    sout_instance_t     *p_sout = (sout_instance_t*)p_this;
    msg_Info( p_sout, "Close" );
}

static int Capability( sout_instance_t *p_sout, int i_query, void *p_args, void *p_answer )
{
   switch( i_query )
   {
        case SOUT_MUX_CAP_GET_ADD_STREAM_ANY_TIME:
            *(vlc_bool_t*)p_answer = VLC_TRUE;
            return( SOUT_MUX_CAP_ERR_OK );
        default:
            return( SOUT_MUX_CAP_ERR_UNIMPLEMENTED );
   }
}

static int AddStream( sout_instance_t *p_sout, sout_input_t *p_input )
{
    msg_Dbg( p_sout, "adding input" );
    return( 0 );
}

static int DelStream( sout_instance_t *p_sout, sout_input_t *p_input )
{

    msg_Dbg( p_sout, "removing input" );
    return( 0 );
}

static int Mux      ( sout_instance_t *p_sout )
{
    int i;
    for( i = 0; i < p_sout->i_nb_inputs; i++ )
    {
        int i_count;
        sout_fifo_t *p_fifo;

        p_fifo = p_sout->pp_inputs[i]->p_fifo;
        i_count = p_fifo->i_depth;
        while( i_count > 0 )
        {
            sout_buffer_t *p_data;

            p_data = sout_FifoGet( p_fifo );

            sout_AccessOutWrite( p_sout->p_access, p_data );

            i_count--;
        }

    }
    return( 0 );
}

