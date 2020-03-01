/*****************************************************************************
 * mpjpeg.c: mime multipart jpeg  muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2001, 2002, 2006 VLC authors and VideoLAN
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
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

#define SOUT_CFG_PREFIX "sout-mpjpeg-"

vlc_module_begin ()
    set_shortname( "MPJPEG" )
    set_description( N_("Multipart JPEG muxer") )
    set_capability( "sout mux", 5 )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_MUX )
    set_callbacks( Open, Close )
    add_shortcut( "mpjpeg" )
vlc_module_end ()

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static void DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

/* This pseudo-random sequence is unlikely to ever happen */
/* This should be the same as in src/network/httpd.c */
#define BOUNDARY "7b3cc56e5f51db803f790dad720ed50a"

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;

    msg_Dbg( p_mux, "Multipart jpeg muxer opened" );

    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;
    p_mux->p_sys        = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    /* TODO: send the ending boundary ("\r\n--"BOUNDARY"--\r\n"),
     * but is the access_output still useable?? */
    msg_Dbg( p_this, "Multipart jpeg muxer closed" );
}

static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    VLC_UNUSED(p_mux);
    bool *pb_bool;
    char **ppsz;

    switch( i_query )
    {
        case MUX_CAN_ADD_STREAM_WHILE_MUXING:
            pb_bool = va_arg( args, bool * );
            *pb_bool = false;
            return VLC_SUCCESS;

        case MUX_GET_MIME:
            ppsz = va_arg( args, char ** );
            *ppsz = strdup( "multipart/x-mixed-replace; boundary="BOUNDARY );
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}

static int AddStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    if( p_mux->i_nb_inputs > 1 )
    {
        msg_Dbg( p_mux, "only 1 input allowed" );
        return VLC_EGENERIC;
    }

    msg_Dbg( p_mux, "adding input" );
    if( p_input->p_fmt->i_codec != VLC_CODEC_MJPG )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static void DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    VLC_UNUSED(p_input);
    msg_Dbg( p_mux, "removing input" );
}

static int Mux( sout_mux_t *p_mux )
{
    block_fifo_t *p_fifo;

    if( !p_mux->i_nb_inputs ) return VLC_SUCCESS;

    p_fifo = p_mux->pp_inputs[0]->p_fifo;

    while( block_FifoCount( p_fifo ) > 0 )
    {
        static const char psz_hfmt[] = "\r\n"
            "--"BOUNDARY"\r\n"
            "Content-Type: image/jpeg\r\n"
            "Content-Length: %zu\r\n"
            "\r\n";
        block_t *p_data = block_FifoGet( p_fifo );
        block_t *p_header = block_Alloc( sizeof( psz_hfmt ) + 20 );

        if( p_header == NULL ) /* uho! */
        {
            block_Release( p_data );
            continue;
        }

        p_header->i_buffer =
            snprintf( (char *)p_header->p_buffer, p_header->i_buffer,
                      psz_hfmt, p_data->i_buffer );
        p_header->i_flags |= BLOCK_FLAG_HEADER;
        sout_AccessOutWrite( p_mux->p_access, p_header );
        sout_AccessOutWrite( p_mux->p_access, p_data );
    }

    return VLC_SUCCESS;
}
