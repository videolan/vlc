/*****************************************************************************
 * vlc_plugin.h : Macros used from within a module.
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

/**
 * \file
 * This file implements plugin (module) macros used to define a vlc module.
 */

/*****************************************************************************
 * If we are not within a module, assume we're in the vlc core.
 *****************************************************************************/
#if !defined( __PLUGIN__ ) && !defined( __BUILTIN__ )
#   define MODULE_NAME main
#endif

/**
 * Current plugin ABI version
 */
# define MODULE_SYMBOL 0_9_0m
# define MODULE_SUFFIX "__0_9_0m"

/*****************************************************************************
 * Add a few defines. You do not want to read this section. Really.
 *****************************************************************************/

/* Explanation:
 *
 * if linking a module statically, we will need:
 * #define MODULE_FUNC( zog ) module_foo_zog
 *
 * this can't easily be done with the C preprocessor, thus a few ugly hacks.
 */

/* I need to do _this_ to change « foo bar » to « module_foo_bar » ! */
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
#else
#   define DLL_SYMBOL
#   define CDECL_SYMBOL
#endif

#if defined( __cplusplus )
#   define EXTERN_SYMBOL           extern "C"
#else
#   define EXTERN_SYMBOL
#endif

/*
 * InitModule: this function is called once and only once, when the module
 * is looked at for the first time. We get the useful data from it, for
 * instance the module name, its shortcuts, its capabilities... we also create
 * a copy of its config because the module can be unloaded at any time.
 */
#define vlc_module_begin( )                                                   \
    EXTERN_SYMBOL DLL_SYMBOL int CDECL_SYMBOL                                 \
    E_(vlc_entry) ( module_t *p_module );                                     \
                                                                              \
    EXTERN_SYMBOL DLL_SYMBOL int CDECL_SYMBOL                                 \
    __VLC_SYMBOL(vlc_entry) ( module_t *p_module )                            \
    {                                                                         \
        module_config_t *p_config = NULL;                                     \
        const char *domain = NULL;                                            \
        if (vlc_module_set (p_module, VLC_MODULE_NAME,                        \
                            (const char *)(MODULE_STRING)))                   \
            goto error;                                                       \
        {                                                                     \
            module_t *p_submodule = p_module;

#define vlc_module_end( )                                                     \
        }                                                                     \
        (void)p_config;                                                       \
        return VLC_SUCCESS;                                                   \
                                                                              \
    error:                                                                    \
        return VLC_EGENERIC;                                                  \
    }                                                                         \
    VLC_METADATA_EXPORTS

#define add_submodule( ) \
    p_submodule = vlc_submodule_create( p_module );

#define add_requirement( cap ) \
    if (vlc_module_set (p_module, VLC_MODULE_CPU_REQUIREMENT, \
                        (int)(CPU_CAPABILITY_##cap))) \
        goto error;

#define add_shortcut( shortcut ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_SHORTCUT, \
        (const char *)(shortcut))) \
        goto error;

#define set_shortname( shortname ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_SHORTNAME, domain, \
                        (const char *)(shortname))) \
        goto error;

#define set_description( desc ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_DESCRIPTION, domain, \
                        (const char *)(desc))) \
        goto error;

#define set_help( help ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_HELP, domain, \
                        (const char *)(help))) \
        goto error;

#define set_capability( cap, score ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_CAPABILITY, \
                        (const char *)(cap)) \
     || vlc_module_set (p_submodule, VLC_MODULE_SCORE, (int)(score))) \
        goto error;

#define set_callbacks( activate, deactivate ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_CB_OPEN, activate) \
     || vlc_module_set (p_submodule, VLC_MODULE_CB_CLOSE, deactivate)) \
        goto error;

#define linked_with_a_crap_library_which_uses_atexit( ) \
    if (vlc_module_set (p_submodule, VLC_MODULE_NO_UNLOAD)) \
        goto error;

#define set_text_domain( dom ) domain = (dom);

VLC_EXPORT( module_t *, vlc_module_create, ( vlc_object_t * ) );
VLC_EXPORT( module_t *, vlc_submodule_create, ( module_t * ) );
VLC_EXPORT( int, vlc_module_set, (module_t *module, int propid, ...) );
VLC_EXPORT( module_config_t *, vlc_config_create, (module_t *, int type) );
VLC_EXPORT( int, vlc_config_set, (module_config_t *, int, ...) );

enum vlc_module_properties
{
    /* DO NOT EVER REMOVE, INSERT OR REPLACE ANY ITEM! It would break the ABI!
     * Append new items at the end ONLY. */
    VLC_MODULE_CPU_REQUIREMENT,
    VLC_MODULE_SHORTCUT,
    VLC_MODULE_SHORTNAME_NODOMAIN,
    VLC_MODULE_DESCRIPTION_NODOMAIN,
    VLC_MODULE_HELP_NODOMAIN,
    VLC_MODULE_CAPABILITY,
    VLC_MODULE_SCORE,
    VLC_MODULE_PROGRAM, /* obsoleted */
    VLC_MODULE_CB_OPEN,
    VLC_MODULE_CB_CLOSE,
    VLC_MODULE_NO_UNLOAD,
    VLC_MODULE_NAME,
    VLC_MODULE_SHORTNAME,
    VLC_MODULE_DESCRIPTION,
    VLC_MODULE_HELP,
};

enum vlc_config_properties
{
    /* DO NOT EVER REMOVE, INSERT OR REPLACE ANY ITEM! It would break the ABI!
     * Append new items at the end ONLY. */

    VLC_CONFIG_NAME,
    /* command line name (args=const char *, vlc_callback_t) */

    VLC_CONFIG_DESC_NODOMAIN,
    /* description (args=const char *, const char *) */

    VLC_CONFIG_VALUE,
    /* actual value (args=int/double/const char *) */

    VLC_CONFIG_RANGE,
    /* minimum value (args=int/double/const char * twice) */

    VLC_CONFIG_ADVANCED,
    /* enable advanced flag (args=none) */

    VLC_CONFIG_VOLATILE,
    /* don't write variable to storage (args=none) */

    VLC_CONFIG_PERSISTENT,
    /* always write variable to storage (args=none) */

    VLC_CONFIG_RESTART,
    /* restart required to apply value change (args=none) */

    VLC_CONFIG_PRIVATE,
    /* hide from user (args=none) */

    VLC_CONFIG_REMOVED,
    /* tag as no longer supported (args=none) */

    VLC_CONFIG_CAPABILITY,
    /* capability for a module or list thereof (args=const char*) */

    VLC_CONFIG_SHORTCUT,
    /* one-character (short) command line option name (args=char) */

    VLC_CONFIG_LIST_NODOMAIN,
    /* possible values list
     * (args=size_t, const <type> *, const char *const *) */

    VLC_CONFIG_ADD_ACTION_NODOMAIN,
    /* add value change callback (args=vlc_callback_t, const char *) */

    VLC_CONFIG_OLDNAME,
    /* former option name (args=const char *) */

    VLC_CONFIG_SAFE,
    /* tag as modifiable by untrusted input item "sources" (args=none) */

    VLC_CONFIG_DESC,
    /* description (args=const char *, const char *, const char *) */

    VLC_CONFIG_LIST,
    /* possible values list
     * (args=const char *, size_t, const <type> *, const char *const *) */

    VLC_CONFIG_ADD_ACTION,
    /* add value change callback
     * (args=const char *, vlc_callback_t, const char *) */
};

/*****************************************************************************
 * Macros used to build the configuration structure.
 *
 * Note that internally we support only 3 types of config data: int, float
 *   and string.
 *   The other types declared here just map to one of these 3 basic types but
 *   have the advantage of also providing very good hints to a configuration
 *   interface so as to make it more user friendly.
 * The configuration structure also includes category hints. These hints can
 *   provide a configuration interface with some very useful data and again
 *   allow for a more user friendly interface.
 *****************************************************************************/

#define add_type_inner( type ) \
    p_config = vlc_config_create (p_module, type);

#define add_typedesc_inner( type, text, longtext ) \
    add_type_inner( type ) \
    vlc_config_set (p_config, VLC_CONFIG_DESC, domain, \
                    (const char *)(text), (const char *)(longtext));

#define add_typeadv_inner( type, text, longtext, advc ) \
    add_typedesc_inner( type, text, longtext ) \
    if (advc) vlc_config_set (p_config, VLC_CONFIG_ADVANCED);

#define add_typename_inner( type, name, text, longtext, advc, cb ) \
    add_typeadv_inner( type, text, longtext, advc ) \
    vlc_config_set (p_config, VLC_CONFIG_NAME, \
                    (const char *)(name), (vlc_callback_t)(cb));

#define add_string_inner( type, name, text, longtext, advc, cb, v ) \
    add_typename_inner( type, name, text, longtext, advc, cb ) \
    vlc_config_set (p_config, VLC_CONFIG_VALUE, (const char *)(v));

#define add_int_inner( type, name, text, longtext, advc, cb, v ) \
    add_typename_inner( type, name, text, longtext, advc, cb ) \
    vlc_config_set (p_config, VLC_CONFIG_VALUE, (int)(v));


#define set_category( i_id ) \
    add_type_inner( CONFIG_CATEGORY ) \
    vlc_config_set (p_config, VLC_CONFIG_VALUE, (int)(i_id));

#define set_subcategory( i_id ) \
    add_type_inner( CONFIG_SUBCATEGORY ) \
    vlc_config_set (p_config, VLC_CONFIG_VALUE, (int)(i_id));

#define set_section( text, longtext ) \
    add_typedesc_inner( CONFIG_SECTION, text, longtext )

#define add_category_hint( text, longtext, advc ) \
    add_typeadv_inner( CONFIG_HINT_CATEGORY, text, longtext, advc )

#define add_subcategory_hint( text, longtext ) \
    add_typedesc_inner( CONFIG_HINT_SUBCATEGORY, text, longtext )

#define end_subcategory_hint \
    add_type_inner( CONFIG_HINT_SUBCATEGORY_END )

#define add_usage_hint( text ) \
    add_typedesc_inner( CONFIG_HINT_USAGE, text, NULL )

#define add_string( name, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_STRING, name, text, longtext, advc, \
                      p_callback, value )

#define add_password( name, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_PASSWORD, name, text, longtext, advc, \
                      p_callback, value )

#define add_file( name, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_FILE, name, text, longtext, advc, \
                      p_callback, value )

#define add_directory( name, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_DIRECTORY, name, text, longtext, advc, \
                      p_callback, value )

#define add_module( name, psz_caps, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE, name, text, longtext, advc, \
                      p_callback, value ) \
    vlc_config_set (p_config, VLC_CONFIG_CAPABILITY, (const char *)(psz_caps));

#define add_module_list( name, psz_caps, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE_LIST, name, text, longtext, advc, \
                      p_callback, value ) \
    vlc_config_set (p_config, VLC_CONFIG_CAPABILITY, (const char *)(psz_caps));

#ifndef __PLUGIN__
#define add_module_cat( name, i_subcategory, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE_CAT, name, text, longtext, advc, \
                      p_callback, value ) \
    p_config->min.i = i_subcategory /* gruik */;

#define add_module_list_cat( name, i_subcategory, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE_LIST_CAT, name, text, longtext, \
                      advc, p_callback, value ) \
    p_config->min.i = i_subcategory /* gruik */;
#endif

#define add_integer( name, value, p_callback, text, longtext, advc ) \
    add_int_inner( CONFIG_ITEM_INTEGER, name, text, longtext, advc, \
                   p_callback, value )

#define add_key( name, value, p_callback, text, longtext, advc ) \
    add_int_inner( CONFIG_ITEM_KEY, name, text, longtext, advc, p_callback, \
                   value )

#define add_integer_with_range( name, value, i_min, i_max, p_callback, text, longtext, advc ) \
    add_integer( name, value, p_callback, text, longtext, advc ) \
    change_integer_range( i_min, i_max )

#define add_float( name, v, p_callback, text, longtext, advc ) \
    add_typename_inner( CONFIG_ITEM_FLOAT, name, text, longtext, advc, p_callback ) \
    vlc_config_set (p_config, VLC_CONFIG_VALUE, (double)(v));

#define add_float_with_range( name, value, f_min, f_max, p_callback, text, longtext, advc ) \
    add_float( name, value, p_callback, text, longtext, advc ) \
    change_float_range( f_min, f_max )

#define add_bool( name, v, p_callback, text, longtext, advc ) \
    add_typename_inner( CONFIG_ITEM_BOOL, name, text, longtext, advc, \
                        p_callback ) \
    if (v) vlc_config_set (p_config, VLC_CONFIG_VALUE, (int)true);

/* For removed option */
#define add_obsolete_inner( name, type ) \
    add_type_inner( type ) \
    vlc_config_set (p_config, VLC_CONFIG_NAME, \
                    (const char *)(name), (vlc_callback_t)NULL); \
    vlc_config_set (p_config, VLC_CONFIG_REMOVED);

#define add_obsolete_bool( name ) \
        add_obsolete_inner( name, CONFIG_ITEM_BOOL )

#define add_obsolete_integer( name ) \
        add_obsolete_inner( name, CONFIG_ITEM_INTEGER )

#define add_obsolete_float( name ) \
        add_obsolete_inner( name, CONFIG_ITEM_FLOAT )

#define add_obsolete_string( name ) \
        add_obsolete_inner( name, CONFIG_ITEM_STRING )

/* Modifier macros for the config options (used for fine tuning) */

#define add_deprecated_alias( name ) \
    vlc_config_set (p_config, VLC_CONFIG_OLDNAME, (const char *)(name))

#define change_short( ch ) \
    vlc_config_set (p_config, VLC_CONFIG_SHORTCUT, (int)(ch));

#define change_string_list( list, list_text, list_update_func ) \
    vlc_config_set (p_config, VLC_CONFIG_LIST, domain, \
                    (size_t)(sizeof (list) / sizeof (char *)), \
                    (const char *const *)(list), \
                    (const char *const *)(list_text), \
                    (vlc_callback_t)(list_update_func));

#define change_integer_list( list, list_text, list_update_func ) \
    vlc_config_set (p_config, VLC_CONFIG_LIST, domain, \
                    (size_t)(sizeof (list) / sizeof (int)), \
                    (const int *)(list), \
                    (const char *const *)(list_text), \
                    (vlc_callback_t)(list_update_func));

#define change_float_list( list, list_text, list_update_func ) \
    vlc_config_set (p_config, VLC_CONFIG_LIST, domain, \
                    (size_t)(sizeof (list) / sizeof (float)), \
                    (const float *)(list), \
                    (const char *const *)(list_text), \
                    (vlc_callback_t)(list_update_func));

#define change_integer_range( minv, maxv ) \
    vlc_config_set (p_config, VLC_CONFIG_RANGE, (int)(minv), (int)(maxv));

#define change_float_range( minv, maxv ) \
    vlc_config_set (p_config, VLC_CONFIG_RANGE, \
                    (double)(minv), (double)(maxv));

#define change_action_add( pf_action, text ) \
    vlc_config_set (p_config, VLC_CONFIG_ADD_ACTION, domain, \
                    (vlc_callback_t)(pf_action), (const char *)(text));

#define change_internal() \
    vlc_config_set (p_config, VLC_CONFIG_PRIVATE);

#define change_need_restart() \
    vlc_config_set (p_config, VLC_CONFIG_RESTART);

#define change_autosave() \
    vlc_config_set (p_config, VLC_CONFIG_PERSISTENT);

#define change_unsaveable() \
    vlc_config_set (p_config, VLC_CONFIG_VOLATILE);

#define change_unsafe() (void)0; /* no-op */

#define change_safe() \
    vlc_config_set (p_config, VLC_CONFIG_SAFE);

/* Meta data plugin exports */
#define VLC_META_EXPORT( name, value ) \
    EXTERN_SYMBOL DLL_SYMBOL const char * CDECL_SYMBOL \
    E_(vlc_entry_ ## name) (void); \
    EXTERN_SYMBOL DLL_SYMBOL const char * CDECL_SYMBOL \
    __VLC_SYMBOL(vlc_entry_ ## name) (void) \
    { \
         return value; \
    }

#if defined (__LIBVLC__)
# define VLC_COPYRIGHT_EXPORT VLC_META_EXPORT (copyright, \
    "\x43\x6f\x70\x79\x72\x69\x67\x68\x74\x20\x28\x43\x29\x20\x74\x68" \
    "\x65\x20\x56\x69\x64\x65\x6f\x4c\x41\x4e\x20\x56\x4c\x43\x20\x6d" \
    "\x65\x64\x69\x61\x20\x70\x6c\x61\x79\x65\x72\x20\x64\x65\x76\x65" \
    "\x6c\x6f\x70\x70\x65\x72\x73" )
#elif !defined (VLC_COPYRIGHT_EXPORT)
# define VLC_COPYRIGHT_EXPORT
#endif
#define VLC_LICENSE_EXPORT VLC_META_EXPORT (license, \
    "\x4c\x69\x63\x65\x6e\x73\x65\x64\x20\x75\x6e\x64\x65\x72\x20\x74" \
    "\x68\x65\x20\x74\x65\x72\x6d\x73\x20\x6f\x66\x20\x74\x68\x65\x20" \
    "\x47\x4e\x55\x20\x47\x65\x6e\x65\x72\x61\x6c\x20\x50\x75\x62\x6c" \
    "\x69\x63\x20\x4c\x69\x63\x65\x6e\x73\x65\x2c\x20\x76\x65\x72\x73" \
    "\x69\x6f\x6e\x20\x32\x20\x6f\x72\x20\x6c\x61\x74\x65\x72\x2e" )

#define VLC_METADATA_EXPORTS \
    VLC_COPYRIGHT_EXPORT \
    VLC_LICENSE_EXPORT

#endif
