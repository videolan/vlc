/*****************************************************************************
 * ftp.c:
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id: ftp.c,v 1.8 2003/02/20 01:52:45 sigmunau Exp $
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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#ifdef WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   ifndef IN_MULTICAST
#       define IN_MULTICAST(a) IN_CLASSD(a)
#   endif
#else
#   include <sys/socket.h>
#   include <netinet/in.h>
#   if HAVE_ARPA_INET_H
#      include <arpa/inet.h>
#   elif defined( SYS_BEOS )
#      include <net/netdb.h>
#   endif
#endif

#include "network.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open        ( vlc_object_t * );
static void Close       ( vlc_object_t * );

static int  Read        ( input_thread_t * p_input, byte_t * p_buffer,
                          size_t i_len );
static void Seek        ( input_thread_t *, off_t );
static int  SetProgram  ( input_thread_t *, pgrm_descriptor_t * );


static ssize_t NetRead ( input_thread_t *, input_socket_t *, byte_t *, size_t );
static void    NetClose( input_thread_t *, input_socket_t *);

static int  ftp_SendCommand( input_thread_t *, char *, ... );
static int  ftp_ReadCommand( input_thread_t *, int *, char ** );
static int  ftp_StartStream( input_thread_t *, off_t );
static int  ftp_StopStream ( input_thread_t *);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for ftp streams. This " \
    "value should be set in miliseconds units." )

vlc_module_begin();
    set_description( _("ftp access module") );
    set_capability( "access", 0 );
    add_category_hint( "stream", NULL, VLC_FALSE );
        add_integer( "ftp-caching", 2 * DEFAULT_PTS_DELAY / 1000, NULL,
                     CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
        add_string( "ftp-user", "anonymous", NULL, "ftp user name", "ftp user name", VLC_FALSE );
        add_string( "ftp-pwd", "anonymous@dummy.org", NULL, "ftp password", "ftp password, be careful with that option...", VLC_FALSE );
        add_string( "ftp-account", "anonymous", NULL, "ftp account", "ftp account", VLC_FALSE );
    add_shortcut( "ftp" );
    set_callbacks( Open, Close );
vlc_module_end();

/* url: [/]host[:port][/path] */
typedef struct url_s
{
    char    *psz_server_addr;
    int     i_server_port;

    char    *psz_bind_addr;
    int     i_bind_port;

    char    *psz_path;

    /* private */
    char *psz_private;
} url_t;

static void ftp_ParseURL( url_t *, char * );

#define FREE( p ) if( p ) free( p )

typedef struct access_s
{
    input_socket_t  socket_cmd;
    input_socket_t  socket_data;

    url_t           url;                        /* connect to this server */

    off_t           i_filesize;

    int             i_eos;

} access_t;


/****************************************************************************
 ****************************************************************************
 *******************                                      *******************
 *******************       Main functions                 *******************
 *******************                                      *******************
 ****************************************************************************
 ****************************************************************************/

/****************************************************************************
 * Open: connect to ftp server and ask for file
 ****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    input_thread_t  *p_input = (input_thread_t*)p_this;

    access_t    *p_access;
    char        *psz_network;

    module_t            *p_network;
    network_socket_t    socket_desc;
    url_t               *p_url;

    int                 i_answer;
    char                *psz_user, *psz_pwd, *psz_account;

    char                *psz_arg;

    /* *** allocate p_access_data *** */
    p_input->p_access_data =
        (void*)p_access = malloc( sizeof( access_t ) );
    memset( p_access, 0, sizeof( access_t ) );
    p_url = &p_access->url;

    /* *** Parse URL and get server addr/port and path *** */
    ftp_ParseURL( p_url, p_input->psz_name );

    if( p_url->psz_server_addr == NULL ||
        !( *p_url->psz_server_addr ) )
    {
        FREE( p_url->psz_private );
        msg_Err( p_input, "invalid server name" );
        return( -1 );
    }
    if( p_url->i_server_port == 0 )
    {
        p_url->i_server_port = 21; /* default port */
    }

    /* 2: look at ip version ipv4/ipv6 */
    psz_network = "";
    if( config_GetInt( p_input, "ipv4" ) )
    {
        psz_network = "ipv4";
    }
    else if( config_GetInt( p_input, "ipv6" ) )
    {
        psz_network = "ipv6";
    }

    /* 3: Open a TCP connection with server *** */
    msg_Dbg( p_input, "waiting for connection..." );
    socket_desc.i_type = NETWORK_TCP;
    socket_desc.psz_server_addr = p_url->psz_server_addr;
    socket_desc.i_server_port   = p_url->i_server_port;
    socket_desc.psz_bind_addr   = "";
    socket_desc.i_bind_port     = 0;
    p_input->p_private = (void*)&socket_desc;
    if( !( p_network = module_Need( p_input, "network", psz_network ) ) )
    {
        msg_Err( p_input, "failed to connect with server" );
        FREE( p_access->url.psz_private );
        FREE( p_input->p_access_data );
        return( -1 );
    }
    module_Unneed( p_input, p_network );
    p_access->socket_cmd.i_handle = socket_desc.i_handle;
    p_input->i_mtu    = socket_desc.i_mtu;
    msg_Dbg( p_input,
             "connection with \"%s:%d\" successful",
             p_url->psz_server_addr,
             p_url->i_server_port );


    for( ;; )
    {
        if( ftp_ReadCommand( p_input, &i_answer, NULL ) < 0)
        {
            msg_Err( p_input, "failed to get answer" );
            goto exit_error;
        }
        if( i_answer / 100 != 1 )
        {
            break;
        }
    }

    if( i_answer / 100 != 2 )
    {
        msg_Err( p_input, "connection rejected" );
        goto exit_error;
    }
    else
    {
        msg_Dbg( p_input, "connection accepted (%d)", i_answer );
    }

    psz_user = config_GetPsz( p_input, "ftp-user" );
    if( ftp_SendCommand( p_input, "USER %s", psz_user ) < 0 )
    {
        FREE( psz_user );
        goto exit_error;
    }
    FREE( psz_user );

    if( ftp_ReadCommand( p_input, &i_answer, NULL ) < 0)
    {
        msg_Err( p_input, "failed to get answer" );
        goto exit_error;
    }
    switch( i_answer / 100 )
    {
        case 2:
            msg_Dbg( p_input, "user accepted" );
            break;
        case 3:
            msg_Dbg( p_input, "password needed" );
            psz_pwd = config_GetPsz( p_input, "ftp-pwd" );
            if( ftp_SendCommand( p_input, "PASS %s", psz_pwd ) < 0 )
            {
                FREE( psz_pwd );
                goto exit_error;
            }
            FREE( psz_pwd );
            if( ftp_ReadCommand( p_input, &i_answer, NULL ) < 0)
            {
                msg_Err( p_input, "failed to get answer" );
                goto exit_error;
            }
            switch( i_answer / 100 )
            {
                case 2:
                    msg_Dbg( p_input, "password accepted" );
                    break;
                case 3:
                    msg_Dbg( p_input, "account needed" );
                    psz_account = config_GetPsz( p_input, "ftp-account" );
                    if( ftp_SendCommand( p_input, "ACCT %s", psz_account ) < 0 )
                    {
                        FREE( psz_account );
                        goto exit_error;
                    }
                    FREE( psz_account );
                    if( ftp_ReadCommand( p_input, &i_answer, NULL ) < 0)
                    {
                        msg_Err( p_input, "failed to get answer" );
                        goto exit_error;
                    }
                    if( i_answer / 100 != 2 )
                    {
                        msg_Err( p_input, "account rejected" );
                        goto exit_error;
                    }
                    else
                    {
                        msg_Dbg( p_input, "account accepted" );
                    }
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

    if( ftp_SendCommand( p_input, "TYPE I" ) < 0 )
    {
        msg_Err( p_input, "cannot set binary transfert mode" );
        goto exit_error;
    }
    if( ftp_ReadCommand( p_input, &i_answer, NULL ) != 2 )
    {
        msg_Err( p_input, "cannot set binary transfert mode" );
        goto exit_error;
    }

    /* get size */
    if( ftp_SendCommand( p_input, "SIZE %s", p_url->psz_path ) < 0 )
    {
        msg_Err( p_input, "cannot get file size" );
        goto exit_error;
    }
    if( ftp_ReadCommand( p_input, &i_answer, &psz_arg ) != 2 )
    {
        msg_Err( p_input, "cannot get file size" );
        goto exit_error;
    }

#ifdef HAVE_ATOLL
    p_access->i_filesize = atoll( psz_arg + 4 );
#else
    {
        int64_t i_size = 0;
        char    *psz_parser = psz_arg + 4;

        while( *psz_parser == ' ' ) psz_parser++;

        while( psz_parser[0] >= '0' && psz_parser[0] <= '9' )
        {
            i_size *= 10;
            i_size += psz_parser[0] - '0';
        }
        p_access->i_filesize = i_size;
    }
#endif

    msg_Dbg( p_input, "file size: "I64Fd, p_access->i_filesize );
    FREE( psz_arg );

    if( ftp_StartStream( p_input, 0 ) < 0 )
    {
        msg_Err( p_input, "cannot retrieve file" );
        goto exit_error;
    }
    /* *** set exported functions *** */
    p_input->pf_read = Read;
    p_input->pf_seek = Seek;
    p_input->pf_set_program = SetProgram;
    p_input->pf_set_area = NULL;

    p_input->p_private = NULL;

    /* *** finished to set some variable *** */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_pace_control = 1;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.b_seekable = 1;
    p_input->stream.b_connected = 1;
    p_input->stream.p_selected_area->i_size = p_access->i_filesize;
    p_input->stream.i_method = INPUT_METHOD_NETWORK;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* Update default_pts to a suitable value for ftp access */
    p_input->i_pts_delay = config_GetInt( p_input, "ftp-caching" ) * 1000;

    return( 0 );

exit_error:
    NetClose( p_input, &p_access->socket_cmd );
    FREE( p_access->url.psz_private );
    FREE( p_input->p_access_data );
    return( -1 );
}

/*****************************************************************************
 * Close: free unused data structures
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t  *p_input = (input_thread_t *)p_this;
    access_t        *p_access = (access_t*)p_input->p_access_data;

    msg_Dbg( p_input, "stopping stream" );
    ftp_StopStream( p_input );

    if( ftp_SendCommand( p_input, "QUIT" ) < 0 )
    {
        msg_Err( p_input, "cannot quit" );
    }
    else
    {
        ftp_ReadCommand( p_input, NULL, NULL );
    }


    NetClose( p_input, &p_access->socket_cmd );

    /* free memory */
    FREE( p_access->url.psz_private );
}

/*****************************************************************************
 * SetProgram: do nothing
 *****************************************************************************/
static int SetProgram( input_thread_t * p_input,
                       pgrm_descriptor_t * p_program )
{
    return( 0 );
}

/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static void Seek( input_thread_t * p_input, off_t i_pos )
{
    //access_t    *p_access = (access_t*)p_input->p_access_data;
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

static int  Read        ( input_thread_t * p_input, byte_t * p_buffer,
                          size_t i_len )
{
    access_t    *p_access = (access_t*)p_input->p_access_data;
    size_t      i_data;

    i_data = NetRead( p_input, &p_access->socket_data, p_buffer, i_len );

    return( i_data );
}

static int  ftp_SendCommand( input_thread_t *p_input, char *psz_fmt, ... )
{
    access_t        *p_access = (access_t*)p_input->p_access_data;
    va_list args;
    char    *psz_buffer;
#if !defined(HAVE_VASPRINTF) || defined(SYS_DARWIN)
        size_t  i_size;
#endif

    va_start( args, psz_fmt );

#if defined(HAVE_VASPRINTF) && !defined(SYS_DARWIN)
    vasprintf( &psz_buffer, psz_fmt, args );
#else
    i_size = strlen( psz_fmt ) + 2048;
    psz_buffer = calloc( i_size, sizeof( char ) );
    vsnprintf( psz_buffer, i_size, psz_fmt, args );
    psz_buffer[i_size - 1] = 0;
#endif
    if( !strncmp( psz_buffer, "PASS", 4 ) )
    {
        msg_Dbg( p_input, "ftp_SendCommand:\"PASS xxx\"" );
    }
    else
    {
        msg_Dbg( p_input, "ftp_SendCommand:\"%s\"", psz_buffer );
    }
    psz_buffer = realloc( psz_buffer, strlen( psz_buffer ) + 3 );
    strcat( psz_buffer, "\r\n" );
    if( send( p_access->socket_cmd.i_handle,
              psz_buffer,
              strlen( psz_buffer ),
              0 ) == -1 )
    {
        FREE( psz_buffer );
        msg_Err( p_input, "failed to send command" );
        return( -1 );
    }
    FREE( psz_buffer );

    va_end( args );

    return( 0 );
}

#define BLOCK_SIZE  1024
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

static int  ftp_ReadCommand( input_thread_t *p_input,
                             int *pi_answer, char **ppsz_answer )
{
    access_t        *p_access = (access_t*)p_input->p_access_data;
    uint8_t *p_buffer;
    int     i_buffer;
    int     i_buffer_size;

    int i_answer;

    i_buffer      = 0;
    i_buffer_size = BLOCK_SIZE + 1;
    p_buffer      = malloc( BLOCK_SIZE + 1);

    for( ;; )
    {
        ssize_t i_read;
        i_read = NetRead( p_input, &p_access->socket_cmd,
                          p_buffer + i_buffer, BLOCK_SIZE );
        if( i_read <= 0 || p_input->b_die || p_input->b_error )
        {
            free( p_buffer );
            if( pi_answer )   *pi_answer    = 500;
            if( ppsz_answer ) *ppsz_answer  = NULL;
            return( -1 );
        }
        if( i_read == 0 )
        {
//            continue;
        }
        i_buffer += i_read;
        if( i_read < BLOCK_SIZE )
        {
            p_buffer[i_buffer] = '\0';
            break;
        }
        i_buffer_size += BLOCK_SIZE;
        p_buffer = realloc( p_buffer, i_buffer_size );
    }

    if( i_buffer < 3 )
    {
        goto exit_error;
    }

    i_answer = atoi( p_buffer );

    if( pi_answer ) *pi_answer = i_answer;
    if( ppsz_answer )
    {
        *ppsz_answer = p_buffer;
    }
    else
    {
        free( p_buffer );
    }
    return( i_answer / 100 );

exit_error:
    free( p_buffer );
    if( pi_answer )   *pi_answer    = 500;
    if( ppsz_answer ) *ppsz_answer  = NULL;
    return( -1 );
}

static int  ftp_StartStream( input_thread_t *p_input, off_t i_start )
{
    access_t        *p_access = (access_t*)p_input->p_access_data;

    char psz_ip[1000];
    int  i_answer;
    char *psz_arg, *psz_parser;
    int  a1,a2,a3,a4;
    int  p1,p2;
    int  i_port;
    module_t            *p_network;
    network_socket_t    socket_desc;

    if( ftp_SendCommand( p_input, "PASV" ) < 0 )
    {
        msg_Err( p_input, "cannot set passive transfert mode" );
        return( -1 );
    }
    if( ftp_ReadCommand( p_input, &i_answer, &psz_arg ) != 2 )
    {
        msg_Err( p_input, "cannot set passive transfert mode" );
        return( -1 );
    }
    psz_parser = strchr( psz_arg, '(' );
    if( !psz_parser || sscanf( psz_parser, "(%d,%d,%d,%d,%d,%d", &a1, &a2, &a3, &a4, &p1, &p2 ) < 6 )
    {
        FREE( psz_arg );
        msg_Err( p_input, "cannot get ip/port for passive transfert mode" );
        return( -1 );
    }
    FREE( psz_arg );

    sprintf( psz_ip, "%d.%d.%d.%d", a1, a2, a3, a4 );
    i_port = p1 * 256 + p2;
    msg_Dbg( p_input, "ip:%s port:%d", psz_ip, i_port );

    if( ftp_SendCommand( p_input, "TYPE I" ) < 0 )
    {
        msg_Err( p_input, "cannot set binary transfert mode" );
        return( -1 );
    }
    if( ftp_ReadCommand( p_input, &i_answer, NULL ) != 2 )
    {
        msg_Err( p_input, "cannot set binary transfert mode" );
        return( -1 );
    }


    if( i_start > 0 )
    {
        if( ftp_SendCommand( p_input, "REST "I64Fu, i_start ) < 0 )
        {
            msg_Err( p_input, "cannot set restart point" );
            return( -1 );
        }
        if( ftp_ReadCommand( p_input, &i_answer, NULL ) > 3 )
        {
            msg_Err( p_input, "cannot set restart point" );
            return( -1 );
        }
    }

    msg_Dbg( p_input, "waiting for data connection..." );
    socket_desc.i_type = NETWORK_TCP;
    socket_desc.psz_server_addr = psz_ip;
    socket_desc.i_server_port   = i_port;
    socket_desc.psz_bind_addr   = "";
    socket_desc.i_bind_port     = 0;
    p_input->p_private = (void*)&socket_desc;
    if( !( p_network = module_Need( p_input, "network", "" ) ) )
    {
        msg_Err( p_input, "failed to connect with server" );
        return( -1 );
    }
    module_Unneed( p_input, p_network );
    p_access->socket_data.i_handle = socket_desc.i_handle;
    p_input->i_mtu    = socket_desc.i_mtu;
    msg_Dbg( p_input,
             "connection with \"%s:%d\" successful",
             psz_ip, i_port );

    if( ftp_SendCommand( p_input, "RETR %s", p_access->url.psz_path ) < 0 )
    {
        msg_Err( p_input, "cannot retreive file" );
        return( -1 );
    }
    /* "1xx" message */
    if( ftp_ReadCommand( p_input, &i_answer, NULL ) > 2 )
    {
        msg_Err( p_input, "cannot retreive file" );
        return( -1 );
    }

    return( 0 );
}

static int  ftp_StopStream ( input_thread_t *p_input)
{
    access_t        *p_access = (access_t*)p_input->p_access_data;

    int i_answer;

    NetClose( p_input, &p_access->socket_data );

    if( ftp_SendCommand( p_input, "ABOR" ) < 0 )
    {
        msg_Err( p_input, "cannot abord file" );
    }
    else
    {
        ftp_ReadCommand( p_input, &i_answer, NULL );
        ftp_ReadCommand( p_input, &i_answer, NULL );
    }

    return( 0 );
}

/****************************************************************************
 *
 ****************************************************************************/
static void ftp_ParseURL( url_t *p_url, char *psz_url )
{
    char *psz_parser;
    char *psz_server_port;

    p_url->psz_private = strdup( psz_url );

    psz_parser = p_url->psz_private;

    while( *psz_parser == '/' )
    {
        psz_parser++;
    }
    p_url->psz_server_addr = psz_parser;

    while( *psz_parser &&
           *psz_parser != ':' &&  *psz_parser != '/' && *psz_parser != '@' )
    {
        psz_parser++;
    }

    if( *psz_parser == ':' )
    {
        *psz_parser = '\0';
        psz_parser++;
        psz_server_port = psz_parser;

        while( *psz_parser && *psz_parser != '/' )
        {
            psz_parser++;
        }
    }
    else
    {
        psz_server_port = "";
    }

    if( *psz_parser == '@' )
    {
        char *psz_bind_port;

        *psz_parser = '\0';
        psz_parser++;

        p_url->psz_bind_addr = psz_parser;

        while( *psz_parser && *psz_parser != ':' && *psz_parser != '/' )
        {
            psz_parser++;
        }

        if( *psz_parser == ':' )
        {
            *psz_parser = '\0';
            psz_parser++;
            psz_bind_port = psz_parser;

            while( *psz_parser && *psz_parser != '/' )
            {
                psz_parser++;
            }
        }
        else
        {
            psz_bind_port = "";
        }
        if( *psz_bind_port )
        {
            p_url->i_bind_port = strtol( psz_bind_port, &psz_parser, 10 );
        }
        else
        {
            p_url->i_bind_port = 0;
        }
    }
    else
    {
        p_url->psz_bind_addr = "";
        p_url->i_bind_port = 0;
    }

    if( *psz_parser == '/' )
    {
        *psz_parser = '\0';
        psz_parser++;
        p_url->psz_path = psz_parser;
    }

    if( *psz_server_port )
    {
        p_url->i_server_port = strtol( psz_server_port, &psz_parser, 10 );
    }
    else
    {
        p_url->i_server_port = 0;
    }
}

/*****************************************************************************
 * Read: read on a file descriptor, checking b_die periodically
 *****************************************************************************/
static ssize_t NetRead( input_thread_t *p_input,
                        input_socket_t *p_socket,
                        byte_t *p_buffer, size_t i_len )
{
#ifdef UNDER_CE
    return -1;

#else
    struct timeval  timeout;
    fd_set          fds;
    int             i_ret;

    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    FD_SET( p_socket->i_handle, &fds );

    /* We'll wait 1 second if nothing happens */
    timeout.tv_sec  = 0;
    timeout.tv_usec = 1000000;

    /* Find if some data is available */
    i_ret = select( p_socket->i_handle + 1, &fds,
                    NULL, NULL, &timeout );

    if( i_ret == -1 && errno != EINTR )
    {
        msg_Err( p_input, "network select error (%s)", strerror(errno) );
    }
    else if( i_ret > 0 )
    {
        ssize_t i_recv = recv( p_socket->i_handle, p_buffer, i_len, 0 );

        if( i_recv < 0 )
        {
            msg_Err( p_input, "recv failed (%s)", strerror(errno) );
        }

        return i_recv;
    }

    return 0;

#endif
}

static void NetClose( input_thread_t *p_input, input_socket_t *p_socket )
{
#if defined( UNDER_CE )
    CloseHandle( (HANDLE)p_socket->i_handle );
#elif defined( WIN32 )
    closesocket( p_socket->i_handle );
#else
    close( p_socket->i_handle );
#endif
}

