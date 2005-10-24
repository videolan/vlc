/*****************************************************************************
 * access.c
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
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

#include "input_internal.h"

/*****************************************************************************
 * access2_InternalNew:
 *****************************************************************************/
static access_t *access2_InternalNew( vlc_object_t *p_obj, char *psz_access,
                                      char *psz_demux, char *psz_path,
                                      access_t *p_source, vlc_bool_t b_quick )
{
    access_t *p_access = vlc_object_create( p_obj, VLC_OBJECT_ACCESS );

    if( p_access == NULL )
    {
        msg_Err( p_obj, "vlc_object_create() failed" );
        return NULL;
    }

    /* Parse URL */
    p_access->p_source = p_source;
    if( p_source )
    {
        msg_Dbg( p_obj, "creating access filter '%s'", psz_access );
        p_access->psz_access = strdup( p_source->psz_access );
        p_access->psz_path   = strdup( p_source->psz_path );
        p_access->psz_demux   = strdup( p_source->psz_demux );
    }
    else
    {
        p_access->psz_path   = strdup( psz_path );
        p_access->psz_access =
            b_quick ? strdup( "file" ) : strdup( psz_access );
        p_access->psz_demux  = strdup( psz_demux );

        if( !b_quick )
            msg_Dbg( p_obj, "creating access '%s' path='%s'",
                     psz_access, psz_path );
    }

    p_access->pf_read    = NULL;
    p_access->pf_block   = NULL;
    p_access->pf_seek    = NULL;
    p_access->pf_control = NULL;
    p_access->p_sys      = NULL;
    p_access->info.i_update = 0;
    p_access->info.i_size   = 0;
    p_access->info.i_pos    = 0;
    p_access->info.b_eof    = VLC_FALSE;
    p_access->info.b_prebuffered = VLC_FALSE;
    p_access->info.i_title  = 0;
    p_access->info.i_seekpoint = 0;


    /* Before module_Need (for var_Create...) */
    vlc_object_attach( p_access, p_obj );

    if( p_source )
    {
        p_access->p_module =
            module_Need( p_access, "access_filter", psz_access, VLC_FALSE );
    }
    else
    {
        p_access->p_module =
            module_Need( p_access, "access2", p_access->psz_access,
                         b_quick ? VLC_TRUE : VLC_FALSE );
    }

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
 * access2_New:
 *****************************************************************************/
access_t *__access2_New( vlc_object_t *p_obj, char *psz_access,
                         char *psz_demux, char *psz_path, vlc_bool_t b_quick )
{
    return access2_InternalNew( p_obj, psz_access, psz_demux,
                                psz_path, NULL, b_quick );
}

/*****************************************************************************
 * access2_FilterNew:
 *****************************************************************************/
access_t *access2_FilterNew( access_t *p_source, char *psz_access_filter )
{
    return access2_InternalNew( VLC_OBJECT(p_source), psz_access_filter,
                                NULL, NULL, p_source, VLC_FALSE );
}

/*****************************************************************************
 * access2_Delete:
 *****************************************************************************/
void access2_Delete( access_t *p_access )
{
    module_Unneed( p_access, p_access->p_module );
    vlc_object_detach( p_access );

    free( p_access->psz_access );
    free( p_access->psz_path );
    free( p_access->psz_demux );

    if( p_access->p_source )
    {
        access2_Delete( p_access->p_source );
    }

    vlc_object_destroy( p_access );
}

