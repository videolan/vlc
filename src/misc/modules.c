/*****************************************************************************
 * modules.c : Builtin and plugin modules management functions
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: modules.c,v 1.88 2002/08/15 12:22:45 sam Exp $
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
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

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
#include "vlc_playlist.h"
#include "intf_eject.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "video.h"
#include "video_output.h"

#include "audio_output.h"
#include "aout_internal.h"

#include "stream_output.h"

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
 * Local prototypes
 *****************************************************************************/
#ifdef HAVE_DYNAMIC_PLUGINS
static void AllocateAllPlugins   ( vlc_object_t * );
static void AllocatePluginDir    ( vlc_object_t *, const char *, int );
static int  AllocatePluginFile   ( vlc_object_t *, char * );
#endif
static int  AllocateBuiltinModule( vlc_object_t *, int ( * ) ( module_t * ) );
static int  DeleteModule ( module_t * );
#ifdef HAVE_DYNAMIC_PLUGINS
static void DupModule    ( module_t * );
static void UndupModule  ( module_t * );
static int  CallEntry    ( module_t * );
#endif

/*****************************************************************************
 * module_InitBank: create the module bank.
 *****************************************************************************
 * This function creates a module bank structure which will be filled later
 * on with all the modules found.
 *****************************************************************************/
void __module_InitBank( vlc_object_t *p_this )
{
    module_bank_t *p_bank;

    p_bank = vlc_object_create( p_this, sizeof(module_bank_t) );
    p_bank->psz_object_name = "module bank";

    /*
     * Store the symbols to be exported
     */
#ifdef HAVE_DYNAMIC_PLUGINS
    STORE_SYMBOLS( &p_bank->symbols );
#endif

    /* Everything worked, attach the object */
    p_this->p_vlc->p_module_bank = p_bank;
    vlc_object_attach( p_bank, p_this->p_vlc );

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

    vlc_object_detach( p_this->p_vlc->p_module_bank );

    while( p_this->p_vlc->p_module_bank->i_children )
    {
        p_next = (module_t *)p_this->p_vlc->p_module_bank->pp_children[0];

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

    vlc_object_destroy( p_this->p_vlc->p_module_bank );

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
    AllocateBuiltinModule( p_this, vlc_entry__main );
}

/*****************************************************************************
 * module_LoadBuiltins: load all modules which we built with.
 *****************************************************************************
 * This function fills the module bank structure with the builtin modules.
 *****************************************************************************/
void __module_LoadBuiltins( vlc_object_t * p_this )
{
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
    msg_Dbg( p_this, "checking plugin modules" );
    AllocateAllPlugins( p_this );
#endif
}

/*****************************************************************************
 * module_Need: return the best module function, given a capability list.
 *****************************************************************************
 * This function returns the module that best fits the asked capabilities.
 *****************************************************************************/
module_t * __module_Need( vlc_object_t *p_this, const char *psz_capability,
                          const char *psz_name )
{
    typedef struct module_list_t module_list_t;

    struct module_list_t
    {
        module_t *p_module;
        int i_score;
        module_list_t *p_next;
    };

    module_list_t *p_list, *p_first, *p_tmp;
    vlc_list_t *p_all;

    int i_index = 0;
    vlc_bool_t b_intf = VLC_FALSE;

    module_t *p_module;
    module_t **pp_parser;

    int   i_shortcuts = 0;
    char *psz_shortcuts = NULL, *psz_var = NULL;

    msg_Dbg( p_this, "looking for %s module", psz_capability );

    /* Deal with variables */
    if( psz_name && psz_name[0] == '$' )
    {
        psz_var = config_GetPsz( p_this, psz_name + 1 );
        psz_name = psz_var;
    }

    /* Count how many different shortcuts were asked for */
    if( psz_name && *psz_name )
    {
        char *psz_parser;

        /* If the user wants none, give him none. */
        if( !strcmp( psz_name, "none" ) )
        {
            if( psz_var ) free( psz_var );
            return NULL;
        }

        i_shortcuts++;
        psz_shortcuts = strdup( psz_name );

        for( psz_parser = psz_shortcuts; *psz_parser; psz_parser++ )
        {
            if( *psz_parser == ',' )
            {
                 *psz_parser = '\0';
                 i_shortcuts++;
            }
        }
    }

    /* Sort the modules and test them */
    p_all = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    p_list = malloc( p_all->i_count * sizeof( module_list_t ) );
    p_first = NULL;

    /* Parse the module list for capabilities and probe each of them */
    for( pp_parser = (module_t**)p_all->pp_objects ; *pp_parser ; pp_parser++ )
    {
        module_t * p_submodule = NULL;
        int i_shortcut_bonus = 0, i_submodule;

        p_module = *pp_parser;

        /* Test that this module can do what we need */
        if( strcmp( p_module->psz_capability, psz_capability ) )
        {
            for( i_submodule = 0;
                 i_submodule < p_module->i_children;
                 i_submodule++ )
            {
                if( !strcmp( ((module_t*)p_module->pp_children[ i_submodule ])
                                           ->psz_capability, psz_capability ) )
                {
                    p_submodule =
                            (module_t*)p_module->pp_children[ i_submodule ];
                    break;
                }
            }

            if( p_submodule == NULL )
            {
                continue;
            }

            p_module = p_submodule;
        }

        /* Test if we have the required CPU */
        if( (p_module->i_cpu & p_this->p_vlc->i_cpu) != p_module->i_cpu )
        {
            continue;
        }

        /* If we required a shortcut, check this plugin provides it. */
        if( i_shortcuts )
        {
            vlc_bool_t b_trash = VLC_TRUE;
            int i_dummy, i_short = i_shortcuts;
            char *psz_name = psz_shortcuts;

            while( i_short )
            {
                for( i_dummy = 0;
                     b_trash && p_module->pp_shortcuts[i_dummy];
                     i_dummy++ )
                {
                    b_trash = ( strcmp(psz_name, "any") || !p_module->i_score )
                        && strcmp( psz_name, p_module->pp_shortcuts[i_dummy] );
                }

                if( !b_trash )
                {
                    i_shortcut_bonus = i_short * 10000;
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

            if( b_trash )
            {
                continue;
            }
        }
        /* If we didn't require a shortcut, trash zero-scored plugins */
        else if( !p_module->i_score )
        {
            continue;
        }

        /* Special case: test if we requested a particular intf plugin */
        if( p_module->psz_program
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

    msg_Dbg( p_this, "probing %i candidate%s",
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

    if( p_module != NULL )
    {
        msg_Info( p_module, "using %s module \"%s\"",
                  psz_capability, p_module->psz_object_name );
    }
    else if( p_first == NULL )
    {
        msg_Err( p_this, "no %s module matched \"%s\"",
                 psz_capability, (psz_name && *psz_name) ? psz_name : "any" );
    }
    else if( psz_name != NULL && *psz_name )
    {
        msg_Err( p_this, "no %s module matching \"%s\" could be loaded",
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

    msg_Info( p_module, "unlocking module \"%s\"", p_module->psz_object_name );

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
    char *          path[] = { "modules", PLUGIN_PATH, NULL, NULL };

    char **         ppsz_path = path;
    char *          psz_fullpath;
#if defined( SYS_BEOS ) || defined( SYS_DARWIN )
    char *          psz_vlcpath = system_GetProgramPath();
    int             i_vlclen = strlen( psz_vlcpath );
    vlc_bool_t      b_notinroot;
#endif

    /* If the user provided a plugin path, we add it to the list */
    path[ sizeof(path)/sizeof(char*) - 2 ] = config_GetPsz( p_this,
                                                            "plugin-path" );

    for( ; *ppsz_path != NULL ; ppsz_path++ )
    {
#if defined( SYS_BEOS ) || defined( SYS_DARWIN )
        /* Store strlen(*ppsz_path) for later use. */
        int i_dirlen = strlen( *ppsz_path );

        b_notinroot = VLC_FALSE;
        /* Under BeOS, we need to add beos_GetProgramPath() to access
         * files under the current directory */
        if( ( i_dirlen > 1 ) && strncmp( *ppsz_path, "/", 1 ) )
        {
            i_dirlen += i_vlclen + 2;
            b_notinroot = VLC_TRUE;

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

        msg_Dbg( p_this, "recursively browsing `%s'", psz_fullpath );

        /* Don't go deeper than 5 subdirectories */
        AllocatePluginDir( p_this, psz_fullpath, 5 );

#if defined( SYS_BEOS ) || defined( SYS_DARWIN )
        if( b_notinroot )
        {
            free( psz_fullpath );
        }
#endif
    }
}

/*****************************************************************************
 * AllocatePluginDir: recursively parse a directory to look for plugins
 *****************************************************************************/
static void AllocatePluginDir( vlc_object_t *p_this, const char *psz_dir,
                               int i_maxdepth )
{
#define PLUGIN_EXT ".so"
    int    i_dirlen;
    DIR *  dir;
    char * psz_file;

    struct dirent * file;

    if( i_maxdepth < 0 )
    {
        return;
    }

    dir = opendir( psz_dir );

    if( !dir )
    {
        return;
    }

    i_dirlen = strlen( psz_dir );

    /* Parse the directory and try to load all files it contains. */
    while( (file = readdir( dir )) )
    {
        struct stat statbuf;
        int i_len = strlen( file->d_name );

        /* Skip ".", ".." and anything starting with "." */
        if( !*file->d_name || *file->d_name == '.' )
        {
            continue;
        }

        psz_file = malloc( i_dirlen + 1 /* / */ + i_len + 1 /* \0 */ );
        sprintf( psz_file, "%s/%s", psz_dir, file->d_name );

        if( !stat( psz_file, &statbuf ) && statbuf.st_mode & S_IFDIR )
        {
            AllocatePluginDir( p_this, psz_file, i_maxdepth - 1 );
        }
        else if( i_len > strlen( PLUGIN_EXT )
                  /* We only load files ending with ".so" */
                  && !strncmp( file->d_name + i_len - strlen( PLUGIN_EXT ),
                               PLUGIN_EXT, strlen( PLUGIN_EXT ) ) )
        {
            AllocatePluginFile( p_this, psz_file );
        }

        free( psz_file );
    }

    /* Close the directory */
    closedir( dir );
}

/*****************************************************************************
 * AllocatePluginFile: load a module into memory and initialize it.
 *****************************************************************************
 * This function loads a dynamically loadable module and allocates a structure
 * for its information data. The module can then be handled by module_Need
 * and module_Unneed. It can be removed by DeleteModule.
 *****************************************************************************/
static int AllocatePluginFile( vlc_object_t * p_this, char * psz_file )
{
    module_t * p_module;
    module_handle_t handle;

    /* Try to dynamically load the module. */
    if( module_load( psz_file, &handle ) )
    {
        char psz_buffer[256];

        /* The plugin module couldn't be opened */
        msg_Warn( p_this, "cannot open `%s' (%s)",
                  psz_file, module_error( psz_buffer ) );
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

    /* We need to fill these since they may be needed by CallEntry() */
    p_module->psz_filename = psz_file;
    p_module->handle = handle;
    p_module->p_symbols = &p_this->p_vlc->p_module_bank->symbols;

    /* Initialize the module: fill p_module->psz_object_name, default config */
    if( CallEntry( p_module ) != 0 )
    {
        /* We couldn't call module_init() */
        msg_Err( p_this, "failed calling entry point in `%s'", psz_file );
        vlc_object_destroy( p_module );
        module_unload( handle );
        return -1;
    }

    DupModule( p_module );
    p_module->psz_filename = strdup( p_module->psz_filename );
    p_module->psz_longname = strdup( p_module->psz_longname );

    /* Everything worked fine ! The module is ready to be added to the list. */
    p_module->b_builtin = VLC_FALSE;

    /* msg_Dbg( p_this, "plugin \"%s\", %s",
                p_module->psz_object_name, p_module->psz_longname ); */

    vlc_object_attach( p_module, p_this->p_vlc->p_module_bank );

    return 0;
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

    vlc_object_attach( p_module, p_this->p_vlc->p_module_bank );

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
        module_unload( p_module->handle );
        UndupModule( p_module );
        free( p_module->psz_filename );
        free( p_module->psz_longname );
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
    pf_symbol = (int (*)(module_t *)) module_getsymbol( p_module->handle, 
                                                        psz_name );

    if( pf_symbol == NULL )
    {
        char psz_buffer[256];

        /* We couldn't load the symbol */
        msg_Warn( p_module, "cannot find symbol \"%s\" in file `%s' (%s)",
                            psz_name, p_module->psz_filename,
                            module_error( psz_buffer ) );
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
#endif /* HAVE_DYNAMIC_PLUGINS */

