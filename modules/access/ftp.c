/*****************************************************************************
 * ftp.c: FTP input module
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 * Copyright © 2006 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr> - original code
 *          Rémi Denis-Courmont <rem # videolan.org> - EPSV support
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

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <assert.h>

#include <vlc_access.h>
#include <vlc_dialog.h>

#include <vlc_network.h>
#include <vlc_url.h>
#include <vlc_tls.h>
#include <vlc_sout.h>
#include <vlc_charset.h>

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

#define USER_TEXT N_("FTP user name")
#define USER_LONGTEXT N_("User name that will " \
    "be used for the connection.")
#define PASS_TEXT N_("FTP password")
#define PASS_LONGTEXT N_("Password that will be " \
    "used for the connection.")
#define ACCOUNT_TEXT N_("FTP account")
#define ACCOUNT_LONGTEXT N_("Account that will be " \
    "used for the connection.")

vlc_module_begin ()
    set_shortname( "FTP" )
    set_description( N_("FTP input") )
    set_capability( "access", 0 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_string( "ftp-user", "anonymous", USER_TEXT, USER_LONGTEXT,
                false )
    add_string( "ftp-pwd", "anonymous@example.com", PASS_TEXT,
                PASS_LONGTEXT, false )
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
static ssize_t Read( access_t *, uint8_t *, size_t );
static int Seek( access_t *, uint64_t );
static int Control( access_t *, int, va_list );
#ifdef ENABLE_SOUT
static int OutSeek( sout_access_out_t *, off_t );
static ssize_t Write( sout_access_out_t *, block_t * );
#endif

static void FeaturesCheck( void *, const char * );

typedef struct ftp_features_t
{
    bool b_unicode;
    bool b_authtls;
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
    vlc_tls_creds_t *p_creds;
    enum tls_mode_e  tlsmode;
    struct
    {
        vlc_tls_t   *p_tls;
        v_socket_t  *p_vs;
        int          fd;
    } cmd, data;

    char       sz_epsv_ip[NI_MAXNUMERICHOST];
    bool       out;
    bool       directory;
    uint64_t   size;
};
#define GET_OUT_SYS( p_this ) \
    ((access_sys_t *)(((sout_access_out_t *)(p_this))->p_sys))

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

    msg_Dbg( obj, "sending request: \"%.*s\" (%d bytes)", val - 2, cmd, val );
    if( net_Write( obj, sys->cmd.fd, sys->cmd.p_vs, cmd, val ) != val )
    {
        msg_Err( obj, "request failure" );
        val = -1;
    }
    else
        val = 0;
    free( cmd );
    return val;
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
static int ftp_RecvAnswer( vlc_object_t *obj, access_sys_t *sys,
                           int *restrict codep, char **restrict strp,
                           void (*cb)(void *, const char *), void *opaque )
{
    if( codep != NULL )
        *codep = 500;
    if( strp != NULL )
        *strp = NULL;

    char *resp = net_Gets( obj, sys->cmd.fd, sys->cmd.p_vs );
    if( resp == NULL )
    {
        msg_Err( obj, "response failure" );
        goto error;
    }

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
            char *line = net_Gets( obj, sys->cmd.fd, sys->cmd.p_vs );
            if( line == NULL )
            {
                msg_Err( obj, "response failure" );
                goto error;
            }

            done = !strncmp( resp, line, 4 );
            if( !done )
                cb( opaque, line );
            free( line );
        }
        while( !done );
    }

    if( codep != NULL )
        *codep = code;
    if( strp != NULL )
        *strp = resp;
    else
        free( resp );
    return code / 100;
error:
    free( resp );
    return -1;
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

static int ftp_StartStream( vlc_object_t *, access_sys_t *, uint64_t );
static int ftp_StopStream ( vlc_object_t *, access_sys_t * );

static void readTLSMode( access_sys_t *p_sys, const char * psz_access )
{
    if ( !strncmp( psz_access, "ftps", 4 ) )
        p_sys->tlsmode = IMPLICIT;
    else
    if ( !strncmp( psz_access, "ftpes", 5 ) )
        p_sys->tlsmode = EXPLICIT;
    else
        p_sys->tlsmode = NONE;
}

static int createCmdTLS( vlc_object_t *p_access, access_sys_t *p_sys, int fd,
                         const char *psz_session_name )
{
    p_sys->p_creds = vlc_tls_ClientCreate( p_access );
    if( p_sys->p_creds == NULL ) return -1;

    /* TLS/SSL handshake */
    p_sys->cmd.p_tls = vlc_tls_ClientSessionCreate( p_sys->p_creds, fd,
                                                    p_sys->url.psz_host,
                                                    psz_session_name );
    if( p_sys->cmd.p_tls == NULL )
    {
        msg_Err( p_access, "cannot establish FTP/TLS session on command channel" );
        return -1;
    }
    p_sys->cmd.p_vs = &p_sys->cmd.p_tls->sock;

    return 0;
}

static void clearCmdTLS( access_sys_t *p_sys )
{
    if ( p_sys->cmd.p_tls ) vlc_tls_SessionDelete( p_sys->cmd.p_tls );
    if ( p_sys->p_creds ) vlc_tls_Delete( p_sys->p_creds );
    p_sys->cmd.p_tls = NULL;
    p_sys->cmd.p_vs = NULL;
    p_sys->p_creds = NULL;
}

static int Login( vlc_object_t *p_access, access_sys_t *p_sys )
{
    int i_answer;
    char *psz;

    /* *** Open a TCP connection with server *** */
    int fd = p_sys->cmd.fd = net_ConnectTCP( p_access, p_sys->url.psz_host,
                                             p_sys->url.i_port );
    if( fd == -1 )
    {
        msg_Err( p_access, "connection failed" );
        dialog_Fatal( p_access, _("Network interaction failed"), "%s",
                        _("VLC could not connect with the given server.") );
        return -1;
    }

    if ( p_sys->tlsmode == IMPLICIT ) /* FTPS Mode */
    {
        if ( createCmdTLS( p_access, p_sys, fd, "ftps") < 0 )
            goto error;
    }

    while( ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) == 1 );

    if( i_answer / 100 != 2 )
    {
        msg_Err( p_access, "connection rejected" );
        dialog_Fatal( p_access, _("Network interaction failed"), "%s",
                        _("VLC's connection to the given server was rejected.") );
        return -1;
    }

    msg_Dbg( p_access, "connection accepted (%d)", i_answer );

    if( p_sys->url.psz_username && *p_sys->url.psz_username )
        psz = strdup( p_sys->url.psz_username );
    else
        psz = var_InheritString( p_access, "ftp-user" );
    if( !psz )
        return -1;

    /* Features check first */
    if( ftp_SendCommand( p_access, p_sys, "FEAT" ) < 0
     || ftp_RecvAnswer( p_access, p_sys, NULL, NULL,
                        FeaturesCheck, &p_sys->features ) < 0 )
    {
         msg_Err( p_access, "cannot get server features" );
         return -1;
    }

    /* Create TLS Session */
    if( p_sys->tlsmode == EXPLICIT )
    {
        if ( ! p_sys->features.b_authtls )
        {
            msg_Err( p_access, "Server does not support TLS" );
            return -1;
        }

        if( ftp_SendCommand( p_access, p_sys, "AUTH TLS" ) < 0
         || ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0
         || i_answer != 234 )
        {
             msg_Err( p_access, "cannot switch to TLS: server replied with code %d",
                      i_answer );
             return -1;
        }

        if ( createCmdTLS( p_access, p_sys, fd, "ftpes") < 0 )
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
            free( psz );
            goto error;
        }

        if( ftp_SendCommand( p_access, p_sys, "PROT P" ) < 0 ||
            ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0 ||
            i_answer != 200 )
        {
            msg_Err( p_access, "Can't set Data channel protection" );
            free( psz );
            goto error;
        }
    }

    /* Send credentials over channel */
    if( ftp_SendCommand( p_access, p_sys, "USER %s", psz ) < 0 ||
        ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0 )
    {
        free( psz );
        goto error;
    }
    free( psz );

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
            if( p_sys->url.psz_password && *p_sys->url.psz_password )
                psz = strdup( p_sys->url.psz_password );
            else
                psz = var_InheritString( p_access, "ftp-pwd" );
            if( !psz )
                goto error;

            if( ftp_SendCommand( p_access, p_sys, "PASS %s", psz ) < 0 ||
                ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0 )
            {
                free( psz );
                goto error;
            }
            free( psz );

            switch( i_answer / 100 )
            {
                case 2:
                    msg_Dbg( p_access, "password accepted" );
                    break;
                case 3:
                    msg_Dbg( p_access, "account needed" );
                    psz = var_InheritString( p_access, "ftp-account" );
                    if( ftp_SendCommand( p_access, p_sys, "ACCT %s",
                                         psz ) < 0 ||
                        ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) < 0 )
                    {
                        free( psz );
                        goto error;
                    }
                    free( psz );

                    if( i_answer / 100 != 2 )
                    {
                        msg_Err( p_access, "account rejected" );
                        dialog_Fatal( p_access,
                                      _("Network interaction failed"),
                                      "%s", _("Your account was rejected.") );
                        goto error;
                    }
                    msg_Dbg( p_access, "account accepted" );
                    break;

                default:
                    msg_Err( p_access, "password rejected" );
                    dialog_Fatal( p_access, _("Network interaction failed"),
                                  "%s",  _("Your password was rejected.") );
                    goto error;
            }
            break;
        default:
            msg_Err( p_access, "user rejected" );
            dialog_Fatal( p_access, _("Network interaction failed"), "%s",
                        _("Your connection attempt to the server was rejected.") );
            goto error;
    }

    return 0;

error:
    clearCmdTLS( p_sys );
    return -1;
}

static void FeaturesCheck( void *opaque, const char *feature )
{
    ftp_features_t *features = opaque;

    if( strcasestr( feature, "UTF8" ) != NULL )
        features->b_unicode = true;
    else
    if( strcasestr( feature, "AUTH TLS" ) != NULL )
        features->b_authtls = true;
}

static const char *IsASCII( const char *str )
{
    int8_t c;
    for( const char *p = str; (c = *p) != '\0'; p++ )
        if( c < 0 )
            return NULL;
    return str;
}

static int Connect( vlc_object_t *p_access, access_sys_t *p_sys )
{
    if( Login( p_access, p_sys ) < 0 )
        return -1;

    /* Extended passive mode */
    if( ftp_SendCommand( p_access, p_sys, "EPSV ALL" ) < 0 )
    {
        msg_Err( p_access, "cannot request extended passive mode" );
        goto error;
    }

    if( ftp_RecvCommand( p_access, p_sys, NULL, NULL ) == 2 )
    {
        if( net_GetPeerAddress( p_sys->cmd.fd, p_sys->sz_epsv_ip, NULL ) )
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
        clearCmdTLS( p_sys );
        net_Close( p_sys->cmd.fd );

        if( Login( p_access, p_sys ) )
            goto error;
    }

    if( (p_sys->features.b_unicode ? IsUTF8 : IsASCII)(p_sys->url.psz_path) == NULL )
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
    clearCmdTLS( p_sys );
    net_Close( p_sys->cmd.fd );
    return -1;
}


static int parseURL( vlc_url_t *url, const char *path, enum tls_mode_e mode )
{
    if( path == NULL )
        return VLC_EGENERIC;

    /* *** Parse URL and get server addr/port and path *** */
    while( *path == '/' )
        path++;

    vlc_UrlParse( url, path, 0 );

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
    decode_URI( url->psz_path );
    return VLC_SUCCESS;
}


/****************************************************************************
 * Open: connect to ftp server and ask for file
 ****************************************************************************/
static int InOpen( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    char         *psz_arg;

    /* Init p_access */
    STANDARD_READ_ACCESS_INIT
    p_sys->data.fd = -1;
    p_sys->out = false;
    p_sys->directory = false;
    p_sys->size = 0;
    readTLSMode( p_sys, p_access->psz_access );

    if( parseURL( &p_sys->url, p_access->psz_location, p_sys->tlsmode ) )
        goto exit_error;

    if( Connect( p_this, p_sys ) )
        goto exit_error;

    /* get size */
    if( p_sys->url.psz_path == NULL )
        p_sys->directory = true;
    else
    if( ftp_SendCommand( p_this, p_sys, "SIZE %s", p_sys->url.psz_path ) < 0 )
        goto error;
    else
    if ( ftp_RecvCommand( p_this, p_sys, NULL, &psz_arg ) == 2 )
    {
        p_sys->size = atoll( &psz_arg[4] );
        free( psz_arg );
        msg_Dbg( p_access, "file size: %"PRIu64, p_sys->size );
    }
    else
    if( ftp_SendCommand( p_this, p_sys, "CWD %s", p_sys->url.psz_path ) < 0 )
        goto error;
    else
    if( ftp_RecvCommand( p_this, p_sys, NULL, NULL ) != 2 )
    {
        msg_Err( p_this, "file or directory does not exist" );
        goto error;
    }
    else
        p_sys->directory = true;

    /* Start the 'stream' */
    if( ftp_StartStream( p_this, p_sys, 0 ) < 0 )
    {
        msg_Err( p_this, "cannot retrieve file" );
        clearCmdTLS( p_sys );
        net_Close( p_sys->cmd.fd );
        goto exit_error;
    }

    return VLC_SUCCESS;

error:
    clearCmdTLS( p_sys );
    net_Close( p_sys->cmd.fd );

exit_error:
    vlc_UrlClean( &p_sys->url );
    free( p_sys );
    return VLC_EGENERIC;
}

#ifdef ENABLE_SOUT
static int OutOpen( vlc_object_t *p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t *)p_this;
    access_sys_t      *p_sys;

    p_sys = calloc( 1, sizeof( *p_sys ) );
    if( !p_sys )
        return VLC_ENOMEM;

    /* Init p_access */
    p_sys->data.fd = -1;
    p_sys->out = true;
    readTLSMode( p_sys, p_access->psz_access );

    if( parseURL( &p_sys->url, p_access->psz_path, p_sys->tlsmode ) )
        goto exit_error;
    if( p_sys->url.psz_path == NULL )
    {
        msg_Err( p_this, "no filename specified" );
        goto exit_error;
    }

    if( Connect( p_this, p_sys ) )
        goto exit_error;

    /* Start the 'stream' */
    if( ftp_StartStream( p_this, p_sys, 0 ) < 0 )
    {
        msg_Err( p_access, "cannot store file" );
        clearCmdTLS( p_sys );
        net_Close( p_sys->cmd.fd );
        goto exit_error;
    }

    p_access->pf_seek = OutSeek;
    p_access->pf_write = Write;
    p_access->p_sys = (void *)p_sys;

    return VLC_SUCCESS;

exit_error:
    vlc_UrlClean( &p_sys->url );
    free( p_sys );
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

    clearCmdTLS( p_sys );
    net_Close( p_sys->cmd.fd );

    /* free memory */
    vlc_UrlClean( &p_sys->url );
    free( p_sys );
}

static void InClose( vlc_object_t *p_this )
{
    Close( p_this, ((access_t *)p_this)->p_sys);
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
static int _Seek( vlc_object_t *p_access, access_sys_t *p_sys, uint64_t i_pos )
{
    msg_Dbg( p_access, "seeking to %"PRIu64, i_pos );

    ftp_StopStream( (vlc_object_t *)p_access, p_sys );
    if( ftp_StartStream( (vlc_object_t *)p_access, p_sys, i_pos ) < 0 )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int Seek( access_t *p_access, uint64_t i_pos )
{
    int val = _Seek( (vlc_object_t *)p_access, p_access->p_sys, i_pos );
    if( val )
        return val;

    p_access->info.b_eof = false;
    p_access->info.i_pos = i_pos;

    return VLC_SUCCESS;
}

#ifdef ENABLE_SOUT
static int OutSeek( sout_access_out_t *p_access, off_t i_pos )
{
    return _Seek( (vlc_object_t *)p_access, GET_OUT_SYS( p_access ), i_pos);
}
#endif

/*****************************************************************************
 * Read:
 *****************************************************************************/
static ssize_t Read( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;

    assert( p_sys->data.fd != -1 );
    assert( !p_sys->out );

    if( p_access->info.b_eof )
        return 0;

    if( p_sys->directory )
    {
        char *psz_line = net_Gets( p_access, p_sys->data.fd, p_sys->data.p_vs );
        if( !psz_line )
        {
            p_access->info.b_eof = true;
            return 0;
        }
        else
        {
            snprintf( (char*)p_buffer, i_len, "%s://%s:%d/%s/%s\n",
                      ( p_sys->tlsmode == NONE ) ? "ftp" :
                      ( ( p_sys->tlsmode == IMPLICIT ) ? "ftps" : "ftpes" ),
                      p_sys->url.psz_host, p_sys->url.i_port,
                      p_sys->url.psz_path, psz_line );
            free( psz_line );
            return strlen( (const char *)p_buffer );
        }
    }
    else
    {
        int i_read = net_Read( p_access, p_sys->data.fd, p_sys->data.p_vs,
                               p_buffer, i_len, false );
        if( i_read == 0 )
            p_access->info.b_eof = true;
        else if( i_read > 0 )
            p_access->info.i_pos += i_read;

        return i_read;
    }
}

/*****************************************************************************
 * Write:
 *****************************************************************************/
#ifdef ENABLE_SOUT
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    access_sys_t *p_sys = GET_OUT_SYS(p_access);
    size_t i_write = 0;

    assert( p_sys->data.fd != -1 );

    while( p_buffer != NULL )
    {
        block_t *p_next = p_buffer->p_next;;

        i_write += net_Write( p_access, p_sys->data.fd, p_sys->data.p_vs,
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
static int Control( access_t *p_access, int i_query, va_list args )
{
    bool    *pb_bool;
    int64_t *pi_64;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = !p_access->p_sys->directory;
            break;
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = false;
            break;
        case ACCESS_CAN_PAUSE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = true;    /* FIXME */
            break;
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = true;    /* FIXME */
            break;
        case ACCESS_GET_SIZE:
            *va_arg( args, uint64_t * ) = p_access->p_sys->size;
            break;

        /* */
        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = INT64_C(1000)
                   * var_InheritInteger( p_access, "network-caching" );
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            pb_bool = (bool*)va_arg( args, bool* );
            if ( !pb_bool )
              return Seek( p_access, p_access->info.i_pos );
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
        case ACCESS_GET_CONTENT_TYPE:
        case ACCESS_GET_META:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control: %d", i_query);
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

static int ftp_StartStream( vlc_object_t *p_access, access_sys_t *p_sys,
                            uint64_t i_start )
{
    char psz_ipv4[16], *psz_ip = p_sys->sz_epsv_ip;
    int  i_answer;
    char *psz_arg, *psz_parser;
    int  i_port;

    assert( p_sys->data.fd == -1 );

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
        char psz_fmt[7] = "(|||%u";
        psz_fmt[1] = psz_fmt[2] = psz_fmt[3] = psz_parser[1];

        if( sscanf( psz_parser, psz_fmt, &i_port ) < 1 )
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
    p_sys->data.fd = net_ConnectTCP( p_access, psz_ip, i_port );
    if( p_sys->data.fd < 0 )
    {
        msg_Err( p_access, "failed to connect with server" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_access, "connection with \"%s:%d\" successful",
             psz_ip, i_port );

    if( p_sys->directory )
    {
        if( ftp_SendCommand( p_access, p_sys, "NLST" ) < 0 ||
            ftp_RecvCommand( p_access, p_sys, NULL, &psz_arg ) > 2 )
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
         || ftp_RecvCommand( p_access, p_sys, &i_answer, NULL ) > 2 )
        {
            msg_Err( p_access, "cannot retrieve file" );
            return VLC_EGENERIC;
        }
    }

    if( p_sys->tlsmode != NONE )
    {
        /* FIXME: Do Reuse TLS Session */
        /* TLS/SSL handshake */
        p_sys->data.p_tls = vlc_tls_ClientSessionCreate( p_sys->p_creds,
                            p_sys->data.fd, p_sys->url.psz_host,
                            ( p_sys->tlsmode == EXPLICIT ) ? "ftpes-data"
                                                           : "ftps-data" );
        if( p_sys->data.p_tls == NULL )
        {
            msg_Err( p_access, "cannot establish FTP/TLS session for data" \
                             ": server not allowing new session ?" );
            return VLC_EGENERIC;
        }
        p_sys->data.p_vs = &p_sys->data.p_tls->sock;
    }
    else
        shutdown( p_sys->data.fd, p_sys->out ? SHUT_RD : SHUT_WR );

    return VLC_SUCCESS;
}

static int ftp_StopStream ( vlc_object_t *p_access, access_sys_t *p_sys )
{
    if( ftp_SendCommand( p_access, p_sys, "ABOR" ) < 0 )
    {
        msg_Warn( p_access, "cannot abort file" );
        if( p_sys->data.fd > 0 )
        {
            if ( p_sys->data.p_tls ) vlc_tls_SessionDelete( p_sys->data.p_tls );
            net_Close( p_sys->data.fd );
        }
        p_sys->data.fd = -1;
        p_sys->data.p_tls = NULL;
        p_sys->data.p_vs = NULL;
        return VLC_EGENERIC;
    }

    if( p_sys->data.fd != -1 )
    {
        if ( p_sys->data.p_tls ) vlc_tls_SessionDelete( p_sys->data.p_tls );
        net_Close( p_sys->data.fd );
        p_sys->data.fd = -1;
        p_sys->data.p_tls = NULL;
        p_sys->data.p_vs = NULL;
        /* Read the final response from RETR/STOR, i.e. 426 or 226 */
        ftp_RecvCommand( p_access, p_sys, NULL, NULL );
    }
    /* Read the response from ABOR, i.e. 226 or 225 */
    ftp_RecvCommand( p_access, p_sys, NULL, NULL );

    return VLC_SUCCESS;
}
