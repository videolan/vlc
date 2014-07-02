/*****************************************************************************
 * bdsm/access.c: liBDSM based SMB/CIFS access module
 *****************************************************************************
 * Copyright (C) 2001-2014 VLC authors and VideoLAN
 *
 * Authors: Julien 'Lta' BALLET <contact # lta 'dot' io>
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

#include "common.h"

static int Control (access_t *p_access, int i_query, va_list args);
static int BrowseShare( access_t *p_access, input_item_node_t *p_node );
static int BrowseDirectory( access_t *p_access, input_item_node_t *p_node );
static bool AddToNode( access_t *p_access, input_item_node_t *p_node,
                       const char *psz_name );

int BrowserInit( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->psz_share == NULL )
        p_access->pf_readdir = BrowseShare;
    else
        p_access->pf_readdir = BrowseDirectory;
    p_access->pf_control = Control;

    return VLC_SUCCESS;
}

static int BrowseShare( access_t *p_access, input_item_node_t *p_node )
{
    smb_share_list  shares;
    const char     *psz_name;
    size_t          share_count;

    share_count = smb_share_get_list( p_access->p_sys->p_session, &shares );
    if( !share_count )
        return VLC_ENOITEM;

    for( size_t i = 0; i < share_count; i++ )
    {
        psz_name = smb_share_list_at( shares, i );

        if( psz_name[strlen( psz_name ) - 1] == '$')
            continue;

        AddToNode( p_access, p_node, psz_name );
    }

    smb_share_list_destroy( shares );
    return VLC_SUCCESS;
}

static int BrowseDirectory( access_t *p_access, input_item_node_t *p_node )
{
    access_sys_t   *p_sys = p_access->p_sys;
    smb_stat_list   files;
    smb_stat        st;
    char           *psz_query;
    const char     *psz_name;
    size_t          files_count;
    int             i_ret;

    if( p_sys->psz_path != NULL )
    {
        i_ret = asprintf( &psz_query, "%s\\*", p_sys->psz_path );
        if( i_ret == -1 )
            return VLC_ENOMEM;
        files = smb_find( p_sys->p_session, p_sys->i_tid, psz_query );
        free( psz_query );
    }
    else
        files = smb_find( p_sys->p_session, p_sys->i_tid, "\\*" );

    if( files == NULL )
        return VLC_ENOITEM;

    files_count = smb_stat_list_count( files );
    for( size_t i = 0; i < files_count; i++ )
    {
        st = smb_stat_list_at( files, i );

        if( st == NULL )
            goto error;

        psz_name = smb_stat_name( st );

        /* Avoid infinite loop */
        if( !strcmp( psz_name, ".") || !strcmp( psz_name, "..") )
            continue;

        AddToNode( p_access, p_node, psz_name );
    }

    smb_stat_list_destroy( files );
    return VLC_SUCCESS;

    error:
        smb_stat_list_destroy( files );
        return VLC_ENOITEM;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    VLC_UNUSED( p_access );

    switch( i_query )
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
            *va_arg( args, bool* ) = false;
            break;

        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
            *va_arg( args, bool* ) = true;
            break;

        case ACCESS_GET_PTS_DELAY:
            *va_arg( args, int64_t * ) = DEFAULT_PTS_DELAY * 1000;
            break;

        default:
            return VLC_EGENERIC;
     }
     return VLC_SUCCESS;
 }

static bool AddToNode( access_t *p_access, input_item_node_t *p_node,
                       const char *psz_name )
{
    access_sys_t *p_sys = p_access->p_sys;
    input_item_t *p_item;
    char         *psz_uri, *psz_option;
    int           i_ret;

    i_ret = asprintf( &psz_uri, "%s/%s", p_node->p_item->psz_uri, psz_name );
    /* XXX Handle ENOMEM by enabling retry */
    if( i_ret == -1 )
        return false;

    p_item = input_item_New( psz_uri, psz_name );
    free( psz_uri );
    if( p_item == NULL )
        return false;

    input_item_CopyOptions( p_node->p_item, p_item );
    input_item_node_AppendItem( p_node, p_item );

    /* Here we save on the node the credentials that allowed us to login.
     * That way the user isn't prompted more than once for credentials */
    i_ret = asprintf( &psz_option, "smb-user=%s", p_sys->creds.login );
    if( i_ret != -1 )
        input_item_AddOption( p_item, psz_option, VLC_INPUT_OPTION_TRUSTED );
    free( psz_option );
    i_ret = asprintf( &psz_option, "smb-pwd=%s", p_sys->creds.password );
    if( i_ret != -1 )
        input_item_AddOption( p_item, psz_option, VLC_INPUT_OPTION_TRUSTED );
    free( psz_option );
    asprintf( &psz_option, "smb-domain=%s", p_sys->creds.domain );
    if( i_ret != -1 )
        input_item_AddOption( p_item, psz_option, VLC_INPUT_OPTION_TRUSTED );
    free( psz_option );

    input_item_Release( p_item );
    return true;
}
