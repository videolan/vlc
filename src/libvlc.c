/*****************************************************************************
 * libvlc.c: main libvlc source
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: libvlc.c,v 1.20 2002/08/04 20:04:11 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * Pretend we are a builtin module
 *****************************************************************************/
#define MODULE_NAME main
#define __BUILTIN__

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                                                /* free() */
#include <signal.h>                               /* SIGHUP, SIGINT, SIGKILL */

#include <vlc/vlc.h>

#ifdef HAVE_GETOPT_LONG
#   ifdef HAVE_GETOPT_H
#       include <getopt.h>                                       /* getopt() */
#   endif
#else
#   include "GNUgetopt/getopt.h"
#endif

#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif

#include "vlc_cpu.h"                                        /* CPU detection */
#include "os_specific.h"

#include "netutils.h"                                 /* network_ChannelJoin */

#include "stream_control.h"
#include "input_ext-intf.h"

#include "vlc_playlist.h"
#include "interface.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"

#include "libvlc.h"

/*****************************************************************************
 * The evil global variables. We handle them with care, don't worry.
 *****************************************************************************/

/* This global lock is used for critical sections - don't abuse it! */
static vlc_mutex_t global_lock;
void *             p_global_data;

/* A list of all the currently allocated vlc objects */
static int volatile i_vlc = 0;
static int volatile i_unique = 0;
static vlc_t ** volatile pp_vlc = NULL;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  GetFilenames  ( vlc_t *, int, char *[] );
static void Usage         ( vlc_t *, const char *psz_module_name );
static void ListModules   ( vlc_t * );
static void Version       ( void );

#ifndef WIN32
static void InitSignalHandler   ( void );
static void SimpleSignalHandler ( int i_signal );
static void FatalSignalHandler  ( int i_signal );
#endif

#ifdef WIN32
static void ShowConsole   ( void );
#endif

/*****************************************************************************
 * vlc_create: allocate a vlc_t structure, and initialize libvlc if needed.
 *****************************************************************************
 * This function allocates a vlc_t structure and returns NULL in case of
 * failure. Also, the thread system and the signal handlers are initialized.
 *****************************************************************************/
vlc_error_t vlc_create( void )
{
    vlc_t * p_vlc = vlc_create_r();
    return p_vlc ? VLC_SUCCESS : VLC_EGENERIC;
}

vlc_t * vlc_create_r( void )
{
    vlc_t * p_vlc = NULL;

    /* Allocate the main structure */
    p_vlc = vlc_object_create( p_vlc, VLC_OBJECT_ROOT );
    if( p_vlc == NULL )
    {
        return NULL;
    }

    p_vlc->psz_object_name = "root";

    p_vlc->p_global_lock = &global_lock;
    p_vlc->pp_global_data = &p_global_data;

    p_vlc->b_verbose = VLC_FALSE;
    p_vlc->b_quiet = VLC_FALSE; /* FIXME: delay message queue output! */

    /* Initialize the threads system */
    vlc_threads_init( p_vlc );

    /* Initialize mutexes */
    vlc_mutex_init( p_vlc, &p_vlc->config_lock );
    vlc_mutex_init( p_vlc, &p_vlc->structure_lock );

    /* Set signal handling policy for all threads */
#ifndef WIN32
    InitSignalHandler( );
#endif

    /* Store our newly allocated structure in the global list */
    vlc_mutex_lock( p_vlc->p_global_lock );
    pp_vlc = realloc( pp_vlc, (i_vlc+1) * sizeof( vlc_t * ) );
    pp_vlc[ i_vlc ] = p_vlc;
    i_vlc++;
    p_vlc->i_unique = i_unique;
    i_unique++;
    vlc_mutex_unlock( p_vlc->p_global_lock );

    /* Update the handle status */
    p_vlc->i_status = VLC_STATUS_CREATED;

    return p_vlc;
}

/*****************************************************************************
 * vlc_init: initialize a vlc_t structure.
 *****************************************************************************
 * This function initializes a previously allocated vlc_t structure:
 *  - CPU detection
 *  - gettext initialization
 *  - message queue, module bank and playlist initialization
 *  - configuration and commandline parsing
 *****************************************************************************/
vlc_error_t vlc_init( int i_argc, char *ppsz_argv[] )
{
    return vlc_init_r( ( i_vlc == 1 ) ? *pp_vlc : NULL, i_argc, ppsz_argv );
}

vlc_error_t vlc_init_r( vlc_t *p_vlc, int i_argc, char *ppsz_argv[] )
{
    char p_capabilities[200];
    char *p_tmp;
    module_t        *p_help_module;
    playlist_t      *p_playlist;

    /* Check that the handle is valid */
    if( !p_vlc || p_vlc->i_status != VLC_STATUS_CREATED )
    {
        fprintf( stderr, "error: invalid status (!CREATED)\n" );
        return VLC_ESTATUS;
    }

    fprintf( stderr, COPYRIGHT_MESSAGE "\n" );

    /* Guess what CPU we have */
    p_vlc->i_cpu = CPUCapabilities( p_vlc );

    /*
     * Support for gettext
     */
#if defined( ENABLE_NLS ) && defined ( HAVE_GETTEXT )
#   if defined( HAVE_LOCALE_H ) && defined( HAVE_LC_MESSAGES )
    if( !setlocale( LC_MESSAGES, "" ) )
    {
        fprintf( stderr, "warning: unsupported locale settings\n" );
    }

    setlocale( LC_CTYPE, "" );
#   endif

    if( !bindtextdomain( PACKAGE, LOCALEDIR ) )
    {
        fprintf( stderr, "warning: no domain %s in directory %s\n",
                 PACKAGE, LOCALEDIR );
    }

    textdomain( PACKAGE );
#endif

    /*
     * System specific initialization code
     */
    system_Init( p_vlc, &i_argc, ppsz_argv );

    /*
     * Initialize message queue
     */
    msg_Create( p_vlc );

    /* Get the executable name (similar to the basename command) */
    if( i_argc > 0 )
    {
        p_vlc->psz_object_name = p_tmp = ppsz_argv[ 0 ];
        while( *p_tmp )
        {
            if( *p_tmp == '/' ) p_vlc->psz_object_name = ++p_tmp;
            else ++p_tmp;
        }
    }
    else
    {
        p_vlc->psz_object_name = "vlc";
    }

    /* Announce who we are */
    msg_Dbg( p_vlc, COPYRIGHT_MESSAGE );
    msg_Dbg( p_vlc, "libvlc was configured with %s", CONFIGURE_LINE );

    /*
     * Initialize the module bank and and load the configuration of the main
     * module. We need to do this at this stage to be able to display a short
     * help if required by the user. (short help == main module options)
     */
    module_InitBank( p_vlc );
    module_LoadMain( p_vlc );

    /* Hack: insert the help module here */
    p_help_module = vlc_object_create( p_vlc, VLC_OBJECT_MODULE );
    if( p_help_module == NULL )
    {
        module_EndBank( p_vlc );
        msg_Destroy( p_vlc );
        return VLC_EGENERIC;
    }
    p_help_module->psz_object_name = "help";
    config_Duplicate( p_help_module, p_help_config );
    p_help_module->next = p_vlc->p_module_bank->first;
    p_vlc->p_module_bank->first = p_help_module;
    /* End hack */

    if( config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_TRUE ) )
    {
        p_vlc->p_module_bank->first = p_help_module->next;
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        module_EndBank( p_vlc );
        msg_Destroy( p_vlc );
        return VLC_EGENERIC;
    }

    /* Check for short help option */
    if( config_GetInt( p_vlc, "help" ) )
    {
        fprintf( stderr, _("Usage: %s [options] [parameters] [file]...\n"),
                         p_vlc->psz_object_name );

        Usage( p_vlc, "help" );
        Usage( p_vlc, "main" );
        p_vlc->p_module_bank->first = p_help_module->next;
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        module_EndBank( p_vlc );
        msg_Destroy( p_vlc );
        return VLC_EEXIT;
    }

    /* Check for version option */
    if( config_GetInt( p_vlc, "version" ) )
    {
        Version();
        p_vlc->p_module_bank->first = p_help_module->next;
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        module_EndBank( p_vlc );
        msg_Destroy( p_vlc );
        return VLC_EEXIT;
    }

    /* Hack: remove the help module here */
    p_vlc->p_module_bank->first = p_help_module->next;
    /* End hack */

    /*
     * Load the builtins and plugins into the module_bank.
     * We have to do it before config_Load*() because this also gets the
     * list of configuration options exported by each module and loads their
     * default values.
     */
    module_LoadBuiltins( p_vlc );
    module_LoadPlugins( p_vlc );
    msg_Dbg( p_vlc, "module bank initialized, found %i modules",
                    p_vlc->p_module_bank->i_count );

    /* Hack: insert the help module here */
    p_help_module->next = p_vlc->p_module_bank->first;
    p_vlc->p_module_bank->first = p_help_module;
    /* End hack */

    /* Check for help on modules */
    if( (p_tmp = config_GetPsz( p_vlc, "module" )) )
    {
        Usage( p_vlc, p_tmp );
        free( p_tmp );
        p_vlc->p_module_bank->first = p_help_module->next;
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        module_EndBank( p_vlc );
        msg_Destroy( p_vlc );
        return VLC_EGENERIC;
    }

    /* Check for long help option */
    if( config_GetInt( p_vlc, "longhelp" ) )
    {
        Usage( p_vlc, NULL );
        p_vlc->p_module_bank->first = p_help_module->next;
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        module_EndBank( p_vlc );
        msg_Destroy( p_vlc );
        return VLC_EEXIT;
    }

    /* Check for module list option */
    if( config_GetInt( p_vlc, "list" ) )
    {
        ListModules( p_vlc );
        p_vlc->p_module_bank->first = p_help_module->next;
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        module_EndBank( p_vlc );
        msg_Destroy( p_vlc );
        return VLC_EEXIT;
    }

    /* Hack: remove the help module here */
    p_vlc->p_module_bank->first = p_help_module->next;
    config_Free( p_help_module );
    vlc_object_destroy( p_help_module );
    /* End hack */

    /*
     * Override default configuration with config file settings
     */
    p_vlc->psz_homedir = config_GetHomeDir();
    config_LoadConfigFile( p_vlc, NULL );

    /*
     * Override configuration with command line settings
     */
    if( config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_FALSE ) )
    {
#ifdef WIN32
        ShowConsole();
        /* Pause the console because it's destroyed when we exit */
        fprintf( stderr, "The command line options couldn't be loaded, check "
                 "that they are valid.\nPress the RETURN key to continue..." );
        getchar();
#endif
        module_EndBank( p_vlc );
        msg_Destroy( p_vlc );
        return VLC_EGENERIC;
    }

    /*
     * System specific configuration
     */
    system_Configure( p_vlc );

    /*
     * Output messages that may still be in the queue
     */
    p_vlc->b_verbose = config_GetInt( p_vlc, "verbose" );
    p_vlc->b_quiet = config_GetInt( p_vlc, "quiet" );
    p_vlc->b_color = config_GetInt( p_vlc, "color" );
    msg_Flush( p_vlc );

    /* p_vlc inititalization. FIXME ? */
    p_vlc->i_desync = config_GetInt( p_vlc, "desync" ) * (mtime_t)1000;
#if defined( __i386__ )
    if( !config_GetInt( p_vlc, "mmx" ) )
        p_vlc->i_cpu &= ~CPU_CAPABILITY_MMX;
    if( !config_GetInt( p_vlc, "3dn" ) )
        p_vlc->i_cpu &= ~CPU_CAPABILITY_3DNOW;
    if( !config_GetInt( p_vlc, "mmxext" ) )
        p_vlc->i_cpu &= ~CPU_CAPABILITY_MMXEXT;
    if( !config_GetInt( p_vlc, "sse" ) )
        p_vlc->i_cpu &= ~CPU_CAPABILITY_SSE;
#endif
#if defined( __powerpc__ ) || defined( SYS_DARWIN )
    if( !config_GetInt( p_vlc, "altivec" ) )
        p_vlc->i_cpu &= ~CPU_CAPABILITY_ALTIVEC;
#endif

#define PRINT_CAPABILITY( capability, string )                              \
    if( p_vlc->i_cpu & capability )                                         \
    {                                                                       \
        strncat( p_capabilities, string " ",                                \
                 sizeof(p_capabilities) - strlen(p_capabilities) );         \
        p_capabilities[sizeof(p_capabilities) - 1] = '\0';                  \
    }

    p_capabilities[0] = '\0';
    PRINT_CAPABILITY( CPU_CAPABILITY_486, "486" );
    PRINT_CAPABILITY( CPU_CAPABILITY_586, "586" );
    PRINT_CAPABILITY( CPU_CAPABILITY_PPRO, "Pentium Pro" );
    PRINT_CAPABILITY( CPU_CAPABILITY_MMX, "MMX" );
    PRINT_CAPABILITY( CPU_CAPABILITY_3DNOW, "3DNow!" );
    PRINT_CAPABILITY( CPU_CAPABILITY_MMXEXT, "MMXEXT" );
    PRINT_CAPABILITY( CPU_CAPABILITY_SSE, "SSE" );
    PRINT_CAPABILITY( CPU_CAPABILITY_ALTIVEC, "AltiVec" );
    PRINT_CAPABILITY( CPU_CAPABILITY_FPU, "FPU" );
    msg_Dbg( p_vlc, "CPU has capabilities %s", p_capabilities );

    /*
     * Choose the best memcpy module
     */
    p_vlc->p_memcpy_module = module_Need( p_vlc, "memcpy", "$memcpy" );

    if( p_vlc->p_memcpy_module == NULL )
    {
        msg_Warn( p_vlc, "no suitable memcpy module, using libc default" );
        p_vlc->pf_memcpy = memcpy;
    }

    /*
     * Initialize shared resources and libraries
     */
    if( config_GetInt( p_vlc, "network-channel" )
         && network_ChannelCreate( p_vlc ) )
    {
        /* On error during Channels initialization, switch off channels */
        msg_Warn( p_vlc,
                  "channels initialization failed, deactivating channels" );
        config_PutInt( p_vlc, "network-channel", VLC_FALSE );
    }

    /*
     * Initialize playlist and get commandline files
     */
    p_playlist = playlist_Create( p_vlc );
    if( !p_playlist )
    {
        msg_Err( p_vlc, "playlist initialization failed" );
        if( p_vlc->p_memcpy_module != NULL )
        {
            module_Unneed( p_vlc, p_vlc->p_memcpy_module );
        }
        module_EndBank( p_vlc );
        msg_Destroy( p_vlc );
        return VLC_EGENERIC;
    }

    /* Update the handle status */
    p_vlc->i_status = VLC_STATUS_STOPPED;

    /*
     * Get input filenames given as commandline arguments
     */
    GetFilenames( p_vlc, i_argc, ppsz_argv );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_run: run vlc
 *****************************************************************************
 * XXX: This function opens an interface plugin and runs it. If b_block is set
 * to 0, vlc_add_intf will return immediately and let the interface run in a
 * separate thread. If b_block is set to 1, vlc_add_intf will continue until
 * user requests to quit.
 *****************************************************************************/
vlc_error_t vlc_run( void )
{
    return vlc_run_r( ( i_vlc == 1 ) ? *pp_vlc : NULL );
}

vlc_error_t vlc_run_r( vlc_t *p_vlc )
{
    /* Check that the handle is valid */
    if( !p_vlc || p_vlc->i_status != VLC_STATUS_STOPPED )
    {
        fprintf( stderr, "error: invalid status (!STOPPED)\n" );
        return VLC_ESTATUS;
    }

    /* Update the handle status */
    p_vlc->i_status = VLC_STATUS_RUNNING;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_add_intf: add an interface
 *****************************************************************************
 * This function opens an interface plugin and runs it. If b_block is set
 * to 0, vlc_add_intf will return immediately and let the interface run in a
 * separate thread. If b_block is set to 1, vlc_add_intf will continue until
 * user requests to quit.
 *****************************************************************************/
vlc_error_t vlc_add_intf( const char *psz_module, vlc_bool_t b_block )
{
    return vlc_add_intf_r( ( i_vlc == 1 ) ? *pp_vlc : NULL,
                           psz_module, b_block );
}

vlc_error_t vlc_add_intf_r( vlc_t *p_vlc, const char *psz_module,
                                          vlc_bool_t b_block )
{
    vlc_error_t err;
    intf_thread_t *p_intf;
    char *psz_oldmodule = NULL;

    /* Check that the handle is valid */
    if( !p_vlc || p_vlc->i_status != VLC_STATUS_RUNNING )
    {
        fprintf( stderr, "error: invalid status (!RUNNING)\n" );
        return VLC_ESTATUS;
    }

    if( psz_module )
    {
        psz_oldmodule = config_GetPsz( p_vlc, "intf" );
        config_PutPsz( p_vlc, "intf", psz_module );
    }

    /* Try to create the interface */
    p_intf = intf_Create( p_vlc );

    if( psz_module )
    {
        config_PutPsz( p_vlc, "intf", psz_oldmodule );
        if( psz_oldmodule )
        {
            free( psz_oldmodule );
        }
    }

    if( p_intf == NULL )
    {
        msg_Err( p_vlc, "interface initialization failed" );
        return VLC_EGENERIC;
    }

    /* Try to run the interface */
    p_intf->b_block = b_block;
    err = intf_RunThread( p_intf );
    if( err )
    {
        vlc_object_detach_all( p_intf );
        intf_Destroy( p_intf );
        return err;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_stop: stop playing.
 *****************************************************************************
 * This function requests the interface threads to finish, waits for their
 * termination, and destroys their structure.
 *****************************************************************************/
vlc_error_t vlc_stop( void )
{
    return vlc_stop_r( ( i_vlc == 1 ) ? *pp_vlc : NULL );
}

vlc_error_t vlc_stop_r( vlc_t *p_vlc )
{
    intf_thread_t *p_intf;
    playlist_t    *p_playlist;
    vout_thread_t *p_vout;
    aout_thread_t *p_aout;

    /* Check that the handle is valid */
    if( !p_vlc || p_vlc->i_status != VLC_STATUS_RUNNING )
    {
        fprintf( stderr, "error: invalid status (!RUNNING)\n" );
        return VLC_ESTATUS;
    }

    /*
     * Ask the interfaces to stop and destroy them
     */
    msg_Dbg( p_vlc, "removing all interfaces" );
    while( (p_intf = vlc_object_find( p_vlc, VLC_OBJECT_INTF, FIND_CHILD )) )
    {
        intf_StopThread( p_intf );
        vlc_object_detach_all( p_intf );
        vlc_object_release( p_intf );
        intf_Destroy( p_intf );
    }

    /*
     * Free playlists
     */
    msg_Dbg( p_vlc, "removing all playlists" );
    while( (p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST,
                                          FIND_CHILD )) )
    {
        vlc_object_detach_all( p_playlist );
        vlc_object_release( p_playlist );
        playlist_Destroy( p_playlist );
    }

    /*
     * Free video outputs
     */
    msg_Dbg( p_vlc, "removing all video outputs" );
    while( (p_vout = vlc_object_find( p_vlc, VLC_OBJECT_VOUT, FIND_CHILD )) )
    {
        vlc_object_detach_all( p_vout );
        vlc_object_release( p_vout );
        vout_DestroyThread( p_vout );
    }

    /*
     * Free audio outputs
     */
    msg_Dbg( p_vlc, "removing all audio outputs" );
    while( (p_aout = vlc_object_find( p_vlc, VLC_OBJECT_AOUT, FIND_CHILD )) )
    {
        vlc_object_detach_all( p_aout );
        vlc_object_release( p_aout );
        aout_DestroyThread( p_aout );
    }

    /* Update the handle status */
    p_vlc->i_status = VLC_STATUS_STOPPED;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_end: uninitialize everything.
 *****************************************************************************
 * This function uninitializes every vlc component that was activated in
 * vlc_init: audio and video outputs, playlist, module bank and message queue.
 *****************************************************************************/
vlc_error_t vlc_end( void )
{
    return vlc_end_r( ( i_vlc == 1 ) ? *pp_vlc : NULL );
}

vlc_error_t vlc_end_r( vlc_t *p_vlc )
{
    /* Check that the handle is valid */
    if( !p_vlc || p_vlc->i_status != VLC_STATUS_STOPPED )
    {
        fprintf( stderr, "error: invalid status (!STOPPED)\n" );
        return VLC_ESTATUS;
    }

    /*
     * Go back into channel 0 which is the network
     */
    if( config_GetInt( p_vlc, "network-channel" ) && p_vlc->p_channel )
    {
        network_ChannelJoin( p_vlc, COMMON_CHANNEL );
    }

    /*
     * Free allocated memory
     */
    if( p_vlc->p_memcpy_module != NULL )
    {
        module_Unneed( p_vlc, p_vlc->p_memcpy_module );
    }

    free( p_vlc->psz_homedir );

    /*
     * Free module bank
     */
    module_EndBank( p_vlc );

    /*
     * System specific cleaning code
     */
    system_End( p_vlc );

    /*
     * Terminate messages interface and program
     */
    msg_Destroy( p_vlc );

    /* Update the handle status */
    p_vlc->i_status = VLC_STATUS_CREATED;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_destroy: free allocated resources.
 *****************************************************************************
 * This function frees the previously allocated vlc_t structure.
 *****************************************************************************/
vlc_error_t vlc_destroy( void )
{
    return vlc_destroy_r( ( i_vlc == 1 ) ? *pp_vlc : NULL );
}

vlc_error_t vlc_destroy_r( vlc_t *p_vlc )
{
    int i_index;

    /* Check that the handle is valid */
    if( !p_vlc || p_vlc->i_status != VLC_STATUS_CREATED )
    {
        fprintf( stderr, "error: invalid status (!CREATED)\n" );
        return VLC_ESTATUS;
    }

    /* Update the handle status, just in case */
    p_vlc->i_status = VLC_STATUS_NONE;

    /* Remove our structure from the global list */
    vlc_mutex_lock( p_vlc->p_global_lock );
    for( i_index = 0 ; i_index < i_vlc ; i_index++ )
    {
        if( pp_vlc[ i_index ] == p_vlc )
        {
            break;
        }
    }

    if( i_index == i_vlc )
    {
        fprintf( stderr, "error: trying to unregister %p which is not in "
                         "the list\n", p_vlc );
        vlc_mutex_unlock( p_vlc->p_global_lock );
        vlc_object_destroy( p_vlc );
        return VLC_EGENERIC;
    }

    for( i_index++ ; i_index < i_vlc ; i_index++ )
    {
        pp_vlc[ i_index - 1 ] = pp_vlc[ i_index ];
    }

    i_vlc--;
    if( i_vlc )
    {
        pp_vlc = realloc( pp_vlc, i_vlc * sizeof( vlc_t * ) );
    }
    else
    {
        free( pp_vlc );
        pp_vlc = NULL;
    }
    vlc_mutex_unlock( p_vlc->p_global_lock );

    /* Stop thread system: last one out please shut the door! */
    vlc_threads_end( p_vlc );

    /* Destroy mutexes */
    vlc_mutex_destroy( &p_vlc->structure_lock );
    vlc_mutex_destroy( &p_vlc->config_lock );

    vlc_object_destroy( p_vlc );

    return VLC_SUCCESS;
}

vlc_status_t vlc_status( void )
{
    return vlc_status_r( ( i_vlc == 1 ) ? *pp_vlc : NULL );
}

vlc_status_t vlc_status_r( vlc_t *p_vlc )
{
    if( !p_vlc )
    {
        return VLC_STATUS_NONE;
    }

    return p_vlc->i_status;
}

vlc_error_t vlc_add_target( const char *psz_target, int i_mode, int i_pos )
{
    return vlc_add_target_r( ( i_vlc == 1 ) ? *pp_vlc : NULL,
                             psz_target, i_mode, i_pos );
}

vlc_error_t vlc_add_target_r( vlc_t *p_vlc, const char *psz_target,
                                            int i_mode, int i_pos )
{
    vlc_error_t err;
    playlist_t *p_playlist;

    if( !p_vlc || ( p_vlc->i_status != VLC_STATUS_STOPPED
                     && p_vlc->i_status != VLC_STATUS_RUNNING ) )
    {
        fprintf( stderr, "error: invalid status (!STOPPED&&!RUNNING)\n" );
        return VLC_ESTATUS;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        msg_Dbg( p_vlc, "no playlist present, creating one" );
        p_playlist = playlist_Create( p_vlc );

        if( p_playlist == NULL )
        {
            return VLC_EGENERIC;
        }

        vlc_object_yield( p_playlist );
    }

    err = playlist_Add( p_playlist, psz_target, i_mode, i_pos );

    vlc_object_release( p_playlist );

    return err;
}

/* following functions are local */

/*****************************************************************************
 * GetFilenames: parse command line options which are not flags
 *****************************************************************************
 * Parse command line for input files.
 *****************************************************************************/
static int GetFilenames( vlc_t *p_vlc, int i_argc, char *ppsz_argv[] )
{
    int i_opt;

    /* We assume that the remaining parameters are filenames */
    for( i_opt = optind; i_opt < i_argc; i_opt++ )
    {
        vlc_add_target_r( p_vlc, ppsz_argv[ i_opt ],
                          PLAYLIST_APPEND, PLAYLIST_END );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Usage: print program usage
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static void Usage( vlc_t *p_this, const char *psz_module_name )
{
#define FORMAT_STRING "      --%s%s%s%s%s%s%s %s%s\n"
    /* option name -------------'     | | | |  | |
     * <bra --------------------------' | | |  | |
     * option type or "" ---------------' | |  | |
     * ket> ------------------------------' |  | |
     * padding spaces ----------------------'  | |
     * comment --------------------------------' |
     * comment suffix ---------------------------'
     *
     * The purpose of having bra and ket is that we might i18n them as well.
     */
#define LINE_START 8
#define PADDING_SPACES 25
    module_t *p_module;
    module_config_t *p_item;
    char psz_spaces[PADDING_SPACES+LINE_START+1];
    char psz_format[sizeof(FORMAT_STRING)];

    memset( psz_spaces, ' ', PADDING_SPACES+LINE_START );
    psz_spaces[PADDING_SPACES+LINE_START] = '\0';

    strcpy( psz_format, FORMAT_STRING );

#ifdef WIN32
    ShowConsole();
#endif

    /* Enumerate the config for each module */
    for( p_module = p_this->p_vlc->p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        vlc_bool_t b_help_module = !strcmp( "help", p_module->psz_object_name );

        if( psz_module_name && strcmp( psz_module_name,
                                       p_module->psz_object_name ) )
        {
            continue;
        }

        /* Ignore modules without config options */
        if( !p_module->i_config_items )
        {
            continue;
        }

        /* Print module name */
        fprintf( stderr, _("%s module options:\n\n"),
                         p_module->psz_object_name );

        for( p_item = p_module->p_config;
             p_item->i_type != CONFIG_HINT_END;
             p_item++ )
        {
            char *psz_bra = NULL, *psz_type = NULL, *psz_ket = NULL;
            char *psz_suf = "", *psz_prefix = NULL;
            int i;

            switch( p_item->i_type )
            {
            case CONFIG_HINT_CATEGORY:
            case CONFIG_HINT_USAGE:
                fprintf( stderr, " %s\n", p_item->psz_text );
                break;

            case CONFIG_ITEM_STRING:
            case CONFIG_ITEM_FILE:
            case CONFIG_ITEM_MODULE: /* We could also have "=<" here */
                if( !p_item->ppsz_list )
                {
                    psz_bra = " <"; psz_type = _("string"); psz_ket = ">";
                    break;
                }
                else
                {
                    psz_bra = " [";
                    psz_type = malloc( 1000 );
                    memset( psz_type, 0, 1000 );
                    for( i=0; p_item->ppsz_list[i]; i++ )
                    {
                        strcat( psz_type, p_item->ppsz_list[i] );
                        strcat( psz_type, "|" );
                    }
                    psz_type[ strlen( psz_type ) - 1 ] = '\0';
                    psz_ket = "]";
                    break;
                }
            case CONFIG_ITEM_INTEGER:
                psz_bra = " <"; psz_type = _("integer"); psz_ket = ">";
                break;
            case CONFIG_ITEM_FLOAT:
                psz_bra = " <"; psz_type = _("float"); psz_ket = ">";
                break;
            case CONFIG_ITEM_BOOL:
                psz_bra = ""; psz_type = ""; psz_ket = "";
                if( !b_help_module )
                {
                    psz_suf = p_item->i_value ? _(" (default enabled)") :
                                                _(" (default disabled)");
                }
                break;
            }

            /* Add short option */
            if( p_item->i_short )
            {
                psz_format[2] = '-';
                psz_format[3] = p_item->i_short;
                psz_format[4] = ',';
            }
            else
            {
                psz_format[2] = ' ';
                psz_format[3] = ' ';
                psz_format[4] = ' ';
            }

            if( psz_type )
            {
                i = PADDING_SPACES - strlen( p_item->psz_name )
                     - strlen( psz_bra ) - strlen( psz_type )
                     - strlen( psz_ket ) - 1;
                if( p_item->i_type == CONFIG_ITEM_BOOL
                     && !b_help_module )
                {
                    vlc_bool_t b_dash = VLC_FALSE;
                    psz_prefix = p_item->psz_name;
                    while( *psz_prefix )
                    {
                        if( *psz_prefix++ == '-' )
                        {
                            b_dash = VLC_TRUE;
                            break;
                        }
                    }

                    if( b_dash )
                    {
                        psz_prefix = ", --no-";
                        i -= strlen( p_item->psz_name ) + strlen( ", --no-" );
                    }
                    else
                    {
                        psz_prefix = ", --no";
                        i -= strlen( p_item->psz_name ) + strlen( ", --no" );
                    }
                }

                if( i < 0 )
                {
                    i = 0;
                    psz_spaces[i] = '\n';
                }
                else
                {
                    psz_spaces[i] = '\0';
                }

                if( p_item->i_type == CONFIG_ITEM_BOOL &&
                    !b_help_module )
                {
                    fprintf( stderr, psz_format, p_item->psz_name, psz_prefix,
                             p_item->psz_name, psz_bra, psz_type, psz_ket,
                             psz_spaces, p_item->psz_text, psz_suf );
                }
                else
                {
                    fprintf( stderr, psz_format, p_item->psz_name, "", "",
                             psz_bra, psz_type, psz_ket, psz_spaces,
                             p_item->psz_text, psz_suf );
                }
                psz_spaces[i] = ' ';
                if ( p_item->ppsz_list )
                {
                    free( psz_type );
                }
            }
        }

        fprintf( stderr, "\n" );

    }

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
        fprintf( stderr, _("\nPress the RETURN key to continue...\n") );
        getchar();
#endif
}

/*****************************************************************************
 * ListModules: list the available modules with their description
 *****************************************************************************
 * Print a list of all available modules (builtins and plugins) and a short
 * description for each one.
 *****************************************************************************/
static void ListModules( vlc_t *p_this )
{
    module_t *p_module;
    char psz_spaces[22];

    memset( psz_spaces, ' ', 22 );

#ifdef WIN32
    ShowConsole();
#endif

    /* Usage */
    fprintf( stderr, _("Usage: %s [options] [parameters] [file]...\n\n"),
                     p_this->p_vlc->psz_object_name );

    fprintf( stderr, _("[module]              [description]\n") );

    /* Enumerate each module */
    for( p_module = p_this->p_vlc->p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        int i;

        /* Nasty hack, but right now I'm too tired to think about a nice
         * solution */
        i = 22 - strlen( p_module->psz_object_name ) - 1;
        if( i < 0 ) i = 0;
        psz_spaces[i] = 0;

        fprintf( stderr, "  %s%s %s\n", p_module->psz_object_name, psz_spaces,
                  p_module->psz_longname );

        psz_spaces[i] = ' ';

    }

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
        fprintf( stderr, _("\nPress the RETURN key to continue...\n") );
        getchar();
#endif
}

/*****************************************************************************
 * Version: print complete program version
 *****************************************************************************
 * Print complete program version and build number.
 *****************************************************************************/
static void Version( void )
{
#ifdef WIN32
    ShowConsole();
#endif

    fprintf( stderr, VERSION_MESSAGE "\n" );
    fprintf( stderr,
      _("This program comes with NO WARRANTY, to the extent permitted by "
        "law.\nYou may redistribute it under the terms of the GNU General "
        "Public License;\nsee the file named COPYING for details.\n"
        "Written by the VideoLAN team at Ecole Centrale, Paris.\n") );

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
    fprintf( stderr, _("\nPress the RETURN key to continue...\n") );
    getchar();
#endif
}

/*****************************************************************************
 * ShowConsole: On Win32, create an output console for debug messages
 *****************************************************************************
 * This function is useful only on Win32.
 *****************************************************************************/
#ifdef WIN32 /*  */
static void ShowConsole( void )
{
    AllocConsole();
    freopen( "CONOUT$", "w", stdout );
    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );
    return;
}
#endif

#ifndef WIN32
/*****************************************************************************
 * InitSignalHandler: system signal handler initialization
 *****************************************************************************
 * Set the signal handlers. SIGTERM is not intercepted, because we need at
 * at least a method to kill the program when all other methods failed, and
 * when we don't want to use SIGKILL.
 *****************************************************************************/
static void InitSignalHandler( void )
{
    /* Termination signals */
    signal( SIGINT,  FatalSignalHandler );
    signal( SIGHUP,  FatalSignalHandler );
    signal( SIGQUIT, FatalSignalHandler );

    /* Other signals */
    signal( SIGALRM, SimpleSignalHandler );
    signal( SIGPIPE, SimpleSignalHandler );
}

/*****************************************************************************
 * SimpleSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when a non fatal signal is received by the program.
 *****************************************************************************/
static void SimpleSignalHandler( int i_signal )
{
    int i_index;

    /* Acknowledge the signal received and warn all the p_vlc structures */
    vlc_mutex_lock( &global_lock );
    for( i_index = 0 ; i_index < i_vlc ; i_index++ )
    {
        msg_Warn( pp_vlc[ i_index ], "ignoring signal %d", i_signal );
    }
    vlc_mutex_unlock( &global_lock );
}

/*****************************************************************************
 * FatalSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when a fatal signal is received by the program.
 * It tries to end the program in a clean way.
 *****************************************************************************/
static void FatalSignalHandler( int i_signal )
{
    static mtime_t abort_time = 0;
    static volatile vlc_bool_t b_die = VLC_FALSE;
    int i_index;

    /* Once a signal has been trapped, the termination sequence will be
     * armed and following signals will be ignored to avoid sending messages
     * to an interface having been destroyed */

    if( !b_die )
    {
        b_die = VLC_TRUE;
        abort_time = mdate();

        fprintf( stderr, "signal %d received, terminating libvlc - do it "
                         "again in case your process gets stuck\n", i_signal );

        /* Try to terminate everything - this is done by requesting the end of
         * all the p_vlc structures */
        for( i_index = 0 ; i_index < i_vlc ; i_index++ )
        {
            /* Acknowledge the signal received */
            pp_vlc[ i_index ]->b_die = VLC_TRUE;
        }
    }
    else if( mdate() > abort_time + 1000000 )
    {
        /* If user asks again 1 second later, die badly */
        signal( SIGINT,  SIG_IGN );
        signal( SIGHUP,  SIG_IGN );
        signal( SIGQUIT, SIG_IGN );

        fprintf( stderr, "user insisted too much, dying badly\n" );

        exit( 1 );
    }
}
#endif

