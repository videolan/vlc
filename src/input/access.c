/*****************************************************************************
 * access.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: demux.c 7546 2004-04-29 13:53:29Z gbazin $
 *
 * Author: Laurent Aimar <fenrir@via.ecp.fr>
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

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "ninput.h"

int access_vaControl( input_thread_t *p_input, int i_query, va_list args )
{
    if( p_input->pf_access_control )
    {
        return p_input->pf_access_control( p_input, i_query, args );
    }
    return VLC_EGENERIC;
}

int access_Control( input_thread_t *p_input, int i_query, ...  )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = access_vaControl( p_input, i_query, args );
    va_end( args );

    return i_result;
}

int access_vaControlDefault( input_thread_t *p_input, int i_query, va_list args )
{
    return VLC_EGENERIC;
}

/*****************************************************************************
 * access2_New:
 *****************************************************************************/
access_t *__access2_New( vlc_object_t *p_obj, char *psz_mrl )
{
    access_t *p_access = vlc_object_create( p_obj, VLC_OBJECT_ACCESS );

    char    *psz_dup = strdup( psz_mrl ? psz_mrl : "" );
    char    *psz = strchr( psz_dup, ':' );

    if( p_access == NULL )
    {
        free( psz_dup );
        return NULL;
    }

    /* Parse URL */
    p_access->psz_access = NULL;
    p_access->psz_path   = NULL;

    if( psz )
    {
        *psz++ = '\0';

        if( psz[0] == '/' && psz[1] == '/' )
        {
            psz += 2;
        }
        p_access->psz_path = strdup( psz );

        psz = strchr( psz_dup, '/' );
        if( psz )
        {
            *psz++ = '\0';
            p_access->psz_access = strdup( psz_dup );
        }
    }
    else
    {
        p_access->psz_path = strdup( psz_mrl );
    }
    free( psz_dup );


    p_access->psz_demux = strdup( "" );
    if( p_access->psz_access == NULL )
    {
        p_access->psz_access = strdup( "" );
    }
    if( p_access->psz_path == NULL )
    {
        p_access->psz_path = strdup( "" );
    }
    msg_Dbg( p_obj, "access2_New: '%s' -> access='%s' path='%s'",
             psz_mrl,
             p_access->psz_access, p_access->psz_path );

    p_access->pf_read    = NULL;
    p_access->pf_block   = NULL;
    p_access->pf_seek    = NULL;
    p_access->pf_control = NULL;
    p_access->p_sys      = NULL;

    /* Before module_Need (for var_Create...) */
    vlc_object_attach( p_access, p_obj );

    p_access->p_module =
        module_Need( p_access, "access2", p_access->psz_access, VLC_FALSE );

    if( p_access->p_module == NULL )
    {
        vlc_object_detach( p_access );
        free( p_access->psz_access );
        free( p_access->psz_path );
        free( p_access->psz_demux );
        vlc_object_destroy( p_access );
        return NULL;
    }

    return p_access;
}

/*****************************************************************************
 * demux2_Delete:
 *****************************************************************************/
void access2_Delete( access_t *p_access )
{
    module_Unneed( p_access, p_access->p_module );
    vlc_object_detach( p_access );

    free( p_access->psz_access );
    free( p_access->psz_path );
    free( p_access->psz_demux );

    vlc_object_destroy( p_access );
}
