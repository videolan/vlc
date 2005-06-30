/*****************************************************************************
 * modules.c : Builtin and plugin modules management functions
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN
 * $Id$
 *
 * Authors: Sam Hocevar <sam@zoy.org>
 *          Ethan C. Baldridge <BaldridgeE@cadmus.com>
 *          Hans-Peter Jansen <hpj@urpla.net>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc/input.h>

#ifdef HAVE_DIRENT_H
#   include <dirent.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#   include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#   include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#if !defined(HAVE_DYNAMIC_PLUGINS)
    /* no support for plugins */
#elif defined(HAVE_DL_DYLD)
#   if defined(HAVE_MACH_O_DYLD_H)
#       include <mach-o/dyld.h>
#   endif
#elif defined(HAVE_DL_BEOS)
#   if defined(HAVE_IMAGE_H)
#       include <image.h>
#   endif
#elif defined(HAVE_DL_WINDOWS)
#   include <windows.h>
#elif defined(HAVE_DL_DLOPEN)
#   if defined(HAVE_DLFCN_H) /* Linux, BSD, Hurd */
#       include <dlfcn.h>
#   endif
#   if defined(HAVE_SYS_DL_H)
#       include <sys/dl.h>
#   endif
#elif defined(HAVE_DL_SHL_LOAD)
#   if defined(HAVE_DL_H)
#       include <dl.h>
#   endif
#endif

#include "vlc_error.h"

#include "vlc_interface.h"
#include "intf_eject.h"

#include "vlc_playlist.h"

#include "vlc_video.h"
#include "video_output.h"
#include "vout_synchro.h"
#include "vlc_spu.h"

#include "audio_output.h"
#include "aout_internal.h"

#include "stream_output.h"
#include "osd.h"
#include "vlc_httpd.h"
#include "vlc_tls.h"
#include "vlc_md5.h"
#include "vlc_xml.h"

#include "iso_lang.h"
#include "charset.h"

#include "vlc_block.h"

#include "vlc_vlm.h"

#include "vlc_image.h"

#if defined( _MSC_VER ) && defined( UNDER_CE )
#    include "modules_builtin_evc.h"
#elif defined( _MSC_VER )
#    include "modules_builtin_msvc.h"
#else
#    include "modules_builtin.h"
#endif
#include "network.h"

#if defined( WIN32 ) || defined( UNDER_CE )
    /* Avoid name collisions */
#   define LoadModule(a,b,c) LoadVlcModule(a,b,c)
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
static void AllocateAllPlugins  ( vlc_object_t * );
static void AllocatePluginDir   ( vlc_object_t *, const char *, int );
static int  AllocatePluginFile  ( vlc_object_t *, char *, int64_t, int64_t );
static module_t * AllocatePlugin( vlc_object_t *, char * );
#endif
static int  AllocateBuiltinModule( vlc_object_t *, int ( * ) ( module_t * ) );
static int  DeleteModule ( module_t * );
#ifdef HAVE_DYNAMIC_PLUGINS
static void   DupModule        ( module_t * );
static void   UndupModule      ( module_t * );
static int    CallEntry        ( module_t * );
static int    LoadModule       ( vlc_object_t *, char *, module_handle_t * );
static void   CloseModule      ( module_handle_t );
static void * GetSymbol        ( module_handle_t, const char * );
static void   CacheLoad        ( vlc_object_t * );
static int    CacheLoadConfig  ( module_t *, FILE * );
static void   CacheSave        ( vlc_object_t * );
static void   CacheSaveConfig  ( module_t *, FILE * );
static char * CacheName        ( void );
static void   CacheMerge       ( vlc_object_t *, module_t *, module_t * );
static module_cache_t * CacheFind( vlc_object_t *, char *, int64_t, int64_t );

#if defined(HAVE_DL_WINDOWS)
static char * GetWindowsError  ( void );
#endif
#endif


/* Sub-version number
 * (only used to avoid breakage in dev version when cache structure changes) */
#define CACHE_SUBVERSION_NUM 1

/*****************************************************************************
 * module_InitBank: create the module bank.
 *****************************************************************************
 * This function creates a module bank structure which will be filled later
 * on with all the modules found.
 *****************************************************************************/
void __module_InitBank( vlc_object_t *p_this )
{
    module_bank_t *p_bank;
    vlc_value_t  lockval;

    var_Create( p_this->p_libvlc, "libvlc", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "libvlc", &lockval );
    vlc_mutex_lock( lockval.p_address );
    if( p_this->p_libvlc->p_module_bank )
    {
        p_this->p_libvlc->p_module_bank->i_usage++;
        vlc_mutex_unlock( lockval.p_address );
        var_Destroy( p_this->p_libvlc, "libvlc" );
        return;
    }
    vlc_mutex_unlock( lockval.p_address );
    var_Destroy( p_this->p_libvlc, "libvlc" );

    p_bank = vlc_object_create( p_this, sizeof(module_bank_t) );
    p_bank->psz_object_name = "module bank";
    p_bank->i_usage = 1;
    p_bank->i_cache = p_bank->i_loaded_cache = 0;
    p_bank->pp_cache = p_bank->pp_loaded_cache = 0;
    p_bank->b_cache = p_bank->b_cache_dirty =
        p_bank->b_cache_delete = VLC_FALSE;

    /*
     * Store the symbols to be exported
     */
#ifdef HAVE_DYNAMIC_PLUGINS
    STORE_SYMBOLS( &p_bank->symbols );
#endif

    /* Everything worked, attach the object */
    p_this->p_libvlc->p_module_bank = p_bank;
    vlc_object_attach( p_bank, p_this->p_libvlc );

    module_LoadMain( p_this );

    return;
}

/*****************************************************************************
 * module_ResetBank: reset the module bank.
 *****************************************************************************
 * This function resets the module bank by unloading all unused plugin
 * modules.
 *****************************************************************************/
void __module_ResetBank( vlc_object_t *p_this )
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
void __module_EndBank( vlc_object_t *p_this )
{
    module_t * p_next;
    vlc_value_t lockval;

    var_Create( p_this->p_libvlc, "libvlc", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "libvlc", &lockval );
    vlc_mutex_lock( lockval.p_address );
    if( !p_this->p_libvlc->p_module_bank )
    {
        vlc_mutex_unlock( lockval.p_address );
        var_Destroy( p_this->p_libvlc, "libvlc" );
        return;
    }
    if( --p_this->p_libvlc->p_module_bank->i_usage )
    {
        vlc_mutex_unlock( lockval.p_address );
        var_Destroy( p_this->p_libvlc, "libvlc" );
        return;
    }
    vlc_mutex_unlock( lockval.p_address );
    var_Destroy( p_this->p_libvlc, "libvlc" );

    config_AutoSaveConfigFile( p_this );

#ifdef HAVE_DYNAMIC_PLUGINS
#define p_bank p_this->p_libvlc->p_module_bank
    if( p_bank->b_cache ) CacheSave( p_this );
    while( p_bank->i_loaded_cache-- )
    {
        free( p_bank->pp_loaded_cache[p_bank->i_loaded_cache]->psz_file );
        free( p_bank->pp_loaded_cache[p_bank->i_loaded_cache] );
    }
    if( p_bank->pp_loaded_cache )
        free( p_bank->pp_loaded_cache );

    while( p_bank->i_cache-- )
    {
        free( p_bank->pp_cache[p_bank->i_cache]->psz_file );
        free( p_bank->pp_cache[p_bank->i_cache] );
    }
    if( p_bank->pp_cache )
        free( p_bank->pp_cache );
#undef p_bank
#endif

    vlc_object_detach( p_this->p_libvlc->p_module_bank );

    while( p_this->p_libvlc->p_module_bank->i_children )
    {
        p_next = (module_t *)p_this->p_libvlc->p_module_bank->pp_children[0];

        if( DeleteModule( p_next ) )
        {
            /* Module deletion failed */
            msg_Err( p_this, "module \"%s\" can't be removed, trying harder",
                     p_next->psz_object_name );

            /* We just free the module by hand. Niahahahahaha. */
            vlc_object_detach( p_next );
            vlc_object_destroy( p_next );
        }
    }

    vlc_object_destroy( p_this->p_libvlc->p_module_bank );
    p_this->p_libvlc->p_module_bank = NULL;

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
void __module_LoadMain( vlc_object_t *p_this )
{
    vlc_value_t lockval;

    var_Create( p_this->p_libvlc, "libvlc", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "libvlc", &lockval );
    vlc_mutex_lock( lockval.p_address );
    if( p_this->p_libvlc->p_module_bank->b_main )
    {
        vlc_mutex_unlock( lockval.p_address );
        var_Destroy( p_this->p_libvlc, "libvlc" );
        return;
    }
    p_this->p_libvlc->p_module_bank->b_main = VLC_TRUE;
    vlc_mutex_unlock( lockval.p_address );
    var_Destroy( p_this->p_libvlc, "libvlc" );

    AllocateBuiltinModule( p_this, vlc_entry__main );
}

/*****************************************************************************
 * module_LoadBuiltins: load all modules which we built with.
 *****************************************************************************
 * This function fills the module bank structure with the builtin modules.
 *****************************************************************************/
void __module_LoadBuiltins( vlc_object_t * p_this )
{
    vlc_value_t lockval;

    var_Create( p_this->p_libvlc, "libvlc", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "libvlc", &lockval );
    vlc_mutex_lock( lockval.p_address );
    if( p_this->p_libvlc->p_module_bank->b_builtins )
    {
        vlc_mutex_unlock( lockval.p_address );
        var_Destroy( p_this->p_libvlc, "libvlc" );
        return;
    }
    p_this->p_libvlc->p_module_bank->b_builtins = VLC_TRUE;
    vlc_mutex_unlock( lockval.p_address );
    var_Destroy( p_this->p_libvlc, "libvlc" );

    msg_Dbg( p_this, "checking builtin modules" );
    ALLOCATE_ALL_BUILTINS();
}

/*****************************************************************************
 * module_LoadPlugins: load all plugin modules we can find.
 *****************************************************************************
 * This function fills the module bank structure with the plugin modules.
 *****************************************************************************/
void __module_LoadPlugins( vlc_object_t * p_this )
{
#ifdef HAVE_DYNAMIC_PLUGINS
    vlc_value_t lockval;

    var_Create( p_this->p_libvlc, "libvlc", VLC_VAR_MUTEX );
    var_Get( p_this->p_libvlc, "libvlc", &lockval );
    vlc_mutex_lock( lockval.p_address );
    if( p_this->p_libvlc->p_module_bank->b_plugins )
    {
        vlc_mutex_unlock( lockval.p_address );
        var_Destroy( p_this->p_libvlc, "libvlc" );
        return;
    }
    p_this->p_libvlc->p_module_bank->b_plugins = VLC_TRUE;
    vlc_mutex_unlock( lockval.p_address );
    var_Destroy( p_this->p_libvlc, "libvlc" );

    msg_Dbg( p_this, "checking plugin modules" );

    if( config_GetInt( p_this, "plugins-cache" ) )
        p_this->p_libvlc->p_module_bank->b_cache = VLC_TRUE;

    if( p_this->p_libvlc->p_module_bank->b_cache ||
        p_this->p_libvlc->p_module_bank->b_cache_delete ) CacheLoad( p_this );

    AllocateAllPlugins( p_this );
#endif
}

/*****************************************************************************
 * module_Need: return the best module function, given a capability list.
 *****************************************************************************
 * This function returns the module that best fits the asked capabilities.
 *****************************************************************************/
module_t * __module_Need( vlc_object_t *p_this, const char *psz_capability,
                          const char *psz_name, vlc_bool_t b_strict )
{
    typedef struct module_list_t module_list_t;

    struct module_list_t
    {
        module_t *p_module;
        int i_score;
        vlc_bool_t b_force;
        module_list_t *p_next;
    };

    module_list_t *p_list, *p_first, *p_tmp;
    vlc_list_t *p_all;

    int i_which_module, i_index = 0;
    vlc_bool_t b_intf = VLC_FALSE;

    module_t *p_module;

    int   i_shortcuts = 0;
    char *psz_shortcuts = NULL, *psz_var = NULL;
    vlc_bool_t b_force_backup = p_this->b_force;


    /* Deal with variables */
    if( psz_name && psz_name[0] == '$' )
    {
        vlc_value_t val;
        var_Create( p_this, psz_name + 1, VLC_VAR_MODULE | VLC_VAR_DOINHERIT );
        var_Get( p_this, psz_name + 1, &val );
        psz_var = val.psz_string;
        psz_name = psz_var;
    }

    /* Count how many different shortcuts were asked for */
    if( psz_name && *psz_name )
    {
        char *psz_parser, *psz_last_shortcut;

        /* If the user wants none, give him none. */
        if( !strcmp( psz_name, "none" ) )
        {
            if( psz_var ) free( psz_var );
            return NULL;
        }

        i_shortcuts++;
        psz_shortcuts = psz_last_shortcut = strdup( psz_name );

        for( psz_parser = psz_shortcuts; *psz_parser; psz_parser++ )
        {
            if( *psz_parser == ',' )
            {
                 *psz_parser = '\0';
                 i_shortcuts++;
                 psz_last_shortcut = psz_parser + 1;
            }
        }

        /* Check if the user wants to override the "strict" mode */
        if( psz_last_shortcut )
        {
            if( !strcmp(psz_last_shortcut, "none") )
            {
                b_strict = VLC_TRUE;
                i_shortcuts--;
            }
            else if( !strcmp(psz_last_shortcut, "any") )
            {
                b_strict = VLC_FALSE;
                i_shortcuts--;
            }
        }
    }

    /* Sort the modules and test them */
    p_all = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    p_list = malloc( p_all->i_count * sizeof( module_list_t ) );
    p_first = NULL;

    /* Parse the module list for capabilities and probe each of them */
    for( i_which_module = 0; i_which_module < p_all->i_count; i_which_module++ )
    {
        int i_shortcut_bonus = 0;

        p_module = (module_t *)p_all->p_values[i_which_module].p_object;

        /* Test that this module can do what we need */
        if( strcmp( p_module->psz_capability, psz_capability ) )
        {
            /* Don't recurse through the sub-modules because vlc_list_find()
             * will list them anyway. */
            continue;
        }

        /* Test if we have the required CPU */
        if( (p_module->i_cpu & p_this->p_libvlc->i_cpu) != p_module->i_cpu )
        {
            continue;
        }

        /* If we required a shortcut, check this plugin provides it. */
        if( i_shortcuts > 0 )
        {
            vlc_bool_t b_trash;
            int i_dummy, i_short = i_shortcuts;
            char *psz_name = psz_shortcuts;

            /* Let's drop modules with a <= 0 score (unless they are
             * explicitly requested) */
            b_trash = p_module->i_score <= 0;

            while( i_short > 0 )
            {
                for( i_dummy = 0; p_module->pp_shortcuts[i_dummy]; i_dummy++ )
                {
                    if( !strcasecmp( psz_name,
                                     p_module->pp_shortcuts[i_dummy] ) )
                    {
                        /* Found it */
                        b_trash = VLC_FALSE;
                        i_shortcut_bonus = i_short * 10000;
                        break;
                    }
                }

                if( i_shortcut_bonus )
                {
                    /* We found it... remember ? */
                    break;
                }

                /* Go to the next shortcut... This is so lame! */
                while( *psz_name )
                {
                    psz_name++;
                }
                psz_name++;
                i_short--;
            }

            /* If we are in "strict" mode and we couldn't
             * find the module in the list of provided shortcuts,
             * then kick the bastard out of here!!! */
            if( i_short == 0 && b_strict )
            {
                b_trash = VLC_TRUE;
            }

            if( b_trash )
            {
                continue;
            }
        }
        /* If we didn't require a shortcut, trash <= 0 scored plugins */
        else if( p_module->i_score <= 0 )
        {
            continue;
        }

        /* Special case: test if we requested a particular intf plugin */
        if( !i_shortcuts && p_module->psz_program
             && !strcmp( psz_capability, "interface" )
             && !strcmp( p_module->psz_program,
                         p_this->p_vlc->psz_object_name ) )
        {
            if( !b_intf )
            {
                /* Remove previous non-matching plugins */
                i_index = 0;
                b_intf = VLC_TRUE;
            }
        }
        else if( b_intf )
        {
            /* This one doesn't match */
            continue;
        }

        /* Store this new module */
        p_list[ i_index ].p_module = p_module;
        p_list[ i_index ].i_score = p_module->i_score + i_shortcut_bonus;
        p_list[ i_index ].b_force = !!i_shortcut_bonus;

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

            if( p_first->i_score < p_list[ i_index ].i_score )
            {
                p_list[ i_index ].p_next = p_first;
                p_first = &p_list[ i_index ];
            }
            else
            {
                while( p_newlist->p_next != NULL &&
                    p_newlist->p_next->i_score >= p_list[ i_index ].i_score )
                {
                    p_newlist = p_newlist->p_next;
                }

                p_list[ i_index ].p_next = p_newlist->p_next;
                p_newlist->p_next = &p_list[ i_index ];
            }
        }

        i_index++;
    }

    msg_Dbg( p_this, "looking for %s module: %i candidate%s", psz_capability,
                                            i_index, i_index == 1 ? "" : "s" );

    /* Lock all candidate modules */
    p_tmp = p_first;
    while( p_tmp != NULL )
    {
        vlc_object_yield( p_tmp->p_module );
        p_tmp = p_tmp->p_next;
    }

    /* We can release the list, interesting modules were yielded */
    vlc_list_release( p_all );

    /* Parse the linked list and use the first successful module */
    p_tmp = p_first;
    while( p_tmp != NULL )
    {
#ifdef HAVE_DYNAMIC_PLUGINS
        /* Make sure the module is loaded in mem */
        module_t *p_module = p_tmp->p_module->b_submodule ?
            (module_t *)p_tmp->p_module->p_parent : p_tmp->p_module;
        if( !p_module->b_builtin && !p_module->b_loaded )
        {
            module_t *p_new_module =
                AllocatePlugin( p_this, p_module->psz_filename );
            if( p_new_module )
            {
                CacheMerge( p_this, p_module, p_new_module );
                vlc_object_attach( p_new_module, p_module );
                DeleteModule( p_new_module );
            }
        }
#endif

        p_this->b_force = p_tmp->b_force;
        if( p_tmp->p_module->pf_activate
             && p_tmp->p_module->pf_activate( p_this ) == VLC_SUCCESS )
        {
            break;
        }

        vlc_object_release( p_tmp->p_module );
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
        vlc_object_release( p_tmp->p_module );
        p_tmp = p_tmp->p_next;
    }

    free( p_list );
    p_this->b_force = b_force_backup;

    if( p_module != NULL )
    {
        msg_Dbg( p_module, "using %s module \"%s\"",
                 psz_capability, p_module->psz_object_name );
    }
    else if( p_first == NULL )
    {
        if( !strcmp( psz_capability, "access_demux" ) )
        {
            msg_Warn( p_this, "no %s module matched \"%s\"",
                 psz_capability, (psz_name && *psz_name) ? psz_name : "any" );
        }
        else
        {  
            msg_Err( p_this, "no %s module matched \"%s\"",
                 psz_capability, (psz_name && *psz_name) ? psz_name : "any" );
        }
    }
    else if( psz_name != NULL && *psz_name )
    {
        msg_Warn( p_this, "no %s module matching \"%s\" could be loaded",
                  psz_capability, (psz_name && *psz_name) ? psz_name : "any" );
    }

    if( psz_shortcuts )
    {
        free( psz_shortcuts );
    }

    if( psz_var )
    {
        free( psz_var );
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
void __module_Unneed( vlc_object_t * p_this, module_t * p_module )
{
    /* Use the close method */
    if( p_module->pf_deactivate )
    {
        p_module->pf_deactivate( p_this );
    }

    msg_Dbg( p_module, "unlocking module \"%s\"", p_module->psz_object_name );

    vlc_object_release( p_module );

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
    /* Yes, there are two NULLs because we replace one with "plugin-path". */
#if defined( WIN32 ) || defined( UNDER_CE )
    char *path[] = { "modules", "", "plugins", 0, 0 };
#else
    char *path[] = { "modules", PLUGIN_PATH, "plugins", 0, 0 };
#endif

    char **ppsz_path = path;
    char *psz_fullpath;

    /* If the user provided a plugin path, we add it to the list */
    path[ sizeof(path)/sizeof(char*) - 2 ] =
        config_GetPsz( p_this, "plugin-path" );

    for( ; *ppsz_path != NULL ; ppsz_path++ )
    {
        if( !(*ppsz_path)[0] ) continue;

#if defined( SYS_BEOS ) || defined( SYS_DARWIN ) || defined( WIN32 )

        /* Handle relative as well as absolute paths */
#ifdef WIN32
        if( (*ppsz_path)[0] != '\\' && (*ppsz_path)[0] != '/' &&
            (*ppsz_path)[1] != ':' )
#else
        if( (*ppsz_path)[0] != '/' )
#endif
        {
            int i_dirlen = strlen( *ppsz_path );
            i_dirlen += strlen( p_this->p_libvlc->psz_vlcpath ) + 2;

            psz_fullpath = malloc( i_dirlen );
            if( psz_fullpath == NULL )
            {
                continue;
            }
#ifdef WIN32
            sprintf( psz_fullpath, "%s\\%s",
                     p_this->p_libvlc->psz_vlcpath, *ppsz_path );
#else
            sprintf( psz_fullpath, "%s/%s",
                     p_this->p_libvlc->psz_vlcpath, *ppsz_path );
#endif
        }
        else
#endif
        {
            psz_fullpath = strdup( *ppsz_path );
        }

        msg_Dbg( p_this, "recursively browsing `%s'", psz_fullpath );

        /* Don't go deeper than 5 subdirectories */
        AllocatePluginDir( p_this, psz_fullpath, 5 );

        free( psz_fullpath );
    }

    /* Free plugin-path */
    if( path[ sizeof(path)/sizeof(char*) - 2 ] )
        free( path[ sizeof(path)/sizeof(char*) - 2 ] );
    path[ sizeof(path)/sizeof(char*) - 2 ] = NULL;
}

/*****************************************************************************
 * AllocatePluginDir: recursively parse a directory to look for plugins
 *****************************************************************************/
static void AllocatePluginDir( vlc_object_t *p_this, const char *psz_dir,
                               int i_maxdepth )
{
#if defined( UNDER_CE ) || defined( _MSC_VER )
#ifdef UNDER_CE
    wchar_t psz_wpath[MAX_PATH + 256];
    wchar_t psz_wdir[MAX_PATH];
#endif
    char psz_path[MAX_PATH + 256];
    WIN32_FIND_DATA finddata;
    HANDLE handle;
    int rc;
#else
    int    i_dirlen;
    DIR *  dir;
    struct dirent * file;
#endif
    char * psz_file;

    if( p_this->p_vlc->b_die || i_maxdepth < 0 )
    {
        return;
    }

#if defined( UNDER_CE ) || defined( _MSC_VER )
#ifdef UNDER_CE
    MultiByteToWideChar( CP_ACP, 0, psz_dir, -1, psz_wdir, MAX_PATH );

    rc = GetFileAttributes( psz_wdir );
    if( rc<0 || !(rc&FILE_ATTRIBUTE_DIRECTORY) ) return; /* Not a directory */

    /* Parse all files in the directory */
    swprintf( psz_wpath, L"%ls\\*", psz_wdir );
#else
    rc = GetFileAttributes( psz_dir );
    if( rc<0 || !(rc&FILE_ATTRIBUTE_DIRECTORY) ) return; /* Not a directory */
#endif

    /* Parse all files in the directory */
    sprintf( psz_path, "%s\\*", psz_dir );

#ifdef UNDER_CE
    handle = FindFirstFile( psz_wpath, &finddata );
#else
    handle = FindFirstFile( psz_path, &finddata );
#endif
    if( handle == INVALID_HANDLE_VALUE )
    {
        /* Empty directory */
        return;
    }

    /* Parse the directory and try to load all files it contains. */
    do
    {
#ifdef UNDER_CE
        unsigned int i_len = wcslen( finddata.cFileName );
        swprintf( psz_wpath, L"%ls\\%ls", psz_wdir, finddata.cFileName );
        sprintf( psz_path, "%s\\%ls", psz_dir, finddata.cFileName );
#else
        unsigned int i_len = strlen( finddata.cFileName );
        sprintf( psz_path, "%s\\%s", psz_dir, finddata.cFileName );
#endif

        /* Skip ".", ".." and anything starting with "." */
        if( !*finddata.cFileName || *finddata.cFileName == '.' )
        {
            if( !FindNextFile( handle, &finddata ) ) break;
            continue;
        }

#ifdef UNDER_CE
        if( GetFileAttributes( psz_wpath ) & FILE_ATTRIBUTE_DIRECTORY )
#else
        if( GetFileAttributes( psz_path ) & FILE_ATTRIBUTE_DIRECTORY )
#endif
        {
            AllocatePluginDir( p_this, psz_path, i_maxdepth - 1 );
        }
        else if( i_len > strlen( LIBEXT )
                  /* We only load files ending with LIBEXT */
                  && !strncasecmp( psz_path + strlen( psz_path)
                                   - strlen( LIBEXT ),
                                   LIBEXT, strlen( LIBEXT ) ) )
        {
            WIN32_FILE_ATTRIBUTE_DATA attrbuf;
            int64_t i_time = 0, i_size = 0;

#ifdef UNDER_CE
            if( GetFileAttributesEx( psz_wpath, GetFileExInfoStandard,
                                     &attrbuf ) )
#else
            if( GetFileAttributesEx( psz_path, GetFileExInfoStandard,
                                     &attrbuf ) )
#endif
            {
                i_time = attrbuf.ftLastWriteTime.dwHighDateTime;
                i_time <<= 32;
                i_time |= attrbuf.ftLastWriteTime.dwLowDateTime;
                i_size = attrbuf.nFileSizeHigh;
                i_size <<= 32;
                i_size |= attrbuf.nFileSizeLow;
            }
            psz_file = psz_path;

            AllocatePluginFile( p_this, psz_file, i_time, i_size );
        }
    }
    while( !p_this->p_vlc->b_die && FindNextFile( handle, &finddata ) );

    /* Close the directory */
    FindClose( handle );

#else
    dir = opendir( psz_dir );
    if( !dir )
    {
        return;
    }

    i_dirlen = strlen( psz_dir );

    /* Parse the directory and try to load all files it contains. */
    while( !p_this->p_vlc->b_die && (file = readdir( dir )) )
    {
        struct stat statbuf;
        unsigned int i_len;
        int i_stat;

        /* Skip ".", ".." and anything starting with "." */
        if( !*file->d_name || *file->d_name == '.' )
        {
            continue;
        }

        i_len = strlen( file->d_name );
        psz_file = malloc( i_dirlen + 1 + i_len + 1 );
#ifdef WIN32
        sprintf( psz_file, "%s\\%s", psz_dir, file->d_name );
#else
        sprintf( psz_file, "%s/%s", psz_dir, file->d_name );
#endif

        i_stat = stat( psz_file, &statbuf );
        if( !i_stat && statbuf.st_mode & S_IFDIR )
        {
            AllocatePluginDir( p_this, psz_file, i_maxdepth - 1 );
        }
        else if( i_len > strlen( LIBEXT )
                  /* We only load files ending with LIBEXT */
                  && !strncasecmp( file->d_name + i_len - strlen( LIBEXT ),
                                   LIBEXT, strlen( LIBEXT ) ) )
        {
            int64_t i_time = 0, i_size = 0;

            if( !i_stat )
            {
                i_time = statbuf.st_mtime;
                i_size = statbuf.st_size;
            }

            AllocatePluginFile( p_this, psz_file, i_time, i_size );
        }

        free( psz_file );
    }

    /* Close the directory */
    closedir( dir );

#endif
}

/*****************************************************************************
 * AllocatePluginFile: load a module into memory and initialize it.
 *****************************************************************************
 * This function loads a dynamically loadable module and allocates a structure
 * for its information data. The module can then be handled by module_Need
 * and module_Unneed. It can be removed by DeleteModule.
 *****************************************************************************/
static int AllocatePluginFile( vlc_object_t * p_this, char * psz_file,
                               int64_t i_file_time, int64_t i_file_size )
{
    module_t * p_module;
    module_cache_t *p_cache_entry = NULL;

    /*
     * Check our plugins cache first then load plugin if needed
     */
    p_cache_entry =
        CacheFind( p_this, psz_file, i_file_time, i_file_size );

    if( !p_cache_entry )
    {
        p_module = AllocatePlugin( p_this, psz_file );
    }
    else
    {
        /* If junk dll, don't try to load it */
        if( p_cache_entry->b_junk )
        {
            p_module = NULL;
        }
        else
        {
            module_config_t *p_item;

            p_module = p_cache_entry->p_module;
            p_module->b_loaded = VLC_FALSE;

            /* For now we force loading if the module's config contains
             * callbacks or actions.
             * Could be optimized by adding an API call.*/
            for( p_item = p_module->p_config;
                 p_item->i_type != CONFIG_HINT_END; p_item++ )
            {
                if( p_item->pf_callback || p_item->i_action )
                    p_module = AllocatePlugin( p_this, psz_file );
            }
        }
    }

    if( p_module )
    {
        /* Everything worked fine !
         * The module is ready to be added to the list. */
        p_module->b_builtin = VLC_FALSE;

        /* msg_Dbg( p_this, "plugin \"%s\", %s",
                    p_module->psz_object_name, p_module->psz_longname ); */

        vlc_object_attach( p_module, p_this->p_libvlc->p_module_bank );
    }

    if( !p_this->p_libvlc->p_module_bank->b_cache ) return 0;

    /* Add entry to cache */
#define p_bank p_this->p_libvlc->p_module_bank
    p_bank->pp_cache =
        realloc( p_bank->pp_cache, (p_bank->i_cache + 1) * sizeof(void *) );
    p_bank->pp_cache[p_bank->i_cache] = malloc( sizeof(module_cache_t) );
    p_bank->pp_cache[p_bank->i_cache]->psz_file = strdup( psz_file );
    p_bank->pp_cache[p_bank->i_cache]->i_time = i_file_time;
    p_bank->pp_cache[p_bank->i_cache]->i_size = i_file_size;
    p_bank->pp_cache[p_bank->i_cache]->b_junk = p_module ? 0 : 1;
    p_bank->pp_cache[p_bank->i_cache]->p_module = p_module;
    p_bank->i_cache++;

    return p_module ? 0 : -1;
}

/*****************************************************************************
 * AllocatePlugin: load a module into memory and initialize it.
 *****************************************************************************
 * This function loads a dynamically loadable module and allocates a structure
 * for its information data. The module can then be handled by module_Need
 * and module_Unneed. It can be removed by DeleteModule.
 *****************************************************************************/
static module_t * AllocatePlugin( vlc_object_t * p_this, char * psz_file )
{
    module_t * p_module;
    module_handle_t handle;

    if( LoadModule( p_this, psz_file, &handle ) ) return NULL;

    /* Now that we have successfully loaded the module, we can
     * allocate a structure for it */
    p_module = vlc_object_create( p_this, VLC_OBJECT_MODULE );
    if( p_module == NULL )
    {
        msg_Err( p_this, "out of memory" );
        CloseModule( handle );
        return NULL;
    }

    /* We need to fill these since they may be needed by CallEntry() */
    p_module->psz_filename = psz_file;
    p_module->handle = handle;
    p_module->p_symbols = &p_this->p_libvlc->p_module_bank->symbols;
    p_module->b_loaded = VLC_TRUE;

    /* Initialize the module: fill p_module, default config */
    if( CallEntry( p_module ) != 0 )
    {
        /* We couldn't call module_init() */
        vlc_object_destroy( p_module );
        CloseModule( handle );
        return NULL;
    }

    DupModule( p_module );
    p_module->psz_filename = strdup( p_module->psz_filename );

    /* Everything worked fine ! The module is ready to be added to the list. */
    p_module->b_builtin = VLC_FALSE;

    return p_module;
}

/*****************************************************************************
 * DupModule: make a plugin module standalone.
 *****************************************************************************
 * This function duplicates all strings in the module, so that the dynamic
 * object can be unloaded. It acts recursively on submodules.
 *****************************************************************************/
static void DupModule( module_t *p_module )
{
    char **pp_shortcut;
    int i_submodule;

    for( pp_shortcut = p_module->pp_shortcuts ; *pp_shortcut ; pp_shortcut++ )
    {
        *pp_shortcut = strdup( *pp_shortcut );
    }

    /* We strdup() these entries so that they are still valid when the
     * module is unloaded. */
    p_module->psz_object_name = strdup( p_module->psz_object_name );
    p_module->psz_capability = strdup( p_module->psz_capability );
    p_module->psz_shortname = p_module->psz_shortname ?
                                 strdup( p_module->psz_shortname ) : NULL;
    p_module->psz_longname = strdup( p_module->psz_longname );

    if( p_module->psz_program != NULL )
    {
        p_module->psz_program = strdup( p_module->psz_program );
    }

    for( i_submodule = 0; i_submodule < p_module->i_children; i_submodule++ )
    {
        DupModule( (module_t*)p_module->pp_children[ i_submodule ] );
    }
}

/*****************************************************************************
 * UndupModule: free a duplicated module.
 *****************************************************************************
 * This function frees the allocations done in DupModule().
 *****************************************************************************/
static void UndupModule( module_t *p_module )
{
    char **pp_shortcut;
    int i_submodule;

    for( i_submodule = 0; i_submodule < p_module->i_children; i_submodule++ )
    {
        UndupModule( (module_t*)p_module->pp_children[ i_submodule ] );
    }

    for( pp_shortcut = p_module->pp_shortcuts ; *pp_shortcut ; pp_shortcut++ )
    {
        free( *pp_shortcut );
    }

    free( p_module->psz_object_name );
    free( p_module->psz_capability );
    if( p_module->psz_shortname ) free( p_module->psz_shortname );
    free( p_module->psz_longname );

    if( p_module->psz_program != NULL )
    {
        free( p_module->psz_program );
    }
}

#endif /* HAVE_DYNAMIC_PLUGINS */

/*****************************************************************************
 * AllocateBuiltinModule: initialize a builtin module.
 *****************************************************************************
 * This function registers a builtin module and allocates a structure
 * for its information data. The module can then be handled by module_Need
 * and module_Unneed. It can be removed by DeleteModule.
 *****************************************************************************/
static int AllocateBuiltinModule( vlc_object_t * p_this,
                                  int ( *pf_entry ) ( module_t * ) )
{
    module_t * p_module;

    /* Now that we have successfully loaded the module, we can
     * allocate a structure for it */
    p_module = vlc_object_create( p_this, VLC_OBJECT_MODULE );
    if( p_module == NULL )
    {
        msg_Err( p_this, "out of memory" );
        return -1;
    }

    /* Initialize the module : fill p_module->psz_object_name, etc. */
    if( pf_entry( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        msg_Err( p_this, "failed calling entry point in builtin module" );
        vlc_object_destroy( p_module );
        return -1;
    }

    /* Everything worked fine ! The module is ready to be added to the list. */
    p_module->b_builtin = VLC_TRUE;

    /* msg_Dbg( p_this, "builtin \"%s\", %s",
                p_module->psz_object_name, p_module->psz_longname ); */

    vlc_object_attach( p_module, p_this->p_libvlc->p_module_bank );

    return 0;
}

/*****************************************************************************
 * DeleteModule: delete a module and its structure.
 *****************************************************************************
 * This function can only be called if the module isn't being used.
 *****************************************************************************/
static int DeleteModule( module_t * p_module )
{
    vlc_object_detach( p_module );

    /* We free the structures that we strdup()ed in Allocate*Module(). */
#ifdef HAVE_DYNAMIC_PLUGINS
    if( !p_module->b_builtin )
    {
        if( p_module->b_loaded && p_module->b_unloadable )
        {
            CloseModule( p_module->handle );
        }
        UndupModule( p_module );
        free( p_module->psz_filename );
    }
#endif

    /* Free and detach the object's children */
    while( p_module->i_children )
    {
        vlc_object_t *p_this = p_module->pp_children[0];
        vlc_object_detach( p_this );
        vlc_object_destroy( p_this );
    }

    config_Free( p_module );
    vlc_object_destroy( p_module );

    return 0;
}

#ifdef HAVE_DYNAMIC_PLUGINS
/*****************************************************************************
 * CallEntry: call an entry point.
 *****************************************************************************
 * This function calls a symbol given its name and a module structure. The
 * symbol MUST refer to a function returning int and taking a module_t* as
 * an argument.
 *****************************************************************************/
static int CallEntry( module_t * p_module )
{
    static char *psz_name = "vlc_entry" MODULE_SUFFIX;
    int (* pf_symbol) ( module_t * p_module );

    /* Try to resolve the symbol */
    pf_symbol = (int (*)(module_t *)) GetSymbol( p_module->handle, psz_name );

    if( pf_symbol == NULL )
    {
#if defined(HAVE_DL_DYLD) || defined(HAVE_DL_BEOS)
        msg_Warn( p_module, "cannot find symbol \"%s\" in file `%s'",
                            psz_name, p_module->psz_filename );
#elif defined(HAVE_DL_WINDOWS)
        char *psz_error = GetWindowsError();
        msg_Warn( p_module, "cannot find symbol \"%s\" in file `%s' (%s)",
                            psz_name, p_module->psz_filename, psz_error );
        free( psz_error );
#elif defined(HAVE_DL_DLOPEN)
        msg_Warn( p_module, "cannot find symbol \"%s\" in file `%s' (%s)",
                            psz_name, p_module->psz_filename, dlerror() );
#elif defined(HAVE_DL_SHL_LOAD)
        msg_Warn( p_module, "cannot find symbol \"%s\" in file `%s' (%s)",
                            psz_name, p_module->psz_filename, strerror(errno) );
#else
#   error "Something is wrong in modules.c"
#endif
        return -1;
    }

    /* We can now try to call the symbol */
    if( pf_symbol( p_module ) != 0 )
    {
        /* With a well-written module we shouldn't have to print an
         * additional error message here, but just make sure. */
        msg_Err( p_module, "failed calling symbol \"%s\" in file `%s'",
                           psz_name, p_module->psz_filename );
        return -1;
    }

    /* Everything worked fine, we can return */
    return 0;
}

/*****************************************************************************
 * LoadModule: loads a dynamic library
 *****************************************************************************
 * This function loads a dynamically linked library using a system dependant
 * method. Will return 0 on success as well as the module handle.
 *****************************************************************************/
static int LoadModule( vlc_object_t *p_this, char *psz_file,
                       module_handle_t *p_handle )
{
    module_handle_t handle;

#if defined(HAVE_DL_DYLD)
    NSObjectFileImage image;
    NSObjectFileImageReturnCode ret;

    ret = NSCreateObjectFileImageFromFile( psz_file, &image );

    if( ret != NSObjectFileImageSuccess )
    {
        msg_Warn( p_this, "cannot create image from `%s'", psz_file );
        return -1;
    }

    /* Open the dynamic module */
    handle = NSLinkModule( image, psz_file,
                           NSLINKMODULE_OPTION_RETURN_ON_ERROR );

    if( !handle )
    {
        NSLinkEditErrors errors;
        const char *psz_file, *psz_err;
        int i_errnum;
        NSLinkEditError( &errors, &i_errnum, &psz_file, &psz_err );
        msg_Warn( p_this, "cannot link module `%s' (%s)", psz_file, psz_err );
        NSDestroyObjectFileImage( image );
        return -1;
    }

    /* Destroy our image, we won't need it */
    NSDestroyObjectFileImage( image );

#elif defined(HAVE_DL_BEOS)
    handle = load_add_on( psz_file );
    if( handle < 0 )
    {
        msg_Warn( p_this, "cannot load module `%s'", psz_file );
        return -1;
    }

#elif defined(HAVE_DL_WINDOWS)
#ifdef UNDER_CE
    {
        wchar_t psz_wfile[MAX_PATH];
        MultiByteToWideChar( CP_ACP, 0, psz_file, -1, psz_wfile, MAX_PATH );
        handle = LoadLibrary( psz_wfile );
    }
#else
    handle = LoadLibrary( psz_file );
#endif
    if( handle == NULL )
    {
        char *psz_err = GetWindowsError();
        msg_Warn( p_this, "cannot load module `%s' (%s)", psz_file, psz_err );
        free( psz_err );
        return -1;
    }

#elif defined(HAVE_DL_DLOPEN) && defined(RTLD_NOW)
    /* static is OK, we are called atomically */

#   if defined(SYS_LINUX)
    /* XXX HACK #1 - we should NOT open modules with RTLD_GLOBAL, or we
     * are going to get namespace collisions when two modules have common
     * public symbols, but ALSA is being a pest here. */
    if( strstr( psz_file, "alsa_plugin" ) )
    {
        handle = dlopen( psz_file, RTLD_NOW | RTLD_GLOBAL );
        if( handle == NULL )
        {
            msg_Warn( p_this, "cannot load module `%s' (%s)",
                              psz_file, dlerror() );
            return -1;
        }
    }
#   endif

    handle = dlopen( psz_file, RTLD_NOW );
    if( handle == NULL )
    {
        msg_Warn( p_this, "cannot load module `%s' (%s)",
                          psz_file, dlerror() );
        return -1;
    }

#elif defined(HAVE_DL_DLOPEN)
#   if defined(DL_LAZY)
    handle = dlopen( psz_file, DL_LAZY );
#   else
    handle = dlopen( psz_file, 0 );
#   endif
    if( handle == NULL )
    {
        msg_Warn( p_this, "cannot load module `%s' (%s)",
                          psz_file, dlerror() );
        return -1;
    }

#elif defined(HAVE_DL_SHL_LOAD)
    handle = shl_load( psz_file, BIND_IMMEDIATE | BIND_NONFATAL, NULL );
    if( handle == NULL )
    {
        msg_Warn( p_this, "cannot load module `%s' (%s)",
                          psz_file, strerror(errno) );
        return -1;
    }

#else
#   error "Something is wrong in modules.c"

#endif

    *p_handle = handle;
    return 0;
}

/*****************************************************************************
 * CloseModule: unload a dynamic library
 *****************************************************************************
 * This function unloads a previously opened dynamically linked library
 * using a system dependant method. No return value is taken in consideration,
 * since some libraries sometimes refuse to close properly.
 *****************************************************************************/
static void CloseModule( module_handle_t handle )
{
#if defined(HAVE_DL_DYLD)
    NSUnLinkModule( handle, FALSE );

#elif defined(HAVE_DL_BEOS)
    unload_add_on( handle );

#elif defined(HAVE_DL_WINDOWS)
    FreeLibrary( handle );

#elif defined(HAVE_DL_DLOPEN)
    dlclose( handle );

#elif defined(HAVE_DL_SHL_LOAD)
    shl_unload( handle );

#endif
    return;
}

/*****************************************************************************
 * GetSymbol: get a symbol from a dynamic library
 *****************************************************************************
 * This function queries a loaded library for a symbol specified in a
 * string, and returns a pointer to it. We don't check for dlerror() or
 * similar functions, since we want a non-NULL symbol anyway.
 *****************************************************************************/
static void * _module_getsymbol( module_handle_t, const char * );

static void * GetSymbol( module_handle_t handle, const char * psz_function )
{
    void * p_symbol = _module_getsymbol( handle, psz_function );

    /* MacOS X dl library expects symbols to begin with "_". So do
     * some other operating systems. That's really lame, but hey, what
     * can we do ? */
    if( p_symbol == NULL )
    {
        char *psz_call = malloc( strlen( psz_function ) + 2 );

        strcpy( psz_call + 1, psz_function );
        psz_call[ 0 ] = '_';
        p_symbol = _module_getsymbol( handle, psz_call );
        free( psz_call );
    }

    return p_symbol;
}

static void * _module_getsymbol( module_handle_t handle,
                                 const char * psz_function )
{
#if defined(HAVE_DL_DYLD)
    NSSymbol sym = NSLookupSymbolInModule( handle, psz_function );
    return NSAddressOfSymbol( sym );

#elif defined(HAVE_DL_BEOS)
    void * p_symbol;
    if( B_OK == get_image_symbol( handle, psz_function,
                                  B_SYMBOL_TYPE_TEXT, &p_symbol ) )
    {
        return p_symbol;
    }
    else
    {
        return NULL;
    }

#elif defined(HAVE_DL_WINDOWS) && defined(UNDER_CE)
    wchar_t psz_real[256];
    MultiByteToWideChar( CP_ACP, 0, psz_function, -1, psz_real, 256 );

    return (void *)GetProcAddress( handle, psz_real );

#elif defined(HAVE_DL_WINDOWS) && defined(WIN32)
    return (void *)GetProcAddress( handle, (char *)psz_function );

#elif defined(HAVE_DL_DLOPEN)
    return dlsym( handle, psz_function );

#elif defined(HAVE_DL_SHL_LOAD)
    void *p_sym;
    shl_findsym( &handle, psz_function, TYPE_UNDEFINED, &p_sym );
    return p_sym;

#endif
}

#if defined(HAVE_DL_WINDOWS)
static char * GetWindowsError( void )
{
#if defined(UNDER_CE)
    wchar_t psz_tmp[MAX_PATH];
    char * psz_buffer = malloc( MAX_PATH );
#else
    char * psz_tmp = malloc( MAX_PATH );
#endif
    int i = 0, i_error = GetLastError();

    FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, i_error, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPTSTR)psz_tmp, MAX_PATH, NULL );

    /* Go to the end of the string */
    while( psz_tmp[i] && psz_tmp[i] != _T('\r') && psz_tmp[i] != _T('\n') )
    {
        i++;
    }

    if( psz_tmp[i] )
    {
#if defined(UNDER_CE)
        swprintf( psz_tmp + i, L" (error %i)", i_error );
        psz_tmp[ 255 ] = L'\0';
#else
        snprintf( psz_tmp + i, 256 - i, " (error %i)", i_error );
        psz_tmp[ 255 ] = '\0';
#endif
    }

#if defined(UNDER_CE)
    wcstombs( psz_buffer, psz_tmp, MAX_PATH );
    return psz_buffer;
#else
    return psz_tmp;
#endif
}
#endif /* HAVE_DL_WINDOWS */

/*****************************************************************************
 * LoadPluginsCache: loads the plugins cache file
 *****************************************************************************
 * This function will load the plugin cache if present and valid. This cache
 * will in turn be queried by AllocateAllPlugins() to see if it needs to
 * actually load the dynamically loadable module.
 * This allows us to only fully load plugins when they are actually used.
 *****************************************************************************/
static void CacheLoad( vlc_object_t *p_this )
{
    char *psz_filename, *psz_homedir;
    FILE *file;
    int i, j, i_size, i_read;
    char p_cachestring[sizeof(PLUGINSCACHE_DIR COPYRIGHT_MESSAGE)];
    char p_cachelang[6], p_lang[6];
    int i_cache;
    module_cache_t **pp_cache = 0;
    int32_t i_file_size, i_marker;

    psz_homedir = p_this->p_vlc->psz_homedir;
    if( !psz_homedir )
    {
        msg_Err( p_this, "psz_homedir is null" );
        return;
    }

    i_size = asprintf( &psz_filename, "%s/%s/%s/%s", psz_homedir, CONFIG_DIR,
                       PLUGINSCACHE_DIR, CacheName() );
    if( i_size <= 0 )
    {
        msg_Err( p_this, "out of memory" );
        return;
    }

    if( p_this->p_libvlc->p_module_bank->b_cache_delete )
    {
#if !defined( UNDER_CE )
        unlink( psz_filename );
#else
        wchar_t psz_wf[MAX_PATH];
        MultiByteToWideChar( CP_ACP, 0, psz_filename, -1, psz_wf, MAX_PATH );
        DeleteFile( psz_wf );
#endif
        msg_Dbg( p_this, "removing plugins cache file %s", psz_filename );
        free( psz_filename );
        return;
    }

    msg_Dbg( p_this, "loading plugins cache file %s", psz_filename );

    file = fopen( psz_filename, "rb" );
    if( !file )
    {
        msg_Warn( p_this, "could not open plugins cache file %s for reading",
                  psz_filename );
        free( psz_filename );
        return;
    }
    free( psz_filename );

    /* Check the file size */
    i_read = fread( &i_file_size, sizeof(char), sizeof(i_file_size), file );
    if( i_read != sizeof(i_file_size) )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(too short)" );
        fclose( file );
        return;
    }

    fseek( file, 0, SEEK_END );
    if( ftell( file ) != i_file_size )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(corrupted size)" );
        fclose( file );
        return;
    }
    fseek( file, sizeof(i_file_size), SEEK_SET );

    /* Check the file is a plugins cache */
    i_size = sizeof(PLUGINSCACHE_DIR COPYRIGHT_MESSAGE) - 1;
    i_read = fread( p_cachestring, sizeof(char), i_size, file );
    if( i_read != i_size ||
        memcmp( p_cachestring, PLUGINSCACHE_DIR COPYRIGHT_MESSAGE, i_size ) )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache" );
        fclose( file );
        return;
    }

    /* Check Sub-version number */
    i_read = fread( &i_marker, sizeof(char), sizeof(i_marker), file );
    if( i_read != sizeof(i_marker) || i_marker != CACHE_SUBVERSION_NUM )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(corrupted header)" );
        fclose( file );
        return;
    }

    /* Check the language hasn't changed */
    sprintf( p_lang, "%5.5s", _("C") ); i_size = 5;
    i_read = fread( p_cachelang, sizeof(char), i_size, file );
    if( i_read != i_size || memcmp( p_cachelang, p_lang, i_size ) )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(language changed)" );
        fclose( file );
        return;
    }

    /* Check header marker */
    i_read = fread( &i_marker, sizeof(char), sizeof(i_marker), file );
    if( i_read != sizeof(i_marker) ||
        i_marker != ftell( file ) - (int)sizeof(i_marker) )
    {
        msg_Warn( p_this, "This doesn't look like a valid plugins cache "
                  "(corrupted header)" );
        fclose( file );
        return;
    }

    p_this->p_libvlc->p_module_bank->i_loaded_cache = 0;
    fread( &i_cache, sizeof(char), sizeof(i_cache), file );
    if( i_cache )
        pp_cache = p_this->p_libvlc->p_module_bank->pp_loaded_cache =
                   malloc( i_cache * sizeof(void *) );

#define LOAD_IMMEDIATE(a) \
    if( fread( &a, sizeof(char), sizeof(a), file ) != sizeof(a) ) goto error
#define LOAD_STRING(a) \
    { if( fread( &i_size, sizeof(char), sizeof(i_size), file ) \
          != sizeof(i_size) ) goto error; \
      if( i_size && i_size < 16384 ) { \
          a = malloc( i_size ); \
          if( fread( a, sizeof(char), i_size, file ) != (size_t)i_size ) \
              goto error; \
          if( a[i_size-1] ) { \
              free( a ); a = 0; \
              goto error; } \
      } else a = 0; \
    } while(0)


    for( i = 0; i < i_cache; i++ )
    {
        int16_t i_size;
        int i_submodules;

        pp_cache[i] = malloc( sizeof(module_cache_t) );
        p_this->p_libvlc->p_module_bank->i_loaded_cache++;

        /* Load common info */
        LOAD_STRING( pp_cache[i]->psz_file );
        LOAD_IMMEDIATE( pp_cache[i]->i_time );
        LOAD_IMMEDIATE( pp_cache[i]->i_size );
        LOAD_IMMEDIATE( pp_cache[i]->b_junk );

        if( pp_cache[i]->b_junk ) continue;

        pp_cache[i]->p_module = vlc_object_create( p_this, VLC_OBJECT_MODULE );

        /* Load additional infos */
        LOAD_STRING( pp_cache[i]->p_module->psz_object_name );
        LOAD_STRING( pp_cache[i]->p_module->psz_shortname );
        LOAD_STRING( pp_cache[i]->p_module->psz_longname );
        LOAD_STRING( pp_cache[i]->p_module->psz_program );
        for( j = 0; j < MODULE_SHORTCUT_MAX; j++ )
        {
            LOAD_STRING( pp_cache[i]->p_module->pp_shortcuts[j] ); // FIX
        }
        LOAD_STRING( pp_cache[i]->p_module->psz_capability );
        LOAD_IMMEDIATE( pp_cache[i]->p_module->i_score );
        LOAD_IMMEDIATE( pp_cache[i]->p_module->i_cpu );
        LOAD_IMMEDIATE( pp_cache[i]->p_module->b_unloadable );
        LOAD_IMMEDIATE( pp_cache[i]->p_module->b_reentrant );
        LOAD_IMMEDIATE( pp_cache[i]->p_module->b_submodule );

        /* Config stuff */
        if( CacheLoadConfig( pp_cache[i]->p_module, file ) != VLC_SUCCESS )
            goto error;

        LOAD_STRING( pp_cache[i]->p_module->psz_filename );

        LOAD_IMMEDIATE( i_submodules );

        while( i_submodules-- )
        {
            module_t *p_module = vlc_object_create( p_this, VLC_OBJECT_MODULE);
            vlc_object_attach( p_module, pp_cache[i]->p_module );
            p_module->b_submodule = VLC_TRUE;

            LOAD_STRING( p_module->psz_object_name );
            LOAD_STRING( p_module->psz_shortname );
            LOAD_STRING( p_module->psz_longname );
            LOAD_STRING( p_module->psz_program );
            for( j = 0; j < MODULE_SHORTCUT_MAX; j++ )
            {
                LOAD_STRING( p_module->pp_shortcuts[j] ); // FIX
            }
            LOAD_STRING( p_module->psz_capability );
            LOAD_IMMEDIATE( p_module->i_score );
            LOAD_IMMEDIATE( p_module->i_cpu );
            LOAD_IMMEDIATE( p_module->b_unloadable );
            LOAD_IMMEDIATE( p_module->b_reentrant );
        }
    }

    fclose( file );
    return;

 error:

    msg_Warn( p_this, "plugins cache not loaded (corrupted)" );

    /* TODO: cleanup */
    p_this->p_libvlc->p_module_bank->i_loaded_cache = 0;

    fclose( file );
    return;
}

int CacheLoadConfig( module_t *p_module, FILE *file )
{
    int i, j, i_lines;
    int16_t i_size;

    /* Calculate the structure length */
    LOAD_IMMEDIATE( p_module->i_config_items );
    LOAD_IMMEDIATE( p_module->i_bool_items );

    LOAD_IMMEDIATE( i_lines );

    /* Allocate memory */
    p_module->p_config =
        (module_config_t *)malloc( sizeof(module_config_t) * (i_lines + 1));
    if( p_module->p_config == NULL )
    {
        msg_Err( p_module, "config error: can't duplicate p_config" );
        return VLC_ENOMEM;
    }

    /* Do the duplication job */
    for( i = 0; i < i_lines ; i++ )
    {
        LOAD_IMMEDIATE( p_module->p_config[i] );

        LOAD_STRING( p_module->p_config[i].psz_type );
        LOAD_STRING( p_module->p_config[i].psz_name );
        LOAD_STRING( p_module->p_config[i].psz_text );
        LOAD_STRING( p_module->p_config[i].psz_longtext );
        LOAD_STRING( p_module->p_config[i].psz_current );
        LOAD_STRING( p_module->p_config[i].psz_value_orig );

        p_module->p_config[i].psz_value =
            p_module->p_config[i].psz_value_orig ?
                strdup( p_module->p_config[i].psz_value_orig ) : 0;
        p_module->p_config[i].i_value = p_module->p_config[i].i_value_orig;
        p_module->p_config[i].f_value = p_module->p_config[i].f_value_orig;
        p_module->p_config[i].i_value_saved = p_module->p_config[i].i_value;
        p_module->p_config[i].f_value_saved = p_module->p_config[i].f_value;
        p_module->p_config[i].psz_value_saved = 0;
        p_module->p_config[i].b_dirty = VLC_FALSE;

        p_module->p_config[i].p_lock = &p_module->object_lock;

        if( p_module->p_config[i].i_list )
        {
            if( p_module->p_config[i].ppsz_list )
            {
                p_module->p_config[i].ppsz_list =
                    malloc( (p_module->p_config[i].i_list+1) * sizeof(char *));
                if( p_module->p_config[i].ppsz_list )
                {
                    for( j = 0; j < p_module->p_config[i].i_list; j++ )
                        LOAD_STRING( p_module->p_config[i].ppsz_list[j] );
                    p_module->p_config[i].ppsz_list[j] = NULL;
                }
            }
            if( p_module->p_config[i].ppsz_list_text )
            {
                p_module->p_config[i].ppsz_list_text =
                    malloc( (p_module->p_config[i].i_list+1) * sizeof(char *));
                if( p_module->p_config[i].ppsz_list_text )
                {
                  for( j = 0; j < p_module->p_config[i].i_list; j++ )
                      LOAD_STRING( p_module->p_config[i].ppsz_list_text[j] );
                  p_module->p_config[i].ppsz_list_text[j] = NULL;
                }
            }
            if( p_module->p_config[i].pi_list )
            {
                p_module->p_config[i].pi_list =
                    malloc( (p_module->p_config[i].i_list + 1) * sizeof(int) );
                if( p_module->p_config[i].pi_list )
                {
                    for( j = 0; j < p_module->p_config[i].i_list; j++ )
                        LOAD_IMMEDIATE( p_module->p_config[i].pi_list[j] );
                }
            }
        }

        if( p_module->p_config[i].i_action )
        {
            p_module->p_config[i].ppf_action =
                malloc( p_module->p_config[i].i_action * sizeof(void *) );
            p_module->p_config[i].ppsz_action_text =
                malloc( p_module->p_config[i].i_action * sizeof(char *) );

            for( j = 0; j < p_module->p_config[i].i_action; j++ )
            {
                p_module->p_config[i].ppf_action[j] = 0;
                LOAD_STRING( p_module->p_config[i].ppsz_action_text[j] );
            }
        }

        LOAD_IMMEDIATE( p_module->p_config[i].pf_callback );
    }

    p_module->p_config[i].i_type = CONFIG_HINT_END;

    return VLC_SUCCESS;

 error:

    return VLC_EGENERIC;
}

/*****************************************************************************
 * SavePluginsCache: saves the plugins cache to a file
 *****************************************************************************/
static void CacheSave( vlc_object_t *p_this )
{
    static char const psz_tag[] =
        "Signature: 8a477f597d28d172789f06886806bc55\r\n"
        "# This file is a cache directory tag created by VLC.\r\n"
        "# For information about cache directory tags, see:\r\n"
        "#   http://www.brynosaurus.com/cachedir/\r\n";

    char *psz_filename, *psz_homedir;
    FILE *file;
    int i, j, i_cache;
    module_cache_t **pp_cache;
    int32_t i_file_size = 0;

    psz_homedir = p_this->p_vlc->psz_homedir;
    if( !psz_homedir )
    {
        msg_Err( p_this, "psz_homedir is null" );
        return;
    }
    psz_filename =
       (char *)malloc( sizeof("/" CONFIG_DIR "/" PLUGINSCACHE_DIR "/" ) +
                       strlen(psz_homedir) + strlen(CacheName()) );

    if( !psz_filename )
    {
        msg_Err( p_this, "out of memory" );
        return;
    }

    sprintf( psz_filename, "%s/%s", psz_homedir, CONFIG_DIR );

    config_CreateDir( p_this, psz_filename );

    strcat( psz_filename, "/" PLUGINSCACHE_DIR );

    config_CreateDir( p_this, psz_filename );

    strcat( psz_filename, "/CACHEDIR.TAG" );

    file = fopen( psz_filename, "wb" );
    if( file )
    {
        fwrite( psz_tag, 1, strlen(psz_tag), file );
        fclose( file );
    }

    sprintf( psz_filename, "%s/%s/%s/%s", psz_homedir, CONFIG_DIR,
             PLUGINSCACHE_DIR, CacheName() );

    msg_Dbg( p_this, "saving plugins cache file %s", psz_filename );

    file = fopen( psz_filename, "wb" );
    if( !file )
    {
        msg_Warn( p_this, "could not open plugins cache file %s for writing",
                  psz_filename );
        free( psz_filename );
        return;
    }
    free( psz_filename );

    /* Empty space for file size */
    fwrite( &i_file_size, sizeof(char), sizeof(i_file_size), file );

    /* Contains version number */
    fprintf( file, "%s", PLUGINSCACHE_DIR COPYRIGHT_MESSAGE );

    /* Sub-version number (to avoid breakage in the dev version when cache
     * structure changes) */
    i_file_size = CACHE_SUBVERSION_NUM;
    fwrite( &i_file_size, sizeof(char), sizeof(i_file_size), file );

    /* Language */
    fprintf( file, "%5.5s", _("C") );

    /* Header marker */
    i_file_size = ftell( file );
    fwrite( &i_file_size, sizeof(char), sizeof(i_file_size), file );

    i_cache = p_this->p_libvlc->p_module_bank->i_cache;
    pp_cache = p_this->p_libvlc->p_module_bank->pp_cache;

    fwrite( &i_cache, sizeof(char), sizeof(i_cache), file );

#define SAVE_IMMEDIATE(a) \
    fwrite( &a, sizeof(char), sizeof(a), file )
#define SAVE_STRING(a) \
    { i_size = a ? strlen( a ) + 1 : 0; \
      fwrite( &i_size, sizeof(char), sizeof(i_size), file ); \
      if( a ) fwrite( a, sizeof(char), i_size, file ); \
    } while(0)

    for( i = 0; i < i_cache; i++ )
    {
        int16_t i_size;
        int32_t i_submodule;

        /* Save common info */
        SAVE_STRING( pp_cache[i]->psz_file );
        SAVE_IMMEDIATE( pp_cache[i]->i_time );
        SAVE_IMMEDIATE( pp_cache[i]->i_size );
        SAVE_IMMEDIATE( pp_cache[i]->b_junk );

        if( pp_cache[i]->b_junk ) continue;

        /* Save additional infos */
        SAVE_STRING( pp_cache[i]->p_module->psz_object_name );
        SAVE_STRING( pp_cache[i]->p_module->psz_shortname );
        SAVE_STRING( pp_cache[i]->p_module->psz_longname );
        SAVE_STRING( pp_cache[i]->p_module->psz_program );
        for( j = 0; j < MODULE_SHORTCUT_MAX; j++ )
        {
            SAVE_STRING( pp_cache[i]->p_module->pp_shortcuts[j] ); // FIX
        }
        SAVE_STRING( pp_cache[i]->p_module->psz_capability );
        SAVE_IMMEDIATE( pp_cache[i]->p_module->i_score );
        SAVE_IMMEDIATE( pp_cache[i]->p_module->i_cpu );
        SAVE_IMMEDIATE( pp_cache[i]->p_module->b_unloadable );
        SAVE_IMMEDIATE( pp_cache[i]->p_module->b_reentrant );
        SAVE_IMMEDIATE( pp_cache[i]->p_module->b_submodule );

        /* Config stuff */
        CacheSaveConfig( pp_cache[i]->p_module, file );

        SAVE_STRING( pp_cache[i]->p_module->psz_filename );

        i_submodule = pp_cache[i]->p_module->i_children;
        SAVE_IMMEDIATE( i_submodule );
        for( i_submodule = 0; i_submodule < pp_cache[i]->p_module->i_children;
             i_submodule++ )
        {
            module_t *p_module =
                (module_t *)pp_cache[i]->p_module->pp_children[i_submodule];

            SAVE_STRING( p_module->psz_object_name );
            SAVE_STRING( p_module->psz_shortname );
            SAVE_STRING( p_module->psz_longname );
            SAVE_STRING( p_module->psz_program );
            for( j = 0; j < MODULE_SHORTCUT_MAX; j++ )
            {
                SAVE_STRING( p_module->pp_shortcuts[j] ); // FIX
            }
            SAVE_STRING( p_module->psz_capability );
            SAVE_IMMEDIATE( p_module->i_score );
            SAVE_IMMEDIATE( p_module->i_cpu );
            SAVE_IMMEDIATE( p_module->b_unloadable );
            SAVE_IMMEDIATE( p_module->b_reentrant );
        }
    }

    /* Fill-up file size */
    i_file_size = ftell( file );
    fseek( file, 0, SEEK_SET );
    fwrite( &i_file_size, sizeof(char), sizeof(i_file_size), file );

    fclose( file );

    return;
}

void CacheSaveConfig( module_t *p_module, FILE *file )
{
    int i, j, i_lines = 0;
    module_config_t *p_item;
    int16_t i_size;

    SAVE_IMMEDIATE( p_module->i_config_items );
    SAVE_IMMEDIATE( p_module->i_bool_items );

    for( p_item = p_module->p_config; p_item->i_type != CONFIG_HINT_END;
         p_item++ ) i_lines++;

    SAVE_IMMEDIATE( i_lines );

    for( i = 0; i < i_lines ; i++ )
    {
        SAVE_IMMEDIATE( p_module->p_config[i] );

        SAVE_STRING( p_module->p_config[i].psz_type );
        SAVE_STRING( p_module->p_config[i].psz_name );
        SAVE_STRING( p_module->p_config[i].psz_text );
        SAVE_STRING( p_module->p_config[i].psz_longtext );
        SAVE_STRING( p_module->p_config[i].psz_current );
        SAVE_STRING( p_module->p_config[i].psz_value_orig );

        if( p_module->p_config[i].i_list )
        {
            if( p_module->p_config[i].ppsz_list )
            {
                for( j = 0; j < p_module->p_config[i].i_list; j++ )
                    SAVE_STRING( p_module->p_config[i].ppsz_list[j] );
            }

            if( p_module->p_config[i].ppsz_list_text )
            {
                for( j = 0; j < p_module->p_config[i].i_list; j++ )
                    SAVE_STRING( p_module->p_config[i].ppsz_list_text[j] );
            }
            if( p_module->p_config[i].pi_list )
            {
                for( j = 0; j < p_module->p_config[i].i_list; j++ )
                    SAVE_IMMEDIATE( p_module->p_config[i].pi_list[j] );
            }
        }

        for( j = 0; j < p_module->p_config[i].i_action; j++ )
            SAVE_STRING( p_module->p_config[i].ppsz_action_text[j] );

        SAVE_IMMEDIATE( p_module->p_config[i].pf_callback );
    }
}

/*****************************************************************************
 * CacheName: Return the cache file name for this platform.
 *****************************************************************************/
static char *CacheName( void )
{
    static char psz_cachename[32];
    static vlc_bool_t b_initialised = VLC_FALSE;

    if( !b_initialised )
    {
        /* Code int size, pointer size and endianness in the filename */
        int32_t x = 0xbe00001e;
        sprintf( psz_cachename, "plugins-%.2x%.2x%.2x.dat", sizeof(int),
                 sizeof(void *), (unsigned int)((unsigned char *)&x)[0] );
        b_initialised = VLC_TRUE;
    }

    return psz_cachename;
}

/*****************************************************************************
 * CacheMerge: Merge a cache module descriptor with a full module descriptor.
 *****************************************************************************/
static void CacheMerge( vlc_object_t *p_this, module_t *p_cache,
                        module_t *p_module )
{
    int i_submodule;

    p_cache->pf_activate = p_module->pf_activate;
    p_cache->pf_deactivate = p_module->pf_deactivate;
    p_cache->p_symbols = p_module->p_symbols;
    p_cache->handle = p_module->handle;

    for( i_submodule = 0; i_submodule < p_module->i_children; i_submodule++ )
    {
        module_t *p_child = (module_t*)p_module->pp_children[i_submodule];
        module_t *p_cchild = (module_t*)p_cache->pp_children[i_submodule];
        p_cchild->pf_activate = p_child->pf_activate;
        p_cchild->pf_deactivate = p_child->pf_deactivate;
        p_cchild->p_symbols = p_child->p_symbols;
    }

    p_cache->b_loaded = VLC_TRUE;
    p_module->b_loaded = VLC_FALSE;
}

/*****************************************************************************
 * FindPluginCache: finds the cache entry corresponding to a file
 *****************************************************************************/
static module_cache_t *CacheFind( vlc_object_t *p_this, char *psz_file,
                                  int64_t i_time, int64_t i_size )
{
    module_cache_t **pp_cache;
    int i_cache, i;

    pp_cache = p_this->p_libvlc->p_module_bank->pp_loaded_cache;
    i_cache = p_this->p_libvlc->p_module_bank->i_loaded_cache;

    for( i = 0; i < i_cache; i++ )
    {
        if( !strcmp( pp_cache[i]->psz_file, psz_file ) &&
            pp_cache[i]->i_time == i_time &&
            pp_cache[i]->i_size == i_size ) return pp_cache[i];
    }

    return NULL;
}

#endif /* HAVE_DYNAMIC_PLUGINS */
