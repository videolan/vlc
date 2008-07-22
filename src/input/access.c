/*****************************************************************************
 * access.c
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "input_internal.h"

/*****************************************************************************
 * access_InternalNew:
 *****************************************************************************/
static access_t *access_InternalNew( vlc_object_t *p_obj, const char *psz_access,
                                      const char *psz_demux, const char *psz_path,
                                      access_t *p_source )
{
    static const char typename[] = "access";
    access_t *p_access = vlc_custom_create( p_obj, sizeof (*p_access),
                                            VLC_OBJECT_GENERIC, typename );

    if( p_access == NULL )
        return NULL;

    /* Parse URL */
    p_access->p_source = p_source;
    if( p_source )
    {
        msg_Dbg( p_obj, "creating access filter '%s'", psz_access );
    }
    else
    {
        msg_Dbg( p_obj, "creating access '%s' path='%s'",
                 psz_access, psz_path );
        p_access->psz_path   = strdup( psz_path );
    }
    p_access->psz_access = strdup( psz_access );
    p_access->psz_demux  = strdup( psz_demux );

    p_access->pf_read    = NULL;
    p_access->pf_block   = NULL;
    p_access->pf_seek    = NULL;
    p_access->pf_control = NULL;
    p_access->p_sys      = NULL;
    p_access->info.i_update = 0;
    p_access->info.i_size   = 0;
    p_access->info.i_pos    = 0;
    p_access->info.b_eof    = false;
    p_access->info.b_prebuffered = false;
    p_access->info.i_title  = 0;
    p_access->info.i_seekpoint = 0;


    /* Before module_Need (for var_Create...) */
    vlc_object_attach( p_access, p_obj );

    p_access->p_module =
         module_Need( p_access, p_source ? "access_filter" : "access",
                      psz_access, true );

    if( p_access->p_module == NULL )
    {
        msg_StackAdd( "could not create access" );
        vlc_object_detach( p_access );
        free( p_access->psz_access );
        free( p_access->psz_path );
        free( p_access->psz_demux );
        vlc_object_release( p_access );
        return NULL;
    }

    return p_access;
}

/*****************************************************************************
 * access_New:
 *****************************************************************************/
access_t *__access_New( vlc_object_t *p_obj, const char *psz_access,
                         const char *psz_demux, const char *psz_path )
{
    return access_InternalNew( p_obj, psz_access, psz_demux,
                                psz_path, NULL );
}

/*****************************************************************************
 * access_FilterNew:
 *****************************************************************************/
access_t *access_FilterNew( access_t *p_source, const char *psz_access_filter )
{
    return access_InternalNew( VLC_OBJECT(p_source), psz_access_filter,
                                p_source->psz_demux, p_source->psz_path,
                                p_source );
}

/*****************************************************************************
 * access_Delete:
 *****************************************************************************/
void access_Delete( access_t *p_access )
{
    module_Unneed( p_access, p_access->p_module );
    vlc_object_detach( p_access );

    free( p_access->psz_access );
    free( p_access->psz_path );
    free( p_access->psz_demux );

    if( p_access->p_source )
    {
        access_Delete( p_access->p_source );
    }

    vlc_object_release( p_access );
}

