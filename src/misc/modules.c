/*****************************************************************************
 * modules.c : Builtin and plugin modules management functions
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: modules.c,v 1.63 2002/06/01 12:32:01 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
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

/* Some faulty libcs have a broken struct dirent when _FILE_OFFSET_BITS
 * is set to 64. Don't try to be cleverer. */
#ifdef _FILE_OFFSET_BITS
#undef _FILE_OFFSET_BITS
#endif

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                              /* strdup() */

#include <vlc/vlc.h>

#include <dirent.h>

#if defined(HAVE_DLFCN_H)                                /* Linux, BSD, Hurd */
#   include <dlfcn.h>                        /* dlopen(), dlsym(), dlclose() */
#   define HAVE_DYNAMIC_PLUGINS
#elif defined(HAVE_IMAGE_H)                                          /* BeOS */
#   include <image.h>
#   define HAVE_DYNAMIC_PLUGINS
#elif defined(WIN32)
#   define HAVE_DYNAMIC_PLUGINS
#else
#   undef HAVE_DYNAMIC_PLUGINS
#endif


#include "netutils.h"

#include "interface.h"
#include "playlist.h"
#include "intf_eject.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "video.h"
#include "video_output.h"

#include "audio_output.h"

#include "iso_lang.h"

#ifdef HAVE_DYNAMIC_PLUGINS
#   include "modules_plugin.h"
#endif

#if !defined( _MSC_VER )
#    include "modules_builtin.h"
#else
#    include "modules_builtin_msvc.h"
#endif

/*****************************************************************************
 * Static variables
 *****************************************************************************/
DECLARE_MODULE_CAPABILITY_TABLE;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
static void AllocateAllPlugins   ( vlc_object_t * );
static int  AllocatePluginModule ( vlc_object_t *, char * );
#endif
static int  AllocateBuiltinModule( vlc_object_t *,
                                   int ( * ) ( module_t * ),
                                   int ( * ) ( module_t * ),
                                   int ( * ) ( module_t * ) );
static int  DeleteModule ( module_t * );
static int  LockModule   ( module_t * );
static int  UnlockModule ( module_t * );
#ifdef HAVE_DYNAMIC_PLUGINS
static int  HideModule   ( module_t * );
static int  CallSymbol   ( module_t *, char * );
#endif

#ifdef HAVE_DYNAMIC_PLUGINS
static module_symbols_t symbols;
#endif

/*****************************************************************************
 * module_InitBank: create the module bank.
 *****************************************************************************
 * This function creates a module bank structure which will be filled later
 * on with all the modules found.
 *****************************************************************************/
void module_InitBank( vlc_object_t *p_this )
{
    p_this->p_vlc->module_bank.first = NULL;
    p_this->p_vlc->module_bank.i_count = 0;
    vlc_mutex_init( p_this, &p_this->p_vlc->module_bank.lock );

    /*
     * Store the symbols to be exported
     */
#ifdef HAVE_DYNAMIC_PLUGINS
    STORE_SYMBOLS( &symbols );
#endif

    return;
}

/*****************************************************************************
 * module_ResetBank: reset the module bank.
 *****************************************************************************
 * This function resets the module bank by unloading all unused plugin
 * modules.
 *****************************************************************************/
void module_ResetBank( vlc_object_t *p_this )
{
    msg_Err( p_this, "FIXME: module_ResetBank unimplemented" );
    return;
}

/*****************************************************************************
 * module_EndBank: empty the module bank.
 *****************************************************************************
 * This function unloads all unused plugin modules and empties the module
 * bank in case of success.
 *****************************************************************************/
void module_EndBank( vlc_object_t *p_this )
{
    module_t * p_next;

    while( p_this->p_vlc->module_bank.first != NULL )
    {
        if( DeleteModule( p_this->p_vlc->module_bank.first ) )
        {
            /* Module deletion failed */
            msg_Err( p_this, "`%s' can't be removed, trying harder",
                     p_this->p_vlc->module_bank.first->psz_object_name );

            /* We just free the module by hand. Niahahahahaha. */
            p_next = p_this->p_vlc->module_bank.first->next;
            free( p_this->p_vlc->module_bank.first );
            p_this->p_vlc->module_bank.first = p_next;
        }
    }

    /* Destroy the lock */
    vlc_mutex_destroy( &p_this->p_vlc->module_bank.lock );

    return;
}

/*****************************************************************************
 * module_LoadMain: load the main program info into the module bank.
 *****************************************************************************
 * This function fills the module bank structure with the main module infos.
 * This is very useful as it will allow us to consider the main program just
 * as another module, and for instance the configuration options of main will
 * be available in the module bank structure just as for every other module.
 *****************************************************************************/
void module_LoadMain( vlc_object_t *p_this )
{
    AllocateBuiltinModule( p_this, InitModule__MODULE_main,
                                   ActivateModule__MODULE_main,
                                   DeactivateModule__MODULE_main );
}

/*****************************************************************************
 * module_LoadBuiltins: load all modules which we built with.
 *****************************************************************************
 * This function fills the module bank structure with the builtin modules.
 *****************************************************************************/
void module_LoadBuiltins( vlc_object_t * p_this )
{
    msg_Dbg( p_this, "checking builtin modules" );
    ALLOCATE_ALL_BUILTINS();
}

/*****************************************************************************
 * module_LoadPlugins: load all plugin modules we can find.
 *****************************************************************************
 * This function fills the module bank structure with the plugin modules.
 *****************************************************************************/
void module_LoadPlugins( vlc_object_t * p_this )
{
#ifdef HAVE_DYNAMIC_PLUGINS
    msg_Dbg( p_this, "checking plugin modules" );
    AllocateAllPlugins( p_this );
#endif
}

/*****************************************************************************
 * module_ManageBank: manage the module bank.
 *****************************************************************************
 * This function parses the module bank and hides modules that have been
 * unused for a while.
 *****************************************************************************/
void module_ManageBank( vlc_object_t *p_this )
{
#ifdef HAVE_DYNAMIC_PLUGINS
    module_t * p_module;

    /* We take the global lock */
    vlc_mutex_lock( &p_this->p_vlc->module_bank.lock );

    /* Parse the module list to see if any modules need to be unloaded */
    for( p_module = p_this->p_vlc->module_bank.first ;
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
                msg_Dbg( p_this, "hiding unused plugin module `%s'",
                                 p_module->psz_object_name );
                HideModule( p_module );

                /* Break here, so that we only hide one module at a time */
                break;
            }
        }
    }

    /* We release the global lock */
    vlc_mutex_unlock( &p_this->p_vlc->module_bank.lock );
#endif /* HAVE_DYNAMIC_PLUGINS */

    return;
}

/*****************************************************************************
 * module_Need: return the best module function, given a capability list.
 *****************************************************************************
 * This function returns the module that best fits the asked capabilities.
 *****************************************************************************/
module_t * __module_Need( vlc_object_t *p_this,
                          int i_capability, char *psz_name, void *p_data )
{
    typedef struct module_list_s module_list_t;

    struct module_list_s
    {
        module_t *p_module;
        module_list_t *p_next;
    };

    module_list_t *p_list, *p_first, *p_tmp;

    int i_ret, i_index = 0;
    vlc_bool_t b_intf = 0;

    module_t *p_module;
    char     *psz_realname = NULL;

    msg_Info( p_this, "looking for %s module",
                      MODULE_CAPABILITY( i_capability ) );

    /* We take the global lock */
    vlc_mutex_lock( &p_this->p_vlc->module_bank.lock );

    if( psz_name != NULL && *psz_name )
    {
        /* A module name was requested. */
        psz_realname = strdup( psz_name );
        if( psz_realname )
        {
            char *p;
            p = strchr( psz_realname, ':' );
            if( p )
            {
                *p = '\0';
            }
            psz_name = psz_realname;
        }
    }

    /* Sort the modules and test them */
    p_list = malloc( p_this->p_vlc->module_bank.i_count
                      * sizeof( module_list_t ) );
    p_first = NULL;

    /* Parse the module list for capabilities and probe each of them */
    for( p_module = p_this->p_vlc->module_bank.first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        /* Test that this module can do everything we need */
        if( !(p_module->i_capabilities & ( 1 << i_capability )) )
        {
            continue;
        }

        /* Test if we have the required CPU */
        if( (p_module->i_cpu_capabilities
                & p_this->p_vlc->i_cpu_capabilities)
              != p_module->i_cpu_capabilities )
        {
            continue;
        }

        /* If we required a shortcut, check this plugin provides it. */
        if( psz_name != NULL && *psz_name )
        {
            vlc_bool_t b_trash = 1;
            int i_dummy;

            for( i_dummy = 0;
                 b_trash && p_module->pp_shortcuts[i_dummy];
                 i_dummy++ )
            {
                b_trash = strcmp( psz_name, p_module->pp_shortcuts[i_dummy] );
            }

            if( b_trash )
            {
                continue;
            }
        }
        /* If we didn't require a shortcut, trash zero-scored plugins */
        else if( !p_module->pi_score[i_capability] )
        {
            continue;
        }

        /* Special case: test if we requested a particular intf plugin */
        if( i_capability == MODULE_CAPABILITY_INTF )
        {
            if( p_module->psz_program != NULL
                 && !strcmp( p_module->psz_program,
                             p_this->p_vlc->psz_object_name ) )
            {
                if( !b_intf ) 
                {
                    /* Remove previous non-matching plugins */
                    i_index = 0;
                    b_intf = 1;
                }
            }
            else
            {
                if( b_intf )
                {
                    /* This one doesn't match */
                    continue;
                }
            }
        }

        /* Store this new module */
        p_list[ i_index ].p_module = p_module;

        /* Add it to the modules-to-probe list */
        if( i_index == 0 )
        {
            p_list[ 0 ].p_next = NULL;
            p_first = p_list;
        }
        else
        {
            /* Ok, so at school you learned that quicksort is quick, and
             * bubble sort sucks raw eggs. But that's when dealing with
             * thousands of items. Here we have barely 50. */
            module_list_t *p_newlist = p_first;

            if( p_first->p_module->pi_score[i_capability]
                 < p_module->pi_score[i_capability] )
            {
                p_list[ i_index ].p_next = p_first;
                p_first = &p_list[ i_index ];
            }
            else
            {
                while( p_newlist->p_next != NULL
                        && p_newlist->p_next->p_module->pi_score[i_capability]
                            >= p_module->pi_score[i_capability] )
                {
                    p_newlist = p_newlist->p_next;
                }

                p_list[ i_index ].p_next = p_newlist->p_next;
                p_newlist->p_next = &p_list[ i_index ];
            }
        }

        i_index++;
    }

    /* Lock all selected modules */
    p_tmp = p_first;
    while( p_tmp != NULL )
    {
        LockModule( p_tmp->p_module );
        p_tmp = p_tmp->p_next;
    }

    /* We can release the global lock, module refcounts were incremented */
    vlc_mutex_unlock( &p_this->p_vlc->module_bank.lock );

    /* Parse the linked list and use the first successful module */
    p_tmp = p_first;
    while( p_tmp != NULL )
    {
        /* Test the requested capability */
        switch( i_capability )
        {
            case MODULE_CAPABILITY_ACCESS:
                i_ret = p_tmp->p_module->p_functions->access.functions.
                              access.pf_open( (input_thread_t *)p_data );
                break;

            case MODULE_CAPABILITY_DEMUX:
                i_ret = p_tmp->p_module->p_functions->demux.functions.
                              demux.pf_init( (input_thread_t *)p_data );
                break;

            case MODULE_CAPABILITY_NETWORK:
                i_ret = p_tmp->p_module->p_functions->network.functions.
                         network.pf_open( p_this, (network_socket_t *)p_data );
                break;

            case MODULE_CAPABILITY_DECODER:
                i_ret = p_tmp->p_module->p_functions->dec.functions.
                              dec.pf_probe( (u8 *)p_data );
                break;

            case MODULE_CAPABILITY_INTF:
                i_ret = p_tmp->p_module->p_functions->intf.functions.
                              intf.pf_open( (intf_thread_t *)p_data );
                break;

            case MODULE_CAPABILITY_AOUT:
                i_ret = p_tmp->p_module->p_functions->aout.functions.
                              aout.pf_open( (aout_thread_t *)p_data );
                break;

            case MODULE_CAPABILITY_VOUT:
                i_ret = p_tmp->p_module->p_functions->vout.functions.
                              vout.pf_create( (vout_thread_t *)p_data );
                break;

            case MODULE_CAPABILITY_CHROMA:
                i_ret = p_tmp->p_module->p_functions->chroma.functions.
                              chroma.pf_init( (vout_thread_t *)p_data );
                break;

            case MODULE_CAPABILITY_IDCT:
            case MODULE_CAPABILITY_IMDCT:
            case MODULE_CAPABILITY_MOTION:
            case MODULE_CAPABILITY_DOWNMIX:
            case MODULE_CAPABILITY_MEMCPY:
                /* This one always works */
                i_ret = 0;
                break;

            default:
                msg_Err( p_this, "unknown module type %i", i_capability );
                i_ret = -1;
                break;
        }

        /* If the high score was broken, we have a new champion */
        if( i_ret == 0 )
        {
            break;
        }
        else
        {
            UnlockModule( p_tmp->p_module );
        }

        p_tmp = p_tmp->p_next;
    }

    /* Store the locked module value */
    if( p_tmp != NULL )
    {
        p_module = p_tmp->p_module;
        p_tmp = p_tmp->p_next;
    }
    else
    {
        p_module = NULL;
    }

    /* Unlock the remaining modules */
    while( p_tmp != NULL )
    {
        UnlockModule( p_tmp->p_module );
        p_tmp = p_tmp->p_next;
    }

    free( p_list );

    if( p_module != NULL )
    {
        msg_Info( p_module, "found and locked %s module `%s'",
                  MODULE_CAPABILITY( i_capability ),
                  p_module->psz_object_name );
    }
    else if( p_first == NULL )
    {
        msg_Err( p_this, "no %s module named `%s'",
                 MODULE_CAPABILITY( i_capability ), psz_name );
    }
    else if( psz_name != NULL && *psz_name )
    {
        msg_Err( p_this, "could not load %s module `%s'",
                 MODULE_CAPABILITY( i_capability ), psz_name );
    }

    if( psz_realname )
    {
        free( psz_realname );
    }

    /* Don't forget that the module is still locked */
    return p_module;
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
    vlc_mutex_lock( &p_module->p_vlc->module_bank.lock );

    /* Just unlock the module - we can't do anything if it fails,
     * so there is no need to check the return value. */
    UnlockModule( p_module );

    msg_Info( p_module, "unlocking module `%s'", p_module->psz_object_name );

    /* We release the global lock */
    vlc_mutex_unlock( &p_module->p_vlc->module_bank.lock );

    return;
}

/*****************************************************************************
 * Following functions are local.
 *****************************************************************************/

/*****************************************************************************
 * AllocateAllPlugins: load all plugin modules we can find.
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
static void AllocateAllPlugins( vlc_object_t *p_this )
{
    static char * path[] = { ".", "plugins", PLUGIN_PATH, NULL, NULL };

    char **         ppsz_path = path;
    char *          psz_fullpath;
    char *          psz_file;
#if defined( SYS_BEOS ) || defined( SYS_DARWIN )
    char *          psz_vlcpath = system_GetProgramPath();
    int             i_vlclen = strlen( psz_vlcpath );
    vlc_bool_t      b_notinroot;
#endif
    DIR *           dir;
    struct dirent * file;

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

        msg_Dbg( p_this, "browsing `%s'", psz_fullpath );

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
                    AllocatePluginModule( p_this, psz_file );

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
}

/*****************************************************************************
 * AllocatePluginModule: load a module into memory and initialize it.
 *****************************************************************************
 * This function loads a dynamically loadable module and allocates a structure
 * for its information data. The module can then be handled by module_Need,
 * module_Unneed and HideModule. It can be removed by DeleteModule.
 *****************************************************************************/
static int AllocatePluginModule( vlc_object_t * p_this, char * psz_filename )
{
    char **pp_shortcut;
    module_t * p_module, * p_othermodule;
    module_handle_t handle;

    /* Try to dynamically load the module. */
    if( module_load( psz_filename, &handle ) )
    {
        char psz_buffer[256];

        /* The plugin module couldn't be opened */
        msg_Warn( p_this, "cannot open %s (%s)",
                  psz_filename, module_error( psz_buffer ) );
        return -1;
    }

    /* Now that we have successfully loaded the module, we can
     * allocate a structure for it */ 
    p_module = vlc_object_create( p_this, VLC_OBJECT_MODULE );
    if( p_module == NULL )
    {
        msg_Err( p_this, "out of memory" );
        module_unload( handle );
        return -1;
    }

    vlc_object_attach( p_module, p_this );

    /* We need to fill these since they may be needed by CallSymbol() */
    p_module->is.plugin.psz_filename = psz_filename;
    p_module->is.plugin.handle = handle;
    p_module->p_symbols = &symbols;

    /* Initialize the module: fill p_module->psz_object_name, default config */
    if( CallSymbol( p_module, "InitModule" MODULE_SUFFIX ) != 0 )
    {
        /* We couldn't call InitModule() */
        vlc_object_destroy( p_module );
        module_unload( handle );
        return -1;
    }

    /* Check that we don't already have a module with this name */
    for( p_othermodule = p_this->p_vlc->module_bank.first ;
         p_othermodule != NULL ;
         p_othermodule = p_othermodule->next )
    {
        if( !strcmp( p_othermodule->psz_object_name,
                     p_module->psz_object_name ) )
        {
            msg_Warn( p_this,
                      "cannot load %s, a module named `%s' already exists",
                      psz_filename, p_module->psz_object_name );
            config_Free( p_module );
            vlc_object_destroy( p_module );
            module_unload( handle );
            return -1;
        }
    }

    /* Activate the module : fill the capability structure, etc. */
    if( CallSymbol( p_module, "ActivateModule" MODULE_SUFFIX ) != 0 )
    {
        /* We couldn't call ActivateModule() */
        config_Free( p_module );
        vlc_object_destroy( p_module );
        module_unload( handle );
        return -1;
    }

    for( pp_shortcut = p_module->pp_shortcuts ; *pp_shortcut ; pp_shortcut++ )
    {
        *pp_shortcut = strdup( *pp_shortcut );
    }

    /* We strdup() these entries so that they are still valid when the
     * module is unloaded. */
    p_module->is.plugin.psz_filename =
            strdup( p_module->is.plugin.psz_filename );
    p_module->psz_object_name = strdup( p_module->psz_object_name );
    p_module->psz_longname = strdup( p_module->psz_longname );

    if( p_module->is.plugin.psz_filename == NULL 
            || p_module->psz_object_name == NULL
            || p_module->psz_longname == NULL )
    {
        msg_Err( p_this, "out of memory" );

        free( p_module->is.plugin.psz_filename );
        free( p_module->psz_object_name );
        free( p_module->psz_longname );
        free( p_module->psz_program );

        config_Free( p_module );
        vlc_object_destroy( p_module );
        module_unload( handle );
        return -1;
    }

    if( p_module->psz_program != NULL )
    {
        p_module->psz_program = strdup( p_module->psz_program );
    }

    /* Everything worked fine ! The module is ready to be added to the list. */
    p_module->i_usage = 0;
    p_module->i_unused_delay = 0;

    p_module->b_builtin = 0;

    /* Link module into the linked list */
    if( p_this->p_vlc->module_bank.first != NULL )
    {
        p_this->p_vlc->module_bank.first->prev = p_module;
    }
    p_module->next = p_this->p_vlc->module_bank.first;
    p_module->prev = NULL;
    p_this->p_vlc->module_bank.first = p_module;
    p_this->p_vlc->module_bank.i_count++;

    msg_Dbg( p_this, "plugin `%s', %s",
             p_module->psz_object_name, p_module->psz_longname );

    return 0;
}
#endif /* HAVE_DYNAMIC_PLUGINS */

/*****************************************************************************
 * AllocateBuiltinModule: initialize a builtin module.
 *****************************************************************************
 * This function registers a builtin module and allocates a structure
 * for its information data. The module can then be handled by module_Need,
 * module_Unneed and HideModule. It can be removed by DeleteModule.
 *****************************************************************************/
static int AllocateBuiltinModule( vlc_object_t * p_this,
                                  int ( *pf_init ) ( module_t * ),
                                  int ( *pf_activate ) ( module_t * ),
                                  int ( *pf_deactivate ) ( module_t * ) )
{
    module_t * p_module, * p_othermodule;

    /* Now that we have successfully loaded the module, we can
     * allocate a structure for it */ 
    p_module = vlc_object_create( p_this, VLC_OBJECT_MODULE );
    if( p_module == NULL )
    {
        msg_Err( p_this, "out of memory" );
        return -1;
    }

    /* Initialize the module : fill p_module->psz_object_name, etc. */
    if( pf_init( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        msg_Err( p_this, "failed calling init in builtin module" );
        vlc_object_destroy( p_module );
        return -1;
    }

    /* Check that we don't already have a module with this name */
    for( p_othermodule = p_this->p_vlc->module_bank.first ;
         p_othermodule != NULL ;
         p_othermodule = p_othermodule->next )
    {
        if( !strcmp( p_othermodule->psz_object_name,
                     p_module->psz_object_name ) )
        {
            msg_Warn( p_this, "cannot load builtin `%s', "
                      "a module named `%s' already exists",
                      p_module->psz_object_name, p_module->psz_object_name );
            config_Free( p_module );
            vlc_object_destroy( p_module );
            return -1;
        }
    }

    if( pf_activate( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        msg_Err( p_this, "failed calling activate in builtin module" );
        config_Free( p_module );
        vlc_object_destroy( p_module );
        return -1;
    }

    /* Everything worked fine ! The module is ready to be added to the list. */
    p_module->i_usage = 0;
    p_module->i_unused_delay = 0;

    p_module->b_builtin = 1;
    p_module->is.builtin.pf_deactivate = pf_deactivate;

    /* Link module into the linked list */
    if( p_this->p_vlc->module_bank.first != NULL )
    {
        p_this->p_vlc->module_bank.first->prev = p_module;
    }
    p_module->next = p_this->p_vlc->module_bank.first;
    p_module->prev = NULL;
    p_this->p_vlc->module_bank.first = p_module;
    p_this->p_vlc->module_bank.i_count++;

    msg_Dbg( p_this, "builtin `%s', %s",
             p_module->psz_object_name, p_module->psz_longname );

    vlc_object_attach( p_module, p_this );

    return 0;
}

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
            msg_Err( p_module, "trying to free builtin module `%s' with "
                     "usage %i", p_module->psz_object_name, p_module->i_usage );
            return -1;
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
            msg_Err( p_module, "trying to free module `%s' which is "
                               "still in use", p_module->psz_object_name );
            return -1;
        }

        /* Two possibilities here: i_usage == -1 and the module is already
         * unloaded, we can continue, or i_usage == 0, and we have to hide
         * the module before going on. */
        if( p_module->i_usage == 0 )
        {
            if( HideModule( p_module ) != 0 )
            {
                return -1;
            }
        }
    }
#endif

    vlc_object_unlink_all( p_module );

    /* Unlink the module from the linked list. */
    if( p_module->prev != NULL )
    {
        p_module->prev->next = p_module->next;
    }
    else
    {
        p_module->p_vlc->module_bank.first = p_module->next;
    }

    if( p_module->next != NULL )
    {
        p_module->next->prev = p_module->prev;
    }

    p_module->p_vlc->module_bank.i_count--;

    /* We free the structures that we strdup()ed in Allocate*Module(). */
#ifdef HAVE_DYNAMIC_PLUGINS
    if( !p_module->b_builtin )
    {
        char **pp_shortcut = p_module->pp_shortcuts;

        for( ; *pp_shortcut ; pp_shortcut++ )
        {
            free( *pp_shortcut );
        }

        free( p_module->is.plugin.psz_filename );
        free( p_module->psz_object_name );
        free( p_module->psz_longname );
        free( p_module->psz_program );
    }
#endif

    config_Free( p_module );
    vlc_object_destroy( p_module );

    return 0;
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
        return 0;
    }

    if( p_module->b_builtin )
    {
        /* A builtin module should always have a refcount >= 0 ! */
        msg_Err( p_module, "builtin module `%s' has refcount %i",
                           p_module->psz_object_name, p_module->i_usage );
        return -1;
    }

#ifdef HAVE_DYNAMIC_PLUGINS
    if( p_module->i_usage != -1 )
    {
        /* This shouldn't happen. Ever. We have serious problems here. */
        msg_Err( p_module, "plugin module `%s' has refcount %i",
                           p_module->psz_object_name, p_module->i_usage );
        return -1;
    }

    /* i_usage == -1, which means that the module isn't in memory */
    if( module_load( p_module->is.plugin.psz_filename,
                     &p_module->is.plugin.handle ) )
    {
        char psz_buffer[256];

        /* The plugin module couldn't be opened */
        msg_Err( p_module, "cannot open %s (%s)",
                 p_module->is.plugin.psz_filename, module_error(psz_buffer) );
        return -1;
    }

    /* FIXME: what to do if the guy modified the plugin while it was
     * unloaded ? It makes XMMS crash nastily, perhaps we should try
     * to be a bit more clever here. */

    /* Activate the module : fill the capability structure, etc. */
    if( CallSymbol( p_module, "ActivateModule" MODULE_SUFFIX ) != 0 )
    {
        /* We couldn't call ActivateModule() -- looks nasty, but
         * we can't do much about it. Just try to unload module. */
        module_unload( p_module->is.plugin.handle );
        p_module->i_usage = -1;
        return -1;
    }

    /* Everything worked fine ! The module is ready to be used */
    p_module->i_usage = 1;
#endif /* HAVE_DYNAMIC_PLUGINS */

    return 0;
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
        msg_Err( p_module, "trying to call module_Unneed() on `%s' "
                           "which is not in use", p_module->psz_object_name );
        return -1;
    }

    /* This module is still in use, we can return */
    p_module->i_usage--;
    p_module->i_unused_delay = 0;

    return 0;
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
        /* A builtin module should never be hidden. */
        msg_Err( p_module, "trying to hide builtin module `%s'",
                           p_module->psz_object_name );
        return -1;
    }

    if( p_module->i_usage >= 1 )
    {
        msg_Err( p_module, "trying to hide module `%s' which is still "
                           "in use", p_module->psz_object_name );
        return -1;
    }

    if( p_module->i_usage <= -1 )
    {
        msg_Err( p_module, "trying to hide module `%s' which is already "
                           "hidden", p_module->psz_object_name );
        return -1;
    }

    /* Deactivate the module : free the capability structure, etc. */
    if( CallSymbol( p_module, "DeactivateModule" MODULE_SUFFIX ) != 0 )
    {
        /* We couldn't call DeactivateModule() -- looks nasty, but
         * we can't do much about it. Just try to unload module anyway. */
        module_unload( p_module->is.plugin.handle );
        p_module->i_usage = -1;
        return -1;
    }

    /* Everything worked fine, we can safely unload the module. */
    module_unload( p_module->is.plugin.handle );
    p_module->i_usage = -1;

    return 0;
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
        char psz_buffer[256];

        /* We couldn't load the symbol */
        msg_Warn( p_module, "cannot find symbol %s in module %s (%s)",
                            psz_name, p_module->is.plugin.psz_filename,
                            module_error( psz_buffer ) );
        return -1;
    }

    /* We can now try to call the symbol */
    if( pf_symbol( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        msg_Err( p_module, "failed calling symbol %s in module %s",
                           psz_name, p_module->is.plugin.psz_filename );
        return -1;
    }

    /* Everything worked fine, we can return */
    return 0;
}
#endif /* HAVE_DYNAMIC_PLUGINS */

