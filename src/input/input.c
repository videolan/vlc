/*****************************************************************************
 * input.c: input thread
 * Read an MPEG2 stream, demultiplex and parse it before sending it to
 * decoders.
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: input.c,v 1.119 2001/06/07 01:10:33 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include "defs.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#ifdef WIN32
#   include <winsock2.h>
#elif !defined( SYS_BEOS ) && !defined( SYS_NTO )
#   include <netdb.h>                                         /* hostent ... */
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <sys/types.h>
#   include <sys/socket.h>
#endif

#ifdef STATS
#   include <sys/times.h>
#endif

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "netutils.h"
#include "modules.h"

#include "intf_msg.h"
#include "intf_playlist.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"
#include "interface.h"

#include "main.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RunThread       ( input_thread_t *p_input );
static  int InitThread      ( input_thread_t *p_input );
static void ErrorThread     ( input_thread_t *p_input );
static void DestroyThread   ( input_thread_t *p_input );
static void EndThread       ( input_thread_t *p_input );

static void FileOpen        ( input_thread_t *p_input );
static void FileClose       ( input_thread_t *p_input );
#if !defined( SYS_BEOS ) && !defined( SYS_NTO )
static void NetworkOpen     ( input_thread_t *p_input );
static void NetworkClose    ( input_thread_t *p_input );
#endif

/*****************************************************************************
 * input_CreateThread: creates a new input thread
 *****************************************************************************
 * This function creates a new input, and returns a pointer
 * to its description. On error, it returns NULL.
 * If pi_status is NULL, then the function will block until the thread is ready.
 * If not, it will be updated using one of the THREAD_* constants.
 *****************************************************************************/
input_thread_t *input_CreateThread ( playlist_item_t *p_item, int *pi_status )
{
    input_thread_t *    p_input;                        /* thread descriptor */
    int                 i_status;                           /* thread status */

    /* Allocate descriptor */
    p_input = (input_thread_t *)malloc( sizeof(input_thread_t) );
    if( p_input == NULL )
    {
        intf_ErrMsg( "input error: can't allocate input thread (%s)",
                     strerror(errno) );
        return( NULL );
    }

    /* Packets read once */
    p_input->i_read_once = INPUT_READ_ONCE;

    /* Initialize thread properties */
    p_input->b_die              = 0;
    p_input->b_error            = 0;
    p_input->b_eof              = 0;

    /* Set target */
    p_input->p_source           = p_item->psz_name;

    /* I have never understood that stuff --Meuuh */
    p_input->pi_status          = (pi_status != NULL) ? pi_status : &i_status;
    *p_input->pi_status         = THREAD_CREATE;

    /* Initialize stream description */
    p_input->stream.i_es_number = 0;
    p_input->stream.i_selected_es_number = 0;
    p_input->stream.i_pgrm_number = 0;
    p_input->stream.i_new_status = p_input->stream.i_new_rate = 0;
    p_input->stream.i_mux_rate = 0;

    /* no stream, no area */
    p_input->stream.i_area_nb = 0;
    p_input->stream.pp_areas = NULL;
    p_input->stream.p_selected_area = NULL;
    p_input->stream.p_new_area = NULL;
    /* By default there is one areas in a stream */
    input_AddArea( p_input );
    p_input->stream.p_selected_area = p_input->stream.pp_areas[0];

    /* Initialize stream control properties. */
    p_input->stream.control.i_status = PLAYING_S;
    p_input->stream.control.i_rate = DEFAULT_RATE;
    p_input->stream.control.b_mute = 0;
    p_input->stream.control.b_bw = 0;

    /* Setup callbacks */
    p_input->pf_file_open     = FileOpen;
    p_input->pf_file_close    = FileClose;
#if !defined( SYS_BEOS ) && !defined( SYS_NTO )
    p_input->pf_network_open  = NetworkOpen;
    p_input->pf_network_close = NetworkClose;
#endif

    /* Create thread and set locks. */
    vlc_mutex_init( &p_input->stream.stream_lock );
    vlc_cond_init( &p_input->stream.stream_wait );
    vlc_mutex_init( &p_input->stream.control.control_lock );
    if( vlc_thread_create( &p_input->thread_id, "input", (void *) RunThread,
                           (void *) p_input ) )
    {
        intf_ErrMsg( "input error: can't create input thread (%s)",
                     strerror(errno) );
        free( p_input );
        return( NULL );
    }

    /* If status is NULL, wait until the thread is created */
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        } while( (i_status != THREAD_READY) && (i_status != THREAD_ERROR)
                && (i_status != THREAD_FATAL) );
        if( i_status != THREAD_READY )
        {
            return( NULL );
        }
    }
    return( p_input );
}

/*****************************************************************************
 * input_DestroyThread: mark an input thread as zombie
 *****************************************************************************
 * This function should not return until the thread is effectively cancelled.
 *****************************************************************************/
void input_DestroyThread( input_thread_t *p_input, int *pi_status )
{
    int         i_status;                                   /* thread status */

    /* Set status */
    p_input->pi_status = (pi_status != NULL) ? pi_status : &i_status;
    *p_input->pi_status = THREAD_DESTROY;

    /* Request thread destruction */
    p_input->b_die = 1;

    /* Make the thread exit of an eventual vlc_cond_wait() */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    vlc_cond_signal( &p_input->stream.stream_wait );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* If status is NULL, wait until thread has been destroyed */
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        } while ( (i_status != THREAD_OVER) && (i_status != THREAD_ERROR)
                  && (i_status != THREAD_FATAL) );
    }
}

/*****************************************************************************
 * RunThread: main thread loop
 *****************************************************************************
 * Thread in charge of processing the network packets and demultiplexing.
 *****************************************************************************/
static void RunThread( input_thread_t *p_input )
{
    int                     i_error, i;
    data_packet_t **        pp_packets;

    if( InitThread( p_input ) )
    {

        /* If we failed, wait before we are killed, and exit */
        *p_input->pi_status = THREAD_ERROR;
        p_input->b_error = 1;
        ErrorThread( p_input );
        DestroyThread( p_input );
        return;
    }

    /* initialization is completed */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_changed = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    pp_packets = (data_packet_t **) malloc( p_input->i_read_once *
                                        sizeof( data_packet_t * ) );
    if( pp_packets == NULL )
    {
        intf_ErrMsg( "input error: out of memory" );
        free( pp_packets );
        p_input->b_error = 1;
    }

    while( !p_input->b_die && !p_input->b_error && !p_input->b_eof )
    {
#ifdef STATS
        p_input->c_loops++;
#endif

        vlc_mutex_lock( &p_input->stream.stream_lock );

        if( p_input->stream.p_new_area )
        {
            p_input->pf_set_area( p_input, p_input->stream.p_new_area );
            p_input->stream.p_new_area = NULL;
        }

        if( p_input->stream.p_selected_area->i_seek != NO_SEEK )
        {
            if( p_input->stream.b_seekable && p_input->pf_seek != NULL )
            {
                p_input->pf_seek( p_input,
                                  p_input->stream.p_selected_area->i_seek );

                for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
                {
                    pgrm_descriptor_t * p_pgrm
                                            = p_input->stream.pp_programs[i];
                    /* Escape all decoders for the stream discontinuity they
                     * will encounter. */
                    input_EscapeDiscontinuity( p_input, p_pgrm );

                    /* Reinitialize synchro. */
                    p_pgrm->i_synchro_state = SYNCHRO_REINIT;
                }
            }
            p_input->stream.p_selected_area->i_seek = NO_SEEK;
        }

        if( p_input->stream.p_removed_es )
        {
            input_UnselectES( p_input, p_input->stream.p_removed_es );
            p_input->stream.p_removed_es = NULL;
        }

        if( p_input->stream.p_newly_selected_es )
        {
            input_SelectES( p_input, p_input->stream.p_newly_selected_es );
            p_input->stream.p_newly_selected_es = NULL;
        }

        vlc_mutex_unlock( &p_input->stream.stream_lock );

        i_error = p_input->pf_read( p_input, pp_packets );

        /* Demultiplex read packets. */
        for( i = 0; i < p_input->i_read_once && pp_packets[i] != NULL; i++ )
        {
            p_input->pf_demux( p_input, pp_packets[i] );
        }

        if( i_error )
        {
            if( i_error == 1 )
            {
                /* End of file - we do not set b_die because only the
                 * interface is allowed to do so. */
                intf_WarnMsg( 3, "input: EOF reached" );
                p_input->b_eof = 1;
            }
            else
            {
                p_input->b_error = 1;
            }
        }
    }

    free( pp_packets );

    if( p_input->b_error || p_input->b_eof )
    {
        ErrorThread( p_input );
    }

    EndThread( p_input );

    DestroyThread( p_input );

    intf_DbgMsg("input: Thread end");
}

/*****************************************************************************
 * InitThread: init the input Thread
 *****************************************************************************/
static int InitThread( input_thread_t * p_input )
{

#ifdef STATS
    /* Initialize statistics */
    p_input->c_loops                    = 0;
    p_input->c_bytes                    = 0;
    p_input->c_payload_bytes            = 0;
    p_input->c_packets_read             = 0;
    p_input->c_packets_trashed          = 0;
#endif

    /* Default, might get overwritten */
    p_input->pf_open = p_input->pf_file_open;
    p_input->pf_close = p_input->pf_file_close;

    p_input->p_input_module = module_Need( MODULE_CAPABILITY_INPUT,
                                           (probedata_t *)p_input );

    if( p_input->p_input_module == NULL )
    {
        intf_ErrMsg( "input error: no suitable input module for `%s'",
                     p_input->p_source );
        return( -1 );
    }

#define f p_input->p_input_module->p_functions->input.functions.input
    p_input->pf_init          = f.pf_init;
    if( f.pf_open != NULL )
    {
        p_input->pf_open          = f.pf_open;
    }
    if( f.pf_close != NULL )
    {
        p_input->pf_close         = f.pf_close;
    }
    p_input->pf_end           = f.pf_end;
    p_input->pf_read          = f.pf_read;
    p_input->pf_set_area      = f.pf_set_area;
    p_input->pf_demux         = f.pf_demux;
    p_input->pf_new_packet    = f.pf_new_packet;
    p_input->pf_new_pes       = f.pf_new_pes;
    p_input->pf_delete_packet = f.pf_delete_packet;
    p_input->pf_delete_pes    = f.pf_delete_pes;
    p_input->pf_rewind        = f.pf_rewind;
    p_input->pf_seek          = f.pf_seek;
#undef f

    /* We found the appropriate plugin, open the target */
    p_input->pf_open( p_input );

    if( p_input->b_error )
    {
        /* We barfed -- exit nicely */
        p_input->pf_close( p_input );
        module_Unneed( p_input->p_input_module );
        return( -1 );
    }

    p_input->pf_init( p_input );

    if( p_input->b_error )
    {
        /* We barfed -- exit nicely */
        p_input->pf_close( p_input );
        module_Unneed( p_input->p_input_module );
        return( -1 );
    }

    *p_input->pi_status = THREAD_READY;

    return( 0 );
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop.
 *****************************************************************************/
static void ErrorThread( input_thread_t *p_input )
{
    while( !p_input->b_die )
    {
        /* Sleep a while */
        msleep( INPUT_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * EndThread: end the input thread
 *****************************************************************************/
static void EndThread( input_thread_t * p_input )
{
    int *       pi_status;                                  /* thread status */

    /* Store status */
    pi_status = p_input->pi_status;
    *pi_status = THREAD_END;

#ifdef STATS
    {
        struct tms cpu_usage;
        times( &cpu_usage );

        intf_Msg("input stats: cpu usage (user: %d, system: %d)",
                 cpu_usage.tms_utime, cpu_usage.tms_stime);
    }
#endif

    /* Free all ES and destroy all decoder threads */
    input_EndStream( p_input );

    /* Free demultiplexer's data */
    p_input->pf_end( p_input );

    /* Close stream */
    p_input->pf_close( p_input );

    /* Release modules */
    module_Unneed( p_input->p_input_module );

}

/*****************************************************************************
 * DestroyThread: destroy the input thread
 *****************************************************************************/
static void DestroyThread( input_thread_t * p_input )
{
    int *       pi_status;                                  /* thread status */

    /* Store status */
    pi_status = p_input->pi_status;

    /* Destroy Mutex locks */
    vlc_mutex_destroy( &p_input->stream.control.control_lock );
    vlc_mutex_destroy( &p_input->stream.stream_lock );
    
    /* Free input structure */
    free( p_input );

    /* Update status */
    *pi_status = THREAD_OVER;
}

/*****************************************************************************
 * FileOpen : open a file descriptor
 *****************************************************************************/
static void FileOpen( input_thread_t * p_input )
{
    struct stat         stat_info;
    int                 i_stat;

#if defined( WIN32 )
    char buf[7] = { 0 };
#endif

    char *psz_name = p_input->p_source;

    /* FIXME: this code ought to be in the plugin so that code can
     * be shared with the *_Probe function */
    if( ( i_stat = stat( psz_name, &stat_info ) ) == (-1) )
    {
        int i_size = strlen( psz_name );

        if( ( i_size > 4 )
            && !strncasecmp( psz_name, "dvd:", 4 ) )
        {
            /* get rid of the 'dvd:' stuff and try again */
            psz_name += 4;
            i_stat = stat( psz_name, &stat_info );
#if defined( WIN32 )
            _snprintf( buf, 7, "\\\\.\\%c:", psz_name[0] );
#endif
        }
        else if( ( i_size > 5 )
                 && !strncasecmp( psz_name, "file:", 5 ) )
        {
            /* get rid of the 'file:' stuff and try again */
            psz_name += 5;
            i_stat = stat( psz_name, &stat_info );
        }

        if( i_stat == (-1) 
#if defined( WIN32 )
        && !buf[0]      
#endif
            )
        {
            intf_ErrMsg( "input error: cannot stat() file `%s' (%s)",
                         psz_name, strerror(errno));
            p_input->b_error = 1;
            return;
        }
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* If we are here we can control the pace... */
    p_input->stream.b_pace_control = 1;

    if( S_ISREG(stat_info.st_mode) || S_ISCHR(stat_info.st_mode)
         || S_ISBLK(stat_info.st_mode)
#if defined( WIN32 )
         || ( buf[0] && ( ( stat_info.st_size = 0 ) == 0 ) )
#endif
         )
    {
        p_input->stream.b_seekable = 1;
        p_input->stream.p_selected_area->i_size = stat_info.st_size;
    }
    else if( S_ISFIFO(stat_info.st_mode)
#if !defined( SYS_BEOS ) && !defined( WIN32 )
             || S_ISSOCK(stat_info.st_mode)
#endif
             )
    {
        p_input->stream.b_seekable = 0;
        p_input->stream.p_selected_area->i_size = 0;
    }
    else
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        intf_ErrMsg( "input error: unknown file type for `%s'",
                     psz_name );
        p_input->b_error = 1;
        return;
    }

    p_input->stream.p_selected_area->i_tell = 0;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    intf_WarnMsg( 1, "input: opening file `%s'", p_input->p_source );
#ifndef WIN32
    if( (p_input->i_handle = open( psz_name,
                                   /*O_NONBLOCK | O_LARGEFILE*/0 )) == (-1) )
#else
    if( ( buf[0] && ( (HANDLE) p_input->i_handle = CreateFile( buf, 
        GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 
        NULL, OPEN_EXISTING, 0, NULL ) ) == INVALID_HANDLE_VALUE ) ||
        ( !buf[0] && (p_input->i_handle = open( psz_name, O_BINARY ) ) == (-1) ) )
#endif
    {
        intf_ErrMsg( "input error: cannot open file (%s)", strerror(errno) );
        p_input->b_error = 1;
        return;
    }

}

/*****************************************************************************
 * FileClose : close a file descriptor
 *****************************************************************************/
static void FileClose( input_thread_t * p_input )
{
    intf_WarnMsg( 1, "input: closing file `%s'", p_input->p_source );
#if defined( WIN32 )
    if( ( strlen( p_input->p_source ) > 4 ) &&
        !strncasecmp( p_input->p_source, "dvd:", 4 ) )
    {
        CloseHandle( (HANDLE) p_input->i_handle );
    }
    else
#endif
    close( p_input->i_handle );

    return;
}

#if !defined( SYS_BEOS ) && !defined( SYS_NTO )
/*****************************************************************************
 * NetworkOpen : open a network socket 
 *****************************************************************************/
static void NetworkOpen( input_thread_t * p_input )
{
    char                *psz_server = NULL;
    char                *psz_broadcast = NULL;
    int                 i_port = 0;
    int                 i_opt;
    struct sockaddr_in  sock;

#ifdef WIN32
    /* WinSock Library Init. */
    WSADATA Data;
    int i_err = WSAStartup( MAKEWORD( 1, 1 ), &Data );

    if( i_err )
    {
        intf_ErrMsg( "input: can't initiate WinSocks, error %i", i_err );
        return ;
    }
#endif
    
    /* Get the remote server */
    if( p_input->p_source != NULL )
    {
        psz_server = p_input->p_source;

        /* Skip the protocol name */
        while( *psz_server && *psz_server != ':' )
        {
            psz_server++;
        }

        /* Skip the "://" part */
        while( *psz_server && (*psz_server == ':' || *psz_server == '/') )
        {
            psz_server++;
        }

        /* Found a server name */
        if( *psz_server )
        {
            char *psz_port = psz_server;

            /* Skip the hostname part */
            while( *psz_port && *psz_port != ':' )
            {
                psz_port++;
            }

            /* Found a port name */
            if( *psz_port )
            {
                /* Replace ':' with '\0' */
                *psz_port = '\0';
                psz_port++;

                psz_broadcast = psz_port;
                while( *psz_broadcast && *psz_broadcast != ':' )
                {
                    psz_broadcast++;
                }

                if( *psz_broadcast )
                {
                    *psz_broadcast = '\0';
                    psz_broadcast++;
                    while( *psz_broadcast && *psz_broadcast == ':' )
                    {
                        psz_broadcast++;
                    }
                }
                else
                {
                    psz_broadcast = NULL;
                }

                /* port before broadcast address */
                if( *psz_port != ':' )
                {
                    i_port = atoi( psz_port );
                }
            }
        }
        else
        {
            psz_server = NULL;
        }
    }

    /* Check that we got a valid server */
    if( psz_server == NULL )
    {
        psz_server = main_GetPszVariable( INPUT_SERVER_VAR, 
                                          INPUT_SERVER_DEFAULT );
    }

    /* Check that we got a valid port */
    if( i_port == 0 )
    {
        i_port = main_GetIntVariable( INPUT_PORT_VAR, INPUT_PORT_DEFAULT );
    }

    if( psz_broadcast == NULL )
    {
        /* Are we broadcasting ? */
        if( main_GetIntVariable( INPUT_BROADCAST_VAR,
                                 INPUT_BROADCAST_DEFAULT ) )
        {
            psz_broadcast = main_GetPszVariable( INPUT_BCAST_ADDR_VAR,
                                                 INPUT_BCAST_ADDR_DEFAULT );
        }
        else
        {
           psz_broadcast = NULL; 
        }
    }

    intf_WarnMsg( 2, "input: server: %s port: %d broadcast: %s",
                     psz_server, i_port, psz_broadcast );

    /* Open a SOCK_DGRAM (UDP) socket, in the AF_INET domain, automatic (0)
     * protocol */
    p_input->i_handle = socket( AF_INET, SOCK_DGRAM, 0 );
    if( p_input->i_handle == -1 )
    {
        intf_ErrMsg("input error: can't create socket : %s", strerror(errno));
        p_input->b_error = 1;
        return;
    }

    /* We may want to reuse an already used socket */
    i_opt = 1;
    if( setsockopt( p_input->i_handle, SOL_SOCKET, SO_REUSEADDR,
                    (void*) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        intf_ErrMsg( "input error: can't configure socket (SO_REUSEADDR: %s)",
                     strerror(errno));
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }

    /* Increase the receive buffer size to 1/2MB (8Mb/s during 1/2s) to avoid
     * packet loss caused by scheduling problems */
    i_opt = 0x80000;
    if( setsockopt( p_input->i_handle, SOL_SOCKET, SO_RCVBUF,
                    (void*) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        intf_ErrMsg( "input error: can't configure socket (SO_RCVBUF: %s)", 
                     strerror(errno));
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }

    /* Build the local socket */
    if ( network_BuildLocalAddr( &sock, i_port, psz_broadcast ) == -1 )
    {
        intf_ErrMsg( "input error: can't build local address" );
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }
    
    /* Bind it */
    if( bind( p_input->i_handle, (struct sockaddr *)&sock, 
              sizeof( sock ) ) < 0 )
    {
        intf_ErrMsg("input error: can't bind socket (%s)", strerror(errno));
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }

    /* Build socket for remote connection */
    if ( network_BuildRemoteAddr( &sock, psz_server ) == -1 )
    {
        intf_ErrMsg( "input error: can't build remote address" );
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }

    /* And connect it ... should we really connect ? */
    if( connect( p_input->i_handle, (struct sockaddr *) &sock,
                 sizeof( sock ) ) == (-1) )
    {
        intf_ErrMsg( "input error: can't connect socket, %s", 
                     strerror(errno) );
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }

    /* We can't pace control, but FIXME : bug in meuuh's code to sync PCR
     * with the server. */
    p_input->stream.b_pace_control = 0;
    p_input->stream.b_seekable = 0;

    intf_WarnMsg( 3, "input: successfully opened network mode" );
    
    return;
}

/*****************************************************************************
 * NetworkClose : close a network socket
 *****************************************************************************/
static void NetworkClose( input_thread_t * p_input )
{
    close( p_input->i_handle );

#ifdef WIN32 
    WSACleanup();
#endif

}
#endif

