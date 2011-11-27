/*****************************************************************************
 * sql.c: SQL Connection: Creators and destructors
 *****************************************************************************
 * Copyright (C) 2008-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Srikanth Raju <srikiraju at gmail dot com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_sql.h>
#include <vlc_modules.h>
#include <assert.h>
#include "libvlc.h"

#undef sql_Create
sql_t *sql_Create( vlc_object_t *p_this, const char *psz_name,
        const char* psz_host, int i_port,
        const char* psz_user, const char* psz_pass )
{
    sql_t *p_sql;

    p_sql = ( sql_t * ) vlc_custom_create( p_this, sizeof( sql_t ), "sql" );
    if( !p_sql )
    {
        msg_Err( p_this, "unable to create sql object" );
        return NULL;
    }

    p_sql->psz_host = strdup( psz_host );
    p_sql->psz_user = strdup( psz_user );
    p_sql->psz_pass = strdup( psz_pass );
    p_sql->i_port = i_port;

    p_sql->p_module = module_need( p_sql, "sql", psz_name,
                                   psz_name && *psz_name );
    if( !p_sql->p_module )
    {
        free( p_sql->psz_host );
        free( p_sql->psz_user );
        free( p_sql->psz_pass );
        vlc_object_release( p_sql );
        msg_Err( p_this, "SQL provider not found" );
        return NULL;
    }

    return p_sql;
}

#undef sql_Destroy
void sql_Destroy( vlc_object_t* obj )
{
    sql_t *p_sql = (sql_t *)obj;
    assert( p_sql );

    free( p_sql->psz_host );
    free( p_sql->psz_user );
    free( p_sql->psz_pass );

    module_unneed( p_sql, p_sql->p_module );

    vlc_object_release( obj );
}
