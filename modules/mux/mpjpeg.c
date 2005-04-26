/*****************************************************************************
 * mpjpeg.c: mime multipart jpeg  muxer module for vlc
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: dummy.c 7047 2004-03-11 17:37:50Z fenrir $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
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
#include <vlc/sout.h>

#define SEPARATOR_TEXT N_( "Multipart separator string" )
#define SEPARATOR_LONGTEXT N_( "Multipart strings like MPJPEG use a " \
                               "separator string betwen content pieces. "\
                               "You can select this string. Default is "\
                               "--myboundary" )


#define CONTENT_TYPE "Content-Type: image/jpeg"
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-mpjpeg-"

vlc_module_begin();
    set_shortname( _("MPJPEG"));
    set_description( _("Multipart jpeg muxer") );
    set_capability( "sout mux", 5 );
    add_string( SOUT_CFG_PREFIX "separator", "--myboundary", NULL,
                              SEPARATOR_TEXT, SEPARATOR_LONGTEXT, VLC_TRUE );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_MUX );
    set_callbacks( Open, Close );
    add_shortcut( "mpjpeg" );
vlc_module_end();

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = { "separator", NULL };

static int Control  ( sout_mux_t *, int, va_list );
static int AddStream( sout_mux_t *, sout_input_t * );
static int DelStream( sout_mux_t *, sout_input_t * );
static int Mux      ( sout_mux_t * );

struct sout_mux_sys_t
{
    block_t *p_separator;
    vlc_bool_t b_send_headers;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    int i_size;
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t  *p_sys;
    char *psz_separator_block, *psz_separator;

    msg_Dbg( p_mux, "Multipart jpeg muxer opened" );
    sout_CfgParse( p_mux, SOUT_CFG_PREFIX, ppsz_sout_options, p_mux->p_cfg );

    p_sys = p_mux->p_sys = malloc( sizeof(sout_mux_sys_t) );
    p_sys->b_send_headers = VLC_TRUE;

    psz_separator = var_GetString( p_mux, SOUT_CFG_PREFIX "separator" );
    i_size = strlen( psz_separator ) + 2 + 2 + 2 + strlen( CONTENT_TYPE );
    psz_separator_block = (char*)malloc( i_size );
    sprintf( psz_separator_block, "\r\n%s\r\n%s\r\n", psz_separator,
                                  CONTENT_TYPE );
    p_sys->p_separator = block_New( p_mux, i_size );
    memcpy( p_sys->p_separator->p_buffer, psz_separator_block , i_size );

    if( psz_separator_block ) free( psz_separator_block );

    p_mux->pf_control   = Control;
    p_mux->pf_addstream = AddStream;
    p_mux->pf_delstream = DelStream;
    p_mux->pf_mux       = Mux;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/

static void Close( vlc_object_t * p_this )
{
    sout_mux_t *p_mux = (sout_mux_t*)p_this;
    sout_mux_sys_t *p_sys = p_mux->p_sys;

    msg_Dbg( p_mux, "Multipart jpeg muxer closed" );
    block_Release( p_sys->p_separator );
    free( p_sys );
}

static int Control( sout_mux_t *p_mux, int i_query, va_list args )
{
    vlc_bool_t *pb_bool;
    char **ppsz;

   switch( i_query )
   {
       case MUX_CAN_ADD_STREAM_WHILE_MUXING:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_TRUE;
           return VLC_SUCCESS;

       case MUX_GET_ADD_STREAM_WAIT:
           pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
           *pb_bool = VLC_TRUE;
           return VLC_SUCCESS;

       case MUX_GET_MIME:
           ppsz = (char**)va_arg( args, char ** );
           *ppsz = strdup( "multipart/x-mixed-replace; boundary=This Random String" );
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
    if( p_input->p_fmt->i_codec != VLC_FOURCC('M','J','P','G') &&
        p_input->p_fmt->i_codec != VLC_FOURCC('m','j','p','g') &&
        p_input->p_fmt->i_codec != VLC_FOURCC('j','p','e','g') &&
        p_input->p_fmt->i_codec != VLC_FOURCC('J','P','E','G') &&
        p_input->p_fmt->i_codec != VLC_FOURCC('J','F','I','F') &&
        p_input->p_fmt->i_codec != VLC_FOURCC('J','P','G','L') &&
        p_input->p_fmt->i_codec != VLC_FOURCC('m','j','p','a') )
    {
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int DelStream( sout_mux_t *p_mux, sout_input_t *p_input )
{
    msg_Dbg( p_mux, "removing input" );
    return VLC_SUCCESS;
}

static int Mux( sout_mux_t *p_mux )
{
    block_fifo_t *p_fifo;
    sout_mux_sys_t *p_sys = p_mux->p_sys;
    int i_count;
    /* Content-Length:.......\r\n */
    char psz_content_length[25];

    if( p_sys->b_send_headers )
    {
        block_t *p_header;
        char *psz_separator = var_CreateGetString( p_mux,
                                             SOUT_CFG_PREFIX "separator" );
        char *psz_separator_block = (char *)malloc( strlen( psz_separator ) +
                                              2 + strlen( CONTENT_TYPE ) );

        sprintf( psz_separator_block, "%s\r\n%s\r\n", psz_separator,
                                      CONTENT_TYPE );

        p_header = block_New( p_mux, strlen( psz_separator_block ) );
        memcpy( p_header->p_buffer, psz_separator_block ,
                                    strlen( psz_separator_block ) );
        p_header->i_flags |= BLOCK_FLAG_HEADER;
        sout_AccessOutWrite( p_mux->p_access, p_header );
        p_sys->b_send_headers = VLC_FALSE;
        if( psz_separator_block ) free( psz_separator_block );
    }

    if( !p_mux->i_nb_inputs ) return VLC_SUCCESS;

    p_fifo = p_mux->pp_inputs[0]->p_fifo;
    i_count = p_fifo->i_depth;
    while( i_count > 0 )
    {
        block_t *p_length = block_New( p_mux, 25 );
        block_t *p_data = block_FifoGet( p_fifo );
        sout_AccessOutWrite( p_mux->p_access,
                             block_Duplicate( p_sys->p_separator ) );
        memset( psz_content_length, 0, 25 );
        snprintf( psz_content_length, 25, "Content-Length: %i\r\n\r\n",
                                          p_data->i_buffer );
        memcpy( p_length->p_buffer, psz_content_length, 25 );
        sout_AccessOutWrite( p_mux->p_access, p_length );
        sout_AccessOutWrite( p_mux->p_access, p_data );

        i_count--;
    }

    return VLC_SUCCESS;
}
