/*****************************************************************************
 * dummy.c: dummy muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: dummy.c,v 1.9 2003/11/21 20:49:14 gbazin Exp $
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

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("Dummy/Raw muxer") );
    set_capability( "sout mux", 5 );
    add_shortcut( "dummy" );
    add_shortcut( "es" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int  Capability(sout_mux_t *, int, void *, void * );
static int  AddStream( sout_mux_t *, sout_input_t * );
static int  DelStream( sout_mux_t *, sout_input_t * );
static int  Mux      ( sout_mux_t * );

struct sout_mux_sys_t
{
    /* Some streams have special initialization data, we'll output this
     * data as an header in the stream. */
    vlc_bool_t b_header;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys;

    msg_Dbg( p_mux, "Dummy/Raw muxer opened" );
    msg_Info( p_mux, "Open" );

    p_mux->pf_capacity  = Capability;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    p_mux->p_sys = p_sys = malloc( sizeof( sout_mux_sys_t ) );
    p_sys->b_header      = VLC_TRUE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    msg_Dbg( p_mux, "Dummy/Raw muxer closed" );
    free( p_sys );
}

static int Capability( sout_mux_t *p_mux, int i_query,
                       void *p_args, void *p_answer )
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

static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    msg_Dbg( p_mux, "adding input" );
    return VLC_SUCCESS;
}

static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    msg_Dbg( p_mux, "removing input" );
    return VLC_SUCCESS;
}

static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    int i;

    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        int i_count;
        sout_fifo_t *p_fifo;

        if( p_sys->b_header && p_mux->pp_inputs[i]->p_fmt->i_extra )
        {
            /* Write header data */
            sout_buffer_t *p_data;
            p_data = sout_BufferNew( p_mux->p_sout,
                                     p_mux->pp_inputs[i]->p_fmt->i_extra );

            memcpy( p_data->p_buffer, p_mux->pp_inputs[i]->p_fmt->p_extra,
                    p_mux->pp_inputs[i]->p_fmt->i_extra );

            p_data->i_size = p_mux->pp_inputs[i]->p_fmt->i_extra;
            p_data->i_dts = p_data->i_pts = p_data->i_length = 0;

            msg_Dbg( p_mux, "writing header data" );
            sout_AccessOutWrite( p_mux->p_access, p_data );
        }

        p_fifo = p_mux->pp_inputs[i]->p_fifo;
        i_count = p_fifo->i_depth;
        while( i_count > 0 )
        {
            sout_buffer_t *p_data;

            p_data = sout_FifoGet( p_fifo );

            sout_AccessOutWrite( p_mux->p_access, p_data );

            i_count--;
        }

    }
    p_sys->b_header = VLC_FALSE;

    return VLC_SUCCESS;
}
