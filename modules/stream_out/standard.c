/*****************************************************************************
 * standard.c: standard stream output module
 *****************************************************************************
 * Copyright (C) 2003-2004 VideoLAN
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
#include <string.h>
#include <unistd.h>

#include <vlc/vlc.h>
#include <vlc/sout.h>

#include "announce.h"
#include "network.h"

#define DEFAULT_IPV6_SCOPE '8'
#define DEFAULT_PORT 1234

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, block_t* );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Standard stream output") );
    set_capability( "sout stream", 50 );
    add_shortcut( "standard" );
    add_shortcut( "std" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_stream_sys_t
{
    sout_mux_t           *p_mux;
    slp_session_t        *p_slp;
    session_descriptor_t *p_session;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_instance_t     *p_sout = p_stream->p_sout;
    slp_session_t       *p_slp = NULL;
    session_descriptor_t *p_session = NULL;

    char *psz_mux      = sout_cfg_find_value( p_stream->p_cfg, "mux" );
    char *psz_access   = sout_cfg_find_value( p_stream->p_cfg, "access" );
    char *psz_url      = sout_cfg_find_value( p_stream->p_cfg, "url" );
    char *psz_sdp      = NULL;

    vlc_url_t      *p_url;
    sout_cfg_t *p_sap_cfg = sout_cfg_find( p_stream->p_cfg, "sap" );
#ifdef HAVE_SLP_H
    sout_cfg_t *p_slp_cfg = sout_cfg_find( p_stream->p_cfg, "slp" );
#endif

    sout_access_out_t   *p_access;
    sout_mux_t          *p_mux;

    char                *psz_mux_byext = NULL;

    p_stream->p_sys        = malloc( sizeof( sout_stream_sys_t) );
    p_stream->p_sys->p_session = NULL;

    msg_Dbg( p_this, "creating `%s/%s://%s'",
             psz_access, psz_mux, psz_url );

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

    if( ( psz_access == NULL || *psz_access == '\0' )&&
        ( psz_mux == NULL ||  *psz_mux == '\0' ) )
    {
        if( psz_mux_byext )
        {
            msg_Warn( p_stream,
                      "no access _and_ no muxer, extention gives file/%s",
                      psz_mux_byext );
            psz_access = "file";
            psz_mux    = psz_mux_byext;
        }
        else
        {
            msg_Err( p_stream, "no access _and_ no muxer (fatal error)" );
            return VLC_EGENERIC;
        }
    }

    if( psz_access && *psz_access &&
        ( psz_mux == NULL || *psz_mux == '\0' ) )
    {
        /* access given, no mux */
        if( !strncmp( psz_access, "mmsh", 4 ) )
        {
            psz_mux = "asfh";
        }
        else if( !strncmp( psz_access, "udp", 3 ) )
        {
            psz_mux = "ts";
        }
        else
        {
            psz_mux = psz_mux_byext;
        }
    }
    else if( psz_mux && *psz_mux &&
             ( psz_access == NULL || *psz_access == '\0' ) )
    {
        /* mux given, no access */
        if( !strncmp( psz_mux, "asfh", 4 ) )
        {
            psz_access = "mmsh";
        }
        else
        {
            /* default file */
            psz_access = "file";
        }
    }

    /* fix or warm of incompatible couple */
    if( psz_mux && *psz_mux && psz_access && *psz_access )
    {
        if( !strncmp( psz_access, "mmsh", 4 ) && strncmp( psz_mux, "asfh", 4 ) )
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
                psz_mux = "asfh";
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
        return( VLC_EGENERIC );
    }
    msg_Dbg( p_stream, "access opened" );

    /* *** find and open appropriate mux module *** */
    p_mux = sout_MuxNew( p_sout, psz_mux, p_access );
    if( p_mux == NULL )
    {
        msg_Err( p_stream, "no suitable sout mux module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_url );

        sout_AccessOutDelete( p_access );
        return( VLC_EGENERIC );
    }
    msg_Dbg( p_stream, "mux opened" );

    /*  *** Create the SAP Session structure *** */
    if( psz_access &&  p_sap_cfg && ( strstr( psz_access, "udp" ) ||
                    strstr( psz_access ,  "rtp" ) ) )
    {
        session_descriptor_t *p_session=  sout_AnnounceSessionCreate();
        announce_method_t *p_method = sout_AnnounceMethodCreate(
                                                  METHOD_TYPE_SAP);

        /* Parse user input */
        if( p_sap_cfg->psz_value )
        {
            char *psz_sap = p_sap_cfg->psz_value;
            /* subconfig */
            if( ! strncmp(psz_sap, "sap{", 4 ) )
            {
                sout_cfg_t *p_cfg;
                char *psz_curr,*psz_null;
                sout_cfg_parser( &psz_null, &p_cfg, psz_sap );
                psz_curr =  sout_cfg_find_value( p_cfg,"name");
                if( psz_curr != NULL)
                {
                    p_session->psz_name = strdup( psz_curr );
                }
                else
                {
                    p_session->psz_name = strdup( psz_url );

                }

                psz_curr = sout_cfg_find_value( p_cfg,"ip_version");
                if( psz_curr != NULL)
                {
                    p_method->i_ip_version = atoi( psz_curr ) != 0 ?
                                                     atoi(psz_curr) :
                                                     4;
                }
            }
            else
            {
                p_session->psz_name = strdup( p_sap_cfg->psz_value );
            }
        }
        else
        {
            p_session->psz_name = strdup( psz_url );
        }

        /* Now, parse the URL to extract host and port */
        p_url = (vlc_url_t *)malloc( sizeof(vlc_url_t ) );
        if ( ! p_url )
        {
            return NULL;
        }

        vlc_UrlParse( p_url, psz_url , 0);

        if (!p_url->psz_host)
        {
            return NULL;
        }

        if(p_url->i_port == 0)
        {
                p_url->i_port = DEFAULT_PORT;
        }

        p_session->psz_uri = p_url->psz_host;
        p_session->i_port = p_url->i_port;
        p_session->psz_sdp = NULL;

        p_session->i_ttl = config_GetInt( p_sout,"ttl" );
        p_session->i_payload = 33;

        msg_Info( p_this, "SAP Enabled");

        sout_AnnounceRegister( p_sout, p_session, p_method );

        /* FIXME: Free p_method */

        p_stream->p_sys->p_session = p_session;

        if( p_url )
        {
            vlc_UrlClean( p_url );
            free( p_url );
            p_url = NULL;
        }
    }

    /* *** Register with slp *** */
#ifdef HAVE_SLP_H
    if( p_slp_cfg && ( strstr( psz_access, "udp" ) ||
                       strstr( psz_access ,  "rtp" ) ) )
    {
        msg_Info( p_this, "SLP Enabled");
        if( sout_SLPReg( p_sout, psz_url,
            p_slp_cfg->psz_value ? p_slp_cfg->psz_value : psz_url) )
        {
           msg_Warn( p_sout, "SLP Registering failed");
        }
        else
        {
            p_slp = (slp_session_t*)malloc(sizeof(slp_session_t));
            if(!p_slp)
            {
                msg_Warn(p_sout,"out of memory");
//                if( p_sap ) free( p_sap );
                return -1;
            }
            p_slp->psz_url= strdup(psz_url);
            p_slp->psz_name = strdup(
                    p_slp_cfg->psz_value ? p_slp_cfg->psz_value : psz_url);
        }
    }
#endif

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys->p_mux = p_mux;
    p_stream->p_sys->p_slp = p_slp;

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
    }

#ifdef HAVE_SLP_H
    if( p_sys->p_slp )
    {
            sout_SLPDereg( (sout_instance_t *)p_this,
                        p_sys->p_slp->psz_url,
                        p_sys->p_slp->psz_name);
            free( p_sys->p_slp);
    }
#endif


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
    sout_instance_t   *p_sout = p_stream->p_sout;

    sout_MuxSendBuffer( p_sys->p_mux, id->p_input, p_buffer );

    return VLC_SUCCESS;
}
