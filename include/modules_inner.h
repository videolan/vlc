/*****************************************************************************
 * modules_inner.h : Macros used from within a module.
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * If we are not within a module, assume we're in the vlc core.
 *****************************************************************************/
#if !defined( __PLUGIN__ ) && !defined( __BUILTIN__ )
#   define MODULE_NAME main
#endif

/*****************************************************************************
 * Add a few defines. You do not want to read this section. Really.
 *****************************************************************************/

/* Explanation:
 *
 * if user has #defined MODULE_NAME foo, then we will need:
 * #define MODULE_STRING "foo"
 *
 * and, if __BUILTIN__ is set, we will also need:
 * #define MODULE_FUNC( zog ) module_foo_zog
 *
 * this can't easily be done with the C preprocessor, thus a few ugly hacks.
 */

/* I can't believe I need to do this to change « foo » to « "foo" » */
#define STRINGIFY( z )   UGLY_KLUDGE( z )
#define UGLY_KLUDGE( z ) #z
/* And I need to do _this_ to change « foo bar » to « module_foo_bar » ! */
#define CONCATENATE( y, z ) CRUDE_HACK( y, z )
#define CRUDE_HACK( y, z )  y##__##z

/* If the module is built-in, then we need to define foo_InitModule instead
 * of InitModule. Same for Activate- and DeactivateModule. */
#if defined( __BUILTIN__ )
#   define E_( function )          CONCATENATE( function, MODULE_NAME )
#   define __VLC_SYMBOL( symbol )  CONCATENATE( symbol, MODULE_NAME )
#   define DECLARE_SYMBOLS         struct _u_n_u_s_e_d_
#   define STORE_SYMBOLS           struct _u_n_u_s_e_d_
#elif defined( __PLUGIN__ )
#   define E_( function )          CONCATENATE( function, MODULE_SYMBOL )
#   define __VLC_SYMBOL( symbol  ) CONCATENATE( symbol, MODULE_SYMBOL )
#   define DECLARE_SYMBOLS         module_symbols_t* p_symbols
#   define STORE_SYMBOLS           p_symbols = p_module->p_symbols
#endif

#if defined( __PLUGIN__ ) && ( defined( WIN32 ) || defined( UNDER_CE ) )
#   define DLL_SYMBOL              __declspec(dllexport)
#   define CDECL_SYMBOL            __cdecl
#else
#   define DLL_SYMBOL
#   define CDECL_SYMBOL
#endif

#if defined( __cplusplus )
#   define EXTERN_SYMBOL           extern "C"
#else
#   define EXTERN_SYMBOL
#endif

#if defined( USE_DLL )
#   define IMPORT_SYMBOL __declspec(dllimport)
#else
#   define IMPORT_SYMBOL
#endif

#define MODULE_STRING STRINGIFY( MODULE_NAME )

/*
 * InitModule: this function is called once and only once, when the module
 * is looked at for the first time. We get the useful data from it, for
 * instance the module name, its shortcuts, its capabilities... we also create
 * a copy of its config because the module can be unloaded at any time.
 */
#define vlc_module_begin( )                                                   \
    DECLARE_SYMBOLS;                                                          \
    EXTERN_SYMBOL DLL_SYMBOL int CDECL_SYMBOL                                 \
    __VLC_SYMBOL(vlc_entry) ( module_t *p_module )                            \
    {                                                                         \
        int i_shortcut = 1, i_config = -1;                                    \
        module_config_t *p_config = NULL;                                     \
        static module_config_t config_end = {                                 \
            CONFIG_HINT_END, NULL, NULL, 0, NULL, NULL, NULL, 0, 0., 0, 0,    \
            0., 0., NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, 0, NULL,     \
            VLC_FALSE, NULL, VLC_FALSE, VLC_FALSE, NULL, 0, 0., NULL, 0, 0.,  \
            VLC_FALSE                                                         \
        };                                                                    \
        STORE_SYMBOLS;                                                        \
        p_module->b_submodule = VLC_FALSE;                                    \
        p_module->b_unloadable = VLC_TRUE;                                    \
        p_module->b_reentrant = VLC_TRUE;                                     \
        p_module->psz_object_name = MODULE_STRING;                            \
        p_module->psz_shortname = NULL;                                       \
        p_module->psz_longname = MODULE_STRING;                               \
        p_module->pp_shortcuts[ 0 ] = MODULE_STRING;                          \
        p_module->i_cpu = 0;                                                  \
        p_module->psz_program = NULL;                                         \
        p_module->psz_capability = "";                                        \
        p_module->i_score = 1;                                                \
        p_module->pf_activate = NULL;                                         \
        p_module->pf_deactivate = NULL;                                       \
        {                                                                     \
            module_t *p_submodule = p_module /* the ; gets added */

#define vlc_module_end( )                                                     \
            p_submodule->pp_shortcuts[ i_shortcut ] = NULL;                   \
        }                                                                     \
        if( p_config )                                                        \
        {                                                                     \
            int i;                                                            \
            p_config[ ++i_config ] = config_end;                              \
            config_Duplicate( p_module, p_config );                           \
            for( i = 0; i < i_config; i++ )                                   \
            {                                                                 \
                if( p_config[ i ].i_action )                                  \
                {                                                             \
                    free( p_config[ i ].ppf_action );                         \
                    free( p_config[ i ].ppsz_action_text );                   \
                }                                                             \
            }                                                                 \
            free( p_config );                                                 \
        }                                                                     \
        else config_Duplicate( p_module, &config_end );                       \
        if( p_module->p_config == NULL )                                      \
        {                                                                     \
            return VLC_EGENERIC;                                              \
        }                                                                     \
        return VLC_SUCCESS && i_shortcut;                                     \
    }                                                                         \
    struct _u_n_u_s_e_d_ /* the ; gets added */


#define add_submodule( )                                                      \
    p_submodule->pp_shortcuts[ i_shortcut ] = NULL;                           \
    p_submodule =                                                             \
            (module_t *)vlc_object_create( p_module, VLC_OBJECT_MODULE );     \
    vlc_object_attach( p_submodule, p_module );                               \
    p_submodule->b_submodule = VLC_TRUE;                                      \
    /* Nuahahaha! Heritage! Polymorphism! Ugliness!! */                       \
    for( i_shortcut = 0; p_module->pp_shortcuts[ i_shortcut ]; i_shortcut++ ) \
    {                                                                         \
        p_submodule->pp_shortcuts[ i_shortcut ] =                             \
                                p_module->pp_shortcuts[ i_shortcut ];         \
    }                                                                         \
    p_submodule->psz_object_name = p_module->psz_object_name;                 \
    p_submodule->psz_shortname = p_module->psz_shortname;                     \
    p_submodule->psz_longname = p_module->psz_longname;                       \
    p_submodule->psz_program = p_module->psz_program;                         \
    p_submodule->psz_capability = p_module->psz_capability;                   \
    p_submodule->i_score = p_module->i_score;                                 \
    p_submodule->i_cpu = p_module->i_cpu;                                     \
    p_submodule->pf_activate = NULL;                                          \
    p_submodule->pf_deactivate = NULL

#define add_requirement( cap )                                                \
    p_module->i_cpu |= CPU_CAPABILITY_##cap

#define add_shortcut( shortcut )                                              \
    p_submodule->pp_shortcuts[ i_shortcut ] = shortcut;                       \
    i_shortcut++

#define set_shortname( desc )                                                 \
    p_submodule->psz_shortname = desc

#define set_description( desc )                                               \
    p_submodule->psz_longname = desc

#define set_capability( cap, score )                                          \
    p_submodule->psz_capability = cap;                                        \
    p_submodule->i_score = score

#define set_program( program )                                                \
    p_submodule->psz_program = program

#define set_callbacks( activate, deactivate )                                 \
    p_submodule->pf_activate = activate;                                      \
    p_submodule->pf_deactivate = deactivate

#define linked_with_a_crap_library_which_uses_atexit( )                       \
    p_module->b_unloadable = VLC_FALSE

