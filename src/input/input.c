/*****************************************************************************
 * input.c: input thread
 * Read an MPEG2 stream, demultiplex and parse it before sending it to
 * decoders.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input.c,v 1.178 2002/02/26 17:22:12 xav Exp $
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <videolan/vlc.h>

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
#   include <ws2tcpip.h>
#elif !defined( SYS_BEOS ) && !defined( SYS_NTO )
#   include <netdb.h>                                         /* hostent ... */
#   include <sys/socket.h>
#   include <netinet/in.h>
#   ifdef HAVE_ARPA_INET_H
#       include <arpa/inet.h>                    /* inet_ntoa(), inet_aton() */
#   endif
#endif

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "netutils.h"

#include "intf_playlist.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "interface.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static  int RunThread       ( input_thread_t *p_input );
static  int InitThread      ( input_thread_t *p_input );
static void ErrorThread     ( input_thread_t *p_input );
static void CloseThread     ( input_thread_t *p_input );
static void DestroyThread   ( input_thread_t *p_input );
static void EndThread       ( input_thread_t *p_input );

static void FileOpen        ( input_thread_t *p_input );
static void StdOpen         ( input_thread_t *p_input );
static void FileClose       ( input_thread_t *p_input );
#if !defined( SYS_BEOS ) && !defined( SYS_NTO )
static void NetworkOpen     ( input_thread_t *p_input );
static void HTTPOpen        ( input_thread_t *p_input );
static void NetworkClose    ( input_thread_t *p_input );
#endif

/*****************************************************************************
 * input_InitBank: initialize the input bank.
 *****************************************************************************/
void input_InitBank ( void )
{
    p_input_bank->i_count = 0;

    /* XXX: Workaround for old interface modules */
    p_input_bank->pp_input[0] = NULL;

    vlc_mutex_init( &p_input_bank->lock );
}

/*****************************************************************************
 * input_EndBank: empty the input bank.
 *****************************************************************************
 * This function ends all unused inputs and empties the bank in
 * case of success.
 *****************************************************************************/
void input_EndBank ( void )
{
    int i_input;

    /* Ask all remaining video outputs to die */
    for( i_input = 0; i_input < p_input_bank->i_count; i_input++ )
    {
        input_StopThread(
                p_input_bank->pp_input[ i_input ], NULL );
        input_DestroyThread(
                p_input_bank->pp_input[ i_input ] );
    }

    vlc_mutex_destroy( &p_input_bank->lock );
}

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

    /* Allocate descriptor */
    p_input = (input_thread_t *)malloc( sizeof(input_thread_t) );
    if( p_input == NULL )
    {
        intf_ErrMsg( "input error: can't allocate input thread (%s)",
                     strerror(errno) );
        return( NULL );
    }

    /* Initialize thread properties */
    p_input->b_die      = 0;
    p_input->b_error    = 0;
    p_input->b_eof      = 0;

    /* Set target */
    p_input->p_source   = p_item->psz_name;

    /* Set status */
    p_input->i_status   = THREAD_CREATE;
    
    /* Initialize statistics */
    p_input->c_loops                    = 0;
    p_input->stream.c_packets_read      = 0;
    p_input->stream.c_packets_trashed   = 0;
    p_input->p_stream                   = NULL;

    /* Set locks. */
    vlc_mutex_init( &p_input->stream.stream_lock );
    vlc_cond_init( &p_input->stream.stream_wait );
    vlc_mutex_init( &p_input->stream.control.control_lock );

    /* Initialize stream description */
    p_input->stream.b_changed = 0;
    p_input->stream.i_es_number = 0;
    p_input->stream.i_selected_es_number = 0;
    p_input->stream.i_pgrm_number = 0;
    p_input->stream.i_new_status = p_input->stream.i_new_rate = 0;
    p_input->stream.b_new_mute = MUTE_NO_CHANGE;
    p_input->stream.i_mux_rate = 0;

    /* no stream, no program, no area, no es */
    p_input->stream.p_new_program = NULL;

    p_input->stream.i_area_nb = 0;
    p_input->stream.pp_areas = NULL;
    p_input->stream.p_selected_area = NULL;
    p_input->stream.p_new_area = NULL;

    p_input->stream.pp_selected_es = NULL;
    p_input->stream.p_removed_es = NULL;
    p_input->stream.p_newly_selected_es = NULL;

    /* By default there is one area in a stream */
    input_AddArea( p_input );
    p_input->stream.p_selected_area = p_input->stream.pp_areas[0];

    /* Initialize stream control properties. */
    p_input->stream.control.i_status = PLAYING_S;
    p_input->stream.control.i_rate = DEFAULT_RATE;
    p_input->stream.control.b_mute = 0;
    p_input->stream.control.b_grayscale = config_GetIntVariable(
			                      VOUT_GRAYSCALE_VAR );
    p_input->stream.control.i_smp = config_GetIntVariable( VDEC_SMP_VAR );

    intf_WarnMsg( 1, "input: playlist item `%s'", p_input->p_source );

    /* Create thread. */
    if( vlc_thread_create( &p_input->thread_id, "input",
                           (vlc_thread_func_t)RunThread, (void *) p_input ) )
    {
        intf_ErrMsg( "input error: can't create input thread (%s)",
                     strerror(errno) );
        free( p_input );
        return( NULL );
    }

#if 0
    /* If status is NULL, wait until the thread is created */
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        } while( (i_status != THREAD_READY) && (i_status != THREAD_ERROR)
                && (i_status != THREAD_FATAL) );
    }
#endif

    return( p_input );
}

/*****************************************************************************
 * input_StopThread: mark an input thread as zombie
 *****************************************************************************
 * This function should not return until the thread is effectively cancelled.
 *****************************************************************************/
void input_StopThread( input_thread_t *p_input, int *pi_status )
{
    /* Make the thread exit from a possible vlc_cond_wait() */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Request thread destruction */
    p_input->b_die = 1;

    vlc_cond_signal( &p_input->stream.stream_wait );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* If status is NULL, wait until thread has been destroyed */
#if 0
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        } while ( (i_status != THREAD_OVER) && (i_status != THREAD_ERROR)
                  && (i_status != THREAD_FATAL) );
    }
#endif
}

/*****************************************************************************
 * input_DestroyThread: mark an input thread as zombie
 *****************************************************************************
 * This function should not return until the thread is effectively cancelled.
 *****************************************************************************/
void input_DestroyThread( input_thread_t *p_input )
{
    /* Join the thread */
    vlc_thread_join( p_input->thread_id );

    /* Destroy Mutex locks */
    vlc_mutex_destroy( &p_input->stream.control.control_lock );
    vlc_cond_destroy( &p_input->stream.stream_wait );
    vlc_mutex_destroy( &p_input->stream.stream_lock );
    
    /* Free input structure */
    free( p_input );
}

/*****************************************************************************
 * RunThread: main thread loop
 *****************************************************************************
 * Thread in charge of processing the network packets and demultiplexing.
 *****************************************************************************/
static int RunThread( input_thread_t *p_input )
{
    if( InitThread( p_input ) )
    {
        /* If we failed, wait before we are killed, and exit */
        p_input->i_status = THREAD_ERROR;
        p_input->b_error = 1;
        ErrorThread( p_input );
        DestroyThread( p_input );
        return 0;
    }

    p_input->i_status = THREAD_READY;

    /* initialization is complete */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_changed = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    while( !p_input->b_die && !p_input->b_error && !p_input->b_eof )
    {
        data_packet_t * p_data;
        int i_count, i;

        p_input->c_loops++;

        vlc_mutex_lock( &p_input->stream.stream_lock );

        if( p_input->stream.p_new_program )
        {
            if( p_input->pf_set_program != NULL )
            {

                p_input->pf_set_program( p_input, 
                        p_input->stream.p_new_program );

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
            p_input->stream.p_new_program = NULL;
        }
        
        if( p_input->stream.p_new_area )
        {
            if( p_input->stream.b_seekable && p_input->pf_set_area != NULL )
            {

                p_input->pf_set_area( p_input, p_input->stream.p_new_area );

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

        if( p_input->stream.b_new_mute != MUTE_NO_CHANGE )
        {
            if( p_input->stream.b_new_mute )
            {
                input_EscapeAudioDiscontinuity( p_input );
            }

            vlc_mutex_lock( &p_input->stream.control.control_lock );
            p_input->stream.control.b_mute = p_input->stream.b_new_mute;
            vlc_mutex_unlock( &p_input->stream.control.control_lock );

            p_input->stream.b_new_mute = MUTE_NO_CHANGE;
        }

        vlc_mutex_unlock( &p_input->stream.stream_lock );

        i_count = p_input->pf_read( p_input, &p_data );

        /* Demultiplex read packets. */
        while( p_data != NULL )
        {
            data_packet_t * p_next = p_data->p_next;
            p_data->p_next = NULL;

            p_input->stream.c_packets_read++;
            p_input->pf_demux( p_input, p_data );

            p_data = p_next;
        }

        if( i_count == 0 && p_input->stream.b_seekable )
        {
            /* End of file - we do not set b_die because only the
             * interface is allowed to do so. */
            intf_WarnMsg( 3, "input: EOF reached" );
            p_input->b_eof = 1;
        }
        else if( i_count < 0 )
        {
            p_input->b_error = 1;
        }
    }

    if( p_input->b_error || p_input->b_eof )
    {
        ErrorThread( p_input );
    }

    EndThread( p_input );

    DestroyThread( p_input );

    return 0;
}

/*****************************************************************************
 * InitThread: init the input Thread
 *****************************************************************************/
static int InitThread( input_thread_t * p_input )
{
    char *psz_name;

    /* Find appropriate module. */
    psz_name = config_GetPszVariable( INPUT_METHOD_VAR );
    p_input->p_input_module = module_Need( MODULE_CAPABILITY_INPUT, psz_name,
                                           (void *)p_input );

    if( psz_name ) free( psz_name );
    if( p_input->p_input_module == NULL )
    {
        intf_ErrMsg( "input error: no suitable input module for `%s'",
                     p_input->p_source );
        return( -1 );
    }

#define f p_input->p_input_module->p_functions->input.functions.input
    p_input->pf_probe         = f.pf_probe;
    p_input->pf_init          = f.pf_init;
    p_input->pf_end           = f.pf_end;
    p_input->pf_read          = f.pf_read;
    p_input->pf_set_area      = f.pf_set_area;
    p_input->pf_set_program   = f.pf_set_program;
    p_input->pf_demux         = f.pf_demux;
    p_input->pf_new_packet    = f.pf_new_packet;
    p_input->pf_new_pes       = f.pf_new_pes;
    p_input->pf_delete_packet = f.pf_delete_packet;
    p_input->pf_delete_pes    = f.pf_delete_pes;
    p_input->pf_rewind        = f.pf_rewind;
    p_input->pf_seek          = f.pf_seek;

    if( f.pf_open != NULL )
    {
        f.pf_open( p_input );
    }
#if !defined( SYS_BEOS ) && !defined( SYS_NTO )
    /* FIXME : this is waaaay too kludgy */
    else if( ( strlen( p_input->p_source ) >= 10
               && !strncasecmp( p_input->p_source, "udpstream:", 10 ) )
               || ( strlen( p_input->p_source ) >= 4
                     && !strncasecmp( p_input->p_source, "udp:", 4 ) ) )
    {
        /* Network stream */
        NetworkOpen( p_input );
        p_input->stream.i_method = INPUT_METHOD_NETWORK;
    }
    else if( ( strlen( p_input->p_source ) > 5 )
               && !strncasecmp( p_input->p_source, "http:", 5 ) )
    {
        /* HTTP stream */
        HTTPOpen( p_input );
        p_input->stream.i_method = INPUT_METHOD_NETWORK;
    }
#endif
    else if( ( strlen( p_input->p_source ) == 1 )
               && *p_input->p_source == '-' )
    {
        /* Stdin */
        StdOpen( p_input );
    }
    else
    {
        /* File input */
        FileOpen( p_input );
        p_input->stream.i_method = INPUT_METHOD_FILE;
    }
#undef f

    if( p_input->b_error )
    {
        /* We barfed -- exit nicely */
        module_Unneed( p_input->p_input_module );
        return( -1 );
    }

    p_input->pf_init( p_input );

    if( p_input->b_error )
    {
        /* We barfed -- exit nicely */
        CloseThread( p_input );
        module_Unneed( p_input->p_input_module );
        return( -1 );
    }

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
    /* Store status */
    p_input->i_status = THREAD_END;

    if( p_main->b_stats )
    {
#ifdef HAVE_SYS_TIMES_H
        /* Display statistics */
        struct tms  cpu_usage;
        times( &cpu_usage );

        intf_StatMsg( "input stats: %d loops consuming user: %d, system: %d",
                      p_input->c_loops,
                      cpu_usage.tms_utime, cpu_usage.tms_stime );
#else
        intf_StatMsg( "input stats: %d loops", p_input->c_loops );
#endif

        input_DumpStream( p_input );
    }

    /* Free all ES and destroy all decoder threads */
    input_EndStream( p_input );

    /* Free demultiplexer's data */
    p_input->pf_end( p_input );

    /* Close the input method */
    CloseThread( p_input );

    /* Release modules */
    module_Unneed( p_input->p_input_module );
}

/*****************************************************************************
 * CloseThread: close the target
 *****************************************************************************/
static void CloseThread( input_thread_t * p_input )
{
#define f p_input->p_input_module->p_functions->input.functions.input

    if( f.pf_close != NULL )
    {
        f.pf_close( p_input );
    }
#if !defined( SYS_BEOS ) && !defined( SYS_NTO )
    /* Close stream */
    else if( ( strlen( p_input->p_source ) > 10
               && !strncasecmp( p_input->p_source, "udpstream:", 10 ) )
               || ( strlen( p_input->p_source ) > 4
                     && !strncasecmp( p_input->p_source, "udp:", 4 ) ) )
    {
        NetworkClose( p_input );
    }
    else if( ( strlen( p_input->p_source ) > 5 )
               && !strncasecmp( p_input->p_source, "http:", 5 ) )
    {
        NetworkClose( p_input );
    }
#endif
    else
    {
        FileClose( p_input );
    }
#undef f
}

/*****************************************************************************
 * DestroyThread: destroy the input thread
 *****************************************************************************/
static void DestroyThread( input_thread_t * p_input )
{
    /* Update status */
    p_input->i_status = THREAD_OVER;
}

/*****************************************************************************
 * StdOpen : open standard input
 *****************************************************************************/
static void StdOpen( input_thread_t * p_input )
{
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Suppose we can control the pace - this won't work in some cases ! */
    p_input->stream.b_pace_control = 1;

    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    intf_WarnMsg( 2, "input: opening stdin" );
    p_input->i_handle = 0;
}

/*****************************************************************************
 * FileOpen : open a file descriptor
 *****************************************************************************/
static void FileOpen( input_thread_t * p_input )
{
    struct stat         stat_info;
    int                 i_stat;

    char *psz_name = p_input->p_source;

    if( ( i_stat = stat( psz_name, &stat_info ) ) == (-1) )
    {
        int i_size = strlen( psz_name );

        if( ( i_size > 8 )
            && !strncasecmp( psz_name, "dvdread:", 8 ) )
        {
            /* get rid of the 'dvdread:' stuff and try again */
            psz_name += 8;
            i_stat = stat( psz_name, &stat_info );
        }
        else if( ( i_size > 4 )
            && !strncasecmp( psz_name, "dvd:", 4 ) )
        {
            /* get rid of the 'dvd:' stuff and try again */
            psz_name += 4;
            i_stat = stat( psz_name, &stat_info );
        }
        else if( ( i_size > 4 )
                 && !strncasecmp( psz_name, "vcd:", 4 ) )
        {
            /* get rid of the 'vcd:' stuff and try again */
            psz_name += 4;
            i_stat = stat( psz_name, &stat_info );
        }
        else if( ( i_size > 5 )
                 && !strncasecmp( psz_name, "file:", 5 ) )
        {
            /* get rid of the 'file:' stuff and try again */
            psz_name += 5;
            i_stat = stat( psz_name, &stat_info );
        }

        if( i_stat == (-1) )
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
         || S_ISBLK(stat_info.st_mode) )
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

    intf_WarnMsg( 2, "input: opening file `%s'", p_input->p_source );
    if( (p_input->i_handle = open( psz_name,
                                   /*O_NONBLOCK | O_LARGEFILE*/0 )) == (-1) )
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
    intf_WarnMsg( 2, "input: closing file `%s'", p_input->p_source );

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
    char                *psz_bind = NULL;
    int                 i_server_port = 0;
    int                 i_bind_port = 0;
    int                 i_opt;
    int                 i_opt_size;
    struct sockaddr_in  sock;

    /* Get the remote server. Syntax is :
     * udp[stream]:[/][/][serveraddr[:serverport]][@[bindaddr]:[bindport]] */
    if( p_input->p_source != NULL )
    {
        char * psz_parser = p_input->p_source;
        char * psz_server_port = NULL;
        char * psz_bind_port = NULL;

        /* Skip the protocol name */
        while( *psz_parser && *psz_parser != ':' )
        {
            psz_parser++;
        }

        /* Skip the "://" part */
        while( *psz_parser && (*psz_parser == ':' || *psz_parser == '/') )
        {
            psz_parser++;
        }

        if( *psz_parser && *psz_parser != '@' )
        {
            /* Found server */
            psz_server = psz_parser;

            while( *psz_parser && *psz_parser != ':' && *psz_parser != '@' )
            {
                psz_parser++;
            }

            if( *psz_parser == ':' )
            {
                /* Found server port */
                *psz_parser = '\0'; /* Terminate server name */
                psz_parser++;
                psz_server_port = psz_parser;

                while( *psz_parser && *psz_parser != '@' )
                {
                    psz_parser++;
                }
            }
        }

        if( *psz_parser == '@' )
        {
            /* Found bind address or bind port */
            *psz_parser = '\0'; /* Terminate server port or name if necessary */
            psz_parser++;

            if( *psz_parser && *psz_parser != ':' )
            {
                /* Found bind address */
                psz_bind = psz_parser;

                while( *psz_parser && *psz_parser != ':' )
                {
                    psz_parser++;
                }
            }

            if( *psz_parser == ':' )
            {
                /* Found bind port */
                *psz_parser = '\0'; /* Terminate bind address if necessary */
                psz_parser++;

                psz_bind_port = psz_parser;
            }
        }

        /* Convert ports format */
        if( psz_server_port != NULL )
        {
            i_server_port = strtol( psz_server_port, &psz_parser, 10 );
            if( *psz_parser )
            {
                intf_ErrMsg( "input error: cannot parse server port near %s",
                             psz_parser );
                p_input->b_error = 1;
                return;
            }
        }

        if( psz_bind_port != NULL )
        {
            i_bind_port = strtol( psz_bind_port, &psz_parser, 10 );
            if( *psz_parser )
            {
                intf_ErrMsg( "input error: cannot parse bind port near %s",
                             psz_parser );
                p_input->b_error = 1;
                return;
            }
        }
    }
    else
    {
        /* This is required or NetworkClose will never be called */
        p_input->p_source = "ts: network input";
    }

    /* Check that we got a valid port */
    if( i_bind_port == 0 )
    {
        i_bind_port = config_GetIntVariable( INPUT_PORT_VAR );
    }

    intf_WarnMsg( 2, "input: server=%s:%d local=%s:%d",
                     psz_server, i_server_port, psz_bind, i_bind_port );

    /* Open a SOCK_DGRAM (UDP) socket, in the AF_INET domain, automatic (0)
     * protocol */
    p_input->i_handle = socket( AF_INET, SOCK_DGRAM, 0 );
    if( p_input->i_handle == -1 )
    {
        intf_ErrMsg( "input error: can't create socket (%s)", strerror(errno) );
        p_input->b_error = 1;
        return;
    }

    /* We may want to reuse an already used socket */
    i_opt = 1;
    if( setsockopt( p_input->i_handle, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
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
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        intf_WarnMsg( 1, "input warning: can't configure socket (SO_RCVBUF: %s)", 
                         strerror(errno));
    }

    /* Check if we really got what we have asked for, because Linux, etc.
     * will silently limit the max buffer size to net.core.rmem_max which
     * is typically only 65535 bytes */
    i_opt = 0;
    i_opt_size = sizeof( i_opt );
    if( getsockopt( p_input->i_handle, SOL_SOCKET, SO_RCVBUF,
                    (void*) &i_opt, &i_opt_size ) == -1 )
    {
        intf_WarnMsg( 1, "input warning: can't query socket (SO_RCVBUF: %s)", 
                         strerror(errno));
    }
    else if( i_opt < 0x80000 )
    {
        intf_WarnMsg( 1, "input warning: socket buffer size is 0x%x"
                         " instead of 0x%x", i_opt, 0x80000 );
    }

    /* Build the local socket */

/* As we have a problem with multicast under win32, let's bind on INADDR_ANY */
    
#ifdef WIN32
    if ( network_BuildAddr( &sock, NULL, i_bind_port ) == -1 )
#else    
    if ( network_BuildAddr( &sock, psz_bind, i_bind_port ) == -1 )
#endif
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
        intf_ErrMsg( "input error: can't bind socket (%s)", strerror(errno) );
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }

    /* Allow broadcast reception if we bound on INADDR_ANY */
    if( psz_bind == NULL )
    {
        i_opt = 1;
        if( setsockopt( p_input->i_handle, SOL_SOCKET, SO_BROADCAST,
                        (void*) &i_opt, sizeof( i_opt ) ) == -1 )
        {
            intf_WarnMsg( 1, "input warning: can't configure socket (SO_BROADCAST: %s)", 
                             strerror(errno));
        }
    }

    /* Join the multicast group if the socket is a multicast address */
#ifndef IN_MULTICAST
#   define IN_MULTICAST(a)         IN_CLASSD(a)
#endif

#ifdef WIN32
    if( IN_MULTICAST( ntohl( inet_addr (psz_bind) ) ) )
    {
        struct ip_mreq imr;
        imr.imr_interface.s_addr = INADDR_ANY;
        imr.imr_multiaddr.s_addr = inet_addr (psz_bind) ;
#else
    if( IN_MULTICAST( ntohl(sock.sin_addr.s_addr) ) )
    {
        struct ip_mreq imr;

        imr.imr_interface.s_addr = INADDR_ANY;
        imr.imr_multiaddr.s_addr = sock.sin_addr.s_addr;
#endif
        if( setsockopt( p_input->i_handle, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                        (char*)&imr, sizeof(struct ip_mreq) ) == -1 )
        {
            intf_ErrMsg( "input error: failed to join IP multicast group (%s)",
                         strerror(errno) );
            close( p_input->i_handle );
            p_input->b_error = 1;
            return;
        }
    }
    
    if( psz_server != NULL )
    {
        /* Build socket for remote connection */
        if ( network_BuildAddr( &sock, psz_server, i_server_port ) == -1 )
        {
            intf_ErrMsg( "input error: can't build remote address" );
            close( p_input->i_handle );
            p_input->b_error = 1;
            return;
        }

        /* Connect the socket */
        if( connect( p_input->i_handle, (struct sockaddr *) &sock,
                     sizeof( sock ) ) == (-1) )
        {
            intf_ErrMsg( "input error: can't connect socket (%s)", 
                         strerror(errno) );
            close( p_input->i_handle );
            p_input->b_error = 1;
            return;
        }
    }

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
    intf_WarnMsg( 2, "input: closing network target `%s'", p_input->p_source );

    close( p_input->i_handle );
}

/*****************************************************************************
 * HTTPOpen : make an HTTP request
 *****************************************************************************/
static void HTTPOpen( input_thread_t * p_input )
{
    char                *psz_server = NULL;
    char                *psz_path = NULL;
    char                *psz_proxy;
    int                 i_port = 0;
    int                 i_opt;
    struct sockaddr_in  sock;
    char                psz_buffer[256];

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
            while( *psz_port && *psz_port != ':' && *psz_port != '/' )
            {
                psz_port++;
            }

            /* Found a port name */
            if( *psz_port )
            {
                if( *psz_port == ':' )
                {
                    /* Replace ':' with '\0' */
                    *psz_port = '\0';
                    psz_port++;
                }

                psz_path = psz_port;
                while( *psz_path && *psz_path != '/' )
                {
                    psz_path++;
                }

                if( *psz_path )
                {
                    *psz_path = '\0';
                    psz_path++;
                }
                else
                {
                    psz_path = NULL;
                }

                if( *psz_port != '\0' )
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
        intf_ErrMsg( "input error: No server given" );
        p_input->b_error = 1;
        return;
    }

    /* Check that we got a valid port */
    if( i_port == 0 )
    {
        i_port = 80; /* FIXME */
    }

    intf_WarnMsg( 2, "input: server=%s port=%d path=%s", psz_server,
                  i_port, psz_path );

    /* Open a SOCK_STREAM (TCP) socket, in the AF_INET domain, automatic (0)
     *      * protocol */
    p_input->i_handle = socket( AF_INET, SOCK_STREAM, 0 );
    if( p_input->i_handle == -1 )
    {
        intf_ErrMsg( "input error: can't create socket (%s)", strerror(errno) );        p_input->b_error = 1;
        return;
    }

    /* We may want to reuse an already used socket */
    i_opt = 1;
    if( setsockopt( p_input->i_handle, SOL_SOCKET, SO_REUSEADDR,
                    (void *) &i_opt, sizeof( i_opt ) ) == -1 )
    {
        intf_ErrMsg( "input error: can't configure socket (SO_REUSEADDR: %s)",
                     strerror(errno));
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }

    /* Check proxy */
    if( (psz_proxy = getenv( "http_proxy" )) != NULL )
    {
        /* http://myproxy.mydomain:myport/ */
        int                 i_proxy_port = 0;

        /* Skip the protocol name */
        while( *psz_proxy && *psz_proxy != ':' )
        {
            psz_proxy++;
        }

        /* Skip the "://" part */
        while( *psz_proxy && (*psz_proxy == ':' || *psz_proxy == '/') )
        {
            psz_proxy++;
        }

        /* Found a proxy name */
        if( *psz_proxy )
        {
            char *psz_port = psz_proxy;

            /* Skip the hostname part */
            while( *psz_port && *psz_port != ':' && *psz_port != '/' )
            {
                psz_port++;
            }

            /* Found a port name */
            if( *psz_port )
            {
                char * psz_junk;

                /* Replace ':' with '\0' */
                *psz_port = '\0';
                psz_port++;

                psz_junk = psz_port;
                while( *psz_junk && *psz_junk != '/' )
                {
                    psz_junk++;
                }

                if( *psz_junk )
                {
                    *psz_junk = '\0';
                }

                if( *psz_port != '\0' )
                {
                    i_proxy_port = atoi( psz_port );
                }
            }
        }
        else
        {
            intf_ErrMsg( "input error: http_proxy environment variable is invalid !" );
            close( p_input->i_handle );
            p_input->b_error = 1;
            return;
        }

        /* Build socket for proxy connection */
        if ( network_BuildAddr( &sock, psz_proxy, i_proxy_port ) == -1 )
        {
            intf_ErrMsg( "input error: can't build remote address" );
            close( p_input->i_handle );
            p_input->b_error = 1;
            return;
        }
    }
    else
    {
        /* No proxy, direct connection */
        if ( network_BuildAddr( &sock, psz_server, i_port ) == -1 )
        {
            intf_ErrMsg( "input error: can't build remote address" );
            close( p_input->i_handle );
            p_input->b_error = 1;
            return;
        }
    }

    /* Connect the socket */
    if( connect( p_input->i_handle, (struct sockaddr *) &sock,
                 sizeof( sock ) ) == (-1) )
    {
        intf_ErrMsg( "input error: can't connect socket (%s)",
                     strerror(errno) );
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }

    p_input->stream.b_seekable = 0;
    p_input->stream.b_pace_control = 1; /* TCP/IP... */

#   define HTTP_USERAGENT "User-Agent: " COPYRIGHT_MESSAGE "\r\n"
#   define HTTP_END       "\r\n"

    /* Prepare GET ... */
    if( psz_proxy != NULL )
    {
        snprintf( psz_buffer, sizeof(psz_buffer),
                  "GET http://%s:%d/%s HTTP/1.0\r\n"
                  HTTP_USERAGENT HTTP_END,
                  psz_server, i_port, psz_path );
    }
    else
    {
        snprintf( psz_buffer, sizeof(psz_buffer),
                  "GET /%s HTTP/1.0\r\nHost: %s\r\n"
                  HTTP_USERAGENT HTTP_END,
                  psz_path, psz_server );
    }
    psz_buffer[sizeof(psz_buffer) - 1] = '\0';

    /* Send GET ... */
    if( write( p_input->i_handle, psz_buffer, strlen( psz_buffer ) ) == (-1) )
    {
        intf_ErrMsg( "input error: can't send request (%s)", strerror(errno) );
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }

    /* Read HTTP header - this is gonna be fun with plug-ins which do not
     * use p_input->p_stream :-( */
    if( (p_input->p_stream = fdopen( p_input->i_handle, "r+" )) == NULL )
    {
        intf_ErrMsg( "input error: can't reopen socket (%s)", strerror(errno) );
        close( p_input->i_handle );
        p_input->b_error = 1;
        return;
    }

    while( !feof( p_input->p_stream ) && !ferror( p_input->p_stream ) )
    {
        if( fgets( psz_buffer, sizeof(psz_buffer), p_input->p_stream ) == NULL
             || *psz_buffer == '\r' || *psz_buffer == '\0' )
        {
            break;
        }
        /* FIXME : check Content-Type one day */
    }

    intf_WarnMsg( 3, "input: successfully opened HTTP mode" );
}

#endif /* !defined( SYS_BEOS ) && !defined( SYS_NTO ) */

