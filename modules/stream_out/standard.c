/*****************************************************************************
 * standard.c
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: standard.c,v 1.8 2003/07/07 15:50:43 gbazin Exp $
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
#include <announce.h>

#define DEFAULT_IPV6_SCOPE "8"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add ( sout_stream_t *, sout_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, sout_buffer_t* );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Standard stream") );
    set_capability( "sout stream", 50 );
    add_shortcut( "standard" );
    add_shortcut( "std" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_stream_sys_t
{
    sout_mux_t           *p_mux;
    sap_session_t        *p_sap;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t       *p_stream = (sout_stream_t*)p_this;
    sout_instance_t     *p_sout = p_stream->p_sout;
    sout_stream_sys_t   *p_sys = malloc( sizeof( sout_stream_sys_t) );

    char *psz_mux      = sout_cfg_find_value( p_stream->p_cfg, "mux" );
    char *psz_access   = sout_cfg_find_value( p_stream->p_cfg, "access" );
    char *psz_url      = sout_cfg_find_value( p_stream->p_cfg, "url" );
    char *psz_ipv      = sout_cfg_find_value( p_stream->p_cfg, "sap_ipv" );
    char *psz_v6_scope = sout_cfg_find_value( p_stream->p_cfg, "sap_v6scope" );
    sout_cfg_t *p_sap_cfg = sout_cfg_find( p_stream->p_cfg, "sap" );

    sout_access_out_t   *p_access;
    sout_mux_t          *p_mux;    

    /* SAP is only valid for UDP or RTP streaming */
    if( psz_access == NULL ) psz_access = "udp";

    /* Get SAP IP version to use */
    if(psz_ipv == NULL) psz_ipv = "4";
    if(psz_v6_scope == NULL) psz_v6_scope = DEFAULT_IPV6_SCOPE;
    p_sys->p_sap = NULL;

    msg_Dbg( p_this, "creating `%s/%s://%s'",
             psz_access, psz_mux, psz_url );

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
    if( p_sap_cfg && ( strstr( psz_access, "udp" ) ||
                       strstr( psz_access ,  "rtp" ) ) )
    {
        msg_Info( p_this, "SAP Enabled");
        msg_Dbg( p_sout , "Creating SAP with IPv%i", atoi(psz_ipv) );

        p_sys->p_sap = sout_SAPNew( p_sout , psz_url ,
            p_sap_cfg->psz_value ? p_sap_cfg->psz_value : psz_url,
            atoi(psz_ipv), psz_v6_scope );

        if( !p_sys->p_sap )
            msg_Err( p_sout,"Unable to initialize SAP. SAP disabled");
    }   

    /* XXX beurk */
    p_sout->i_preheader = __MAX( p_sout->i_preheader, p_mux->i_preheader );


    p_sys->p_mux = p_mux;

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;
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

    if( p_sys->p_sap )
        sout_SAPDelete( (sout_instance_t *)p_this , p_sys->p_sap ); 

    sout_MuxDelete( p_sys->p_mux );
    sout_AccessOutDelete( p_access );

    free( p_sys );
}

struct sout_stream_id_t
{
    sout_input_t *p_input;
};


static sout_stream_id_t * Add( sout_stream_t *p_stream, sout_format_t *p_fmt )
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
                 sout_buffer_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_instance_t   *p_sout = p_stream->p_sout;

    sout_MuxSendBuffer( p_sys->p_mux, id->p_input, p_buffer );

    if( p_sys->p_sap )
       sout_SAPSend( p_sout , p_sys->p_sap );

    return VLC_SUCCESS;
}
