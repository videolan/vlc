/*****************************************************************************
 * mms.c: MMS over tcp, udp and http access plug-in
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "mms.h"

/****************************************************************************
 * NOTES:
 *  MMSProtocole documentation found at http://get.to/sdp
 ****************************************************************************/

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for MMS streams. This " \
    "value should be set in millisecond units." )

#define ALL_TEXT N_("Force selection of all streams")

#define BITRATE_TEXT N_("Select maximum bitrate stream")
#define BITRATE_LONGTEXT N_( \
    "Always select the stream with the maximum bitrate." )

vlc_module_begin();
    set_shortname( _("MMS") );
    set_description( _("Microsoft Media Server (MMS) input") );
    set_capability( "access2", -1 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_integer( "mms-caching", 19 * DEFAULT_PTS_DELAY / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );

    add_bool( "mms-all", 0, NULL, ALL_TEXT, "", VLC_TRUE );
    add_integer( "mms-maxbitrate", 0, NULL, BITRATE_TEXT, BITRATE_LONGTEXT ,
                 VLC_FALSE );

    add_shortcut( "mms" );
    add_shortcut( "mmsu" );
    add_shortcut( "mmst" );
    add_shortcut( "mmsh" );
    add_shortcut( "http" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct access_sys_t
{
    int i_proto;
};


/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t *p_access = (access_t*)p_this;

    /* First set ipv4/ipv6 */
    var_Create( p_access, "ipv4", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_access, "ipv6", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* mms-caching */
    var_Create( p_access, "mms-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* use specified method */
    if( *p_access->psz_access )
    {
        if( !strncmp( p_access->psz_access, "mmsu", 4 ) )
        {
            return E_( MMSTUOpen )( p_access );
        }
        else if( !strncmp( p_access->psz_access, "mmst", 4 ) )
        {
            return E_( MMSTUOpen )( p_access );
        }
        else if( !strncmp( p_access->psz_access, "mmsh", 4 ) ||
                 !strncmp( p_access->psz_access, "http", 4 ) )
        {
            return E_( MMSHOpen )( p_access );
        }
    }

    if( E_( MMSTUOpen )( p_access ) )
    {
        /* try mmsh if mmstu failed */
        return E_( MMSHOpen )( p_access );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->i_proto == MMS_PROTO_TCP || p_sys->i_proto == MMS_PROTO_UDP )
    {
        E_( MMSTUClose )( p_access );
    }
    else if( p_sys->i_proto == MMS_PROTO_HTTP )
    {
        E_( MMSHClose )( p_access );
    }
}
