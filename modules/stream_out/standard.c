/*****************************************************************************
 * standard.c: standard stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#include "network.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ACCESS_TEXT N_("Output access method")
#define ACCESS_LONGTEXT N_( \
    "Allows you to specify the output access method used for the streaming " \
    "output." )
#define MUX_TEXT N_("Output muxer")
#define MUX_LONGTEXT N_( \
    "Allows you to specify the output muxer method used for the streaming " \
    "output." )
#define URL_TEXT N_("Output URL (deprecated)")
#define URL_LONGTEXT N_( \
    "Allows you to specify the output URL used for the streaming output." \
    "Deprecated, use dst instead." )

#define DST_TEXT N_("Output destination")
#define DST_LONGTEXT N_( \
    "Allows you to specify the output destination used for the streaming output." )

#define NAME_TEXT N_("Session name")
#define NAME_LONGTEXT N_( \
    "Name of the session that will be announced with SAP" )

#define GROUP_TEXT N_("Session groupname")
#define GROUP_LONGTEXT N_( \
    "Name of the group that will be announced for the session" )

#define SAP_TEXT N_("SAP announcing")
#define SAP_LONGTEXT N_("Announce this session with SAP")

static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-standard-"

vlc_module_begin();
    set_shortname( _("Standard"));
    set_description( _("Standard stream output") );
    set_capability( "sout stream", 50 );
    add_shortcut( "standard" );
    add_shortcut( "std" );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_STREAM );

    add_string( SOUT_CFG_PREFIX "access", "", NULL, ACCESS_TEXT,
                ACCESS_LONGTEXT, VLC_FALSE );
    add_string( SOUT_CFG_PREFIX "mux", "", NULL, MUX_TEXT,
                MUX_LONGTEXT, VLC_FALSE );
    add_string( SOUT_CFG_PREFIX "dst", "", NULL, DST_TEXT,
                DST_LONGTEXT, VLC_FALSE );
        add_deprecated( SOUT_CFG_PREFIX "url", VLC_FALSE );

    add_bool( SOUT_CFG_PREFIX "sap", 0, NULL, SAP_TEXT, SAP_LONGTEXT, VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "name", "", NULL, NAME_TEXT, NAME_LONGTEXT,
                                        VLC_TRUE );
    add_string( SOUT_CFG_PREFIX "group", "", NULL, GROUP_TEXT, GROUP_LONGTEXT,
                                        VLC_TRUE );
    add_suppressed_bool( SOUT_CFG_PREFIX "sap-ipv6" );

    set_callbacks( Open, Close );
vlc_module_end();


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *ppsz_sout_options[] = {
    "access", "mux", "url", "dst",
    "sap", "name", "group",  NULL
};

#define DEFAULT_PORT 1234

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, block_t* );

struct sout_stream_sys_t
{
    sout_mux_t           *p_mux;
    session_descriptor_t *p_session;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_instance_t     *p_sout = p_stream->p_sout;

    char *psz_mux;
    char *psz_access;
    char *psz_url;

    vlc_value_t val;

    sout_access_out_t   *p_access;
    sout_mux_t          *p_mux;

    char                *psz_mux_byext = NULL;

    sout_CfgParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                   p_stream->p_cfg );

    var_Get( p_stream, SOUT_CFG_PREFIX "access", &val );
    psz_access = *val.psz_string ? val.psz_string : NULL;
    if( !*val.psz_string ) free( val.psz_string );

    var_Get( p_stream, SOUT_CFG_PREFIX "mux", &val );
    psz_mux = *val.psz_string ? val.psz_string : NULL;
    if( !*val.psz_string ) free( val.psz_string );


    var_Get( p_stream, SOUT_CFG_PREFIX "dst", &val );
    psz_url = *val.psz_string ? val.psz_string : NULL;
    if( !*val.psz_string ) free( val.psz_string );

    p_stream->p_sys = malloc( sizeof( sout_stream_sys_t) );
    p_stream->p_sys->p_session = NULL;

    msg_Dbg( p_this, "creating `%s/%s://%s'", psz_access, psz_mux, psz_url );

    /* ext -> muxer name */
    if( psz_url && strrchr( psz_url, '.' ) )
    {
        /* by extention */
        static struct { char *ext; char *mux; } exttomux[] =
        {
            { "avi", "avi" },
            { "ogg", "ogg" },
            { "ogm", "ogg" },
            { "mp4", "mp4" },
            { "mov", "mov" },
            { "moov","mov" },
            { "asf", "asf" },
            { "wma", "asf" },
            { "wmv", "asf" },
            { "trp", "ts" },
            { "ts",  "ts" },
            { "mpg", "ps" },
            { "mpeg","ps" },
            { "ps",  "ps" },
            { "mpeg1","mpeg1" },
            { "wav","wav" },
            { NULL,  NULL }
        };
        char *psz_ext = strrchr( psz_url, '.' ) + 1;
        int  i;

        msg_Dbg( p_this, "extention is %s", psz_ext );
        for( i = 0; exttomux[i].ext != NULL; i++ )
        {
            if( !strcasecmp( psz_ext, exttomux[i].ext ) )
            {
                psz_mux_byext = exttomux[i].mux;
                break;
            }
        }
        msg_Dbg( p_this, "extention -> mux=%s", psz_mux_byext );
    }

    /* We fix access/mux to valid couple */

    if( !psz_access && !psz_mux )
    {
        if( psz_mux_byext )
        {
            msg_Warn( p_stream,
                      "no access _and_ no muxer, extention gives file/%s",
                      psz_mux_byext );
            psz_access = strdup("file");
            psz_mux    = strdup(psz_mux_byext);
        }
        else
        {
            msg_Err( p_stream, "no access _and_ no muxer (fatal error)" );
            return VLC_EGENERIC;
        }
    }

    if( psz_access && !psz_mux )
    {
        /* access given, no mux */
        if( !strncmp( psz_access, "mmsh", 4 ) )
        {
            psz_mux = strdup("asfh");
        }
        else if( !strncmp( psz_access, "udp", 3 ) ||
                 !strncmp( psz_access, "rtp", 3 ) )
        {
            psz_mux = strdup("ts");
        }
        else if( psz_mux_byext )
        {
            psz_mux = strdup(psz_mux_byext);
        }
        else
        {
            msg_Err( p_stream, "no mux specified or found by extention" );
            return VLC_EGENERIC;
        }
    }
    else if( psz_mux && !psz_access )
    {
        /* mux given, no access */
        if( !strncmp( psz_mux, "asfh", 4 ) )
        {
            psz_access = strdup("mmsh");
        }
        else
        {
            /* default file */
            psz_access = strdup("file");
        }
    }

    /* fix or warn of incompatible couple */
    if( psz_mux && psz_access )
    {
        if( !strncmp( psz_access, "mmsh", 4 ) &&
            strncmp( psz_mux, "asfh", 4 ) )
        {
            char *p = strchr( psz_mux,'{' );

            msg_Warn( p_stream, "fixing to mmsh/asfh" );
            if( p )
            {
                /* -> a little memleak but ... */
                psz_mux = malloc( strlen( "asfh" ) + strlen( p ) + 1);
                sprintf( psz_mux, "asfh%s", p );
            }
            else
            {
                psz_mux = strdup("asfh");
            }
        }
        else if( ( !strncmp( psz_access, "rtp", 3 ) ||
                   !strncmp( psz_access, "udp", 3 ) ) &&
                 strncmp( psz_mux, "ts", 2 ) )
        {
            msg_Err( p_stream, "for now udp and rtp are only valid with TS" );
        }
        else if( strncmp( psz_access, "file", 4 ) &&
                 ( !strncmp( psz_mux, "mov", 3 ) ||
                   !strncmp( psz_mux, "mp4", 3 ) ) )
        {
            msg_Err( p_stream, "mov and mp4 work only with file output" );
        }
    }

    msg_Dbg( p_this, "using `%s/%s://%s'", psz_access, psz_mux, psz_url );

    /* *** find and open appropriate access module *** */
    p_access = sout_AccessOutNew( p_sout, psz_access, psz_url );
    if( p_access == NULL )
    {
        msg_Err( p_stream, "no suitable sout access module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_url );
        if( psz_access ) free( psz_access );
        if( psz_mux ) free( psz_mux );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_stream, "access opened" );

    /* *** find and open appropriate mux module *** */
    p_mux = sout_MuxNew( p_sout, psz_mux, p_access );
    if( p_mux == NULL )
    {
        msg_Err( p_stream, "no suitable sout mux module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_url );

        sout_AccessOutDelete( p_access );
        if( psz_access ) free( psz_access );
        if( psz_mux ) free( psz_mux );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_stream, "mux opened" );

    /*  *** Create the SAP Session structure *** */
    var_Get( p_stream, SOUT_CFG_PREFIX "sap", &val );
    if( val.b_bool &&
        ( strstr( psz_access, "udp" ) || strstr( psz_access , "rtp" ) ) )
    {
        session_descriptor_t *p_session = sout_AnnounceSessionCreate();
        announce_method_t *p_method =
            sout_AnnounceMethodCreate( METHOD_TYPE_SAP );
        vlc_url_t url;

        var_Get( p_stream, SOUT_CFG_PREFIX "name", &val );
        if( *val.psz_string )
            p_session->psz_name = val.psz_string;
        else
        {
            p_session->psz_name = strdup( psz_url );
            free( val.psz_string );
        }

        var_Get( p_stream, SOUT_CFG_PREFIX "group", &val );
        if( *val.psz_string )
            p_session->psz_group = val.psz_string;
        else
            free( val.psz_string );

        /* Now, parse the URL to extract host and port */
        vlc_UrlParse( &url, psz_url , 0);

        if( url.psz_host )
        {
            if( url.i_port == 0 ) url.i_port = DEFAULT_PORT;

            p_session->psz_uri = strdup( url.psz_host );
            p_session->i_port = url.i_port;
            p_session->psz_sdp = NULL;

            var_Get( p_access, "sout-udp-ttl", &val );
            p_session->i_ttl = val.i_int;
            p_session->i_payload = 33;
            p_session->b_rtp = strstr( psz_access, "rtp") ? 1 : 0;

            msg_Info( p_this, "SAP Enabled");

            sout_AnnounceRegister( p_sout, p_session, p_method );
            p_stream->p_sys->p_session = p_session;
        }
        vlc_UrlClean( &url );

        free( p_method );
    }

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys->p_mux = p_mux;

    if( psz_access ) free( psz_access );
    if( psz_mux ) free( psz_mux );
    if( psz_url ) free( psz_url );


    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys    = p_stream->p_sys;
    sout_access_out_t *p_access = p_sys->p_mux->p_access;

    if( p_sys->p_session != NULL )
    {
        sout_AnnounceUnRegister( p_stream->p_sout, p_sys->p_session );
        sout_AnnounceSessionDestroy( p_sys->p_session );
    }


    sout_MuxDelete( p_sys->p_mux );
    sout_AccessOutDelete( p_access );

    free( p_sys );
}

struct sout_stream_id_t
{
    sout_input_t *p_input;
};


static sout_stream_id_t * Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t  *id;

    id = malloc( sizeof( sout_stream_id_t ) );
    if( ( id->p_input = sout_MuxAddStream( p_sys->p_mux, p_fmt ) ) == NULL )
    {
        free( id );

        return NULL;
    }

    return id;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    sout_MuxDeleteStream( p_sys->p_mux, id->p_input );

    free( id );

    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 block_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    sout_MuxSendBuffer( p_sys->p_mux, id->p_input, p_buffer );

    return VLC_SUCCESS;
}
