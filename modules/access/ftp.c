/*****************************************************************************
 * ftp.c: FTP input module
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
 * Copyright © 2006 Rémi Denis-Courmont
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr> - original code
 *          Rémi Denis-Courmont <rem # videolan.org> - EPSV support
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

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <assert.h>

#include <vlc_access.h>
#include <vlc_interface.h>

#include <vlc_network.h>
#include "vlc_url.h"
#include <vlc_sout.h>

#ifndef IPPORT_FTP
# define IPPORT_FTP 21u
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int   InOpen ( vlc_object_t * );
static void  InClose( vlc_object_t * );
static int  OutOpen ( vlc_object_t * );
static void OutClose( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Caching value for FTP streams. This " \
    "value should be set in milliseconds." )
#define USER_TEXT N_("FTP user name")
#define USER_LONGTEXT N_("User name that will " \
    "be used for the connection.")
#define PASS_TEXT N_("FTP password")
#define PASS_LONGTEXT N_("Password that will be " \
    "used for the connection.")
#define ACCOUNT_TEXT N_("FTP account")
#define ACCOUNT_LONGTEXT N_("Account that will be " \
    "used for the connection.")

vlc_module_begin();
    set_shortname( "FTP" );
    set_description( N_("FTP input") );
    set_capability( "access", 0 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    add_integer( "ftp-caching", 2 * DEFAULT_PTS_DELAY / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, true );
    add_string( "ftp-user", "anonymous", NULL, USER_TEXT, USER_LONGTEXT,
                false );
    add_string( "ftp-pwd", "anonymous@example.com", NULL, PASS_TEXT,
                PASS_LONGTEXT, false );
    add_string( "ftp-account", "anonymous", NULL, ACCOUNT_TEXT,
                ACCOUNT_LONGTEXT, false );
    add_shortcut( "ftp" );
    set_callbacks( InOpen, InClose );

    add_submodule();
    set_shortname( "FTP" );
    set_description( N_("FTP upload output") );
    set_capability( "sout access", 0 );
    set_category( CAT_SOUT );
    set_subcategory( SUBCAT_SOUT_ACO );
    set_callbacks( OutOpen, OutClose );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t Read( access_t *, uint8_t *, size_t );
static ssize_t Write( sout_access_out_t *, block_t * );
static int Seek( access_t *, int64_t );
static int OutSeek( sout_access_out_t *, off_t );
static int Control( access_t *, int, va_list );

struct access_sys_t
{
    vlc_url_t  url;

    int        fd_cmd;
    int        fd_data;

    char       sz_epsv_ip[NI_MAXNUMERICHOST];
    bool       out;
};
#define GET_OUT_SYS( p_this ) \
    ((access_sys_t *)(((sout_access_out_t *)(p_this))->p_sys))

static int ftp_SendCommand( vlc_object_t *, access_sys_t *, const char *, ... );
static int ftp_ReadCommand( vlc_object_t *, access_sys_t *, int *, char ** );
static int ftp_StartStream( vlc_object_t *, access_sys_t *, int64_t );
static int ftp_StopStream ( vlc_object_t *, access_sys_t * );

static int Login( vlc_object_t *p_access, access_sys_t *p_sys )
{
    int i_answer;
    char *psz;

    /* *** Open a TCP connection with server *** */
    int fd = p_sys->fd_cmd = net_ConnectTCP( p_access, p_sys->url.psz_host,
                                             p_sys->url.i_port );
    if( fd == -1 )
    {
        msg_Err( p_access, "connection failed" );
        intf_UserFatal( p_access, false, _("Network interaction failed"),
                        _("VLC could not connect with the given server.") );
        return -1;
    }

    while( ftp_ReadCommand( p_access, p_sys, &i_answer, NULL ) == 1 );

    if( i_answer / 100 != 2 )
    {
        msg_Err( p_access, "connection rejected" );
        intf_UserFatal( p_access, false, _("Network interaction failed"),
                        _("VLC's connection to the given server was rejected.") );
        return -1;
    }

    msg_Dbg( p_access, "connection accepted (%d)", i_answer );

    if( p_sys->url.psz_username && *p_sys->url.psz_username )
        psz = strdup( p_sys->url.psz_username );
    else
        psz = var_CreateGetString( p_access, "ftp-user" );
    if( !psz )
        return -1;

    if( ftp_SendCommand( p_access, p_sys, "USER %s", psz ) < 0 ||
        ftp_ReadCommand( p_access, p_sys, &i_answer, NULL ) < 0 )
    {
        free( psz );
        return -1;
    }
    free( psz );

    switch( i_answer / 100 )
    {
        case 2:
            msg_Dbg( p_access, "user accepted" );
            break;
        case 3:
            msg_Dbg( p_access, "password needed" );
            if( p_sys->url.psz_password && *p_sys->url.psz_password )
                psz = strdup( p_sys->url.psz_password );
            else
                psz = var_CreateGetString( p_access, "ftp-pwd" );
            if( !psz )
                return -1;

            if( ftp_SendCommand( p_access, p_sys, "PASS %s", psz ) < 0 ||
                ftp_ReadCommand( p_access, p_sys, &i_answer, NULL ) < 0 )
            {
                free( psz );
                return -1;
            }
            free( psz );

            switch( i_answer / 100 )
            {
                case 2:
                    msg_Dbg( p_access, "password accepted" );
                    break;
                case 3:
                    msg_Dbg( p_access, "account needed" );
                    psz = var_CreateGetString( p_access, "ftp-account" );
                    if( ftp_SendCommand( p_access, p_sys, "ACCT %s",
                                         psz ) < 0 ||
                        ftp_ReadCommand( p_access, p_sys, &i_answer, NULL ) < 0 )
                    {
                        free( psz );
                        return -1;
                    }
                    free( psz );

                    if( i_answer / 100 != 2 )
                    {
                        msg_Err( p_access, "account rejected" );
                        intf_UserFatal( p_access, false,
                                        _("Network interaction failed"),
                                        _("Your account was rejected.") );
                        return -1;
                    }
                    msg_Dbg( p_access, "account accepted" );
                    break;

                default:
                    msg_Err( p_access, "password rejected" );
                    intf_UserFatal( p_access, false,
                                    _("Network interaction failed"),
                                    _("Your password was rejected.") );
                    return -1;
            }
            break;
        default:
            msg_Err( p_access, "user rejected" );
            intf_UserFatal( p_access, false,
                        _("Network interaction failed"),
                        _("Your connection attempt to the server was rejected.") );
            return -1;
    }

    return 0;
}

static int Connect( vlc_object_t *p_access, access_sys_t *p_sys )
{
    if( Login( p_access, p_sys ) < 0 )
        return -1;

    /* Extended passive mode */
    if( ftp_SendCommand( p_access, p_sys, "EPSV ALL" ) < 0 )
    {
        msg_Err( p_access, "cannot request extended passive mode" );
        net_Close( p_sys->fd_cmd );
        return -1;
    }

    if( ftp_ReadCommand( p_access, p_sys, NULL, NULL ) == 2 )
    {
        if( net_GetPeerAddress( p_sys->fd_cmd, p_sys->sz_epsv_ip, NULL ) )
        {
            net_Close( p_sys->fd_cmd );
            return -1;
        }
    }
    else
    {
        /* If ESPV ALL fails, we fallback to PASV.
         * We have to restart the connection in case there is a NAT that
         * understands EPSV ALL in the way, and hence won't allow PASV on
         * the initial connection.
         */
        msg_Info( p_access, "FTP Extended passive mode disabled" );
        net_Close( p_sys->fd_cmd );

        if( Login( p_access, p_sys ) )
        {
            net_Close( p_sys->fd_cmd );
            return -1;
        }
    }

    /* check binary mode support */
    if( ftp_SendCommand( p_access, p_sys, "TYPE I" ) < 0 ||
        ftp_ReadCommand( p_access, p_sys, NULL, NULL ) != 2 )
    {
        msg_Err( p_access, "cannot set binary transfer mode" );
        net_Close( p_sys->fd_cmd );
        return -1;
    }

    return 0;
}


static int parseURL( vlc_url_t *url, const char *path )
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
        url->i_port = IPPORT_FTP; /* default port */

    /* FTP URLs are relative to user's default directory (RFC1738)
    For absolute path use ftp://foo.bar//usr/local/etc/filename */

    if( url->psz_path && *url->psz_path == '/' )
        url->psz_path++;

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
    p_sys->fd_data = -1;
    p_sys->out = false;

    if( parseURL( &p_sys->url, p_access->psz_path ) )
        goto exit_error;

    if( Connect( p_this, p_sys ) )
        goto exit_error;

    /* get size */
    if( ftp_SendCommand( p_this, p_sys, "SIZE %s", p_sys->url.psz_path ? : "" ) < 0 ||
        ftp_ReadCommand( p_this, p_sys, NULL, &psz_arg ) != 2 )
    {
        msg_Err( p_access, "cannot get file size" );
        net_Close( p_sys->fd_cmd );
        goto exit_error;
    }
    p_access->info.i_size = atoll( &psz_arg[4] );
    free( psz_arg );
    msg_Dbg( p_access, "file size: %"PRId64, p_access->info.i_size );

    /* Start the 'stream' */
    if( ftp_StartStream( p_this, p_sys, 0 ) < 0 )
    {
        msg_Err( p_access, "cannot retrieve file" );
        net_Close( p_sys->fd_cmd );
        goto exit_error;
    }

    /* Update default_pts to a suitable value for ftp access */
    var_Create( p_access, "ftp-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    return VLC_SUCCESS;

exit_error:
    vlc_UrlClean( &p_sys->url );
    free( p_sys );
    return VLC_EGENERIC;
}

static int OutOpen( vlc_object_t *p_this )
{
    sout_access_out_t *p_access = (sout_access_out_t *)p_this;
    access_sys_t      *p_sys;

    p_sys = malloc( sizeof( *p_sys ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;
    memset( p_sys, 0, sizeof( *p_sys ) );

    /* Init p_access */
    p_sys->fd_data = -1;
    p_sys->out = true;

    if( parseURL( &p_sys->url, p_access->psz_path ) )
        goto exit_error;

    if( Connect( p_this, p_sys ) )
        goto exit_error;

    /* Start the 'stream' */
    if( ftp_StartStream( p_this, p_sys, 0 ) < 0 )
    {
        msg_Err( p_access, "cannot store file" );
        net_Close( p_sys->fd_cmd );
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
        ftp_ReadCommand( p_access, p_sys, NULL, NULL );
    }
    net_Close( p_sys->fd_cmd );

    /* free memory */
    vlc_UrlClean( &p_sys->url );
    free( p_sys );
}

static void InClose( vlc_object_t *p_this )
{
    Close( p_this, ((access_t *)p_this)->p_sys);
}

static void OutClose( vlc_object_t *p_this )
{
    Close( p_this, GET_OUT_SYS(p_this));
}


/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static int _Seek( vlc_object_t *p_access, access_sys_t *p_sys, int64_t i_pos )
{
    if( i_pos < 0 )
        return VLC_EGENERIC;

    msg_Dbg( p_access, "seeking to %"PRId64, i_pos );

    ftp_StopStream( (vlc_object_t *)p_access, p_sys );
    if( ftp_StartStream( (vlc_object_t *)p_access, p_sys, i_pos ) < 0 )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int Seek( access_t *p_access, int64_t i_pos )
{
    int val = _Seek( (vlc_object_t *)p_access, p_access->p_sys, i_pos );
    if( val )
        return val;

    p_access->info.b_eof = false;
    p_access->info.i_pos = i_pos;

    return VLC_SUCCESS;
}

static int OutSeek( sout_access_out_t *p_access, off_t i_pos )
{
    return _Seek( (vlc_object_t *)p_access, GET_OUT_SYS( p_access ), i_pos);
}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static ssize_t Read( access_t *p_access, uint8_t *p_buffer, size_t i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    assert( p_sys->fd_data != -1 );
    assert( !p_sys->out );

    if( p_access->info.b_eof )
        return 0;

    i_read = net_Read( p_access, p_sys->fd_data, NULL, p_buffer, i_len,
                       false );
    if( i_read == 0 )
        p_access->info.b_eof = true;
    else if( i_read > 0 )
        p_access->info.i_pos += i_read;

    return i_read;
}

/*****************************************************************************
 * Write:
 *****************************************************************************/
static ssize_t Write( sout_access_out_t *p_access, block_t *p_buffer )
{
    access_sys_t *p_sys = GET_OUT_SYS(p_access);
    size_t i_write = 0;

    assert( p_sys->fd_data != -1 );

    while( p_buffer != NULL )
    {
        block_t *p_next = p_buffer->p_next;;

        i_write += net_Write( p_access, p_sys->fd_data, NULL,
                              p_buffer->p_buffer, p_buffer->i_buffer );
        block_Release( p_buffer );

        p_buffer = p_next;
    }

    return i_write;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    bool   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;
    vlc_value_t  val;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
            pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = true;
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

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            var_Get( p_access, "ftp-caching", &val );
            *pi_64 = (int64_t)var_GetInteger( p_access, "ftp-caching" ) * INT64_C(1000);
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

/*****************************************************************************
 * ftp_*:
 *****************************************************************************/
static int ftp_SendCommand( vlc_object_t *p_access, access_sys_t *p_sys,
                            const char *psz_fmt, ... )
{
    va_list      args;
    char         *psz_cmd;

    va_start( args, psz_fmt );
    if( vasprintf( &psz_cmd, psz_fmt, args ) == -1 )
        return VLC_EGENERIC;

    va_end( args );

    msg_Dbg( p_access, "ftp_SendCommand:\"%s\"", psz_cmd);

    if( net_Printf( VLC_OBJECT(p_access), p_sys->fd_cmd, NULL, "%s\r\n",
                    psz_cmd ) < 0 )
    {
        msg_Err( p_access, "failed to send command" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
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
static int ftp_ReadCommand( vlc_object_t *p_access, access_sys_t *p_sys,
                            int *pi_answer, char **ppsz_answer )
{
    char         *psz_line;
    int          i_answer;

    psz_line = net_Gets( p_access, p_sys->fd_cmd, NULL );
    if( psz_line == NULL || strlen( psz_line ) < 3 )
    {
        msg_Err( p_access, "cannot get answer" );
        free( psz_line );
        if( pi_answer ) *pi_answer    = 500;
        if( ppsz_answer ) *ppsz_answer  = NULL;
        return -1;
    }
    msg_Dbg( p_access, "answer=%s", psz_line );

    if( psz_line[3] == '-' )    /* Multiple response */
    {
        char end[4];

        memcpy( end, psz_line, 3 );
        end[3] = ' ';

        for( ;; )
        {
            char *psz_tmp = net_Gets( p_access, p_sys->fd_cmd, NULL );

            if( psz_tmp == NULL )   /* Error */
                break;

            if( !strncmp( psz_tmp, end, 4 ) )
            {
                free( psz_tmp );
                break;
            }
            free( psz_tmp );
        }
    }

    i_answer = atoi( psz_line );

    if( pi_answer ) *pi_answer = i_answer;
    if( ppsz_answer )
    {
        *ppsz_answer = psz_line;
    }
    else
    {
        free( psz_line );
    }
    return( i_answer / 100 );
}

static int ftp_StartStream( vlc_object_t *p_access, access_sys_t *p_sys,
                            int64_t i_start )
{
    char psz_ipv4[16], *psz_ip = p_sys->sz_epsv_ip;
    int  i_answer;
    char *psz_arg, *psz_parser;
    int  i_port;

    assert( p_sys->fd_data == -1 );

    if( ( ftp_SendCommand( p_access, p_sys, *psz_ip ? "EPSV" : "PASV" ) < 0 )
     || ( ftp_ReadCommand( p_access, p_sys, &i_answer, &psz_arg ) != 2 ) )
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
        ftp_ReadCommand( p_access, p_sys, &i_answer, NULL ) != 2 )
    {
        msg_Err( p_access, "cannot set binary transfer mode" );
        return VLC_EGENERIC;
    }

    if( i_start > 0 )
    {
        if( ftp_SendCommand( p_access, p_sys, "REST %"PRIu64, i_start ) < 0 ||
            ftp_ReadCommand( p_access, p_sys, &i_answer, NULL ) > 3 )
        {
            msg_Err( p_access, "cannot set restart offset" );
            return VLC_EGENERIC;
        }
    }

    msg_Dbg( p_access, "waiting for data connection..." );
    p_sys->fd_data = net_ConnectTCP( p_access, psz_ip, i_port );
    if( p_sys->fd_data < 0 )
    {
        msg_Err( p_access, "failed to connect with server" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_access, "connection with \"%s:%d\" successful",
             psz_ip, i_port );

    /* "1xx" message */
    if( ftp_SendCommand( p_access, p_sys, "%s %s",
                         p_sys->out ? "STOR" : "RETR",
                         p_sys->url.psz_path ?: "" ) < 0 ||
        ftp_ReadCommand( p_access, p_sys, &i_answer, NULL ) > 2 )
    {
        msg_Err( p_access, "cannot retrieve file" );
        return VLC_EGENERIC;
    }

    shutdown( p_sys->fd_data, p_sys->out ? SHUT_RD : SHUT_WR );

    return VLC_SUCCESS;
}

static int ftp_StopStream ( vlc_object_t *p_access, access_sys_t *p_sys )
{
    if( ftp_SendCommand( p_access, p_sys, "ABOR" ) < 0 )
    {
        msg_Warn( p_access, "cannot abort file" );
        if(  p_sys->fd_data > 0 )
            net_Close( p_sys->fd_data );
        p_sys->fd_data = -1;
        return VLC_EGENERIC;
    }

    if( p_sys->fd_data != -1 )
    {
        net_Close( p_sys->fd_data );
        p_sys->fd_data = -1;
        /* Read the final response from RETR/STOR, i.e. 426 or 226 */
        ftp_ReadCommand( p_access, p_sys, NULL, NULL );
    }
    /* Read the response from ABOR, i.e. 226 or 225 */
    ftp_ReadCommand( p_access, p_sys, NULL, NULL );

    return VLC_SUCCESS;
}
