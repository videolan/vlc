/*****************************************************************************
 * ftp.c: FTP input module
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 * Copyright © 2006 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr> - original code
 *          Rémi Denis-Courmont - EPSV support
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

#include <assert.h>
#include <stdint.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_access.h>
#include <vlc_dialog.h>
#include <vlc_input_item.h>
#include <vlc_network.h>
#include <vlc_url.h>
#include <vlc_tls.h>
#include <vlc_sout.h>
#include <vlc_charset.h>
#include <vlc_interrupt.h>
#include <vlc_keystore.h>

#ifndef IPPORT_FTP
# define IPPORT_FTP 21u
#endif

#ifndef IPPORT_FTPS
# define IPPORT_FTPS 990u
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int   InOpen ( vlc_object_t * );
static void  InClose( vlc_object_t * );
#ifdef ENABLE_SOUT
static int  OutOpen ( vlc_object_t * );
static void OutClose( vlc_object_t * );
#endif

#define USER_TEXT N_("Username")
#define USER_LONGTEXT N_("Username that will be used for the connection, " \
        "if no username is set in the URL.")
#define PASS_TEXT N_("Password")
#define PASS_LONGTEXT N_("Password that will be used for the connection, " \
        "if no username or password are set in URL.")
#define ACCOUNT_TEXT N_("FTP account")
#define ACCOUNT_LONGTEXT N_("Account that will be " \
    "used for the connection.")

#define LOGIN_DIALOG_TITLE _("FTP authentication")
#define LOGIN_DIALOG_TEXT _("Please enter a valid login and password for " \
        "the ftp connexion to %s")

vlc_module_begin ()
    set_shortname( "FTP" )
    set_description( N_("FTP input") )
    set_capability( "access", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_string( "ftp-user", NULL, USER_TEXT, USER_LONGTEXT, false )
    add_string( "ftp-pwd", NULL, PASS_TEXT, PASS_LONGTEXT, false )
    add_string( "ftp-account", "anonymous", ACCOUNT_TEXT,
                ACCOUNT_LONGTEXT, false )
    add_shortcut( "ftp", "ftps", "ftpes" )
    set_callbacks( InOpen, InClose )

#ifdef ENABLE_SOUT
    add_submodule ()
        set_shortname( "FTP" )
        set_description( N_("FTP upload output") )
        set_capability( "sout access", 0 )
        set_category( CAT_SOUT )
        set_subcategory( SUBCAT_SOUT_ACO )
        add_shortcut( "ftp", "ftps", "ftpes" )
        set_callbacks( OutOpen, OutClose )
#endif
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct access_sys_t access_sys_t;

static ssize_t Read( stream_t *, void *, size_t );
static int Seek( stream_t *, uint64_t );
static int Control( stream_t *, int, va_list );
static int DirRead( stream_t *, input_item_node_t * );
#ifdef ENABLE_SOUT
static int OutSeek( sout_access_out_t *, off_t );
static ssize_t Write( sout_access_out_t *, block_t * );
#endif

static int LoginUserPwd( vlc_object_t *, access_sys_t *,
                         const char *, const char *, bool * );
static void FeaturesCheck( void *, const char * );

typedef struct ftp_features_t
{
    bool b_unicode;
    bool b_authtls;
    bool b_mlst;
} ftp_features_t;

enum tls_mode_e
{
    NONE = 0,
    IMPLICIT,/* ftps */
    EXPLICIT /* ftpes */
};

struct access_sys_t
{
    vlc_url_t  url;

    ftp_features_t   features;
    vlc_tls_client_t *p_creds;
    enum tls_mode_e  tlsmode;
    vlc_tls_t *cmd;
    vlc_tls_t *data;

    char       sz_epsv_ip[NI_MAXNUMERICHOST];
    bool       out;
    uint64_t   offset;
    uint64_t   size;
};
#define GET_OUT_SYS( p_this ) \
    ((access_sys_t *)(((sout_access_out_t *)(p_this))->p_sys))

VLC_FORMAT(3, 4)
static int ftp_SendCommand( vlc_object_t *obj, access_sys_t *sys,
                            const char *fmt, ... )
{
    size_t fmtlen = strlen( fmt );
    char fmtbuf[fmtlen + 3];

    memcpy( fmtbuf, fmt, fmtlen );
    memcpy( fmtbuf + fmtlen, "\r\n", 3 );

    va_list args;
    char *cmd;
    int val;

    va_start( args, fmt );
    val = vasprintf( &cmd, fmtbuf, args );
    va_end( args );
    if( unlikely(val == -1) )
        return -1;

    if( strncmp( cmd, "PASS ", 5 ) && strncmp( cmd, "ACCT ", 5 ) )
        msg_Dbg( obj, "sending request: \"%.*s\" (%d bytes)", val-2, cmd, val );
    else
        msg_Dbg( obj, "sending request: \"%.*s XXXX\" (XX bytes)", 4, cmd );

    if( vlc_tls_Write( sys->cmd, cmd, val ) != val )
    {
        msg_Err( obj, "request failure" );
        val = -1;
    }
    else
        val = 0;
    free( cmd );
    return val;
}

static char *ftp_GetLine( vlc_object_t *obj, access_sys_t *sys )
{
    char *resp = vlc_tls_GetLine( sys->cmd );
    if( resp == NULL )
        msg_Err( obj, "response failure" );
    return resp;
}

/* TODO support this s**t :
 RFC 959 allows the client to send certain TELNET strings at any moment,
 even in the middle of a request:

 * \377\377.
 * \377\376x where x is one byte.
 * \377\375x where x is one byte. The server is obliged to send \377\374x
 *                                immediately after reading x.
 * \377\374x where x is one byte.
 * \377\373x where x is one byte. The server is obliged to send \377\376x
 *                                immediately after reading x.
 * \377x for any other byte x.

 These strings are not part of the requests, except in the case \377\377,
 where the request contains one \377. */
static int ftp_RecvReply( vlc_object_t *obj, access_sys_t *sys,
                          char **restrict strp,
                          void (*cb)(void *, const char *), void *opaque )
{
    char *resp = ftp_GetLine( obj, sys );
    if( resp == NULL )
        return -1;

    char *end;
    unsigned code = strtoul( resp, &end, 10 );
    if( (end - resp) != 3 || (*end != '-' && *end != ' ') )
    {
        msg_Err( obj, "malformatted response" );
        goto error;
    }
    msg_Dbg( obj, "received response: \"%s\"", resp );

    if( *end == '-' ) /* Multi-line response */
    {
        bool done;

        *end = ' ';
        do
        {
            char *line = ftp_GetLine( obj, sys );
            if( line == NULL )
                goto error;

            done = !strncmp( resp, line, 4 );
            if( !done )
                cb( opaque, line );
            free( line );
        }
        while( !done );
    }

    if( strp != NULL )
        *strp = resp;
    else
        free( resp );
    return code;
error:
    free( resp );
    return -1;
}

static int ftp_RecvAnswer( vlc_object_t *obj, access_sys_t *sys,
                           int *restrict codep, char **restrict strp,
                           void (*cb)(void *, const char *), void *opaque )
{
    char *str;
    int val = ftp_RecvReply( obj, sys, &str, cb, opaque );
    if( (val / 100) == 1 )
    {   /* There can be zero or one preliminary reply per command */
        free( str );
        val = ftp_RecvReply( obj, sys, &str, cb, opaque );
    }

    if( val >= 0 )
    {
        if( codep != NULL )
            *codep = val;
        if( strp != NULL )
            *strp = str;
        else
            free( str );
        val /= 100;
    }
    else
    {
        if( codep != NULL )
            *codep = 500;
        if( strp != NULL )
            *strp = NULL;
    }
    return val;
}

static void DummyLine( void *data, const char *str )
{
    (void) data; (void) str;
}

static int ftp_RecvCommand( vlc_object_t *obj, access_sys_t *sys,
                            int *restrict codep, char **restrict strp )
{
    return ftp_RecvAnswer( obj, sys, codep, strp, DummyLine, NULL );
}

static int ftp_RecvCommandInit( vlc_object_t *obj, access_sys_t *sys )
{
    int val = ftp_RecvReply( obj, sys, NULL, DummyLine, NULL );
    if( val >= 0 )
        val /= 100;
    return val;
}

static int ftp_StartStream( vlc_object_t *, access_sys_t *, uint64_t, bool );
static int ftp_StopStream ( vlc_object_t *, access_sys_t * );

static int readTLSMode( vlc_object_t *obj, access_sys_t *p_sys,
                        const char * psz_access )
{
    if ( !strncmp( psz_access, "ftps", 4 ) )
        p_sys->tlsmode = IMPLICIT;
    else
    if ( !strncmp( psz_access, "ftpes", 5 ) )
        p_sys->tlsmode = EXPLICIT;
    else
    {
        p_sys->p_creds = NULL;
        p_sys->tlsmode = NONE;
        return 0;
    }

    p_sys->p_creds = vlc_tls_ClientCreate( obj );
    return (p_sys->p_creds != NULL) ? 0 : -1;
}

static int createCmdTLS( vlc_object_t *p_access, access_sys_t *p_sys,
                         const char *psz_session_name )
{
    /* TLS/SSL handshake */
    vlc_tls_t *secure = vlc_tls_ClientSessionCreate( p_sys->p_creds,
                                                     p_sys->cmd,
                                                     p_sys->url.psz_host,
                                                     psz_session_name,
                                                     NULL, NULL );
    if( secure == NULL )
    {
        msg_Err( p_access, "cannot establish FTP/TLS session on command channel" );
        return -1;
    }
    p_sys->cmd = secure;
    return 0;
}

static void clearCmd( access_sys_t *p_sys )
{
    if( p_sys->cmd != NULL )
    {
        vlc_tls_Close( p_sys->cmd );
        p_sys->cmd = NULL;
    }
}

static int Login( vlc_object_t *p_access, access_sys_t *p_sys, const char *path )
{
    int i_answer;

    /* *** Open a TCP connection with server *** */
    p_sys->cmd = vlc_tls_SocketOpenTCP( p_access, p_sys->url.psz_host,
                                        p_sys->url.i_port );
    if( p_sys->cmd == NULL )
    {
        msg_Err( p_access, "connection failed" );
        vlc_dialog_display_error( p_access, _("Network interaction failed"), "%s",
            _("VLC could not connect with the given server.") );
        goto error;
    }

    if ( p_sys->tlsmode == IMPLICIT ) /* FTPS Mode */
    {
        if ( createCmdTLS( p_access, p_sys, "ftps") < 0 )
            goto error;
    }

    while( ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) == 1 );

    if( i_answer / 100 != 2 )
    {
        msg_Err( p_access, "connection rejected" );
        vlc_dialog_display_error( p_access, _("Network interaction failed"), "%s",
            _("VLC's connection to the given server was rejected.") );
        goto error;
    }

    msg_Dbg( p_access, "connection accepted (%d)", i_answer );

    /* Features check first */
    if( ftp_SendCommand( p_access, p_sys, "FEAT" ) < 0
     || ftp_RecvAnswer( p_access, p_sys, NULL, NULL,
                        FeaturesCheck, &p_sys->features ) < 0 )
    {
         msg_Err( p_access, "cannot get server features" );
         goto error;
    }

    /* Create TLS Session */
    if( p_sys->tlsmode == EXPLICIT )
    {
        if ( ! p_sys->features.b_authtls )
        {
            msg_Err( p_access, "Server does not support TLS" );
            goto error;
        }

        if( ftp_SendCommand( p_access, p_sys, "AUTH TLS" ) < 0
         || ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0
         || i_answer != 234 )
        {
             msg_Err( p_access, "cannot switch to TLS: server replied with code %d",
                      i_answer );
             goto error;
        }

        if( createCmdTLS( p_access, p_sys, "ftpes") < 0 )
        {
            goto error;
        }
    }

    if( p_sys->tlsmode != NONE )
    {
        if( ftp_SendCommand( p_access, p_sys, "PBSZ 0" ) < 0 ||
            ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0 ||
            i_answer != 200 )
        {
            msg_Err( p_access, "Can't truncate Protection buffer size for TLS" );
            goto error;
        }

        if( ftp_SendCommand( p_access, p_sys, "PROT P" ) < 0 ||
            ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0 ||
            i_answer != 200 )
        {
            msg_Err( p_access, "Can't set Data channel protection" );
            goto error;
        }
    }

    vlc_url_t url;
    vlc_credential credential;
    if( vlc_UrlParseFixup( &url, path ) != 0 )
    {
        vlc_UrlClean( &url );
        goto error;
    }
    vlc_credential_init( &credential, &url );
    bool b_logged = false;

    /* First: try credentials from url / option */
    vlc_credential_get( &credential, p_access, "ftp-user", "ftp-pwd",
                        NULL, NULL );
    do
    {
        const char *psz_username = credential.psz_username;

        if( psz_username == NULL ) /* use anonymous by default */
            psz_username = "anonymous";

        if( LoginUserPwd( p_access, p_sys, psz_username,
                          credential.psz_password, &b_logged ) != 0
         || b_logged )
            break;
    }
    while( vlc_credential_get( &credential, p_access, "ftp-user", "ftp-pwd",
                               LOGIN_DIALOG_TITLE, LOGIN_DIALOG_TEXT,
                               url.psz_host ) );

    if( b_logged )
    {
        vlc_credential_store( &credential, p_access );
        vlc_credential_clean( &credential );
        vlc_UrlClean( &url );
        return 0;
    }
    vlc_credential_clean( &credential );
    vlc_UrlClean( &url );
error:
    clearCmd( p_sys );
    return -1;
}

static int LoginUserPwd( vlc_object_t *p_access, access_sys_t *p_sys,
                         const char *psz_user, const char *psz_pwd,
                         bool *p_logged )
{
    int i_answer;

    /* Send credentials over channel */
    if( ftp_SendCommand( p_access, p_sys, "USER %s", psz_user ) < 0 ||
        ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0 )
        return -1;

    switch( i_answer / 100 )
    {
        case 2:
            /* X.509 auth successful after AUTH TLS / RFC 2228 sec. 4 */
            if ( i_answer == 232 )
                msg_Dbg( p_access, "user accepted and authenticated" );
            else
                msg_Dbg( p_access, "user accepted" );
            break;
        case 3:
            msg_Dbg( p_access, "password needed" );

            if( ftp_SendCommand( p_access, p_sys, "PASS %s", psz_pwd ) < 0 ||
                ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0 )
                return -1;

            switch( i_answer / 100 )
            {
                case 2:
                    msg_Dbg( p_access, "password accepted" );
                    break;
                case 3:
                {
                    char *psz;
                    msg_Dbg( p_access, "account needed" );
                    psz = var_InheritString( p_access, "ftp-account" );
                    if( ftp_SendCommand( p_access, p_sys, "ACCT %s",
                                         psz ) < 0 ||
                        ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0 )
                    {
                        free( psz );
                        return -1;
                    }
                    free( psz );

                    if( i_answer / 100 != 2 )
                    {
                        msg_Err( p_access, "account rejected" );
                        vlc_dialog_display_error( p_access,
                          _("Network interaction failed"),
                          "%s", _("Your account was rejected.") );
                        return -1;
                    }
                    msg_Dbg( p_access, "account accepted" );
                    break;
                }

                default:
                    msg_Warn( p_access, "password rejected" );
                    *p_logged = false;
                    return 0;
            }
            break;
        default:
            msg_Warn( p_access, "user rejected" );
            *p_logged = false;
            return 0;
    }

    *p_logged = true;
    return 0;
}

static void FeaturesCheck( void *opaque, const char *feature )
{
    ftp_features_t *features = opaque;

    if( strcasestr( feature, "UTF8" ) != NULL )
        features->b_unicode = true;
    else
    if( strcasestr( feature, "AUTH TLS" ) != NULL )
        features->b_authtls = true;

    if( strcasestr( feature, "MLST" ) != NULL )
        features->b_mlst = true;
}

static int Connect( vlc_object_t *p_access, access_sys_t *p_sys, const char *path )
{
    if( Login( p_access, p_sys, path ) < 0 )
        return -1;

    /* Extended passive mode */
    if( ftp_SendCommand( p_access, p_sys, "EPSV ALL" ) < 0 )
    {
        msg_Err( p_access, "cannot request extended passive mode" );
        goto error;
    }

    if( ftp_RecvCommand( p_access, p_sys, NULL, NULL ) == 2 )
    {
        int fd = vlc_tls_GetFD(p_sys->cmd);
        if( net_GetPeerAddress( fd, p_sys->sz_epsv_ip, NULL ) )
            goto error;
    }
    else
    {
        /* If ESPV ALL fails, we fallback to PASV.
         * We have to restart the connection in case there is a NAT that
         * understands EPSV ALL in the way, and hence won't allow PASV on
         * the initial connection.
         */
        msg_Info( p_access, "FTP Extended passive mode disabled" );
        clearCmd( p_sys );

        if( Login( p_access, p_sys, path ) )
            goto error;
    }

    if( p_sys->url.psz_path &&
        (p_sys->features.b_unicode ? IsUTF8 : IsASCII)(p_sys->url.psz_path) == NULL )
    {
        msg_Err( p_access, "unsupported path: \"%s\"", p_sys->url.psz_path );
        goto error;
    }

    /* check binary mode support */
    if( ftp_SendCommand( p_access, p_sys, "TYPE I" ) < 0 ||
        ftp_RecvCommand( p_access, p_sys, NULL, NULL ) != 2 )
    {
        msg_Err( p_access, "cannot set binary transfer mode" );
        goto error;
    }

    return 0;

error:
    clearCmd( p_sys );
    return -1;
}


static int parseURL( vlc_url_t *url, const char *path, enum tls_mode_e mode )
{
    if( path == NULL )
        return VLC_EGENERIC;

    /* *** Parse URL and get server addr/port and path *** */
    while( *path == '/' )
        path++;

    vlc_UrlParseFixup( url, path );

    if( url->psz_host == NULL || *url->psz_host == '\0' )
        return VLC_EGENERIC;

    if( url->i_port <= 0 )
    {
        if( mode == IMPLICIT )
            url->i_port = IPPORT_FTPS;
        else
            url->i_port = IPPORT_FTP; /* default port */
    }

    if( url->psz_path == NULL )
        return VLC_SUCCESS;
    /* FTP URLs are relative to user's default directory (RFC1738 §3.2)
    For absolute path use ftp://foo.bar//usr/local/etc/filename */
    /* FIXME: we should issue a series of CWD, one per slash */
    if( url->psz_path )
    {
        assert( url->psz_path[0] == '/' );
        url->psz_path++;
    }

    char *type = strstr( url->psz_path, ";type=" );
    if( type )
    {
        *type = '\0';
        if( strchr( "iI", type[6] ) == NULL )
            return VLC_EGENERIC; /* ASCII and directory not supported */
    }
    vlc_uri_decode( url->psz_path );
    return VLC_SUCCESS;
}


/****************************************************************************
 * Open: connect to ftp server and ask for file
 ****************************************************************************/
static int InOpen( vlc_object_t *p_this )
{
    stream_t     *p_access = (stream_t*)p_this;
    access_sys_t *p_sys;
    char         *psz_arg;
    bool          b_directory;

    /* Init p_access */
    p_sys = p_access->p_sys = (access_sys_t*)vlc_obj_calloc( p_this, 1, sizeof( access_sys_t ) );
    if( !p_sys )
        return VLC_ENOMEM;
    p_sys->data = NULL;
    p_sys->out = false;
    p_sys->offset = 0;
    p_sys->size = UINT64_MAX;

    if( readTLSMode( p_this, p_sys, p_access->psz_name ) )
        goto exit_error;

    if( parseURL( &p_sys->url, p_access->psz_url, p_sys->tlsmode ) )
        goto exit_error;

    if( Connect( p_this, p_sys, p_access->psz_url ) )
        goto exit_error;

    do {
        /* get size */
        if( p_sys->url.psz_path == NULL || !*p_sys->url.psz_path )
        {
            b_directory = true;
            break;
        }

        if( ftp_SendCommand( p_this, p_sys, "SIZE %s",
                             p_sys->url.psz_path ) < 0 )
            goto error;

        int val = ftp_RecvCommand( p_this, p_sys, NULL, &psz_arg );
        if( val == 2 )
        {
            b_directory = false;
            p_sys->size = atoll( &psz_arg[4] );
            free( psz_arg );
            msg_Dbg( p_access, "file size: %"PRIu64, p_sys->size );
            break;
        }
        if( val >= 0 )
            free( psz_arg );

        if( ftp_SendCommand( p_this, p_sys, "CWD %s",
                             p_sys->url.psz_path ) < 0 )
            goto error;

        if( ftp_RecvCommand( p_this, p_sys, NULL, NULL ) == 2 )
        {
            b_directory = true;
            break;
        }

        msg_Err( p_this, "file or directory does not exist" );
        goto error;
    } while (0);

    if( b_directory )
    {
        p_access->pf_readdir = DirRead;
        p_access->pf_control = access_vaDirectoryControlHelper;
    } else
        ACCESS_SET_CALLBACKS( Read, NULL, Control, Seek ); \

    /* Start the 'stream' */
    if( ftp_StartStream( p_this, p_sys, 0, b_directory ) < 0 )
    {
        msg_Err( p_this, "cannot retrieve file" );
        goto error;
    }

    return VLC_SUCCESS;

error:
    clearCmd( p_sys );

exit_error:
    vlc_UrlClean( &p_sys->url );
    vlc_tls_ClientDelete( p_sys->p_creds );
    return VLC_EGENERIC;
}

#ifdef ENABLE_SOUT
static int OutOpen( vlc_object_t *p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t *)p_this;
    access_sys_t      *p_sys;

    p_sys = vlc_obj_calloc( p_this, 1, sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* Init p_access */
    p_sys->data = NULL;
    p_sys->out = true;

    if( readTLSMode( p_this, p_sys, p_access->psz_access ) )
        goto exit_error;

    if( parseURL( &p_sys->url, p_access->psz_path, p_sys->tlsmode ) )
        goto exit_error;
    if( p_sys->url.psz_path == NULL )
    {
        msg_Err( p_this, "no filename specified" );
        goto exit_error;
    }

    if( Connect( p_this, p_sys, p_access->psz_path ) )
        goto exit_error;

    /* Start the 'stream' */
    if( ftp_StartStream( p_this, p_sys, 0, false ) < 0 )
    {
        msg_Err( p_access, "cannot store file" );
        clearCmd( p_sys );
        goto exit_error;
    }

    p_access->pf_seek = OutSeek;
    p_access->pf_write = Write;
    p_access->p_sys = (void *)p_sys;

    return VLC_SUCCESS;

exit_error:
    vlc_UrlClean( &p_sys->url );
    vlc_tls_ClientDelete( p_sys->p_creds );
    return VLC_EGENERIC;
}
#endif

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_access, access_sys_t *p_sys )
{
    msg_Dbg( p_access, "stopping stream" );
    ftp_StopStream( p_access, p_sys );

    if( ftp_SendCommand( p_access, p_sys, "QUIT" ) < 0 )
    {
        msg_Warn( p_access, "cannot quit" );
    }
    else
    {
        ftp_RecvCommand( p_access, p_sys, NULL, NULL );
    }

    clearCmd( p_sys );

    /* free memory */
    vlc_UrlClean( &p_sys->url );
    vlc_tls_ClientDelete( p_sys->p_creds );
}

static void InClose( vlc_object_t *p_this )
{
    Close( p_this, ((stream_t *)p_this)->p_sys);
}

#ifdef ENABLE_SOUT
static void OutClose( vlc_object_t *p_this )
{
    Close( p_this, GET_OUT_SYS(p_this));
}
#endif


/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static int SeekCommon( vlc_object_t *p_access, access_sys_t *p_sys,
                       uint64_t i_pos )
{
    msg_Dbg( p_access, "seeking to %"PRIu64, i_pos );

    ftp_StopStream( p_access, p_sys );

    if( ftp_StartStream( p_access, p_sys, i_pos, false ) < 0 )
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static int Seek( stream_t *p_access, uint64_t i_pos )
{
    access_sys_t *p_sys = p_access->p_sys;

    int val = SeekCommon( (vlc_object_t *)p_access, p_sys, i_pos );
    if( val )
        return val;

    p_sys->offset = i_pos;

    return VLC_SUCCESS;
}

#ifdef ENABLE_SOUT
static int OutSeek( sout_access_out_t *p_access, off_t i_pos )
{
    return SeekCommon((vlc_object_t *)p_access, GET_OUT_SYS(p_access), i_pos);
}
#endif

/*****************************************************************************
 * Read:
 *****************************************************************************/
static ssize_t Read( stream_t *p_access, void *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;

    if( p_sys->data == NULL )
        return 0;
    assert( !p_sys->out );

    ssize_t i_read = vlc_tls_Read( p_sys->data, p_buffer, i_len, false );
    if( i_read >= 0 )
        p_sys->offset += i_read;
    else if( errno != EINTR && errno != EAGAIN )
    {
        msg_Err( p_access, "receive error: %s", vlc_strerror_c(errno) );
        i_read = 0;
    }

    return i_read;
}

/*****************************************************************************
 * DirRead:
 *****************************************************************************/
static int DirRead (stream_t *p_access, input_item_node_t *p_current_node)
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_ret = VLC_SUCCESS;

    assert( p_sys->data != NULL );
    assert( !p_sys->out );

    struct vlc_readdir_helper rdh;
    vlc_readdir_helper_init( &rdh, p_access, p_current_node );

    while (i_ret == VLC_SUCCESS)
    {
        char *psz_file;
        int type = ITEM_TYPE_UNKNOWN;

        char *psz_line = vlc_tls_GetLine( p_sys->data );
        if( psz_line == NULL )
            break;

        if( p_sys->features.b_mlst )
        {
            /* MLST Format is key=val;key=val...; FILENAME */
            if( strstr( psz_line, "type=dir" ) )
                type = ITEM_TYPE_DIRECTORY;
            if( strstr( psz_line, "type=file" ) )
                type = ITEM_TYPE_FILE;

            /* Get the filename or fail */
            psz_file = strchr( psz_line, ' ' );
            if( psz_file )
                psz_file++;
            else
            {
                msg_Warn( p_access, "Empty filename in MLST list" );
                free( psz_line );
                continue;
            }
        }
        else
            psz_file = psz_line;

        char *psz_uri;
        char *psz_filename = vlc_uri_encode( psz_file );
        if( psz_filename != NULL &&
            asprintf( &psz_uri, "%s://%s:%d%s%s/%s",
                      ( p_sys->tlsmode == NONE ) ? "ftp" :
                      ( ( p_sys->tlsmode == IMPLICIT ) ? "ftps" : "ftpes" ),
                      p_sys->url.psz_host, p_sys->url.i_port,
                      p_sys->url.psz_path ? "/" : "",
                      p_sys->url.psz_path ? p_sys->url.psz_path : "",
                      psz_filename ) != -1 )
        {
            i_ret = vlc_readdir_helper_additem( &rdh, psz_uri, NULL, psz_file,
                                                type, ITEM_NET );
            free( psz_uri );
        }
        free( psz_filename );
        free( psz_line );
    }

    vlc_readdir_helper_finish( &rdh, i_ret == VLC_SUCCESS );
    return i_ret;
}

/*****************************************************************************
 * Write:
 *****************************************************************************/
#ifdef ENABLE_SOUT
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    access_sys_t *p_sys = GET_OUT_SYS(p_access);
    size_t i_write = 0;

    assert( p_sys->data != NULL );

    while( p_buffer != NULL )
    {
        block_t *p_next = p_buffer->p_next;

        i_write += vlc_tls_Write( p_sys->data,
                                  p_buffer->p_buffer, p_buffer->i_buffer );
        block_Release( p_buffer );

        p_buffer = p_next;
    }

    return i_write;
}
#endif

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( stream_t *p_access, int i_query, va_list args )
{
    access_sys_t *sys = p_access->p_sys;
    bool    *pb_bool;

    switch( i_query )
    {
        case STREAM_CAN_SEEK:
            pb_bool = va_arg( args, bool * );
            *pb_bool = true;
            break;
        case STREAM_CAN_FASTSEEK:
            pb_bool = va_arg( args, bool * );
            *pb_bool = false;
            break;
        case STREAM_CAN_PAUSE:
            pb_bool = va_arg( args, bool * );
            *pb_bool = true;    /* FIXME */
            break;
        case STREAM_CAN_CONTROL_PACE:
            pb_bool = va_arg( args, bool * );
            *pb_bool = true;    /* FIXME */
            break;
        case STREAM_GET_SIZE:
            if( sys->size == UINT64_MAX )
                return VLC_EGENERIC;
            *va_arg( args, uint64_t * ) = sys->size;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS(var_InheritInteger( p_access, "network-caching" ));
            break;

        case STREAM_SET_PAUSE_STATE:
            pb_bool = va_arg( args, bool * );
            if ( !pb_bool )
                 return Seek( p_access, sys->offset );
            break;

        default:
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

static int ftp_StartStream( vlc_object_t *p_access, access_sys_t *p_sys,
                            uint64_t i_start, bool b_directory )
{
    char psz_ipv4[16], *psz_ip = p_sys->sz_epsv_ip;
    int  i_answer;
    char *psz_arg, *psz_parser;
    int  i_port;

    assert( p_sys->data == NULL );

    if( ( ftp_SendCommand( p_access, p_sys, *psz_ip ? "EPSV" : "PASV" ) < 0 )
     || ( ftp_RecvCommand( p_access, p_sys, &i_answer, &psz_arg ) != 2 ) )
    {
        msg_Err( p_access, "cannot set passive mode" );
        return VLC_EGENERIC;
    }

    psz_parser = strchr( psz_arg, '(' );
    if( psz_parser == NULL )
    {
        free( psz_arg );
        msg_Err( p_access, "cannot parse passive mode response" );
        return VLC_EGENERIC;
    }

    if( *psz_ip )
    {
        if( sscanf( psz_parser, "(%*3c%u", &i_port ) < 1 )
        {
            free( psz_arg );
            msg_Err( p_access, "cannot parse passive mode response" );
            return VLC_EGENERIC;
        }
    }
    else
    {
        unsigned a1, a2, a3, a4, p1, p2;

        if( ( sscanf( psz_parser, "(%u,%u,%u,%u,%u,%u", &a1, &a2, &a3, &a4,
                      &p1, &p2 ) < 6 ) || ( a1 > 255 ) || ( a2 > 255 )
         || ( a3 > 255 ) || ( a4 > 255 ) || ( p1 > 255 ) || ( p2 > 255 ) )
        {
            free( psz_arg );
            msg_Err( p_access, "cannot parse passive mode response" );
            return VLC_EGENERIC;
        }

        sprintf( psz_ipv4, "%u.%u.%u.%u", a1, a2, a3, a4 );
        psz_ip = psz_ipv4;
        i_port = (p1 << 8) | p2;
    }
    free( psz_arg );

    msg_Dbg( p_access, "ip:%s port:%d", psz_ip, i_port );

    if( ftp_SendCommand( p_access, p_sys, "TYPE I" ) < 0 ||
        ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) != 2 )
    {
        msg_Err( p_access, "cannot set binary transfer mode" );
        return VLC_EGENERIC;
    }

    if( i_start > 0 )
    {
        if( ftp_SendCommand( p_access, p_sys, "REST %"PRIu64, i_start ) < 0 ||
            ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) > 3 )
        {
            msg_Err( p_access, "cannot set restart offset" );
            return VLC_EGENERIC;
        }
    }

    msg_Dbg( p_access, "waiting for data connection..." );
    p_sys->data = vlc_tls_SocketOpenTCP( p_access, psz_ip, i_port );
    if( p_sys->data == NULL )
    {
        msg_Err( p_access, "failed to connect with server" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_access, "connection with \"%s:%d\" successful",
             psz_ip, i_port );

    if( b_directory )
    {
        if( p_sys->features.b_mlst &&
            ftp_SendCommand( p_access, p_sys, "MLSD" ) >= 0 &&
            ftp_RecvCommandInit( p_access, p_sys ) == 1 )
        {
            msg_Dbg( p_access, "Using MLST extension to list" );
        }
        else
        if( ftp_SendCommand( p_access, p_sys, "NLST" ) < 0 ||
            ftp_RecvCommandInit( p_access, p_sys ) == 1 )
        {
            msg_Err( p_access, "cannot list directory contents" );
            return VLC_EGENERIC;
        }
    }
    else
    {
        /* "1xx" message */
        assert( p_sys->url.psz_path );
        if( ftp_SendCommand( p_access, p_sys, "%s %s",
                             p_sys->out ? "STOR" : "RETR",
                             p_sys->url.psz_path ) < 0
         || ftp_RecvCommandInit( p_access, p_sys ) != 1 )
        {
            msg_Err( p_access, "cannot retrieve file" );
            return VLC_EGENERIC;
        }
    }

    if( p_sys->tlsmode != NONE )
    {
        /* FIXME: Do Reuse TLS Session */
        /* TLS/SSL handshake */
        vlc_tls_t *secure = vlc_tls_ClientSessionCreate( p_sys->p_creds,
                            p_sys->data, p_sys->url.psz_host,
                            ( p_sys->tlsmode == EXPLICIT ) ? "ftpes-data"
                                                           : "ftps-data",
                                                         NULL, NULL );
        if( secure == NULL )
        {
            msg_Err( p_access, "cannot establish FTP/TLS session for data" \
                             ": server not allowing new session ?" );
            return VLC_EGENERIC;
        }
        p_sys->data = secure;
    }

    return VLC_SUCCESS;
}

static int ftp_StopStream ( vlc_object_t *p_access, access_sys_t *p_sys )
{
    int ret = VLC_SUCCESS;

    if( ftp_SendCommand( p_access, p_sys, "ABOR" ) < 0 )
    {
        msg_Warn( p_access, "cannot abort file" );
        ret = VLC_EGENERIC;
    }

    if( p_sys->data != NULL )
    {
        vlc_tls_Close( p_sys->data );
        p_sys->data = NULL;

        if( ret == VLC_SUCCESS )
            /* Read the final response from RETR/STOR, i.e. 426 or 226 */
            ftp_RecvCommand( p_access, p_sys, NULL, NULL );
    }

    if( ret == VLC_SUCCESS )
        /* Read the response from ABOR, i.e. 226 or 225 */
        ftp_RecvCommand( p_access, p_sys, NULL, NULL );

    return ret;
}
