/*****************************************************************************
 * demux2 adaptation layer.
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: demux2.c,v 1.1 2004/01/04 14:28:11 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Demux2Open    ( vlc_object_t * );
static void Demux2Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("demux2 adaptation layer" ) );
    set_capability( "demux", 0 );
    set_callbacks( Demux2Open, Demux2Close );
    add_shortcut( "demux2" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux2Demux  ( input_thread_t * );
static int  Demux2Control( input_thread_t *, int, va_list );

typedef struct
{
    demux_t  *p_demux;
} demux2_sys_t;

/*****************************************************************************
 * Demux2Open: initializes structures
 *****************************************************************************/
static int Demux2Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux2_sys_t   *p_sys   = malloc( sizeof( demux2_sys_t ) );
    demux_t        *p_demux;

    if( input_InitStream( p_input, 0 ) )
    {
        return VLC_EGENERIC;
    }

    p_demux = demux2_New( p_input, p_input->psz_source, p_input->s, p_input->p_es_out );

    if( !p_demux )
    {
        /* We should handle special access+demuxer but later */
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_input->pf_demux = Demux2Demux;
    p_input->pf_demux_control = Demux2Control;
    p_input->p_demux_data = (demux_sys_t*)p_sys;

    p_sys->p_demux = p_demux;

    return VLC_SUCCESS;
}


/*****************************************************************************
 * Demux2Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux2Demux( input_thread_t * p_input )
{
    demux2_sys_t  *p_sys = (demux2_sys_t*)p_input->p_demux_data;

    return demux2_Demux( p_sys->p_demux );
}

/*****************************************************************************
 * Demux2Control:
 *****************************************************************************/
static int  Demux2Control( input_thread_t *p_input, int i_query, va_list args )
{
    demux2_sys_t  *p_sys = (demux2_sys_t*)p_input->p_demux_data;

    return demux2_vaControl( p_sys->p_demux, i_query, args );
}

/*****************************************************************************
 * Demux2Close: frees unused data
 *****************************************************************************/
static void Demux2Close( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    demux2_sys_t   *p_sys = (demux2_sys_t*)p_input->p_demux_data;

    demux2_Delete( p_sys->p_demux );

    free( p_sys );
}
