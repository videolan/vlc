/*****************************************************************************
 * dummy.c: dummy muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2001, 2002 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>
#include <vlc_block.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin ()
    set_description( N_("Dummy/Raw muxer") )
    set_capability( "sout mux", 5 )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_MUX )
    add_shortcut( "dummy", "raw", "es" )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int Control( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static void DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

typedef struct
{
    /* Some streams have special initialization data, we'll output this
     * data as an header in the stream. */
    bool b_header;
} sout_mux_sys_t;

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys;

    msg_Dbg( p_mux, "Dummy/Raw muxer opened" );
    msg_Info( p_mux, "Open" );

    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    p_mux->p_sys = p_sys = malloc( sizeof( sout_mux_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->b_header      = true;

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

static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    VLC_UNUSED(p_mux);
    bool *pb_bool;

    switch( i_query )
    {
        case MUX_CAN_ADD_STREAM_WHILE_MUXING:
            pb_bool = va_arg( args, bool * );
            *pb_bool = true;
            return VLC_SUCCESS;

        case MUX_GET_MIME:   /* Unknown */
        default:
            return VLC_EGENERIC;
   }
}

static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    VLC_UNUSED(p_input);
    msg_Dbg( p_mux, "adding input" );
    return VLC_SUCCESS;
}

static void DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    VLC_UNUSED(p_input);
    msg_Dbg( p_mux, "removing input" );
}

static int Mux( sout_mux_t *p_mux )
{
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    int i;

    for( i = 0; i < p_mux->i_nb_inputs; i++ )
    {
        block_fifo_t *p_fifo;
        int i_count;

        if( p_sys->b_header && p_mux->pp_inputs[i]->p_fmt->i_extra )
        {
            /* Write header data */
            block_t *p_data;
            p_data = block_Alloc( p_mux->pp_inputs[i]->p_fmt->i_extra );

            memcpy( p_data->p_buffer, p_mux->pp_inputs[i]->p_fmt->p_extra,
                    p_mux->pp_inputs[i]->p_fmt->i_extra );

            p_data->i_flags |= BLOCK_FLAG_HEADER;

            msg_Dbg( p_mux, "writing header data" );
            sout_AccessOutWrite( p_mux->p_access, p_data );
        }

        p_fifo = p_mux->pp_inputs[i]->p_fifo;
        i_count = block_FifoCount( p_fifo );
        while( i_count > 0 )
        {
            block_t *p_data = block_FifoGet( p_fifo );

            sout_AccessOutWrite( p_mux->p_access, p_data );

            i_count--;
        }
    }
    p_sys->b_header = false;

    return VLC_SUCCESS;
}
