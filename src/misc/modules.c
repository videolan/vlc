/*****************************************************************************
 * modules.c : Built-in and plugin modules management functions
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: modules.c,v 1.38 2001/07/11 02:01:05 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
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
#include "defs.h"

#include "config.h"

/* Some faulty libcs have a broken struct dirent when _FILE_OFFSET_BITS
 * is set to 64. Don't try to be cleverer. */
#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                              /* strdup() */

#if !defined( _MSC_VER )
#include <dirent.h>
#endif

#if defined(HAVE_DLFCN_H)                                /* Linux, BSD, Hurd */
#   include <dlfcn.h>                        /* dlopen(), dlsym(), dlclose() */
#   define HAVE_DYNAMIC_PLUGINS
#elif defined(HAVE_IMAGE_H)                                          /* BeOS */
#   include <image.h>
#   define HAVE_DYNAMIC_PLUGINS
#elif defined(WIN32) && defined( __MINGW32__ )
#   define HAVE_DYNAMIC_PLUGINS
#else
#   undef HAVE_DYNAMIC_PLUGINS
#endif

#ifdef SYS_BEOS
#   include "beos_specific.h"
#endif

#ifdef SYS_DARWIN
#   include "darwin_specific.h"
#endif

#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"
#include "netutils.h"
#include "modules.h"

#include "interface.h"
#include "intf_msg.h"
#include "intf_playlist.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input.h"
#include "input_netlist.h"
#include "mpeg_system.h"

#include "video.h"
#include "video_output.h"

#include "audio_output.h"

#ifdef HAVE_DYNAMIC_PLUGINS
#   include "modules_core.h"
#endif
#include "modules_builtin.h"
#include "modules_export.h"

#include "main.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
static int AllocatePluginModule ( char * );
#endif
#ifdef ALLOCATE_ALL_BUILTINS
static int AllocateBuiltinModule( int ( * ) ( module_t * ),
                                  int ( * ) ( module_t * ),
                                  int ( * ) ( module_t * ) );
#endif
static int DeleteModule ( module_t * );
static int LockModule   ( module_t * );
static int UnlockModule ( module_t * );
#ifdef HAVE_DYNAMIC_PLUGINS
static int HideModule   ( module_t * );
static int CallSymbol   ( module_t *, char * );
#endif

static module_symbols_t symbols;

/*****************************************************************************
 * module_InitBank: create the module bank.
 *****************************************************************************
 * This function creates a module bank structure and fills it with the
 * built-in modules, as well as all the plugin modules it can find.
 *****************************************************************************/
void module_InitBank( void )
{
#ifdef HAVE_DYNAMIC_PLUGINS
    static char * path[] = { ".", "plugins", PLUGIN_PATH, NULL, NULL };

    char **         ppsz_path = path;
    char *          psz_fullpath;
    char *          psz_file;
#if defined( SYS_BEOS ) || defined( SYS_DARWIN )
    char *          psz_vlcpath = system_GetProgramPath();
    int             i_vlclen = strlen( psz_vlcpath );
    boolean_t       b_notinroot;
#endif
    DIR *           dir;
    struct dirent * file;
#endif /* HAVE_DYNAMIC_PLUGINS */

    p_module_bank->first = NULL;
    vlc_mutex_init( &p_module_bank->lock );

    /*
     * Store the symbols to be exported
     */
    STORE_SYMBOLS( &symbols );

    /*
     * Check all the built-in modules
     */
#ifdef ALLOCATE_ALL_BUILTINS
    intf_WarnMsg( 2, "module: checking built-in modules" );

    ALLOCATE_ALL_BUILTINS();
#endif

    /*
     * Check all the plugin modules we can find
     */
#ifdef HAVE_DYNAMIC_PLUGINS
    intf_WarnMsg( 2, "module: checking plugin modules" );

    for( ; *ppsz_path != NULL ; ppsz_path++ )
    {
        /* Store strlen(*ppsz_path) for later use. */
        int i_dirlen = strlen( *ppsz_path );

#if defined( SYS_BEOS ) || defined( SYS_DARWIN )
        b_notinroot = 0;
        /* Under BeOS, we need to add beos_GetProgramPath() to access
         * files under the current directory */
        if( ( i_dirlen > 1 ) && strncmp( *ppsz_path, "/", 1 ) )
        {
            i_dirlen += i_vlclen + 2;
            b_notinroot = 1;

            psz_fullpath = malloc( i_dirlen );
            if( psz_fullpath == NULL )
            {
                continue;
            }
            sprintf( psz_fullpath, "%s/%s", psz_vlcpath, *ppsz_path );
        }
        else
#endif
        {
            psz_fullpath = *ppsz_path;
        }

        intf_WarnMsgImm( 1, "module: browsing `%s'", psz_fullpath );

        if( (dir = opendir( psz_fullpath )) )
        {
            /* Parse the directory and try to load all files it contains. */
            while( (file = readdir( dir )) )
            {
                int i_filelen = strlen( file->d_name );

                /* We only load files ending with ".so" */
                if( i_filelen > 3
                        && !strncmp( file->d_name + i_filelen - 3, ".so", 3 ) )
                {
                    psz_file = malloc( i_dirlen + i_filelen + 2 );
                    if( psz_file == NULL )
                    {
                        continue;
                    }
                    sprintf( psz_file, "%s/%s", psz_fullpath, file->d_name );

                    /* We created a nice filename -- now we just try to load
                     * it as a plugin module. */
                    AllocatePluginModule( psz_file );

                    /* We don't care if the allocation succeeded */
                    free( psz_file );
                }
            }

            /* Close the directory if successfully opened */
            closedir( dir );
        }

#if defined( SYS_BEOS ) || defined( SYS_DARWIN )
        if( b_notinroot )
        {
            free( psz_fullpath );
        }
#endif
    }
#endif /* HAVE_DYNAMIC_PLUGINS */

    intf_WarnMsg( 3, "module: module bank initialized" );

    return;
}

/*****************************************************************************
 * module_EndBank: empty the module bank.
 *****************************************************************************
 * This function unloads all unused plugin modules and empties the module
 * bank in case of success.
 *****************************************************************************/
void module_EndBank( void )
{
    module_t * p_next;

    while( p_module_bank->first != NULL )
    {
        if( DeleteModule( p_module_bank->first ) )
        {
            /* Module deletion failed */
            intf_ErrMsg( "module error: `%s' can't be removed. trying harder.",
                         p_module_bank->first->psz_name );

            /* We just free the module by hand. Niahahahahaha. */
            p_next = p_module_bank->first->next;
            free(p_module_bank->first);
            p_module_bank->first = p_next;
        }
    }

    /* Destroy the lock */
    vlc_mutex_destroy( &p_module_bank->lock );

    return;
}

/*****************************************************************************
 * module_ResetBank: reset the module bank.
 *****************************************************************************
 * This function resets the module bank by unloading all unused plugin
 * modules.
 *****************************************************************************/
void module_ResetBank( void )
{
    intf_ErrMsg( "FIXME: module_ResetBank unimplemented" );
    return;
}

/*****************************************************************************
 * module_ManageBank: manage the module bank.
 *****************************************************************************
 * This function parses the module bank and hides modules that have been
 * unused for a while.
 *****************************************************************************/
void module_ManageBank( void )
{
#ifdef HAVE_DYNAMIC_PLUGINS
    module_t * p_module;

    /* We take the global lock */
    vlc_mutex_lock( &p_module_bank->lock );

    /* Parse the module list to see if any modules need to be unloaded */
    for( p_module = p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        /* If the module is unused and if it is a plugin module... */
        if( p_module->i_usage == 0 && !p_module->b_builtin )
        {
            if( p_module->i_unused_delay < MODULE_HIDE_DELAY )
            {
                p_module->i_unused_delay++;
            }
            else
            {
                intf_WarnMsg( 3, "module: hiding unused plugin module `%s'",
                              p_module->psz_name );
                HideModule( p_module );

                /* Break here, so that we only hide one module at a time */
                break;
            }
        }
    }

    /* We release the global lock */
    vlc_mutex_unlock( &p_module_bank->lock );
#endif /* HAVE_DYNAMIC_PLUGINS */

    return;
}

/*****************************************************************************
 * module_Need: return the best module function, given a capability list.
 *****************************************************************************
 * This function returns the module that best fits the asked capabilities.
 *****************************************************************************/
module_t * module_Need( int i_capabilities, void *p_data )
{
    module_t * p_module;
    module_t * p_bestmodule = NULL;
    int i_score, i_totalscore, i_bestscore = 0;
    int i_index;

    /* We take the global lock */
    vlc_mutex_lock( &p_module_bank->lock );

    /* Parse the module list for capabilities and probe each of them */
    for( p_module = p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        /* Test that this module can do everything we need */
        if( ( p_module->i_capabilities & i_capabilities ) == i_capabilities )
        {
            i_totalscore = 0;

            LockModule( p_module );

            /* Parse all the requested capabilities and test them */
            for( i_index = 0 ; (1 << i_index) <= i_capabilities ; i_index++ )
            {
                if( ( (1 << i_index) & i_capabilities ) )
                {
                    i_score = ( (function_list_t *)p_module->p_functions)
                                                  [i_index].pf_probe( p_data );

                    if( i_score )
                    {
                        i_totalscore += i_score;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            /* If the high score was broken, we have a new champion */
            if( i_totalscore > i_bestscore )
            {
                /* Keep the current module locked, but release the previous */
                if( p_bestmodule != NULL )
                {
                    UnlockModule( p_bestmodule );
                }

                /* This is the new best module */
                i_bestscore = i_totalscore;
                p_bestmodule = p_module;
            }
            else
            {
                /* This module wasn't interesting, unlock it and forget it */
                UnlockModule( p_module );
            }
        }
    }

    /* We can release the global lock, module refcount was incremented */
    vlc_mutex_unlock( &p_module_bank->lock );

    if( p_bestmodule != NULL )
    {
        intf_WarnMsg( 1, "module: locking module `%s'",
                      p_bestmodule->psz_name );
    }

    /* Don't forget that the module is still locked if bestmodule != NULL */
    return( p_bestmodule );
}

/*****************************************************************************
 * module_Unneed: decrease the usage count of a module.
 *****************************************************************************
 * This function must be called by the thread that called module_Need, to
 * decrease the reference count and allow for hiding of modules.
 *****************************************************************************/
void module_Unneed( module_t * p_module )
{
    /* We take the global lock */
    vlc_mutex_lock( &p_module_bank->lock );

    /* Just unlock the module - we can't do anything if it fails,
     * so there is no need to check the return value. */
    UnlockModule( p_module );

    intf_WarnMsg( 1, "module: unlocking module `%s'", p_module->psz_name );

    /* We release the global lock */
    vlc_mutex_unlock( &p_module_bank->lock );

    return;
}

/*****************************************************************************
 * Following functions are local.
 *****************************************************************************/

#ifdef HAVE_DYNAMIC_PLUGINS
/*****************************************************************************
 * AllocatePluginModule: load a module into memory and initialize it.
 *****************************************************************************
 * This function loads a dynamically loadable module and allocates a structure
 * for its information data. The module can then be handled by module_Need,
 * module_Unneed and HideModule. It can be removed by DeleteModule.
 *****************************************************************************/
static int AllocatePluginModule( char * psz_filename )
{
    module_t * p_module, * p_othermodule;
    module_handle_t handle;

    /* Try to dynamically load the module. */
    if( module_load( psz_filename, &handle ) )
    {
        /* The plugin module couldn't be opened */
        intf_WarnMsgImm( 1, "module warning: cannot open %s (%s)",
                         psz_filename, module_error() );
        return( -1 );
    }

    /* Now that we have successfully loaded the module, we can
     * allocate a structure for it */ 
    p_module = malloc( sizeof( module_t ) );
    if( p_module == NULL )
    {
        intf_ErrMsg( "module error: can't allocate p_module" );
        module_unload( handle );
        return( -1 );
    }

    /* We need to fill these since they may be needed by CallSymbol() */
    p_module->is.plugin.psz_filename = psz_filename;
    p_module->is.plugin.handle = handle;
    p_module->p_symbols = &symbols;

    /* Initialize the module : fill p_module->psz_name, etc. */
    if( CallSymbol( p_module, "InitModule" ) != 0 )
    {
        /* We couldn't call InitModule() */
        free( p_module );
        module_unload( handle );
        return( -1 );
    }

    /* Check that version numbers match */
    if( strcmp( VERSION, p_module->psz_version ) )
    {
        free( p_module );
        module_unload( handle );
        return( -1 );
    }

    /* Check that we don't already have a module with this name */
    for( p_othermodule = p_module_bank->first ;
         p_othermodule != NULL ;
         p_othermodule = p_othermodule->next )
    {
        if( !strcmp( p_othermodule->psz_name, p_module->psz_name ) )
        {
            free( p_module );
            module_unload( handle );
            return( -1 );
        }
    }

    /* Activate the module : fill the capability structure, etc. */
    if( CallSymbol( p_module, "ActivateModule" ) != 0 )
    {
        /* We couldn't call ActivateModule() */
        free( p_module );
        module_unload( handle );
        return( -1 );
    }

    /* We strdup() these entries so that they are still valid when the
     * module is unloaded. */
    p_module->is.plugin.psz_filename =
            strdup( p_module->is.plugin.psz_filename );
    p_module->psz_name = strdup( p_module->psz_name );
    p_module->psz_longname = strdup( p_module->psz_longname );
    p_module->psz_version = strdup( p_module->psz_version );
    if( p_module->is.plugin.psz_filename == NULL 
            || p_module->psz_name == NULL
            || p_module->psz_longname == NULL
            || p_module->psz_version == NULL )
    {
        intf_ErrMsg( "module error: can't duplicate strings" );

        if( p_module->is.plugin.psz_filename != NULL )
        {
            free( p_module->is.plugin.psz_filename );
        }

        if( p_module->psz_name != NULL )
        {
            free( p_module->psz_name );
        }

        if( p_module->psz_longname != NULL )
        {
            free( p_module->psz_longname );
        }

        if( p_module->psz_version != NULL )
        {
            free( p_module->psz_version );
        }

        free( p_module );
        module_unload( handle );
        return( -1 );
    }

    /* Everything worked fine ! The module is ready to be added to the list. */
    p_module->i_usage = 0;
    p_module->i_unused_delay = 0;

    p_module->b_builtin = 0;

    /* Link module into the linked list */
    if( p_module_bank->first != NULL )
    {
        p_module_bank->first->prev = p_module;
    }
    p_module->next = p_module_bank->first;
    p_module->prev = NULL;
    p_module_bank->first = p_module;

    /* Immediate message so that a slow module doesn't make the user wait */
    intf_WarnMsgImm( 2, "module: new plugin module `%s', %s",
                     p_module->psz_name, p_module->psz_longname );

    return( 0 );
}
#endif /* HAVE_DYNAMIC_PLUGINS */

#ifdef ALLOCATE_ALL_BUILTINS
/*****************************************************************************
 * AllocateBuiltinModule: initialize a built-in module.
 *****************************************************************************
 * This function registers a built-in module and allocates a structure
 * for its information data. The module can then be handled by module_Need,
 * module_Unneed and HideModule. It can be removed by DeleteModule.
 *****************************************************************************/
static int AllocateBuiltinModule( int ( *pf_init ) ( module_t * ),
                                  int ( *pf_activate ) ( module_t * ),
                                  int ( *pf_deactivate ) ( module_t * ) )
{
    module_t * p_module, * p_othermodule;

    /* Now that we have successfully loaded the module, we can
     * allocate a structure for it */ 
    p_module = malloc( sizeof( module_t ) );
    if( p_module == NULL )
    {
        intf_ErrMsg( "module error: can't allocate p_module" );
        return( -1 );
    }

    /* Initialize the module : fill p_module->psz_name, etc. */
    if( pf_init( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        intf_ErrMsg( "module error: failed calling init in builtin module" );
        free( p_module );
        return( -1 );
    }

    /* Check that version numbers match */
    if( strcmp( VERSION, p_module->psz_version ) )
    {
        free( p_module );
        return( -1 );
    }

    /* Check that we don't already have a module with this name */
    for( p_othermodule = p_module_bank->first ;
         p_othermodule != NULL ;
         p_othermodule = p_othermodule->next )
    {
        if( !strcmp( p_othermodule->psz_name, p_module->psz_name ) )
        {
            free( p_module );
            return( -1 );
        }
    }

    if( pf_activate( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        intf_ErrMsg( "module error: failed calling activate "
                     "in builtin module" );
        free( p_module );
        return( -1 );
    }

    /* We strdup() these entries so that they are still valid when the
     * module is unloaded. */
    p_module->psz_name = strdup( p_module->psz_name );
    p_module->psz_longname = strdup( p_module->psz_longname );
    p_module->psz_version = strdup( p_module->psz_version );
    if( p_module->psz_name == NULL 
            || p_module->psz_longname == NULL
            || p_module->psz_version == NULL )
    {
        intf_ErrMsg( "module error: can't duplicate strings" );

        if( p_module->psz_name != NULL )
        {
            free( p_module->psz_name );
        }

        if( p_module->psz_longname != NULL )
        {
            free( p_module->psz_longname );
        }

        if( p_module->psz_version != NULL )
        {
            free( p_module->psz_version );
        }

        free( p_module );
        return( -1 );
    }

    /* Everything worked fine ! The module is ready to be added to the list. */
    p_module->i_usage = 0;
    p_module->i_unused_delay = 0;

    p_module->b_builtin = 1;
    p_module->is.builtin.pf_deactivate = pf_deactivate;

    /* Link module into the linked list */
    if( p_module_bank->first != NULL )
    {
        p_module_bank->first->prev = p_module;
    }
    p_module->next = p_module_bank->first;
    p_module->prev = NULL;
    p_module_bank->first = p_module;

    /* Immediate message so that a slow module doesn't make the user wait */
    intf_WarnMsgImm( 2, "module: new builtin module `%s', %s",
                     p_module->psz_name, p_module->psz_longname );

    return( 0 );
}
#endif /* ALLOCATE_ALL_BUILTINS */

/*****************************************************************************
 * DeleteModule: delete a module and its structure.
 *****************************************************************************
 * This function can only be called if i_usage <= 0.
 *****************************************************************************/
static int DeleteModule( module_t * p_module )
{
    /* If the module is not in use but is still in memory, we first have
     * to hide it and remove it from memory before we can free the
     * data structure. */
    if( p_module->b_builtin )
    {
        if( p_module->i_usage != 0 )
        {
            intf_ErrMsg( "module error: trying to free builtin module `%s' with"
                         " usage %i", p_module->psz_name, p_module->i_usage );
            return( -1 );
        }
        else
        {
            /* We deactivate the module now. */
            p_module->is.builtin.pf_deactivate( p_module );
        }
    }
#ifdef HAVE_DYNAMIC_PLUGINS
    else
    {
        if( p_module->i_usage >= 1 )
        {
            intf_ErrMsg( "module error: trying to free module `%s' which is"
                         " still in use", p_module->psz_name );
            return( -1 );
        }

        /* Two possibilities here: i_usage == -1 and the module is already
         * unloaded, we can continue, or i_usage == 0, and we have to hide
         * the module before going on. */
        if( p_module->i_usage == 0 )
        {
            if( HideModule( p_module ) != 0 )
            {
                return( -1 );
            }
        }
    }
#endif

    /* Unlink the module from the linked list. */
    if( p_module == p_module_bank->first )
    {
        p_module_bank->first = p_module->next;
    }

    if( p_module->prev != NULL )
    {
        p_module->prev->next = p_module->next;
    }

    if( p_module->next != NULL )
    {
        p_module->next->prev = p_module->prev;
    }

    /* We free the structures that we strdup()ed in Allocate*Module(). */
#ifdef HAVE_DYNAMIC_PLUGINS
    if( !p_module->b_builtin )
    {
        free( p_module->is.plugin.psz_filename );
    }
#endif
    free( p_module->psz_name );
    free( p_module->psz_longname );
    free( p_module->psz_version );

    free( p_module );

    return( 0 );
}

/*****************************************************************************
 * LockModule: increase the usage count of a module and load it if needed.
 *****************************************************************************
 * This function has to be called before a thread starts using a module. If
 * the module is already loaded, we just increase its usage count. If it isn't
 * loaded, we have to dynamically open it and initialize it.
 * If you successfully call LockModule() at any moment, be careful to call
 * UnlockModule() when you don't need it anymore.
 *****************************************************************************/
static int LockModule( module_t * p_module )
{
    if( p_module->i_usage >= 0 )
    {
        /* This module is already loaded and activated, we can return */
        p_module->i_usage++;
        return( 0 );
    }

    if( p_module->b_builtin )
    {
        /* A built-in module should always have a refcount >= 0 ! */
        intf_ErrMsg( "module error: built-in module `%s' has refcount %i",
                     p_module->psz_name, p_module->i_usage );
        return( -1 );
    }

#ifdef HAVE_DYNAMIC_PLUGINS
    if( p_module->i_usage != -1 )
    {
        /* This shouldn't happen. Ever. We have serious problems here. */
        intf_ErrMsg( "module error: plugin module `%s' has refcount %i",
                     p_module->psz_name, p_module->i_usage );
        return( -1 );
    }

    /* i_usage == -1, which means that the module isn't in memory */
    if( module_load( p_module->is.plugin.psz_filename,
                     &p_module->is.plugin.handle ) )
    {
        /* The plugin module couldn't be opened */
        intf_ErrMsg( "module error: cannot open %s (%s)",
                     p_module->is.plugin.psz_filename, module_error() );
        return( -1 );
    }

    /* FIXME: what to do if the guy modified the plugin while it was
     * unloaded ? It makes XMMS crash nastily, perhaps we should try
     * to be a bit more clever here. */

    /* Activate the module : fill the capability structure, etc. */
    if( CallSymbol( p_module, "ActivateModule" ) != 0 )
    {
        /* We couldn't call ActivateModule() -- looks nasty, but
         * we can't do much about it. Just try to unload module. */
        module_unload( p_module->is.plugin.handle );
        p_module->i_usage = -1;
        return( -1 );
    }

    /* Everything worked fine ! The module is ready to be used */
    p_module->i_usage = 1;
#endif /* HAVE_DYNAMIC_PLUGINS */

    return( 0 );
}

/*****************************************************************************
 * UnlockModule: decrease the usage count of a module.
 *****************************************************************************
 * We decrease the usage count of a module so that we know when a module
 * becomes unused and can be hidden.
 *****************************************************************************/
static int UnlockModule( module_t * p_module )
{
    if( p_module->i_usage <= 0 )
    {
        /* This shouldn't happen. Ever. We have serious problems here. */
        intf_ErrMsg( "module error: trying to call module_Unneed() on `%s'"
                     " which isn't even in use", p_module->psz_name );
        return( -1 );
    }

    /* This module is still in use, we can return */
    p_module->i_usage--;
    p_module->i_unused_delay = 0;

    return( 0 );
}

#ifdef HAVE_DYNAMIC_PLUGINS
/*****************************************************************************
 * HideModule: remove a module from memory but keep its structure.
 *****************************************************************************
 * This function can only be called if i_usage == 0. It will make a call
 * to the module's inner DeactivateModule() symbol, and then unload it
 * from memory. A call to module_Need() will automagically load it again.
 *****************************************************************************/
static int HideModule( module_t * p_module )
{
    if( p_module->b_builtin )
    {
        /* A built-in module should never be hidden. */
        intf_ErrMsg( "module error: trying to hide built-in module `%s'",
                     p_module->psz_name );
        return( -1 );
    }

    if( p_module->i_usage >= 1 )
    {
        intf_ErrMsg( "module error: trying to hide module `%s' which is still"
                     " in use", p_module->psz_name );
        return( -1 );
    }

    if( p_module->i_usage <= -1 )
    {
        intf_ErrMsg( "module error: trying to hide module `%s' which is already"
                     " hidden", p_module->psz_name );
        return( -1 );
    }

    /* Deactivate the module : free the capability structure, etc. */
    if( CallSymbol( p_module, "DeactivateModule" ) != 0 )
    {
        /* We couldn't call DeactivateModule() -- looks nasty, but
         * we can't do much about it. Just try to unload module anyway. */
        module_unload( p_module->is.plugin.handle );
        p_module->i_usage = -1;
        return( -1 );
    }

    /* Everything worked fine, we can safely unload the module. */
    module_unload( p_module->is.plugin.handle );
    p_module->i_usage = -1;

    return( 0 );
}

/*****************************************************************************
 * CallSymbol: calls a module symbol.
 *****************************************************************************
 * This function calls a symbol given its name and a module structure. The
 * symbol MUST refer to a function returning int and taking a module_t* as
 * an argument.
 *****************************************************************************/
static int CallSymbol( module_t * p_module, char * psz_name )
{
    int (* pf_symbol) ( module_t * p_module );

    /* Try to resolve the symbol */
    pf_symbol = module_getsymbol( p_module->is.plugin.handle, psz_name );

    if( pf_symbol == NULL )
    {
        /* We couldn't load the symbol */
        intf_WarnMsg( 1, "module warning: "
                         "cannot find symbol %s in module %s (%s)",
                         psz_name, p_module->is.plugin.psz_filename,
                         module_error() );
        return( -1 );
    }

    /* We can now try to call the symbol */
    if( pf_symbol( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        intf_ErrMsg( "module error: failed calling symbol %s in module %s",
                     psz_name, p_module->is.plugin.psz_filename );
        return( -1 );
    }

    /* Everything worked fine, we can return */
    return( 0 );
}
#endif /* HAVE_DYNAMIC_PLUGINS */

