/*****************************************************************************
 * libvlc.c: main libvlc source
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: libvlc.c,v 1.88 2003/05/25 17:27:13 massiot Exp $
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
#include <vlc/vlc.h>

#ifdef HAVE_ERRNO_H
#   include <errno.h>                                              /* ENOMEM */
#endif
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                                                /* free() */

#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#ifdef WIN32                       /* optind, getopt(), included in unistd.h */
#   include "extras/getopt.h"
#endif

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif

#include "vlc_cpu.h"                                        /* CPU detection */
#include "os_specific.h"

#include "error.h"

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
static vlc_t *  p_static_vlc;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void SetLanguage   ( char const * );
static int  GetFilenames  ( vlc_t *, int, char *[] );
static void Usage         ( vlc_t *, char const *psz_module_name );
static void ListModules   ( vlc_t * );
static void Version       ( void );

#ifdef WIN32
static void ShowConsole   ( void );
#endif
static int  ConsoleWidth  ( void );

/*****************************************************************************
 * VLC_Version: return the libvlc version.
 *****************************************************************************
 * This function returns full version string (numeric version and codename).
 *****************************************************************************/
char const * VLC_Version( void )
{
    return VERSION_MESSAGE;
}

/*****************************************************************************
 * VLC_Error: strerror() equivalent
 *****************************************************************************
 * This function returns full version string (numeric version and codename).
 *****************************************************************************/
char const * VLC_Error( int i_err )
{
    return vlc_error( i_err );
}

/*****************************************************************************
 * VLC_Create: allocate a vlc_t structure, and initialize libvlc if needed.
 *****************************************************************************
 * This function allocates a vlc_t structure and returns a negative value
 * in case of failure. Also, the thread system is initialized.
 *****************************************************************************/
int VLC_Create( void )
{
    int i_ret;
    vlc_t * p_vlc = NULL;
    vlc_value_t lockval;

    /* vlc_threads_init *must* be the first internal call! No other call is
     * allowed before the thread system has been initialized. */
    i_ret = vlc_threads_init( &libvlc );
    if( i_ret < 0 )
    {
        return i_ret;
    }

    /* Now that the thread system is initialized, we don't have much, but
     * at least we have var_Create */
    var_Create( &libvlc, "libvlc", VLC_VAR_MUTEX );
    var_Get( &libvlc, "libvlc", &lockval );
    vlc_mutex_lock( lockval.p_address );
    if( !libvlc.b_ready )
    {
        char *psz_env;

        /* Guess what CPU we have */
        libvlc.i_cpu = CPUCapabilities();

        /* Find verbosity from VLC_VERBOSE environment variable */
        psz_env = getenv( "VLC_VERBOSE" );
        libvlc.i_verbose = psz_env ? atoi( psz_env ) : -1;

#if defined( HAVE_ISATTY ) && !defined( WIN32 )
        libvlc.b_color = isatty( 2 ); /* 2 is for stderr */
#else
        libvlc.b_color = VLC_FALSE;
#endif

        /* Initialize message queue */
        msg_Create( &libvlc );

        /* Announce who we are */
        msg_Dbg( &libvlc, COPYRIGHT_MESSAGE );
        msg_Dbg( &libvlc, "libvlc was configured with %s", CONFIGURE_LINE );

        /* The module bank will be initialized later */
        libvlc.p_module_bank = NULL;

        libvlc.b_ready = VLC_TRUE;
    }
    vlc_mutex_unlock( lockval.p_address );
    var_Destroy( &libvlc, "libvlc" );

    /* Allocate a vlc object */
    p_vlc = vlc_object_create( &libvlc, VLC_OBJECT_VLC );
    if( p_vlc == NULL )
    {
        return VLC_EGENERIC;
    }
    vlc_thread_set_priority( p_vlc, VLC_THREAD_PRIORITY_LOW );

    p_vlc->psz_object_name = "root";

    /* Initialize mutexes */
    vlc_mutex_init( p_vlc, &p_vlc->config_lock );
#ifdef SYS_DARWIN
    vlc_mutex_init( p_vlc, &p_vlc->quicktime_lock );
#endif

    /* Store our newly allocated structure in the global list */
    vlc_object_attach( p_vlc, &libvlc );

    /* Store data for the non-reentrant API */
    p_static_vlc = p_vlc;

    return p_vlc->i_object_id;
}

/*****************************************************************************
 * VLC_Init: initialize a vlc_t structure.
 *****************************************************************************
 * This function initializes a previously allocated vlc_t structure:
 *  - CPU detection
 *  - gettext initialization
 *  - message queue, module bank and playlist initialization
 *  - configuration and commandline parsing
 *****************************************************************************/
int VLC_Init( int i_object, int i_argc, char *ppsz_argv[] )
{
    char         p_capabilities[200];
    char *       p_tmp;
    char *       psz_modules;
    char *       psz_parser;
    char *       psz_language;
    vlc_bool_t   b_exit = VLC_FALSE;
    vlc_t *      p_vlc;
    module_t    *p_help_module;
    playlist_t  *p_playlist;
    vlc_value_t  lockval;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

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

    /*
     * Support for gettext
     */
    SetLanguage( "" );

    /* Translate "C" to the language code: "fr", "en_GB", "nl", "ru"... */
    msg_Dbg( p_vlc, "translation test: code is \"%s\"", _("C") );

    /* Initialize the module bank and load the configuration of the
     * main module. We need to do this at this stage to be able to display
     * a short help if required by the user. (short help == main module
     * options) */
    var_Create( &libvlc, "libvlc", VLC_VAR_MUTEX );
    var_Get( &libvlc, "libvlc", &lockval );
    vlc_mutex_lock( lockval.p_address );
    if( libvlc.p_module_bank == NULL )
    {
        module_InitBank( &libvlc );
        module_LoadMain( &libvlc );
    }
    vlc_mutex_unlock( lockval.p_address );
    var_Destroy( &libvlc, "libvlc" );

    /* Hack: insert the help module here */
    p_help_module = vlc_object_create( p_vlc, VLC_OBJECT_MODULE );
    if( p_help_module == NULL )
    {
        /*module_EndBank( p_vlc );*/
        if( i_object ) vlc_object_release( p_vlc );
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
        /*module_EndBank( p_vlc );*/
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_EGENERIC;
    }

    /* Check for short help option */
    if( config_GetInt( p_vlc, "help" ) )
    {
        fprintf( stdout, _("Usage: %s [options] [items]...\n\n"),
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
        /*module_EndBank( p_vlc );*/
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_EEXIT;
    }

    /* Check for translation config option */
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )

    /* This ain't really nice to have to reload the config here but it seems
     * the only way to do it. */
    p_vlc->psz_homedir = config_GetHomeDir();
    config_LoadConfigFile( p_vlc, "main" );
    config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_TRUE );

    /* Check if the user specified a custom language */
    psz_language = config_GetPsz( p_vlc, "language" );
    if( psz_language && *psz_language && strcmp( psz_language, "auto" ) )
    {
        /* Reset the default domain */
        SetLanguage( psz_language );

        /* Translate "C" to the language code: "fr", "en_GB", "nl", "ru"... */
        msg_Dbg( p_vlc, "translation test: code is \"%s\"", _("C") );

        textdomain( PACKAGE );

#if defined( SYS_BEOS ) || defined ( SYS_DARWIN )
        /* BeOS only support UTF8 strings */
        /* Mac OS X prefers UTF8 */
        bind_textdomain_codeset( PACKAGE, "UTF-8" );
#endif

        module_EndBank( p_vlc );
        module_InitBank( &libvlc );
        module_LoadMain( &libvlc );
        config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_TRUE );
    }
    if( psz_language ) free( psz_language );
#endif

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
        /*module_EndBank( p_vlc );*/
        if( i_object ) vlc_object_release( p_vlc );
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
        /*module_EndBank( p_vlc );*/
        if( i_object ) vlc_object_release( p_vlc );
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
        libvlc.i_verbose = -1;
    }
    else
    {
        int i_tmp = config_GetInt( p_vlc, "verbose" );
        if( i_tmp >= 0 )
        {
            libvlc.i_verbose = __MIN( i_tmp, 2 );
        }
    }
    libvlc.b_color = libvlc.b_color && config_GetInt( p_vlc, "color" );

    /*
     * Output messages that may still be in the queue
     */
    msg_Flush( p_vlc );

    /* p_vlc initialization. FIXME ? */
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
        /*module_EndBank( p_vlc );*/
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_EGENERIC;
    }

    /*
     * Load background interfaces
     */
    psz_modules = config_GetPsz( p_vlc, "extraintf" );
    psz_parser = psz_modules;
    while ( psz_parser && *psz_parser )
    {
        char *psz_module, *psz_temp;
        psz_module = psz_parser;
        psz_parser = strchr( psz_module, ',' );
        if ( psz_parser )
        {
            *psz_parser = '\0';
            psz_parser++;
        }
        psz_temp = (char *)malloc( strlen(psz_module) + sizeof(",none") );
        if( psz_temp )
        {
            sprintf( psz_temp, "%s,none", psz_module );
            VLC_AddIntf( 0, psz_temp, VLC_FALSE );
            free( psz_temp );
        }
    }
    if ( psz_modules )
    {
        free( psz_modules );
    }

    /*
     * FIXME: kludge to use a p_vlc-local variable for the Mozilla plugin
     */
    var_Create( p_vlc, "drawable", VLC_VAR_INTEGER );

    /*
     * Get input filenames given as commandline arguments
     */
    GetFilenames( p_vlc, i_argc, ppsz_argv );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_AddIntf: add an interface
 *****************************************************************************
 * This function opens an interface plugin and runs it. If b_block is set
 * to 0, VLC_AddIntf will return immediately and let the interface run in a
 * separate thread. If b_block is set to 1, VLC_AddIntf will continue until
 * user requests to quit.
 *****************************************************************************/
int VLC_AddIntf( int i_object, char const *psz_module, vlc_bool_t b_block )
{
    int i_err;
    intf_thread_t *p_intf;
    vlc_t *p_vlc;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    /* Try to create the interface */
    p_intf = intf_Create( p_vlc, psz_module ? psz_module : "$intf" );

    if( p_intf == NULL )
    {
        msg_Err( p_vlc, "interface \"%s\" initialization failed", psz_module );
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_EGENERIC;
    }

    /* Try to run the interface */
    p_intf->b_block = b_block;
    i_err = intf_RunThread( p_intf );
    if( i_err )
    {
        vlc_object_detach( p_intf );
        intf_Destroy( p_intf );
        if( i_object ) vlc_object_release( p_vlc );
        return i_err;
    }

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_Destroy: stop playing and destroy everything.
 *****************************************************************************
 * This function requests the running threads to finish, waits for their
 * termination, and destroys their structure.
 *****************************************************************************/
int VLC_Destroy( int i_object )
{
    vlc_t *p_vlc;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    /*
     * Free allocated memory
     */
    if( p_vlc->p_memcpy_module )
    {
        module_Unneed( p_vlc, p_vlc->p_memcpy_module );
        p_vlc->p_memcpy_module = NULL;
    }

    if( p_vlc->psz_homedir )
    {
        free( p_vlc->psz_homedir );
        p_vlc->psz_homedir = NULL;
    }

    /*
     * XXX: Free module bank !
     */
    /*module_EndBank( p_vlc );*/

    /*
     * System specific cleaning code
     */
    system_End( p_vlc );

    /* Destroy mutexes */
    vlc_mutex_destroy( &p_vlc->config_lock );
#ifdef SYS_DARWIN
    vlc_mutex_destroy( &p_vlc->quicktime_lock );
#endif

    vlc_object_detach( p_vlc );

    /* Release object before destroying it */
    if( i_object ) vlc_object_release( p_vlc );

    vlc_object_destroy( p_vlc );

    /* Stop thread system: last one out please shut the door! */
    vlc_threads_end( &libvlc );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_Die: ask vlc to die.
 *****************************************************************************
 * This function sets p_vlc->b_die to VLC_TRUE, but does not do any other
 * task. It is your duty to call vlc_end and VLC_Destroy afterwards.
 *****************************************************************************/
int VLC_Die( int i_object )
{
    vlc_t *p_vlc;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_vlc->b_die = VLC_TRUE;

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_AddTarget: adds a target for playing.
 *****************************************************************************
 * This function adds psz_target to the current playlist. If a playlist does
 * not exist, it will create one.
 *****************************************************************************/
int VLC_AddTarget( int i_object, char const *psz_target, int i_mode, int i_pos )
{
    int i_err;
    playlist_t *p_playlist;
    vlc_t *p_vlc;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        msg_Dbg( p_vlc, "no playlist present, creating one" );
        p_playlist = playlist_Create( p_vlc );

        if( p_playlist == NULL )
        {
            if( i_object ) vlc_object_release( p_vlc );
            return VLC_EGENERIC;
        }

        vlc_object_yield( p_playlist );
    }

    i_err = playlist_Add( p_playlist, psz_target, i_mode, i_pos );

    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return i_err;
}

/*****************************************************************************
 * VLC_Set: set a vlc variable
 *****************************************************************************
 *
 *****************************************************************************/
int VLC_Set( int i_object, char const *psz_var, vlc_value_t value )
{
    vlc_t *p_vlc;
    int i_ret;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    /* FIXME: Temporary hack for Mozilla, if variable starts with conf:: then
     * we handle it as a configuration variable. Don't tell Gildas :) -- sam */
    if( !strncmp( psz_var, "conf::", 6 ) )
    {
        module_config_t *p_item;
        char const *psz_newvar = psz_var + 6;

        p_item = config_FindConfig( VLC_OBJECT(p_vlc), psz_newvar );

        if( p_item )
        {
            switch( p_item->i_type )
            {
                case CONFIG_ITEM_BOOL:
                    config_PutInt( p_vlc, psz_newvar, value.b_bool );
                    break;
                case CONFIG_ITEM_INTEGER:
                    config_PutInt( p_vlc, psz_newvar, value.i_int );
                    break;
                case CONFIG_ITEM_FLOAT:
                    config_PutFloat( p_vlc, psz_newvar, value.f_float );
                    break;
                default:
                    config_PutPsz( p_vlc, psz_newvar, value.psz_string );
                    break;
            }
            if( i_object ) vlc_object_release( p_vlc );
            return VLC_SUCCESS;
        }
    }

    i_ret = var_Set( p_vlc, psz_var, value );

    if( i_object ) vlc_object_release( p_vlc );
    return i_ret;
}

/*****************************************************************************
 * VLC_Get: get a vlc variable
 *****************************************************************************
 *
 *****************************************************************************/
int VLC_Get( int i_object, char const *psz_var, vlc_value_t *p_value )
{
    vlc_t *p_vlc;
    int i_ret;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    i_ret = var_Get( p_vlc, psz_var, p_value );

    if( i_object ) vlc_object_release( p_vlc );
    return i_ret;
}

/* FIXME: temporary hacks */

/*****************************************************************************
 * VLC_Play: play
 *****************************************************************************/
int VLC_Play( int i_object )
{
    playlist_t * p_playlist;
    vlc_t *p_vlc;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
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

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_Stop: stop
 *****************************************************************************/
int VLC_Stop( int i_object )
{
    intf_thread_t *   p_intf;
    playlist_t    *   p_playlist;
    vout_thread_t *   p_vout;
    aout_instance_t * p_aout;
    vlc_t *p_vlc;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
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
        vout_Destroy( p_vout );
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

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_Pause: toggle pause
 *****************************************************************************/
int VLC_Pause( int i_object )
{
    input_thread_t *p_input;
    vlc_t *p_vlc;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_vlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    input_SetStatus( p_input, INPUT_STATUS_PAUSE );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_FullScreen: toggle fullscreen mode
 *****************************************************************************/
int VLC_FullScreen( int i_object )
{
    vout_thread_t *p_vout;
    vlc_t *p_vlc;

    p_vlc = i_object ? vlc_object_get( &libvlc, i_object ) : p_static_vlc;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_vout = vlc_object_find( p_vlc, VLC_OBJECT_VOUT, FIND_CHILD );

    if( !p_vout )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
    vlc_object_release( p_vout );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/* following functions are local */

/*****************************************************************************
 * SetLanguage: set the interface language.
 *****************************************************************************
 * We set the LC_MESSAGES locale category for interface messages and buttons,
 * as well as the LC_CTYPE category for string sorting and possible wide
 * character support.
 *****************************************************************************/
static void SetLanguage ( char const *psz_lang )
{
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )

    char *          psz_path;
#if defined( SYS_DARWIN ) || defined ( WIN32 ) || defined( SYS_BEOS )
    char            psz_tmp[1024];
#endif

#   if defined( HAVE_INCLUDED_GETTEXT ) && !defined( HAVE_LC_MESSAGES )
    if( *psz_lang )
    {
        /* We set LC_ALL manually because it is the only way to set
         * the language at runtime under eg. Windows. Beware that this
         * makes the environment unconsistent when libvlc is unloaded and
         * should probably be moved to a safer place like vlc.c. */
        static char psz_lcall[20];
        snprintf( psz_lcall, 19, "LC_ALL=%s", psz_lang );
        psz_lcall[19] = '\0';
        putenv( psz_lcall );
    }
#   endif

    if( psz_lang && !*psz_lang )
    {
#   if defined( HAVE_LC_MESSAGES )
        setlocale( LC_MESSAGES, psz_lang );
#   endif
        setlocale( LC_CTYPE, psz_lang );
    }
    else
    {
#ifdef SYS_BEOS 
        static char psz_lcall[20];
#endif
        setlocale( LC_ALL, psz_lang );
#ifdef SYS_DARWIN
        /* I need that under Darwin, please check it doesn't disturb
         * other platforms. --Meuuh */
        setenv( "LANG", psz_lang, 1 );
#endif
#ifdef SYS_BEOS
        /* I need this under BeOS... */
        snprintf( psz_lcall, 19, "LC_ALL=%s", psz_lang );
        psz_lcall[19] = '\0';
        putenv( psz_lcall );
#endif
    }

    /* Specify where to find the locales for current domain */
#if !defined( SYS_DARWIN ) && !defined( WIN32 ) && !defined( SYS_BEOS )
    psz_path = LOCALEDIR;
#else
    snprintf( psz_tmp, sizeof(psz_tmp), "%s/%s", libvlc.psz_vlcpath,
              "locale" );
    psz_path = psz_tmp;
#endif
    if( !bindtextdomain( PACKAGE, psz_path ) )
    {
        fprintf( stderr, "warning: no domain %s in directory %s\n",
                 PACKAGE, psz_path );
    }

    /* Set the default domain */
    textdomain( PACKAGE );

#if defined( SYS_BEOS ) || defined ( SYS_DARWIN )
    /* BeOS only support UTF8 strings */
    /* Mac OS X prefers UTF8 */
    bind_textdomain_codeset( PACKAGE, "UTF-8" );
#endif

#endif
}

/*****************************************************************************
 * GetFilenames: parse command line options which are not flags
 *****************************************************************************
 * Parse command line for input files.
 *****************************************************************************/
static int GetFilenames( vlc_t *p_vlc, int i_argc, char *ppsz_argv[] )
{
    int i_opt;

    /* We assume that the remaining parameters are filenames */
    for( i_opt = i_argc - 1; i_opt > optind; i_opt-- )
    {
        /* TODO: write an internal function of this one, to avoid
         *       unnecessary lookups. */
        VLC_AddTarget( p_vlc->i_object_id, ppsz_argv[ i_opt ],
                       PLAYLIST_INSERT, 0 );
    }

    /* If there is at least one target, play it */
    if( i_argc > optind )
    {
        VLC_AddTarget( p_vlc->i_object_id, ppsz_argv[ optind ],
                       PLAYLIST_INSERT | PLAYLIST_GO, 0 );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Usage: print program usage
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static void Usage( vlc_t *p_this, char const *psz_module_name )
{
#define FORMAT_STRING "  %s --%s%s%s%s%s%s%s "
    /* short option ------'    |     | | | |  | |
     * option name ------------'     | | | |  | |
     * <bra -------------------------' | | |  | |
     * option type or "" --------------' | |  | |
     * ket> -----------------------------' |  | |
     * padding spaces ---------------------'  | |
     * comment -------------------------------' |
     * comment suffix --------------------------'
     *
     * The purpose of having bra and ket is that we might i18n them as well.
     */
#define LINE_START 8
#define PADDING_SPACES 25
    vlc_list_t *p_list;
    module_t *p_parser;
    module_config_t *p_item;
    char psz_spaces[PADDING_SPACES+LINE_START+1];
    char psz_format[sizeof(FORMAT_STRING)];
    char psz_buffer[1000];
    char psz_short[4];
    int i_index;
    int i_width = ConsoleWidth() - (PADDING_SPACES+LINE_START+1);

    memset( psz_spaces, ' ', PADDING_SPACES+LINE_START );
    psz_spaces[PADDING_SPACES+LINE_START] = '\0';

    strcpy( psz_format, FORMAT_STRING );

#ifdef WIN32
    ShowConsole();
#endif

    /* List all modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /* Enumerate the config for each module */
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        vlc_bool_t b_help_module;

        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( psz_module_name && strcmp( psz_module_name,
                                       p_parser->psz_object_name ) )
        {
            continue;
        }

        /* Ignore modules without config options */
        if( !p_parser->i_config_items )
        {
            continue;
        }

        b_help_module = !strcmp( "help", p_parser->psz_object_name );

        /* Print module options */
        for( p_item = p_parser->p_config;
             p_item->i_type != CONFIG_HINT_END;
             p_item++ )
        {
            char *psz_text;
            char *psz_bra = NULL, *psz_type = NULL, *psz_ket = NULL;
            char *psz_suf = "", *psz_prefix = NULL;
            int i;
            if ( p_item->b_advanced && !config_GetInt( p_this, "advanced" ))
            {
                continue;
            }
            switch( p_item->i_type )
            {
            case CONFIG_HINT_CATEGORY:
            case CONFIG_HINT_USAGE:
                fprintf( stdout, " %s\n", p_item->psz_text );
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
                    psz_bra = " {";
                    psz_type = psz_buffer;
                    psz_type[0] = '\0';
                    for( i = 0; p_item->ppsz_list[i]; i++ )
                    {
                        if( i ) strcat( psz_type, "," );
                        strcat( psz_type, p_item->ppsz_list[i] );
                    }
                    psz_ket = "}";
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

            if( !psz_type )
            {
                continue;
            }

            /* Add short option if any */
            if( p_item->i_short )
            {
                sprintf( psz_short, "-%c,", p_item->i_short );
            }
            else
            {
                strcpy( psz_short, "   " );
            }

            i = PADDING_SPACES - strlen( p_item->psz_name )
                 - strlen( psz_bra ) - strlen( psz_type )
                 - strlen( psz_ket ) - 1;

            if( p_item->i_type == CONFIG_ITEM_BOOL && !b_help_module )
            {
                /* If option is of type --foo-bar, we print its counterpart
                 * as --no-foo-bar, but if it is of type --foobar (without
                 * dashes in the name) we print it as --nofoobar. Both
                 * values are of course valid, only the display changes. */
                psz_prefix = strchr( p_item->psz_name, '-' ) ? ", --no-"
                                                             : ", --no";
                i -= strlen( p_item->psz_name ) + strlen( psz_prefix );
            }

            if( i < 0 )
            {
                psz_spaces[0] = '\n';
                i = 0;
            }
            else
            {
                psz_spaces[i] = '\0';
            }

            if( p_item->i_type == CONFIG_ITEM_BOOL && !b_help_module )
            {
                fprintf( stdout, psz_format, psz_short, p_item->psz_name,
                         psz_prefix, p_item->psz_name, psz_bra, psz_type,
                         psz_ket, psz_spaces );
            }
            else
            {
                fprintf( stdout, psz_format, psz_short, p_item->psz_name,
                         "", "", psz_bra, psz_type, psz_ket, psz_spaces );
            }

            psz_spaces[i] = ' ';

            /* We wrap the rest of the output */
            sprintf( psz_buffer, "%s%s", p_item->psz_text, psz_suf );
            psz_text = psz_buffer;
            while( *psz_text )
            {
                char *psz_parser, *psz_word;
                int i_end = strlen( psz_text );

                /* If the remaining text fits in a line, print it. */
                if( i_end <= i_width )
                {
                    fprintf( stdout, "%s\n", psz_text );
                    break;
                }

                /* Otherwise, eat as many words as possible */
                psz_parser = psz_text;
                do
                {
                    psz_word = psz_parser;
                    psz_parser = strchr( psz_word, ' ' );
                    /* If no space was found, we reached the end of the text
                     * block; otherwise, we skip the space we just found. */
                    psz_parser = psz_parser ? psz_parser + 1
                                            : psz_text + i_end;

                } while( psz_parser - psz_text <= i_width );

                /* We cut a word in one of these cases:
                 *  - it's the only word in the line and it's too long.
                 *  - we used less than 80% of the width and the word we are
                 *    going to wrap is longer than 40% of the width, and even
                 *    if the word would have fit in the next line. */
                if( psz_word == psz_text
                     || ( psz_word - psz_text < 80 * i_width / 100
                           && psz_parser - psz_word > 40 * i_width / 100 ) )
                {
                    char c = psz_text[i_width];
                    psz_text[i_width] = '\0';
                    fprintf( stdout, "%s\n%s", psz_text, psz_spaces );
                    psz_text += i_width;
                    psz_text[0] = c;
                }
                else
                {
                    psz_word[-1] = '\0';
                    fprintf( stdout, "%s\n%s", psz_text, psz_spaces );
                    psz_text = psz_word;
                }
            }
        }
    }

    /* Release the module list */
    vlc_list_release( p_list );

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
    fprintf( stdout, _("\nPress the RETURN key to continue...\n") );
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
    module_t *p_parser;
    char psz_spaces[22];
    int i_index;

    memset( psz_spaces, ' ', 22 );

#ifdef WIN32
    ShowConsole();
#endif

    /* Usage */
    fprintf( stdout, _("Usage: %s [options] [items]...\n\n"),
                     p_this->p_vlc->psz_object_name );

    fprintf( stdout, _("[module]              [description]\n") );

    /* List all modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /* Enumerate each module */
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        int i;

        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        /* Nasty hack, but right now I'm too tired to think about a nice
         * solution */
        i = 22 - strlen( p_parser->psz_object_name ) - 1;
        if( i < 0 ) i = 0;
        psz_spaces[i] = 0;

        fprintf( stdout, "  %s%s %s\n", p_parser->psz_object_name,
                         psz_spaces, p_parser->psz_longname );

        psz_spaces[i] = ' ';
    }

    vlc_list_release( p_list );

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
    fprintf( stdout, _("\nPress the RETURN key to continue...\n") );
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

    fprintf( stdout, VERSION_MESSAGE "\n" );
    fprintf( stdout,
      _("This program comes with NO WARRANTY, to the extent permitted by "
        "law.\nYou may redistribute it under the terms of the GNU General "
        "Public License;\nsee the file named COPYING for details.\n"
        "Written by the VideoLAN team at Ecole Centrale, Paris.\n") );

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
    fprintf( stdout, _("\nPress the RETURN key to continue...\n") );
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
#   ifndef UNDER_CE
    AllocConsole();
    freopen( "CONOUT$", "w", stdout );
    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );
#   endif
    return;
}
#endif

/*****************************************************************************
 * ConsoleWidth: Return the console width in characters
 *****************************************************************************
 * We use the stty shell command to get the console width; if this fails or
 * if the width is less than 80, we default to 80.
 *****************************************************************************/
static int ConsoleWidth( void )
{
    int i_width = 80;

#ifndef WIN32
    char buf[20], *psz_parser;
    FILE *file;
    int i_ret;

    file = popen( "stty size 2>/dev/null", "r" );
    if( file )
    {
        i_ret = fread( buf, 1, 20, file );
        if( i_ret > 0 )
        {
            buf[19] = '\0';
            psz_parser = strchr( buf, ' ' );
            if( psz_parser )
            {
                i_ret = atoi( psz_parser + 1 );
                if( i_ret >= 80 )
                {
                    i_width = i_ret;
                }
            }
        }

        pclose( file );
    }
#endif

    return i_width;
}
