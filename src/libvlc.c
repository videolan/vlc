/*****************************************************************************
 * libvlc.c: main libvlc source
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: libvlc.c,v 1.35 2002/10/03 18:56:09 sam Exp $
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
#define MODULE_PATH main
#define __BUILTIN__

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                                                /* free() */

#include <vlc/vlc.h>

#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#ifdef WIN32                       /* optind, getopt(), included in unistd.h */
#   include "extras/GNUgetopt/getopt.h"
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
 * The evil global variable. We handle it with care, don't worry.
 *****************************************************************************/
static libvlc_t libvlc;

//#define GLOBAL_VLC NULL
#define GLOBAL_VLC ((vlc_t*)libvlc.pp_children[1])

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  GetFilenames  ( vlc_t *, int, char *[] );
static void Usage         ( vlc_t *, const char *psz_module_name );
static void ListModules   ( vlc_t * );
static void Version       ( void );

#ifdef WIN32
static void ShowConsole   ( void );
#endif

/*****************************************************************************
 * vlc_create: allocate a vlc_t structure, and initialize libvlc if needed.
 *****************************************************************************
 * This function allocates a vlc_t structure and returns NULL in case of
 * failure. Also, the thread system is initialized.
 *****************************************************************************/
vlc_error_t vlc_create( void )
{
    vlc_t * p_vlc;
    vlc_bool_t b_failed = VLC_FALSE;

    /* This call should be thread-safe, but an additional check will be
     * necessary afterwards to check that only one p_vlc is created. */
    p_vlc = vlc_create_r();

    if( p_vlc == NULL )
    {
        return VLC_EGENERIC;
    }

    /* We have created an object, which ensures us that p_global_lock has
     * been properly initialized. We can now atomically check that we are
     * the only p_vlc object. */
#if 0
    vlc_mutex_lock( libvlc.p_global_lock );
    if( libvlc.i_children != 1 ) /* FIXME !!! FIXME */
    {
        b_failed = VLC_TRUE;
    }
    vlc_mutex_unlock( libvlc.p_global_lock );
#endif

    /* There can be only one */
    if( b_failed )
    {
        vlc_destroy_r( p_vlc );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

vlc_t * vlc_create_r( void )
{
    int i_ret;
    vlc_t * p_vlc = NULL;

    /* vlc_threads_init *must* be the first internal call! No other call is
     * allowed before the thread system has been initialized. */
    i_ret = vlc_threads_init( &libvlc );
    if( i_ret )
    {
        return NULL;
    }

    /* Now that the thread system is initialized, we don't have much, but
     * at least we have libvlc.global_lock */
    vlc_mutex_lock( &libvlc.global_lock );
    if( !libvlc.b_ready )
    {
        char *psz_env;

        vlc_mutex_init( &libvlc, &libvlc.structure_lock );
        libvlc.p_global_data = NULL;
        libvlc.b_ready = VLC_TRUE;

        /* Guess what CPU we have */
        libvlc.i_cpu = CPUCapabilities();

        /* Find verbosity from VLC_VERBOSE environment variable */
        psz_env = getenv( "VLC_VERBOSE" );
        libvlc.i_verbose = psz_env ? atoi( psz_env ) : 0;

#ifdef HAVE_ISATTY
        libvlc.b_color = isatty( 2 ); /* 2 is for stderr */
#else
        libvlc.b_color = VLC_FALSE;
#endif

        /* Initialize message queue */
        msg_Create( &libvlc );

        /* Announce who we are */
        msg_Dbg( &libvlc, COPYRIGHT_MESSAGE );
        msg_Dbg( &libvlc, "libvlc was configured with %s", CONFIGURE_LINE );

        /* Initialize the module bank and and load the configuration of the
         * main module. We need to do this at this stage to be able to display
         * a short help if required by the user. (short help == main module
         * options) */
        module_InitBank( &libvlc );
        module_LoadMain( &libvlc );
    }
    vlc_mutex_unlock( &libvlc.global_lock );

    /* Allocate a vlc object */
    p_vlc = vlc_object_create( &libvlc, VLC_OBJECT_VLC );
    if( p_vlc == NULL )
    {
        return NULL;
    }

    p_vlc->psz_object_name = "root";

    /* Initialize mutexes */
    vlc_mutex_init( p_vlc, &p_vlc->config_lock );

    /* Store our newly allocated structure in the global list */
    vlc_object_attach( p_vlc, &libvlc );

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
    return vlc_init_r( GLOBAL_VLC, i_argc, ppsz_argv );
}

vlc_error_t vlc_init_r( vlc_t *p_vlc, int i_argc, char *ppsz_argv[] )
{
    char         p_capabilities[200];
    char *       p_tmp;
    vlc_bool_t   b_exit;
    module_t    *p_help_module;
    playlist_t  *p_playlist;

    /* Check that the handle is valid */
    if( !p_vlc || p_vlc->i_status != VLC_STATUS_CREATED )
    {
        fprintf( stderr, "error: invalid status (!CREATED)\n" );
        return VLC_ESTATUS;
    }

    /* Support for gettext */
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

    /* Hack: insert the help module here */
    p_help_module = vlc_object_create( p_vlc, VLC_OBJECT_MODULE );
    if( p_help_module == NULL )
    {
        //module_EndBank( p_vlc );
        return VLC_EGENERIC;
    }
    p_help_module->psz_object_name = "help";
    config_Duplicate( p_help_module, p_help_config );
    vlc_object_attach( p_help_module, libvlc.p_module_bank );
    /* End hack */

    if( config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_TRUE ) )
    {
        vlc_object_detach( p_help_module );
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        //module_EndBank( p_vlc );
        return VLC_EGENERIC;
    }

    b_exit = VLC_FALSE;

    /* Check for short help option */
    if( config_GetInt( p_vlc, "help" ) )
    {
        fprintf( stderr, _("Usage: %s [options] [items]...\n\n"),
                         p_vlc->psz_object_name );
        Usage( p_vlc, "main" );
        Usage( p_vlc, "help" );
        b_exit = VLC_TRUE;
    }
    /* Check for version option */
    else if( config_GetInt( p_vlc, "version" ) )
    {
        Version();
        b_exit = VLC_TRUE;
    }

    /* Hack: remove the help module here */
    vlc_object_detach( p_help_module );
    /* End hack */

    if( b_exit )
    {
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        //module_EndBank( p_vlc );
        return VLC_EEXIT;
    }

    /*
     * Load the builtins and plugins into the module_bank.
     * We have to do it before config_Load*() because this also gets the
     * list of configuration options exported by each module and loads their
     * default values.
     */
    module_LoadBuiltins( &libvlc );
    module_LoadPlugins( &libvlc );
    msg_Dbg( p_vlc, "module bank initialized, found %i modules",
                    libvlc.p_module_bank->i_children );

    /* Hack: insert the help module here */
    vlc_object_attach( p_help_module, libvlc.p_module_bank );
    /* End hack */

    /* Check for help on modules */
    if( (p_tmp = config_GetPsz( p_vlc, "module" )) )
    {
        Usage( p_vlc, p_tmp );
        free( p_tmp );
        b_exit = VLC_TRUE;
    }
    /* Check for long help option */
    else if( config_GetInt( p_vlc, "longhelp" ) )
    {
        Usage( p_vlc, NULL );
        b_exit = VLC_TRUE;
    }
    /* Check for module list option */
    else if( config_GetInt( p_vlc, "list" ) )
    {
        ListModules( p_vlc );
        b_exit = VLC_TRUE;
    }

    /* Hack: remove the help module here */
    vlc_object_detach( p_help_module );
    config_Free( p_help_module );
    vlc_object_destroy( p_help_module );
    /* End hack */

    if( b_exit )
    {
        //module_EndBank( p_vlc );
        return VLC_EEXIT;
    }

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
        //module_EndBank( p_vlc );
        return VLC_EGENERIC;
    }

    /*
     * System specific configuration
     */
    system_Configure( p_vlc );

    /*
     * Message queue options
     */
    if( config_GetInt( p_vlc, "quiet" ) )
    {
        libvlc.i_verbose = 0;
    }
    else
    {
        int i_tmp = config_GetInt( p_vlc, "verbose" );
        if( i_tmp >= 0 && i_tmp <= 4 )
        {
            libvlc.i_verbose = i_tmp;
        }
    }
    libvlc.b_color = libvlc.b_color || config_GetInt( p_vlc, "color" );

    /*
     * Output messages that may still be in the queue
     */
    msg_Flush( p_vlc );

    /* p_vlc inititalization. FIXME ? */
    p_vlc->i_desync = config_GetInt( p_vlc, "desync" ) * (mtime_t)1000;

#if defined( __i386__ )
    if( !config_GetInt( p_vlc, "mmx" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_MMX;
    if( !config_GetInt( p_vlc, "3dn" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_3DNOW;
    if( !config_GetInt( p_vlc, "mmxext" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_MMXEXT;
    if( !config_GetInt( p_vlc, "sse" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_SSE;
#endif
#if defined( __powerpc__ ) || defined( SYS_DARWIN )
    if( !config_GetInt( p_vlc, "altivec" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_ALTIVEC;
#endif

#define PRINT_CAPABILITY( capability, string )                              \
    if( libvlc.i_cpu & capability )                                         \
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

    if( p_vlc->pf_memcpy == NULL )
    {
        p_vlc->pf_memcpy = memcpy;
    }

    if( p_vlc->pf_memset == NULL )
    {
        p_vlc->pf_memset = memset;
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
        //module_EndBank( p_vlc );
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
 * vlc_add_intf: add an interface
 *****************************************************************************
 * This function opens an interface plugin and runs it. If b_block is set
 * to 0, vlc_add_intf will return immediately and let the interface run in a
 * separate thread. If b_block is set to 1, vlc_add_intf will continue until
 * user requests to quit.
 *****************************************************************************/
vlc_error_t vlc_add_intf( const char *psz_module, vlc_bool_t b_block )
{
    return vlc_add_intf_r( GLOBAL_VLC,
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
        vlc_object_detach( p_intf );
        intf_Destroy( p_intf );
        return err;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_destroy: stop playing and destroy everything.
 *****************************************************************************
 * This function requests the running threads to finish, waits for their
 * termination, and destroys their structure.
 *****************************************************************************/
vlc_error_t vlc_destroy( void )
{
    return vlc_destroy_r( GLOBAL_VLC );
}

vlc_error_t vlc_destroy_r( vlc_t *p_vlc )
{
    /* Check that the handle is valid */
    if( !p_vlc || (p_vlc->i_status != VLC_STATUS_STOPPED
                    && p_vlc->i_status != VLC_STATUS_CREATED) )
    {
        fprintf( stderr, "error: invalid status "
                         "(!STOPPED&&!CREATED)\n" );
        return VLC_ESTATUS;
    }

    if( p_vlc->i_status == VLC_STATUS_STOPPED )
    {
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
         * XXX: Free module bank !
         */
        //module_EndBank( p_vlc );
    
        /*
         * System specific cleaning code
         */
        system_End( p_vlc );
    
        /* Update the handle status */
        p_vlc->i_status = VLC_STATUS_CREATED;
    }

    /* Update the handle status, just in case */
    p_vlc->i_status = VLC_STATUS_NONE;

    /* Destroy mutexes */
    vlc_mutex_destroy( &p_vlc->config_lock );

    vlc_object_detach( p_vlc );

    vlc_object_destroy( p_vlc );

    /* Stop thread system: last one out please shut the door! */
    vlc_threads_end( &libvlc );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_die: ask vlc to die.
 *****************************************************************************
 * This function sets p_vlc->b_die to VLC_TRUE, but does not do any other
 * task. It is your duty to call vlc_end and vlc_destroy afterwards.
 *****************************************************************************/
vlc_error_t vlc_die( void )
{
    return vlc_die_r( GLOBAL_VLC );
}

vlc_error_t vlc_die_r( vlc_t *p_vlc )
{
    if( !p_vlc )
    {
        fprintf( stderr, "error: invalid status (!EXIST)\n" );
        return VLC_ESTATUS;
    }

    p_vlc->b_die = VLC_TRUE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_status: return the current vlc status.
 *****************************************************************************
 * This function returns the current value of p_vlc->i_status.
 *****************************************************************************/
vlc_status_t vlc_status( void )
{
    return vlc_status_r( GLOBAL_VLC );
}

vlc_status_t vlc_status_r( vlc_t *p_vlc )
{
    if( !p_vlc )
    {
        return VLC_STATUS_NONE;
    }

    return p_vlc->i_status;
}

/*****************************************************************************
 * vlc_add_target: adds a target for playing.
 *****************************************************************************
 * This function adds psz_target to the current playlist. If a playlist does
 * not exist, it will create one.
 *****************************************************************************/
vlc_error_t vlc_add_target( const char *psz_target, int i_mode, int i_pos )
{
    return vlc_add_target_r( GLOBAL_VLC,
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

/*****************************************************************************
 * vlc_set: set a vlc variable
 *****************************************************************************
 *
 *****************************************************************************/
vlc_error_t vlc_set( const char *psz_var, const char *psz_val )
{
    return vlc_set_r( GLOBAL_VLC, psz_var, psz_val );
}

vlc_error_t vlc_set_r( vlc_t *p_vlc, const char *psz_var, const char *psz_val )
{
    module_config_t *p_config;

    if( !p_vlc )
    {
        fprintf( stderr, "error: invalid status\n" );
        return VLC_ESTATUS;
    }

    p_config = config_FindConfig( VLC_OBJECT(p_vlc), psz_var );

    if( !p_config )
    {
        msg_Err( p_vlc, "option %s does not exist", psz_var );
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( p_config->p_lock );

    switch( p_config->i_type )
    {
    case CONFIG_ITEM_BOOL:
        if( psz_val && *psz_val )
        {
            if( !strcmp( psz_val, "off" ) || !strcmp( psz_val, "no" ) )
            {
                p_config->i_value = VLC_FALSE;
            }
            else
            {
                p_config->i_value = atoi( psz_val );
            }
        }
        else
        {
            p_config->i_value = VLC_TRUE;
        }
        break;
    case CONFIG_ITEM_INTEGER:
        if( psz_val && *psz_val )
        {
            p_config->i_value = atoi( psz_val );
        }
        else
        {
            p_config->i_value = 0;
        }
        break;
    case CONFIG_ITEM_FLOAT:
        if( psz_val && *psz_val )
        {
            p_config->f_value = (float)atof( psz_val );
        }
        else
        {
            p_config->f_value = 0.0;
        }
        break;
    case CONFIG_ITEM_STRING:
    case CONFIG_ITEM_FILE:
    case CONFIG_ITEM_MODULE:
    default:
        if( p_config->psz_value )
        {
            free( p_config->psz_value );
        }

        if( psz_val )
        {
            p_config->psz_value = strdup( psz_val );
        }
        else
        {
            p_config->psz_value = NULL;
        }
        break;
    }

    if( p_config->pf_callback )
    {
        vlc_mutex_unlock( p_config->p_lock );
        p_config->pf_callback( VLC_OBJECT(p_vlc) );
    }
    else
    {
        vlc_mutex_unlock( p_config->p_lock );
    }

    return VLC_SUCCESS;
}

/* XXX: temporary hacks */

/*****************************************************************************
 * vlc_play: play
 *****************************************************************************/
vlc_error_t vlc_play( )
{
    return vlc_play_r( GLOBAL_VLC );
}

vlc_error_t vlc_play_r( vlc_t *p_vlc )
{
    playlist_t * p_playlist;

    /* Check that the handle is valid */
    if( !p_vlc || p_vlc->i_status != VLC_STATUS_STOPPED )
    {
        fprintf( stderr, "error: invalid status (!STOPPED)\n" );
        return VLC_ESTATUS;
    }

    /* Update the handle status */
    p_vlc->i_status = VLC_STATUS_RUNNING;

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        return VLC_EOBJECT;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Play( p_playlist );
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
    }

    vlc_object_release( p_playlist );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_stop: stop
 *****************************************************************************/
vlc_error_t vlc_stop( )
{
    return vlc_stop_r( GLOBAL_VLC );
}

vlc_error_t vlc_stop_r( vlc_t *p_vlc )
{
    intf_thread_t *   p_intf;
    playlist_t    *   p_playlist;
    vout_thread_t *   p_vout;
    aout_instance_t * p_aout;

    /* Check that the handle is valid */
    if( !p_vlc || ( p_vlc->i_status != VLC_STATUS_STOPPED
                     && p_vlc->i_status != VLC_STATUS_RUNNING ) )
    {
        fprintf( stderr, "error: invalid status (!STOPPED&&!RUNNING)\n" );
        return VLC_ESTATUS;
    }

    /*
     * Ask the interfaces to stop and destroy them
     */
    msg_Dbg( p_vlc, "removing all interfaces" );
    while( (p_intf = vlc_object_find( p_vlc, VLC_OBJECT_INTF, FIND_CHILD )) )
    {
        intf_StopThread( p_intf );
        vlc_object_detach( p_intf );
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
        vlc_object_detach( p_playlist );
        vlc_object_release( p_playlist );
        playlist_Destroy( p_playlist );
    }

    /*
     * Free video outputs
     */
    msg_Dbg( p_vlc, "removing all video outputs" );
    while( (p_vout = vlc_object_find( p_vlc, VLC_OBJECT_VOUT, FIND_CHILD )) )
    {
        vlc_object_detach( p_vout );
        vlc_object_release( p_vout );
        vout_DestroyThread( p_vout );
    }

    /*
     * Free audio outputs
     */
    msg_Dbg( p_vlc, "removing all audio outputs" );
    while( (p_aout = vlc_object_find( p_vlc, VLC_OBJECT_AOUT, FIND_CHILD )) )
    {
        vlc_object_detach( (vlc_object_t *)p_aout );
        vlc_object_release( (vlc_object_t *)p_aout );
        aout_Delete( p_aout );
    }

    /* Update the handle status */
    p_vlc->i_status = VLC_STATUS_STOPPED;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_pause: toggle pause
 *****************************************************************************/
vlc_error_t vlc_pause( )
{
    return vlc_pause_r( GLOBAL_VLC );
}

vlc_error_t vlc_pause_r( vlc_t *p_vlc )
{
    input_thread_t *p_input;

    p_input = vlc_object_find( p_vlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        return VLC_EOBJECT;
    }

    input_SetStatus( p_input, INPUT_STATUS_PAUSE );
    vlc_object_release( p_input );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vlc_fullscreen: toggle fullscreen mode
 *****************************************************************************/
vlc_error_t vlc_fullscreen( )
{
    return vlc_fullscreen_r( GLOBAL_VLC );
}

vlc_error_t vlc_fullscreen_r( vlc_t *p_vlc )
{
    vout_thread_t *p_vout;

    p_vout = vlc_object_find( p_vlc, VLC_OBJECT_VOUT, FIND_CHILD );

    if( !p_vout )
    {
        return VLC_EOBJECT;
    }

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
    vlc_object_release( p_vout );

    return VLC_SUCCESS;
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
                          PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END );
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
    vlc_list_t *p_list;
    module_t **pp_parser;
    module_config_t *p_item;
    char psz_spaces[PADDING_SPACES+LINE_START+1];
    char psz_format[sizeof(FORMAT_STRING)];

    memset( psz_spaces, ' ', PADDING_SPACES+LINE_START );
    psz_spaces[PADDING_SPACES+LINE_START] = '\0';

    strcpy( psz_format, FORMAT_STRING );

#ifdef WIN32
    ShowConsole();
#endif

    /* List all modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /* Enumerate the config for each module */
    for( pp_parser = (module_t **)p_list->pp_objects ;
         *pp_parser ;
         pp_parser++ )
    {
        vlc_bool_t b_help_module;

        if( psz_module_name && strcmp( psz_module_name,
                                       (*pp_parser)->psz_object_name ) )
        {
            continue;
        }

        /* Ignore modules without config options */
        if( !(*pp_parser)->i_config_items )
        {
            continue;
        }

        b_help_module = !strcmp( "help", (*pp_parser)->psz_object_name );

        /* Print module options */
        for( p_item = (*pp_parser)->p_config;
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

            /* Add short option if any */
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
                    /* If option is of type --foo-bar, we print its counterpart
                     * as --no-foo-bar, but if it is of type --foobar (without
                     * dashes in the name) we print it as --nofoobar. Both
                     * values are of course valid, only the display changes. */
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
    }

    /* Release the module list */
    vlc_list_release( p_list );

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
    vlc_list_t *p_list;
    module_t **pp_parser;
    char psz_spaces[22];

    memset( psz_spaces, ' ', 22 );

#ifdef WIN32
    ShowConsole();
#endif

    /* Usage */
    fprintf( stderr, _("Usage: %s [options] [items]...\n\n"),
                     p_this->p_vlc->psz_object_name );

    fprintf( stderr, _("[module]              [description]\n") );

    /* List all modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /* Enumerate each module */
    for( pp_parser = (module_t **)p_list->pp_objects ;
         *pp_parser ;
         pp_parser++ )
    {
        int i;

        /* Nasty hack, but right now I'm too tired to think about a nice
         * solution */
        i = 22 - strlen( (*pp_parser)->psz_object_name ) - 1;
        if( i < 0 ) i = 0;
        psz_spaces[i] = 0;

        fprintf( stderr, "  %s%s %s\n", (*pp_parser)->psz_object_name,
                         psz_spaces, (*pp_parser)->psz_longname );

        psz_spaces[i] = ' ';
    }

    vlc_list_release( p_list );

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

