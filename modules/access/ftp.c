/*****************************************************************************
 * ftp.c: FTP input module
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN
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

#include "network.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int     Open     ( vlc_object_t * );
static void    Close    ( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for FTP streams. This " \
    "value should be set in millisecond units." )
#define USER_TEXT N_("FTP user name")
#define USER_LONGTEXT N_("Allows you to modify the user name that will " \
    "be used for the connection.")
#define PASS_TEXT N_("FTP password")
#define PASS_LONGTEXT N_("Allows you to modify the password that will be " \
    "used for the connection.")
#define ACCOUNT_TEXT N_("FTP account")
#define ACCOUNT_LONGTEXT N_("Allows you to modify the account that will be " \
    "used for the connection.")

vlc_module_begin();
    set_description( _("FTP input") );
    set_capability( "access", 0 );
    add_integer( "ftp-caching", 2 * DEFAULT_PTS_DELAY / 1000, NULL,
                 CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_string( "ftp-user", "anonymous", NULL, USER_TEXT, USER_LONGTEXT,
                VLC_FALSE );
    add_string( "ftp-pwd", "anonymous@dummy.org", NULL, PASS_TEXT,
                PASS_LONGTEXT, VLC_FALSE );
    add_string( "ftp-account", "anonymous", NULL, ACCOUNT_TEXT,
                ACCOUNT_LONGTEXT, VLC_FALSE );
    add_shortcut( "ftp" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t Read     ( input_thread_t *, byte_t *,  size_t );
static void    Seek     ( input_thread_t *, off_t );

struct access_sys_t
{
    vlc_url_t url;

    int       fd_cmd;
    int       fd_data;

    int64_t   i_size;
};

static int  ftp_SendCommand( input_thread_t *, char *, ... );
static int  ftp_ReadCommand( input_thread_t *, int *, char ** );
static int  ftp_StartStream( input_thread_t *, off_t );
static int  ftp_StopStream ( input_thread_t *);

/****************************************************************************
 * Open: connect to ftp server and ask for file
 ****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t  *p_input = (input_thread_t*)p_this;
    access_sys_t    *p_sys;
    char            *psz;
    vlc_value_t     val;

    int             i_answer;
    char            *psz_arg;

    /* *** allocate access_sys_t *** */
    p_sys = p_input->p_access_data = malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );
    p_sys->fd_cmd = -1;
    p_sys->fd_data = -1;

    /* *** Parse URL and get server addr/port and path *** */
    psz = p_input->psz_name;
    while( *psz == '/' )
    {
        psz++;
    }
    vlc_UrlParse( &p_sys->url, psz, 0 );

    if( p_sys->url.psz_host == NULL || *p_sys->url.psz_host == '\0' )
    {
        msg_Err( p_input, "invalid server name" );
        goto exit_error;
    }
    if( p_sys->url.i_port <= 0 )
    {
        p_sys->url.i_port = 21; /* default port */
    }

    /* *** Open a TCP connection with server *** */
    msg_Dbg( p_input, "waiting for connection..." );
    p_sys->fd_cmd = net_OpenTCP( p_input, p_sys->url.psz_host,
                                 p_sys->url.i_port );
    if( p_sys->fd_cmd < 0 )
    {
        msg_Err( p_input, "failed to connect with server" );
        goto exit_error;
    }
    p_input->i_mtu = 0;

    for( ;; )
    {
        if( ftp_ReadCommand( p_input, &i_answer, NULL ) != 1 )
        {
            break;
        }
    }
    if( i_answer / 100 != 2 )
    {
        msg_Err( p_input, "connection rejected" );
        goto exit_error;
    }

    msg_Dbg( p_input, "connection accepted (%d)", i_answer );

    var_Create( p_input, "ftp-user", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_input, "ftp-user", &val );
    if( ftp_SendCommand( p_input, "USER %s", val.psz_string ) < 0 ||
        ftp_ReadCommand( p_input, &i_answer, NULL ) < 0 )
    {
        if( val.psz_string ) free( val.psz_string );
        goto exit_error;
    }
    if( val.psz_string ) free( val.psz_string );

    switch( i_answer / 100 )
    {
        case 2:
            msg_Dbg( p_input, "user accepted" );
            break;
        case 3:
            msg_Dbg( p_input, "password needed" );
            var_Create( p_input, "ftp-pwd", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
            var_Get( p_input, "ftp-pwd", &val );
            if( ftp_SendCommand( p_input, "PASS %s", val.psz_string ) < 0 ||
                ftp_ReadCommand( p_input, &i_answer, NULL ) < 0 )
            {
                if( val.psz_string ) free( val.psz_string );
                goto exit_error;
            }
            if( val.psz_string ) free( val.psz_string );

            switch( i_answer / 100 )
            {
                case 2:
                    msg_Dbg( p_input, "password accepted" );
                    break;
                case 3:
                    msg_Dbg( p_input, "account needed" );
                    var_Create( p_input, "ftp-account",
                                VLC_VAR_STRING | VLC_VAR_DOINHERIT );
                    var_Get( p_input, "ftp-account", &val );
                    if( ftp_SendCommand( p_input, "ACCT %s",
                                         val.psz_string ) < 0 ||
                        ftp_ReadCommand( p_input, &i_answer, NULL ) < 0 )
                    {
                        if( val.psz_string ) free( val.psz_string );
                        goto exit_error;
                    }
                    if( val.psz_string ) free( val.psz_string );

                    if( i_answer / 100 != 2 )
                    {
                        msg_Err( p_input, "account rejected" );
                        goto exit_error;
                    }
                    msg_Dbg( p_input, "account accepted" );
                    break;

                default:
                    msg_Err( p_input, "password rejected" );
                    goto exit_error;
            }
            break;
        default:
            msg_Err( p_input, "user rejected" );
            goto exit_error;
    }

    /* binary mode */
    if( ftp_SendCommand( p_input, "TYPE I" ) < 0 ||
        ftp_ReadCommand( p_input, &i_answer, NULL ) != 2 )
    {
        msg_Err( p_input, "cannot set binary transfert mode" );
        goto exit_error;
    }

    /* get size */
    if( ftp_SendCommand( p_input, "SIZE %s", p_sys->url.psz_path ) < 0 ||
        ftp_ReadCommand( p_input, &i_answer, &psz_arg ) != 2 )
    {
        msg_Err( p_input, "cannot get file size" );
        goto exit_error;
    }
    p_sys->i_size = atoll( &psz_arg[4] );
    free( psz_arg );
    msg_Dbg( p_input, "file size: "I64Fd, p_sys->i_size );

    /* Start the 'stream' */
    if( ftp_StartStream( p_input, 0 ) < 0 )
    {
        msg_Err( p_input, "cannot retrieve file" );
        goto exit_error;
    }
    /* *** set exported functions *** */
    p_input->pf_read = Read;
    p_input->pf_seek = Seek;
    p_input->pf_set_program = input_SetProgram;
    p_input->pf_set_area = NULL;

    p_input->p_private = NULL;

    /* *** finished to set some variable *** */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = VLC_TRUE;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.b_seekable = VLC_TRUE;
    p_input->stream.p_selected_area->i_size = p_sys->i_size;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update default_pts to a suitable value for ftp access */
    var_Create( p_input, "ftp-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "ftp-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

    return VLC_SUCCESS;

exit_error:
    if( p_sys->fd_cmd > 0 )
    {
        net_Close( p_sys->fd_cmd );
    }
    vlc_UrlClean( &p_sys->url );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t  *p_input = (input_thread_t *)p_this;
    access_sys_t    *p_sys = p_input->p_access_data;

    msg_Dbg( p_input, "stopping stream" );
    ftp_StopStream( p_input );

    if( ftp_SendCommand( p_input, "QUIT" ) < 0 )
    {
        msg_Warn( p_input, "cannot quit" );
    }
    else
    {
        ftp_ReadCommand( p_input, NULL, NULL );
    }
    net_Close( p_sys->fd_cmd );

    /* free memory */
    vlc_UrlClean( &p_sys->url );
    free( p_sys );
}

/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static void Seek( input_thread_t * p_input, off_t i_pos )
{
    if( i_pos < 0 )
    {
        return;
    }
    vlc_mutex_lock( &p_input->stream.stream_lock );

    msg_Dbg( p_input, "seeking to "I64Fd, i_pos );

    ftp_StopStream( p_input );
    ftp_StartStream( p_input, i_pos );

    p_input->stream.p_selected_area->i_tell = i_pos;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer,
                     size_t i_len )
{
    access_sys_t *p_sys = p_input->p_access_data;

    return net_Read( p_input, p_sys->fd_data, p_buffer, i_len, VLC_FALSE );
}

/*****************************************************************************
 * ftp_*:
 *****************************************************************************/
static int ftp_SendCommand( input_thread_t *p_input, char *psz_fmt, ... )
{
    access_sys_t *p_sys = p_input->p_access_data;
    va_list      args;
    char         *psz_cmd;
    int          i_ret;

    va_start( args, psz_fmt );
    vasprintf( &psz_cmd, psz_fmt, args );
    va_end( args );

    msg_Dbg( p_input, "ftp_SendCommand:\"%s\"", psz_cmd);
    if( ( i_ret = net_Printf( VLC_OBJECT(p_input), p_sys->fd_cmd,
                              "%s", psz_cmd ) ) > 0 )
    {
        i_ret = net_Printf( VLC_OBJECT(p_input), p_sys->fd_cmd, "\n" );
    }

    if( i_ret < 0 )
    {
        msg_Err( p_input, "failed to send command" );
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
static int ftp_ReadCommand( input_thread_t *p_input,
                            int *pi_answer, char **ppsz_answer )
{
    access_sys_t *p_sys = p_input->p_access_data;
    char         *psz_line;
    int          i_answer;

    psz_line = net_Gets( p_input, p_sys->fd_cmd );
    msg_Dbg( p_input, "answer=%s", psz_line );
    if( psz_line == NULL || strlen( psz_line ) < 3 )
    {
        msg_Err( p_input, "cannot get answer" );
        if( psz_line ) free( psz_line );
        if( pi_answer ) *pi_answer    = 500;
        if( ppsz_answer ) *ppsz_answer  = NULL;
        return -1;
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

static int ftp_StartStream( input_thread_t *p_input, off_t i_start )
{
    access_sys_t *p_sys = p_input->p_access_data;

    char psz_ip[1000];
    int  i_answer;
    char *psz_arg, *psz_parser;
    int  a1,a2,a3,a4;
    int  p1,p2;
    int  i_port;

    if( ftp_SendCommand( p_input, "PASV" ) < 0 ||
        ftp_ReadCommand( p_input, &i_answer, &psz_arg ) != 2 )
    {
        msg_Err( p_input, "cannot set passive transfert mode" );
        return VLC_EGENERIC;
    }

    psz_parser = strchr( psz_arg, '(' );
    if( !psz_parser ||
        sscanf( psz_parser, "(%d,%d,%d,%d,%d,%d", &a1, &a2, &a3,
                &a4, &p1, &p2 ) < 6 )
    {
        free( psz_arg );
        msg_Err( p_input, "cannot get ip/port for passive transfert mode" );
        return VLC_EGENERIC;
    }
    free( psz_arg );

    sprintf( psz_ip, "%d.%d.%d.%d", a1, a2, a3, a4 );
    i_port = p1 * 256 + p2;
    msg_Dbg( p_input, "ip:%s port:%d", psz_ip, i_port );

    if( ftp_SendCommand( p_input, "TYPE I" ) < 0 ||
        ftp_ReadCommand( p_input, &i_answer, NULL ) != 2 )
    {
        msg_Err( p_input, "cannot set binary transfert mode" );
        return VLC_EGENERIC;
    }

    if( i_start > 0 )
    {
        if( ftp_SendCommand( p_input, "REST "I64Fu, i_start ) < 0 ||
            ftp_ReadCommand( p_input, &i_answer, NULL ) > 3 )
        {
            msg_Err( p_input, "cannot set restart point" );
            return VLC_EGENERIC;
        }
    }

    msg_Dbg( p_input, "waiting for data connection..." );
    p_sys->fd_data = net_OpenTCP( p_input, psz_ip, i_port );
    if( p_sys->fd_data < 0 )
    {
        msg_Err( p_input, "failed to connect with server" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_input, "connection with \"%s:%d\" successful",
             psz_ip, i_port );

    /* "1xx" message */
    if( ftp_SendCommand( p_input, "RETR %s", p_sys->url.psz_path ) < 0 ||
        ftp_ReadCommand( p_input, &i_answer, NULL ) > 2 )
    {
        msg_Err( p_input, "cannot retreive file" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int ftp_StopStream ( input_thread_t *p_input)
{
    access_sys_t *p_sys = p_input->p_access_data;

    int i_answer;

    if( ftp_SendCommand( p_input, "ABOR" ) < 0 )
    {
        msg_Warn( p_input, "cannot abord file" );
        net_Close( p_sys->fd_data ); p_sys->fd_data = -1;
        return VLC_EGENERIC;
    }
    net_Close( p_sys->fd_data ); p_sys->fd_data = -1;
    ftp_ReadCommand( p_input, &i_answer, NULL );
    ftp_ReadCommand( p_input, &i_answer, NULL );

    return VLC_SUCCESS;
}
