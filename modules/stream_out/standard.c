/*****************************************************************************
 * standard.c: standard stream output module
 *****************************************************************************
 * Copyright (C) 2003-2007 the VideoLAN team
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x0500
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_sout.h>

#include <vlc_network.h>
#include "vlc_url.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ACCESS_TEXT N_("Output access method")
#define ACCESS_LONGTEXT N_( \
    "Output method to use for the stream." )
#define MUX_TEXT N_("Output muxer")
#define MUX_LONGTEXT N_( \
    "Muxer to use for the stream." )
#define DEST_TEXT N_("Output destination")
#define DEST_LONGTEXT N_( \
    "Destination (URL) to use for the stream. Overrides path and bind parameters" )
#define BIND_TEXT N_("address to bind to (helper setting for dst)")
#define BIND_LONGTEXT N_( \
  "address:port to bind vlc to listening incoming streams "\
  "helper setting for dst,dst=bind+'/'+path. dst-parameter overrides this" )
#define PATH_TEXT N_("filename for stream (helper setting for dst)")
#define PATH_LONGTEXT N_( \
  "Filename for stream "\
  "helper setting for dst, dst=bind+'/'+path, dst-parameter overrides this" )
#define NAME_TEXT N_("Session name")
#define NAME_LONGTEXT N_( \
    "This is the name of the session that will be announced in the SDP " \
    "(Session Descriptor)." )

#define GROUP_TEXT N_("Session groupname")
#define GROUP_LONGTEXT N_( \
  "This allows you to specify a group for the session, that will be announced "\
  "if you choose to use SAP." )

#define DESC_TEXT N_("Session description")
#define DESC_LONGTEXT N_( \
    "This allows you to give a short description with details about the stream, " \
    "that will be announced in the SDP (Session Descriptor)." )
#define URL_TEXT N_("Session URL")
#define URL_LONGTEXT N_( \
    "This allows you to give an URL with more details about the stream " \
    "(often the website of the streaming organization), that will " \
    "be announced in the SDP (Session Descriptor)." )
#define EMAIL_TEXT N_("Session email")
#define EMAIL_LONGTEXT N_( \
    "This allows you to give a contact mail address for the stream, that will " \
    "be announced in the SDP (Session Descriptor)." )
#define PHONE_TEXT N_("Session phone number")
#define PHONE_LONGTEXT N_( \
    "This allows you to give a contact telephone number for the stream, that will " \
    "be announced in the SDP (Session Descriptor)." )


#define SAP_TEXT N_("SAP announcing")
#define SAP_LONGTEXT N_("Announce this session with SAP.")

static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

#define SOUT_CFG_PREFIX "sout-standard-"

vlc_module_begin ()
    set_shortname( N_("Standard"))
    set_description( N_("Standard stream output") )
    set_capability( "sout stream", 50 )
    add_shortcut( "standard" )
    add_shortcut( "std" )
    set_category( CAT_SOUT )
    set_subcategory( SUBCAT_SOUT_STREAM )

    add_string( SOUT_CFG_PREFIX "access", "", NULL, ACCESS_TEXT,
                ACCESS_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "mux", "", NULL, MUX_TEXT,
                MUX_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "dst", "", NULL, DEST_TEXT,
                DEST_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "bind", "", NULL, BIND_TEXT,
                BIND_LONGTEXT, false )
    add_string( SOUT_CFG_PREFIX "path", "", NULL, PATH_TEXT,
                PATH_LONGTEXT, false )

    add_bool( SOUT_CFG_PREFIX "sap", false, NULL, SAP_TEXT, SAP_LONGTEXT,
              true )
    add_string( SOUT_CFG_PREFIX "name", "", NULL, NAME_TEXT, NAME_LONGTEXT,
                                        true )
    add_string( SOUT_CFG_PREFIX "group", "", NULL, GROUP_TEXT, GROUP_LONGTEXT,
                                        true )
    add_string( SOUT_CFG_PREFIX "description", "", NULL, DESC_TEXT, DESC_LONGTEXT,
                                        true )
    add_string( SOUT_CFG_PREFIX "url", "", NULL, URL_TEXT, URL_LONGTEXT,
                                        true )
    add_string( SOUT_CFG_PREFIX "email", "", NULL, EMAIL_TEXT, EMAIL_LONGTEXT,
                                        true )
    add_string( SOUT_CFG_PREFIX "phone", "", NULL, PHONE_TEXT, PHONE_LONGTEXT,
                                        true )
    add_obsolete_bool( SOUT_CFG_PREFIX "sap-ipv6" )

    set_callbacks( Open, Close )
vlc_module_end ()


/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static const char *const ppsz_sout_options[] = {
    "access", "mux", "url", "dst",
    "sap", "name", "group", "description", "url", "email", "phone",
    "bind", "path", NULL
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
    sout_stream_sys_t   *p_sys;

    char *psz_mux;
    char *psz_access;
    char *psz_url=NULL;
    char *psz_bind;
    char *psz_path;

    vlc_value_t val;

    sout_access_out_t   *p_access;
    sout_mux_t          *p_mux;

    const char          *psz_mux_byext = NULL;

    config_ChainParse( p_stream, SOUT_CFG_PREFIX, ppsz_sout_options,
                   p_stream->p_cfg );

    var_Get( p_stream, SOUT_CFG_PREFIX "access", &val );
    psz_access = *val.psz_string ? val.psz_string : NULL;
    if( !*val.psz_string ) free( val.psz_string );

    var_Get( p_stream, SOUT_CFG_PREFIX "mux", &val );
    psz_mux = *val.psz_string ? val.psz_string : NULL;
    if( !*val.psz_string ) free( val.psz_string );

    var_Get( p_stream, SOUT_CFG_PREFIX "bind", &val );
    psz_bind = *val.psz_string ? val.psz_string : NULL;
    if( !*val.psz_string ) free( val.psz_string);

    var_Get( p_stream, SOUT_CFG_PREFIX "path", &val );
    psz_path = *val.psz_string ? val.psz_string : NULL;
    if( !*val.psz_string ) free( val.psz_string);

    if( psz_bind ) psz_url = psz_bind;
    if( psz_url && psz_path ) 
    {
        if( asprintf( &psz_url,"%s/%s",psz_url,psz_path ) == -1 )
            psz_url = NULL;
        free( psz_path );
    }

    var_Get( p_stream, SOUT_CFG_PREFIX "dst", &val );
    if( *val.psz_string ) 
    {
        free( psz_url);
        psz_url = val.psz_string;
    }
    else
        free( val.psz_string );

    p_sys = p_stream->p_sys = malloc( sizeof( sout_stream_sys_t) );
    if( !p_sys )
    {
        free( psz_url );
        return VLC_ENOMEM;
    }
    p_stream->p_sys->p_session = NULL;

    msg_Dbg( p_this, "creating `%s/%s://%s'", psz_access, psz_mux, psz_url );

    /* ext -> muxer name */
    if( psz_url && strrchr( psz_url, '.' ) )
    {
        /* by extension */
        static struct { const char ext[6]; const char mux[32]; } exttomux[] =
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
            { "wav", "wav" },
            { "flv", "ffmpeg{mux=flv}" },
            { "mkv", "ffmpeg{mux=matroska}"},
            { "",    "" }
        };
        const char *psz_ext = strrchr( psz_url, '.' ) + 1;
        int  i;

        msg_Dbg( p_this, "extension is %s", psz_ext );
        for( i = 0; exttomux[i].ext[0]; i++ )
        {
            if( !strcasecmp( psz_ext, exttomux[i].ext ) )
            {
                psz_mux_byext = exttomux[i].mux;
                break;
            }
        }
        msg_Dbg( p_this, "extension -> mux=%s", psz_mux_byext );
    }

    /* We fix access/mux to valid couple */

    if( !psz_access && !psz_mux )
    {
        if( psz_mux_byext )
        {
            msg_Warn( p_stream,
                      "no access _and_ no muxer, extension gives file/%s",
                      psz_mux_byext );
            psz_access = strdup("file");
            psz_mux    = strdup(psz_mux_byext);
        }
        else
        {
            msg_Err( p_stream, "no access _and_ no muxer (fatal error)" );
            free( psz_url );
            free( p_sys );
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
        else if (!strcmp (psz_access, "udp"))
        {
            psz_mux = strdup("ts");
        }
        else if( psz_mux_byext )
        {
            psz_mux = strdup(psz_mux_byext);
        }
        else
        {
            msg_Err( p_stream, "no mux specified or found by extension" );
            free( p_sys );
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
    if( !strncmp( psz_access, "mmsh", 4 ) &&
        strncmp( psz_mux, "asfh", 4 ) )
    {
        char *p = strchr( psz_mux,'{' );

        msg_Warn( p_stream, "fixing to mmsh/asfh" );
        if( p )
        {
            if( asprintf( &p, "asfh%s", p ) == -1 )
                p = NULL;
            free( psz_mux );
            psz_mux = p;
        }
        else
        {
            free( psz_mux );
            psz_mux = strdup("asfh");
        }
    }
    else if( !strncmp( psz_access, "udp", 3 ) )
    {
        if( !strncmp( psz_mux, "ffmpeg", 6 ) )
        {   /* why would you use ffmpeg's ts muxer ? YOU DON'T LOVE VLC ??? */
            char *psz_ffmpeg_mux = var_CreateGetString( p_this, "ffmpeg-mux" );
            if( !psz_ffmpeg_mux || strncmp( psz_ffmpeg_mux, "mpegts", 6 ) )
                msg_Err( p_stream, "UDP is only valid with TS" );
            free( psz_ffmpeg_mux );
        }
        else if( strncmp( psz_mux, "ts", 2 ) )
        {
            msg_Err( p_stream, "UDP is only valid with TS" );
        }
    }
    else if( strncmp( psz_access, "file", 4 ) &&
             ( !strncmp( psz_mux, "mov", 3 ) ||
               !strncmp( psz_mux, "mp4", 3 ) ) )
    {
        msg_Err( p_stream, "mov and mp4 work only with file output" );
    }

    msg_Dbg( p_this, "using `%s/%s://%s'", psz_access, psz_mux, psz_url );

    /* *** find and open appropriate access module *** */
    p_access = sout_AccessOutNew( p_sout, psz_access, psz_url );
    if( p_access == NULL )
    {
        msg_Err( p_stream, "no suitable sout access module for `%s/%s://%s'",
                 psz_access, psz_mux, psz_url );
        free( psz_access );
        free( psz_mux );
        free( psz_url );
        free( p_sys );
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
        free( psz_access );
        free( psz_mux );
        free( psz_url );
        free( p_sys );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_stream, "mux opened" );

    /* *** Create the SAP Session structure *** */
    if( var_GetBool( p_stream, SOUT_CFG_PREFIX"sap" ) )
    {
        /* Create the SDP */
        static const struct addrinfo hints = {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_DGRAM,
            .ai_protocol = 0,
            .ai_flags = AI_NUMERICHOST | AI_NUMERICSERV
        };
        char *shost = var_GetNonEmptyString (p_access, "src-addr");
        char *dhost = var_GetNonEmptyString (p_access, "dst-addr");
        int sport = var_GetInteger (p_access, "src-port");
        int dport = var_GetInteger (p_access, "dst-port");
        struct sockaddr_storage src, dst;
        socklen_t srclen = 0, dstlen = 0;
        struct addrinfo *res;

        if ( vlc_getaddrinfo ( VLC_OBJECT(p_stream), dhost, dport, &hints, &res) == 0)
        {
            memcpy (&dst, res->ai_addr, dstlen = res->ai_addrlen);
            vlc_freeaddrinfo (res);
        }

        if (vlc_getaddrinfo ( VLC_OBJECT(p_stream), shost, sport, &hints, &res) == 0)
        {
            memcpy (&src, res->ai_addr, srclen = res->ai_addrlen);
            vlc_freeaddrinfo (res);
        }

        char *head = vlc_sdp_Start (VLC_OBJECT (p_stream), SOUT_CFG_PREFIX,
                                    (struct sockaddr *)&src, srclen,
                                    (struct sockaddr *)&dst, dstlen);
        free (shost);

        char *psz_sdp = NULL;
        if (head != NULL)
        {
            if (asprintf (&psz_sdp, "%s"
                          "m=video %d udp mpeg\r\n", head, dport) == -1)
                psz_sdp = NULL;
            free (head);
        }

        /* Register the SDP with the SAP thread */
        if (psz_sdp != NULL)
        {
            announce_method_t *p_method = sout_SAPMethod ();
            msg_Dbg (p_stream, "Generated SDP:\n%s", psz_sdp);

            p_sys->p_session =
                sout_AnnounceRegisterSDP (p_sout, psz_sdp, dhost, p_method);
            sout_MethodRelease (p_method);
            free( psz_sdp );
        }
        free (dhost);
    }

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_sys->p_mux = p_mux;

    free( psz_access );
    free( psz_mux );
    free( psz_url );

    if( !sout_AccessOutCanControlPace( p_access ) )
        p_sout->i_out_pace_nocontrol++;

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
        sout_AnnounceUnRegister( p_stream->p_sout, p_sys->p_session );

    sout_MuxDelete( p_sys->p_mux );
    if( !sout_AccessOutCanControlPace( p_access ) )
        p_stream->p_sout->i_out_pace_nocontrol--;
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
    if( !id )
        return NULL;

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
