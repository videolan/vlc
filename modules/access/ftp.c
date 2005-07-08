/*****************************************************************************
 * ftp.c: FTP input module
 *****************************************************************************
 * Copyright (C) 2001-2005 VideoLAN (Centrale RÃ©seaux) and its contributors
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "network.h"
#if defined( UNDER_CE )
#   include <winsock.h>
#elif defined( WIN32 )
#   include <winsock2.h>
#else
#   include <sys/socket.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int     Open ( vlc_object_t * );
static void    Close( vlc_object_t * );

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
    set_shortname( "FTP" );
    set_description( _("FTP input") );
    set_capability( "access2", 0 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
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
static int Read( access_t *, uint8_t *, int );
static int Seek( access_t *, int64_t );
static int Control( access_t *, int, va_list );

struct access_sys_t
{
    vlc_url_t  url;

    int        fd_cmd;
    int        fd_data;
    
    char       *psz_epsv_ip;
};

static int  ftp_SendCommand( access_t *, char *, ... );
static int  ftp_ReadCommand( access_t *, int *, char ** );
static int  ftp_StartStream( access_t *, int64_t );
static int  ftp_StopStream ( access_t *);

/****************************************************************************
 * Open: connect to ftp server and ask for file
 ****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    access_t     *p_access = (access_t*)p_this;
    access_sys_t *p_sys;
    char         *psz;

    int          i_answer;
    char         *psz_arg;

    /* Init p_access */
    p_access->pf_read = Read;
    p_access->pf_block = NULL;
    p_access->pf_seek = Seek;
    p_access->pf_control = Control;
    p_access->info.i_update = 0;
    p_access->info.i_size = 0;
    p_access->info.i_pos = 0;
    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_title = 0;
    p_access->info.i_seekpoint = 0;
    p_access->p_sys = p_sys = malloc( sizeof( access_sys_t ) );
    memset( p_sys, 0, sizeof( access_sys_t ) );
    p_sys->fd_cmd = -1;
    p_sys->fd_data = -1;
    p_sys->psz_epsv_ip = NULL;

    /* *** Parse URL and get server addr/port and path *** */
    psz = p_access->psz_path;
    while( *psz == '/' )
    {
        psz++;
    }
    vlc_UrlParse( &p_sys->url, psz, 0 );

    if( p_sys->url.psz_host == NULL || *p_sys->url.psz_host == '\0' )
    {
        msg_Err( p_access, "invalid server name" );
        goto exit_error;
    }
    if( p_sys->url.i_port <= 0 )
    {
        p_sys->url.i_port = 21; /* default port */
    }

    /* *** Open a TCP connection with server *** */
    msg_Dbg( p_access, "waiting for connection..." );
    p_sys->fd_cmd = net_OpenTCP( p_access, p_sys->url.psz_host,
                                 p_sys->url.i_port );
    if( p_sys->fd_cmd < 0 )
    {
        msg_Err( p_access, "failed to connect with server" );
        goto exit_error;
    }

    for( ;; )
    {
        if( ftp_ReadCommand( p_access, &i_answer, NULL ) != 1 )
        {
            break;
        }
    }
    if( i_answer / 100 != 2 )
    {
        msg_Err( p_access, "connection rejected" );
        goto exit_error;
    }

    msg_Dbg( p_access, "connection accepted (%d)", i_answer );

    psz = var_CreateGetString( p_access, "ftp-user" );
    if( ftp_SendCommand( p_access, "USER %s", psz ) < 0 ||
        ftp_ReadCommand( p_access, &i_answer, NULL ) < 0 )
    {
        free( psz );
        goto exit_error;
    }
    free( psz );

    switch( i_answer / 100 )
    {
        case 2:
            msg_Dbg( p_access, "user accepted" );
            break;
        case 3:
            msg_Dbg( p_access, "password needed" );
            psz = var_CreateGetString( p_access, "ftp-pwd" );
            if( ftp_SendCommand( p_access, "PASS %s", psz ) < 0 ||
                ftp_ReadCommand( p_access, &i_answer, NULL ) < 0 )
            {
                free( psz );
                goto exit_error;
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
                    if( ftp_SendCommand( p_access, "ACCT %s",
                                         psz ) < 0 ||
                        ftp_ReadCommand( p_access, &i_answer, NULL ) < 0 )
                    {
                        free( psz );
                        goto exit_error;
                    }
                    free( psz );

                    if( i_answer / 100 != 2 )
                    {
                        msg_Err( p_access, "account rejected" );
                        goto exit_error;
                    }
                    msg_Dbg( p_access, "account accepted" );
                    break;

                default:
                    msg_Err( p_access, "password rejected" );
                    goto exit_error;
            }
            break;
        default:
            msg_Err( p_access, "user rejected" );
            goto exit_error;
    }

    /* Extended passive mode */
    if( ftp_SendCommand( p_access, "EPSV ALL" ) < 0 )
    {
        msg_Err( p_access, "cannot request extended passive mode" );
        goto exit_error;
    }

    if( ftp_ReadCommand( p_access, &i_answer, NULL ) == 2 )
    {
        char hostaddr[NI_MAXNUMERICHOST];
        struct sockaddr_storage addr;
        socklen_t len = sizeof (addr);

        if( getpeername( p_sys->fd_cmd, (struct sockaddr *)&addr, &len ) )
        {
            msg_Err( p_access, "getpeername failed" );
            goto exit_error;
        }

        i_answer = vlc_getnameinfo( (struct sockaddr *)&addr, len, hostaddr,
                                    sizeof( hostaddr ), NULL, NI_NUMERICHOST );
        if( i_answer )
        {
            msg_Err( p_access, "getnameinfo failed: %s",
                     vlc_gai_strerror( i_answer ) );
            goto exit_error;
        }
        p_sys->psz_epsv_ip = strdup( hostaddr );
    }
    if( p_sys->psz_epsv_ip == NULL )
        msg_Info( p_access, "FTP Extended passive mode disabled" );

    /* binary mode */
    if( ftp_SendCommand( p_access, "TYPE I" ) < 0 ||
        ftp_ReadCommand( p_access, &i_answer, NULL ) != 2 )
    {
        msg_Err( p_access, "cannot set binary transfer mode" );
        goto exit_error;
    }

    /* get size */
    if( ftp_SendCommand( p_access, "SIZE %s", p_sys->url.psz_path ) < 0 ||
        ftp_ReadCommand( p_access, &i_answer, &psz_arg ) != 2 )
    {
        msg_Err( p_access, "cannot get file size" );
        goto exit_error;
    }
    p_access->info.i_size = atoll( &psz_arg[4] );
    free( psz_arg );
    msg_Dbg( p_access, "file size: "I64Fd, p_access->info.i_size );

    /* Start the 'stream' */
    if( ftp_StartStream( p_access, 0 ) < 0 )
    {
        msg_Err( p_access, "cannot retrieve file" );
        goto exit_error;
    }

    /* Update default_pts to a suitable value for ftp access */
    var_Create( p_access, "ftp-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

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
    access_t      *p_access = (access_t*)p_this;
    access_sys_t  *p_sys = p_access->p_sys;

    msg_Dbg( p_access, "stopping stream" );
    ftp_StopStream( p_access );

    if( ftp_SendCommand( p_access, "QUIT" ) < 0 )
    {
        msg_Warn( p_access, "cannot quit" );
    }
    else
    {
        ftp_ReadCommand( p_access, NULL, NULL );
    }
    net_Close( p_sys->fd_cmd );
    if( p_sys->psz_epsv_ip != NULL )
        free( p_sys->psz_epsv_ip );

    /* free memory */
    vlc_UrlClean( &p_sys->url );
    free( p_sys );
}

/*****************************************************************************
 * Seek: try to go at the right place
 *****************************************************************************/
static int Seek( access_t *p_access, int64_t i_pos )
{
    if( i_pos < 0 )
    {
        return VLC_EGENERIC;
    }
    msg_Dbg( p_access, "seeking to "I64Fd, i_pos );

    ftp_StopStream( p_access );
    if( ftp_StartStream( p_access, i_pos ) < 0 )
    {
        p_access->info.b_eof = VLC_TRUE;
        return VLC_EGENERIC;
    }

    p_access->info.b_eof = VLC_FALSE;
    p_access->info.i_pos = i_pos;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Read:
 *****************************************************************************/
static int Read( access_t *p_access, uint8_t *p_buffer, int i_len )
{
    access_sys_t *p_sys = p_access->p_sys;
    int i_read;

    if( p_access->info.b_eof )
        return 0;

    i_read = net_Read( p_access, p_sys->fd_data, NULL, p_buffer, i_len,
                       VLC_FALSE );
    if( i_read == 0 )
        p_access->info.b_eof = VLC_TRUE;
    else if( i_read > 0 )
        p_access->info.i_pos += i_read;

    return i_read;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( access_t *p_access, int i_query, va_list args )
{
    vlc_bool_t   *pb_bool;
    int          *pi_int;
    int64_t      *pi_64;
    vlc_value_t  val;

    switch( i_query )
    {
        /* */
        case ACCESS_CAN_SEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;
            break;
        case ACCESS_CAN_FASTSEEK:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_FALSE;
            break;
        case ACCESS_CAN_PAUSE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;    /* FIXME */
            break;
        case ACCESS_CAN_CONTROL_PACE:
            pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;    /* FIXME */
            break;

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = 0;
            break;

        case ACCESS_GET_PTS_DELAY:
            pi_64 = (int64_t*)va_arg( args, int64_t * );
            var_Get( p_access, "ftp-caching", &val );
            *pi_64 = (int64_t)var_GetInteger( p_access, "ftp-caching" ) * I64C(1000);
            break;

        /* */
        case ACCESS_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ftp_*:
 *****************************************************************************/
static int ftp_SendCommand( access_t *p_access, char *psz_fmt, ... )
{
    access_sys_t *p_sys = p_access->p_sys;
    va_list      args;
    char         *psz_cmd;

    va_start( args, psz_fmt );
    vasprintf( &psz_cmd, psz_fmt, args );
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
static int ftp_ReadCommand( access_t *p_access,
                            int *pi_answer, char **ppsz_answer )
{
    access_sys_t *p_sys = p_access->p_sys;
    char         *psz_line;
    int          i_answer;

    psz_line = net_Gets( p_access, p_sys->fd_cmd, NULL );
    msg_Dbg( p_access, "answer=%s", psz_line );
    if( psz_line == NULL || strlen( psz_line ) < 3 )
    {
        msg_Err( p_access, "cannot get answer" );
        if( psz_line ) free( psz_line );
        if( pi_answer ) *pi_answer    = 500;
        if( ppsz_answer ) *ppsz_answer  = NULL;
        return -1;
    }

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

static int ftp_StartStream( access_t *p_access, off_t i_start )
{
    access_sys_t *p_sys = p_access->p_sys;

    char psz_ipv4[16], *psz_ip;
    int  i_answer;
    char *psz_arg, *psz_parser;
    unsigned  a1, a2, a3, a4, p1, p2;
    int  i_port;

    if( ( ftp_SendCommand( p_access, p_sys->psz_epsv_ip != NULL
                                     ? "EPSV" : "PASV" ) < 0 )
     || ( ftp_ReadCommand( p_access, &i_answer, &psz_arg ) != 2 ) )
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

    psz_ip = p_sys->psz_epsv_ip;
    if( psz_ip != NULL )
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

    if( ftp_SendCommand( p_access, "TYPE I" ) < 0 ||
        ftp_ReadCommand( p_access, &i_answer, NULL ) != 2 )
    {
        msg_Err( p_access, "cannot set binary transfer mode" );
        return VLC_EGENERIC;
    }

    if( i_start > 0 )
    {
        if( ftp_SendCommand( p_access, "REST "I64Fu, i_start ) < 0 ||
            ftp_ReadCommand( p_access, &i_answer, NULL ) > 3 )
        {
            msg_Err( p_access, "cannot set restart point" );
            return VLC_EGENERIC;
        }
    }

    msg_Dbg( p_access, "waiting for data connection..." );
    p_sys->fd_data = net_OpenTCP( p_access, psz_ip, i_port );
    if( p_sys->fd_data < 0 )
    {
        msg_Err( p_access, "failed to connect with server" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_access, "connection with \"%s:%d\" successful",
             psz_ip, i_port );

    /* "1xx" message */
    if( ftp_SendCommand( p_access, "RETR %s", p_sys->url.psz_path ) < 0 ||
        ftp_ReadCommand( p_access, &i_answer, NULL ) > 2 )
    {
        msg_Err( p_access, "cannot retreive file" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int ftp_StopStream ( access_t *p_access )
{
    access_sys_t *p_sys = p_access->p_sys;

    int i_answer;

    if( ftp_SendCommand( p_access, "ABOR" ) < 0 )
    {
        msg_Warn( p_access, "cannot abord file" );
        if(  p_sys->fd_data > 0 )
            net_Close( p_sys->fd_data );
        p_sys->fd_data = -1;
        return VLC_EGENERIC;
    }
    if(  p_sys->fd_data > 0 )
    {
        net_Close( p_sys->fd_data );
        p_sys->fd_data = -1;
        ftp_ReadCommand( p_access, &i_answer, NULL );
    }
    ftp_ReadCommand( p_access, &i_answer, NULL );

    return VLC_SUCCESS;
}

