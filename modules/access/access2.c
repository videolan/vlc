/*****************************************************************************
 * access2 adaptation layer.
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: demux2.c 7783 2004-05-27 00:02:43Z hartman $
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
static int  Access2Open ( vlc_object_t * );
static void Access2Close( vlc_object_t * );

vlc_module_begin();
    set_description( _("Access2 adaptation layer" ) );
    set_capability( "access", 0 );
    set_callbacks( Access2Open, Access2Close );
    add_shortcut( "access2" );
    add_shortcut( "http" );
    add_shortcut( "ftp" );

    /* Hack */
    //add_shortcut( "file" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Access2Read   ( input_thread_t *, byte_t *, size_t );
static void Access2Seek   ( input_thread_t *, off_t );
static int  Access2Control( input_thread_t *, int, va_list );

typedef struct
{
    access_t  *p_access;
    block_t   *p_block;
} access2_sys_t;

/*****************************************************************************
 * Access2Open: initializes structures
 *****************************************************************************/
static int Access2Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access2_sys_t  *p_sys   = malloc( sizeof( access2_sys_t ) );
    access_t       *p_access;

    char           *psz_uri;
    int            i_int;
    int64_t        i_64;
    vlc_bool_t     b_bool;

    psz_uri = malloc( strlen( p_input->psz_access ) + strlen( p_input->psz_demux ) + strlen( p_input->psz_name  ) + 1 + 3 + 1 );
    if( p_input->psz_demux && *p_input->psz_demux )
    {
        sprintf( psz_uri, "%s/%s://%s", p_input->psz_access, p_input->psz_demux, p_input->psz_name );
    }
    else if( p_input->psz_access && *p_input->psz_access )
    {
        sprintf( psz_uri, "%s://%s", p_input->psz_access, p_input->psz_name );
    }
    else
    {
        sprintf( psz_uri, "://%s", p_input->psz_name );
    }

    p_access = access2_New( p_input, psz_uri );

    free( psz_uri );

    if( !p_access )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* Init p_input->* */
    p_input->pf_read           = Access2Read;
    p_input->pf_seek           = Access2Seek;
    p_input->pf_set_program    = input_SetProgram;
    p_input->pf_set_area       = NULL;
    p_input->pf_access_control = Access2Control;
    p_input->p_access_data = (access_sys_t*)p_sys;
    /* mtu */
    access2_Control( p_access, ACCESS_GET_MTU, &i_int );
    p_input->i_mtu = i_int;
    /* pts delay */
    access2_Control( p_access, ACCESS_GET_PTS_DELAY, &i_64 );
    p_input->i_pts_delay = i_64;

    if( p_access->psz_demux && *p_access->psz_demux )
    {
        if( !p_input->psz_demux || *p_input->psz_demux == '\0' )
            p_input->psz_demux = strdup( p_access->psz_demux );
    }

    /* Init p_input->stream.* */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    /* size */
    access2_Control( p_access, ACCESS_GET_SIZE, &i_64 );
    p_input->stream.p_selected_area->i_size  = i_64;
    /* seek */
    access2_Control( p_access, ACCESS_CAN_SEEK, &b_bool );
    p_input->stream.b_seekable = b_bool;
    /* pace */
    access2_Control( p_access, ACCESS_CAN_CONTROL_PACE, &b_bool );
    p_input->stream.b_pace_control = b_bool;
   /* End of init */
    access2_Control( p_access, ACCESS_CAN_SEEK, &b_bool );
    if( b_bool )
        p_input->stream.i_method = INPUT_METHOD_FILE;   /* FIXME */
    else
        p_input->stream.i_method = INPUT_METHOD_NETWORK;/* FIXME */
    p_input->stream.p_selected_area->i_tell = 0;
    vlc_mutex_unlock( &p_input->stream.stream_lock );


    p_sys->p_access = p_access;
    p_sys->p_block = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Access2Close: frees unused data
 *****************************************************************************/
static void Access2Close( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    access2_sys_t  *p_sys = (access2_sys_t*)p_input->p_access_data;

    access2_Delete( p_sys->p_access );

    if( p_sys->p_block )
        block_Release( p_sys->p_block );

    free( p_sys );
}

/*****************************************************************************
 * Access2Read: Read data
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, size otherwise
 *****************************************************************************/
static int  Access2Read( input_thread_t *p_input, byte_t *p_buffer, size_t i_len )
{
    access2_sys_t *p_sys = (access2_sys_t*)p_input->p_access_data;
    access_t      *p_access = p_sys->p_access;
    int           i_total = 0;

    /* TODO update i_size (ex: when read more than half current size and only every 100 times or ...) */

    if( p_access->pf_read )
        return p_access->pf_read( p_access, (uint8_t*)p_buffer, (int)i_len );

    /* Should never occur */
    if( p_access->pf_block == NULL )
        return 0;

    /* Emulate the read */
    while( i_total < i_len )
    {
        int i_copy;
        if( p_sys->p_block == NULL )
        {
            p_sys->p_block = p_access->pf_block( p_access );
        }
        if( p_sys->p_block == NULL )
        {
            vlc_bool_t b_eof;

            access2_Control( p_access, ACCESS_GET_EOF, &b_eof );
            if( b_eof || i_total > 0 )
                return i_total;
            else
                return -1;
        }

        i_copy = __MIN( i_len - i_total, p_sys->p_block->i_buffer );
        memcpy( &p_buffer[i_total], p_sys->p_block->p_buffer, i_copy );

        p_sys->p_block->i_buffer -= i_copy;
        p_sys->p_block->p_buffer += i_copy;
        if( p_sys->p_block->i_buffer <= 0 )
        {
            block_Release( p_sys->p_block );
            p_sys->p_block = NULL;
        }

        i_total += i_copy;
    }
    return i_total;
}

/*****************************************************************************
 * Access2Seek:
 *****************************************************************************
 * Returns VLC_EGENERIC/VLC_SUCCESS
 *****************************************************************************/
static void Access2Seek( input_thread_t *p_input, off_t i_pos )
{
    access2_sys_t *p_sys = (access2_sys_t*)p_input->p_access_data;
    access_t      *p_access = p_sys->p_access;

    if( p_access->pf_seek != NULL && p_input->stream.b_seekable )
    {
        if( !p_access->pf_seek( p_access, i_pos ) )
        {
            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_input->stream.p_selected_area->i_tell = i_pos;
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
    }
}

/*****************************************************************************
 * Access2Control:
 *****************************************************************************
 * Returns VLC_EGENERIC/VLC_SUCCESS
 *****************************************************************************/
static int  Access2Control( input_thread_t *p_input, int i_query, va_list args )
{
    access2_sys_t *p_sys = (access2_sys_t*)p_input->p_access_data;
    access_t      *p_access = p_sys->p_access;

    return access2_vaControl( p_access, i_query, args );
}
