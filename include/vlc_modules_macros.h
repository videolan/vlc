/*****************************************************************************
 * modules_macros.h : Macros used from within a module.
 *****************************************************************************
 * Copyright (C) 2001-2006 the VideoLAN team
 * Copyright © 2007-2008 Rémi Denis-Courmont
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

#ifndef LIBVLC_MODULES_MACROS_H
# define LIBVLC_MODULES_MACROS_H 1

/*****************************************************************************
 * If we are not within a module, assume we're in the vlc core.
 *****************************************************************************/
#if !defined( __PLUGIN__ ) && !defined( __BUILTIN__ )
#   define MODULE_NAME main
#endif

/**
 * Current plugin ABI version
 */
# define MODULE_SYMBOL 0_9_0k
# define MODULE_SUFFIX "__0_9_0k"

/*****************************************************************************
 * Add a few defines. You do not want to read this section. Really.
 *****************************************************************************/

/* Explanation:
 *
 * if user has #defined MODULE_NAME foo, then we will need:
 * #define MODULE_STRING "foo"
 *
 * and, if HAVE_DYNAMIC_PLUGINS is NOT set, we will also need:
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
#ifdef __PLUGIN__
#   define E_( function )          CONCATENATE( function, MODULE_SYMBOL )
#   define __VLC_SYMBOL( symbol  ) CONCATENATE( symbol, MODULE_SYMBOL )
#else
#   define E_( function )          CONCATENATE( function, MODULE_NAME )
#   define __VLC_SYMBOL( symbol )  CONCATENATE( symbol, MODULE_NAME )
#endif

#if defined( __PLUGIN__ ) && ( defined( WIN32 ) || defined( UNDER_CE ) )
#   define DLL_SYMBOL              __declspec(dllexport)
#   define CDECL_SYMBOL            __cdecl
#elif defined (HAVE_ATTRIBUTE_VISIBILITY)
#   define DLL_SYMBOL __attribute__((visibility("default")))
#   define CDECL_SYMBOL
#else
#   define DLL_SYMBOL
#   define CDECL_SYMBOL
#endif

#if defined( __cplusplus )
#   define EXTERN_SYMBOL           extern "C"
#else
#   define EXTERN_SYMBOL
#endif

#define MODULE_STRING STRINGIFY( MODULE_NAME )

/*
 * InitModule: this function is called once and only once, when the module
 * is looked at for the first time. We get the useful data from it, for
 * instance the module name, its shortcuts, its capabilities... we also create
 * a copy of its config because the module can be unloaded at any time.
 */
#if defined (__PLUGIN__) || defined (__BUILTIN__)
EXTERN_SYMBOL DLL_SYMBOL int CDECL_SYMBOL
E_(vlc_entry) ( module_t *p_module );
#endif

#define vlc_module_begin( )                                                   \
    EXTERN_SYMBOL DLL_SYMBOL int CDECL_SYMBOL                                 \
    __VLC_SYMBOL(vlc_entry) ( module_t *p_module )                            \
    {                                                                         \
        module_config_t *p_config = NULL;                                     \
        if (vlc_module_set (p_module, VLC_MODULE_NAME,                        \
                            (const char *)(MODULE_STRING)))                   \
            goto error;                                                       \
        {                                                                     \
            module_t *p_submodule = p_module /* the ; gets added */

#define vlc_module_end( )                                                     \
        }                                                                     \
        (void)p_config;                                                       \
        return VLC_SUCCESS;                                                   \
                                                                              \
    error:                                                                    \
        return VLC_EGENERIC;                                                  \
    }                                                                         \
    struct _u_n_u_s_e_d_ /* the ; gets added */


#define add_submodule( ) \
    p_submodule = vlc_submodule_create( p_module )

#define add_requirement( cap ) \
    if (vlc_module_set (p_module, VLC_MODULE_CPU_REQUIREMENT, \
                        (int)(CPU_CAPABILITY_##cap))) \
        goto error

#define add_shortcut( shortcut ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_SHORTCUT, (int)(shortcut))) \
        goto error

#define set_shortname( shortname ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_SHORTNAME, \
        (const char *)(shortname))) \
        goto error

#define set_description( desc ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_DESCRIPTION, \
                        (const char *)(desc))) \
        goto error

#define set_help( help ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_HELP, (const char *)(help))) \
        goto error

#define set_capability( cap, score ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_CAPABILITY, (int)(cap)) \
     || vlc_module_set (p_submodule, VLC_MODULE_SCORE, (int)(score))) \
        goto error

#define set_callbacks( activate, deactivate ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_CB_OPEN, activate) \
     || vlc_module_set (p_submodule, VLC_MODULE_CB_CLOSE, deactivate)) \
        goto error

#define linked_with_a_crap_library_which_uses_atexit( ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_NO_UNLOAD)) \
        goto error

VLC_EXPORT( module_t *, vlc_module_create, ( vlc_object_t * ) );
VLC_EXPORT( module_t *, vlc_submodule_create, ( module_t * ) );
VLC_EXPORT( int, vlc_module_set, (module_t *module, int propid, ...) );

enum vlc_module_properties
{
    /* DO NOT EVER REMOVE, INSERT OR REPLACE ANY ITEM! It would break the ABI!
     * Append new items at the end ONLY. */
    VLC_MODULE_CPU_REQUIREMENT,
    VLC_MODULE_SHORTCUT,
    VLC_MODULE_SHORTNAME,
    VLC_MODULE_DESCRIPTION,
    VLC_MODULE_HELP,
    VLC_MODULE_CAPABILITY,
    VLC_MODULE_SCORE,
    VLC_MODULE_PROGRAM,
    VLC_MODULE_CB_OPEN,
    VLC_MODULE_CB_CLOSE,
    VLC_MODULE_NO_UNLOAD,
    VLC_MODULE_NAME,
};
#endif
