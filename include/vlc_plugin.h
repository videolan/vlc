/*****************************************************************************
 * vlc_plugin.h : Macros used from within a module.
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 * Copyright © 2007-2009 Rémi Denis-Courmont
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef LIBVLC_MODULES_MACROS_H
# define LIBVLC_MODULES_MACROS_H 1

/**
 * \file
 * This file implements plugin (module) macros used to define a vlc module.
 */

enum vlc_module_properties
{
    VLC_MODULE_CREATE,
    VLC_CONFIG_CREATE,

    /* DO NOT EVER REMOVE, INSERT OR REPLACE ANY ITEM! It would break the ABI!
     * Append new items at the end ONLY. */
    VLC_MODULE_CPU_REQUIREMENT=0x100,
    VLC_MODULE_SHORTCUT,
    VLC_MODULE_CAPABILITY,
    VLC_MODULE_SCORE,
    VLC_MODULE_CB_OPEN,
    VLC_MODULE_CB_CLOSE,
    VLC_MODULE_NO_UNLOAD,
    VLC_MODULE_NAME,
    VLC_MODULE_SHORTNAME,
    VLC_MODULE_DESCRIPTION,
    VLC_MODULE_HELP,
    VLC_MODULE_TEXTDOMAIN,
    /* Insert new VLC_MODULE_* here */

    /* DO NOT EVER REMOVE, INSERT OR REPLACE ANY ITEM! It would break the ABI!
     * Append new items at the end ONLY. */
    VLC_CONFIG_NAME=0x1000,
    /* command line name (args=const char *) */

    VLC_CONFIG_VALUE,
    /* actual value (args=int64_t/double/const char *) */

    VLC_CONFIG_RANGE,
    /* minimum value (args=int64_t/double/const char * twice) */

    VLC_CONFIG_ADVANCED_RESERVED,
    /* reserved - do not use */

    VLC_CONFIG_VOLATILE,
    /* don't write variable to storage (args=none) */

    VLC_CONFIG_PERSISTENT_OBSOLETE,
    /* unused (ignored) */

    VLC_CONFIG_PRIVATE,
    /* hide from user (args=none) */

    VLC_CONFIG_REMOVED,
    /* tag as no longer supported (args=none) */

    VLC_CONFIG_CAPABILITY,
    /* capability for a module or list thereof (args=const char*) */

    VLC_CONFIG_SHORTCUT,
    /* one-character (short) command line option name (args=char) */

    VLC_CONFIG_OLDNAME_OBSOLETE,
    /* unused (ignored) */

    VLC_CONFIG_SAFE,
    /* tag as modifiable by untrusted input item "sources" (args=none) */

    VLC_CONFIG_DESC,
    /* description (args=const char *, const char *, const char *) */

    VLC_CONFIG_LIST_OBSOLETE,
    /* unused (ignored) */

    VLC_CONFIG_ADD_ACTION_OBSOLETE,
    /* unused (ignored) */

    VLC_CONFIG_LIST,
    /* list of suggested values
     * (args=size_t, const <type> *, const char *const *) */

    VLC_CONFIG_LIST_CB_OBSOLETE,
    /* unused (ignored) */

    /* Insert new VLC_CONFIG_* here */
};

/* Configuration hint types */
#define CONFIG_HINT_CATEGORY                0x02  /* Start of new category */

#define CONFIG_SUBCATEGORY                  0x07 /* Set subcategory */
#define CONFIG_SECTION                      0x08 /* Start of new section */

/* Configuration item types */
#define CONFIG_ITEM_FLOAT                   0x20  /* Float option */
#define CONFIG_ITEM_INTEGER                 0x40  /* Integer option */
#define CONFIG_ITEM_RGB                     0x41  /* RGB color option */
#define CONFIG_ITEM_BOOL                    0x60  /* Bool option */
#define CONFIG_ITEM_STRING                  0x80  /* String option */
#define CONFIG_ITEM_PASSWORD                0x81  /* Password option (*) */
#define CONFIG_ITEM_KEY                     0x82  /* Hot key option */
#define CONFIG_ITEM_MODULE                  0x84  /* Module option */
#define CONFIG_ITEM_MODULE_CAT              0x85  /* Module option */
#define CONFIG_ITEM_MODULE_LIST             0x86  /* Module option */
#define CONFIG_ITEM_MODULE_LIST_CAT         0x87  /* Module option */
#define CONFIG_ITEM_LOADFILE                0x8C  /* Read file option */
#define CONFIG_ITEM_SAVEFILE                0x8D  /* Written file option */
#define CONFIG_ITEM_DIRECTORY               0x8E  /* Directory option */
#define CONFIG_ITEM_FONT                    0x8F  /* Font option */

/* reduce specific type to type class */
#define CONFIG_CLASS(x) ((x) & ~0x1F)

/* is proper option, not a special hint type? */
#define CONFIG_ITEM(x) (((x) & ~0xF) != 0)

#define IsConfigStringType(type) \
    (((type) & CONFIG_ITEM_STRING) != 0)
#define IsConfigIntegerType(type) \
    (((type) & CONFIG_ITEM_INTEGER) != 0)
#define IsConfigFloatType(type) \
    ((type) == CONFIG_ITEM_FLOAT)

/* Config category */
enum vlc_config_cat
{
    /* Hidden category.
       Any options under this will be hidden in the GUI preferences, but will
       be listed in cmdline help output. */
    CAT_HIDDEN    = -1,

    CAT_UNKNOWN   = 0,

    CAT_INTERFACE = 1,
    CAT_AUDIO     = 2,
    CAT_VIDEO     = 3,
    CAT_INPUT     = 4,
    CAT_SOUT      = 5,
    CAT_ADVANCED  = 6,
    CAT_PLAYLIST  = 7,
};

/* Config subcategory */
enum vlc_config_subcat
{
    /* Hidden subcategory.
       Any options under this will be hidden in the GUI preferences, but will
       be listed in cmdline help output. */
    SUBCAT_HIDDEN              = -1,

    SUBCAT_UNKNOWN             = 0,

    SUBCAT_INTERFACE_GENERAL   = 101,
    SUBCAT_INTERFACE_MAIN      = 102,
    SUBCAT_INTERFACE_CONTROL   = 103,
    SUBCAT_INTERFACE_HOTKEYS   = 104,

    SUBCAT_AUDIO_GENERAL       = 201,
    SUBCAT_AUDIO_AOUT          = 202,
    SUBCAT_AUDIO_AFILTER       = 203,
    SUBCAT_AUDIO_VISUAL        = 204,
    SUBCAT_AUDIO_RESAMPLER     = 206,

    SUBCAT_VIDEO_GENERAL       = 301,
    SUBCAT_VIDEO_VOUT          = 302,
    SUBCAT_VIDEO_VFILTER       = 303,
    SUBCAT_VIDEO_SUBPIC        = 305,
    SUBCAT_VIDEO_SPLITTER      = 306,

    SUBCAT_INPUT_GENERAL       = 401,
    SUBCAT_INPUT_ACCESS        = 402,
    SUBCAT_INPUT_DEMUX         = 403,
    SUBCAT_INPUT_VCODEC        = 404,
    SUBCAT_INPUT_ACODEC        = 405,
    SUBCAT_INPUT_SCODEC        = 406,
    SUBCAT_INPUT_STREAM_FILTER = 407,

    SUBCAT_SOUT_GENERAL        = 501,
    SUBCAT_SOUT_STREAM         = 502,
    SUBCAT_SOUT_MUX            = 503,
    SUBCAT_SOUT_ACO            = 504,
    SUBCAT_SOUT_PACKETIZER     = 505,
    SUBCAT_SOUT_VOD            = 507,
    SUBCAT_SOUT_RENDERER       = 508,

    SUBCAT_ADVANCED_MISC       = 602,
    SUBCAT_ADVANCED_NETWORK    = 603,

    SUBCAT_PLAYLIST_GENERAL    = 701,
    SUBCAT_PLAYLIST_SD         = 702,
    SUBCAT_PLAYLIST_EXPORT     = 703,
};

/**
 * Current plugin ABI version
 */
#define VLC_API_VERSION_STRING "4.0.6"

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

#if defined(__cplusplus)
#define EXTERN_SYMBOL extern "C"
#else
#define EXTERN_SYMBOL
#endif

/* If the module is built-in, then we need to define foo_InitModule instead
 * of InitModule. Same for Activate- and DeactivateModule. */
#ifdef __PLUGIN__
# define VLC_SYMBOL(symbol) symbol
# define VLC_MODULE_NAME_HIDDEN_SYMBOL \
    EXTERN_SYMBOL const char vlc_module_name[] = MODULE_STRING;
#else
# define VLC_SYMBOL(symbol)  CONCATENATE(symbol, MODULE_NAME)
# define VLC_MODULE_NAME_HIDDEN_SYMBOL
#endif

#define CDECL_SYMBOL
#if defined (__PLUGIN__)
# if defined (_WIN32)
#   define DLL_SYMBOL              __declspec(dllexport)
#   undef CDECL_SYMBOL
#   define CDECL_SYMBOL            __cdecl
# elif defined (__GNUC__)
#   define DLL_SYMBOL              __attribute__((visibility("default")))
# else
#  define DLL_SYMBOL
# endif
#else
# define DLL_SYMBOL
#endif

struct vlc_param;

EXTERN_SYMBOL typedef int (*vlc_set_cb) (void *, void *, int, ...);

#define vlc_plugin_set(...) vlc_set (opaque,   NULL, __VA_ARGS__)
#define vlc_module_set(...) vlc_set (opaque, module, __VA_ARGS__)
#define vlc_config_set(...) vlc_set (opaque, config, __VA_ARGS__)

EXTERN_SYMBOL DLL_SYMBOL
int CDECL_SYMBOL VLC_SYMBOL(vlc_entry)(vlc_set_cb, void *);
EXTERN_SYMBOL DLL_SYMBOL
int CDECL_SYMBOL VLC_SYMBOL(vlc_entry_cfg_int_enum)(const char *name,
    int64_t **values, char ***descs);
EXTERN_SYMBOL DLL_SYMBOL
int CDECL_SYMBOL VLC_SYMBOL(vlc_entry_cfg_str_enum)(const char *name,
    char ***values, char ***descs);

/* Workaround for lack of C++ compound literals support in some compilers */
#ifdef __cplusplus
# define VLC_CHECKED_TYPE(type, value) [](type v){ return v; }(value)
#else
# define VLC_CHECKED_TYPE(type, value) (type){ value }
#endif

/*
 * InitModule: this function is called once and only once, when the module
 * is looked at for the first time. We get the useful data from it, for
 * instance the module name, its shortcuts, its capabilities... we also create
 * a copy of its config because the module can be unloaded at any time.
 */
#define vlc_module_begin() \
EXTERN_SYMBOL DLL_SYMBOL \
int CDECL_SYMBOL VLC_SYMBOL(vlc_entry)(vlc_set_cb vlc_set, void *opaque) \
{ \
    module_t *module; \
    struct vlc_param *config = NULL; \
    if (vlc_plugin_set (VLC_MODULE_CREATE, &module)) \
        goto error; \
    if (vlc_module_set (VLC_MODULE_NAME, (MODULE_STRING))) \
        goto error;

#define vlc_module_end() \
    (void) config; \
    return 0; \
error: \
    return -1; \
} \
VLC_MODULE_NAME_HIDDEN_SYMBOL \
VLC_METADATA_EXPORTS

#define add_submodule( ) \
    if (vlc_plugin_set (VLC_MODULE_CREATE, &module)) \
        goto error;

#define add_shortcut( ... ) \
{ \
    const char *shortcuts[] = { __VA_ARGS__ }; \
    if (vlc_module_set (VLC_MODULE_SHORTCUT, \
                        sizeof(shortcuts)/sizeof(shortcuts[0]), shortcuts)) \
        goto error; \
}

#define set_shortname( shortname ) \
    if (vlc_module_set (VLC_MODULE_SHORTNAME, VLC_CHECKED_TYPE(const char *, shortname))) \
        goto error;

#define set_description( desc ) \
    if (vlc_module_set (VLC_MODULE_DESCRIPTION, VLC_CHECKED_TYPE(const char *, desc))) \
        goto error;

#define set_help( help ) \
    if (vlc_module_set (VLC_MODULE_HELP, VLC_CHECKED_TYPE(const char *, help))) \
        goto error;

#define set_capability( cap, score ) \
    if (vlc_module_set (VLC_MODULE_CAPABILITY, VLC_CHECKED_TYPE(const char *, cap)) \
     || vlc_module_set (VLC_MODULE_SCORE, VLC_CHECKED_TYPE(int, score))) \
        goto error;

#define set_callback(activate) \
    if (vlc_module_set(VLC_MODULE_CB_OPEN, #activate, (void *)(activate))) \
        goto error;

#define set_callbacks( activate, deactivate ) \
    set_callback(activate) \
    if (vlc_module_set(VLC_MODULE_CB_CLOSE, #deactivate, \
                       (void (*)(vlc_object_t *))( deactivate ))) \
        goto error;

#define cannot_unload_broken_library( ) \
    if (vlc_module_set (VLC_MODULE_NO_UNLOAD)) \
        goto error;

#define set_text_domain( dom ) \
    if (vlc_plugin_set (VLC_MODULE_TEXTDOMAIN, VLC_CHECKED_TYPE(const char *, dom))) \
        goto error;

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
    vlc_plugin_set (VLC_CONFIG_CREATE, (type), &config);

#define add_typedesc_inner( type, text, longtext ) \
    add_type_inner( type ) \
    vlc_config_set (VLC_CONFIG_DESC, VLC_CHECKED_TYPE(const char *, text), \
                                     VLC_CHECKED_TYPE(const char *, longtext));

#define add_typename_inner(type, name, text, longtext) \
    add_typedesc_inner(type, text, longtext) \
    vlc_config_set (VLC_CONFIG_NAME, VLC_CHECKED_TYPE(const char *, name));

#define add_string_inner(type, name, text, longtext, v) \
    add_typename_inner(type, name, text, longtext) \
    vlc_config_set (VLC_CONFIG_VALUE, VLC_CHECKED_TYPE(const char *, v));

#define add_int_inner(type, name, text, longtext, v) \
    add_typename_inner(type, name, text, longtext) \
    vlc_config_set (VLC_CONFIG_VALUE, VLC_CHECKED_TYPE(int64_t, v));


#define set_subcategory( id ) \
    add_type_inner( CONFIG_SUBCATEGORY ) \
    vlc_config_set (VLC_CONFIG_VALUE, VLC_CHECKED_TYPE(int64_t, id));

#define set_section( text, longtext ) \
    add_typedesc_inner( CONFIG_SECTION, text, longtext )

#ifndef __PLUGIN__
#define add_category_hint(text, longtext) \
    add_typedesc_inner( CONFIG_HINT_CATEGORY, text, longtext )
#endif

#define add_string( name, value, text, longtext ) \
    add_string_inner(CONFIG_ITEM_STRING, name, text, longtext, value)

#define add_password(name, value, text, longtext) \
    add_string_inner(CONFIG_ITEM_PASSWORD, name, text, longtext, value)

#define add_loadfile(name, value, text, longtext) \
    add_string_inner(CONFIG_ITEM_LOADFILE, name, text, longtext, value)

#define add_savefile(name, value, text, longtext) \
    add_string_inner(CONFIG_ITEM_SAVEFILE, name, text, longtext, value)

#define add_directory(name, value, text, longtext) \
    add_string_inner(CONFIG_ITEM_DIRECTORY, name, text, longtext, value)

#define add_font(name, value, text, longtext) \
    add_string_inner(CONFIG_ITEM_FONT, name, text, longtext, value)

#define add_module(name, cap, value, text, longtext) \
    add_string_inner(CONFIG_ITEM_MODULE, name, text, longtext, value) \
    vlc_config_set (VLC_CONFIG_CAPABILITY, VLC_CHECKED_TYPE(const char *, cap));

#define add_module_list(name, cap, value, text, longtext) \
    add_string_inner(CONFIG_ITEM_MODULE_LIST, name, text, longtext, value) \
    vlc_config_set (VLC_CONFIG_CAPABILITY, VLC_CHECKED_TYPE(const char *, cap));

#ifndef __PLUGIN__
#define add_module_cat(name, subcategory, value, text, longtext) \
    add_string_inner(CONFIG_ITEM_MODULE_CAT, name, text, longtext, value) \
    change_integer_range (subcategory /* gruik */, 0);

#define add_module_list_cat(name, subcategory, value, text, longtext) \
    add_string_inner(CONFIG_ITEM_MODULE_LIST_CAT, name, text, longtext, \
                     value) \
    change_integer_range (subcategory /* gruik */, 0);
#endif

#define add_integer( name, value, text, longtext ) \
    add_int_inner(CONFIG_ITEM_INTEGER, name, text, longtext, value)

#define add_rgb(name, value, text, longtext) \
    add_int_inner(CONFIG_ITEM_RGB, name, text, longtext, value) \
    change_integer_range( 0, 0xFFFFFF )

#define add_key(name, value, text, longtext) \
    add_string_inner(CONFIG_ITEM_KEY, "global-" name, text, longtext, \
                     KEY_UNSET) \
    add_string_inner(CONFIG_ITEM_KEY, name, text, longtext, value)

#define add_integer_with_range( name, value, min, max, text, longtext ) \
    add_integer( name, value, text, longtext ) \
    change_integer_range( min, max )

#define add_float( name, v, text, longtext ) \
    add_typename_inner(CONFIG_ITEM_FLOAT, name, text, longtext) \
    vlc_config_set (VLC_CONFIG_VALUE, VLC_CHECKED_TYPE(double, v));

#define add_float_with_range( name, value, min, max, text, longtext ) \
    add_float( name, value, text, longtext ) \
    change_float_range( min, max )

#define add_bool( name, v, text, longtext ) \
    add_typename_inner(CONFIG_ITEM_BOOL, name, text, longtext) \
    if (v) vlc_config_set (VLC_CONFIG_VALUE, (int64_t)true);

/* For removed option */
#define add_obsolete_inner( name, type ) \
    add_type_inner( type ) \
    vlc_config_set (VLC_CONFIG_NAME, VLC_CHECKED_TYPE(const char *, name)); \
    vlc_config_set (VLC_CONFIG_REMOVED);

#define add_obsolete_bool( name ) \
        add_obsolete_inner( name, CONFIG_ITEM_BOOL )

#define add_obsolete_integer( name ) \
        add_obsolete_inner( name, CONFIG_ITEM_INTEGER )

#define add_obsolete_float( name ) \
        add_obsolete_inner( name, CONFIG_ITEM_FLOAT )

#define add_obsolete_string( name ) \
        add_obsolete_inner( name, CONFIG_ITEM_STRING )

/* Modifier macros for the config options (used for fine tuning) */

#define change_short( ch ) \
    vlc_config_set (VLC_CONFIG_SHORTCUT, VLC_CHECKED_TYPE(char, ch));

#define change_string_list( list, list_text ) \
    vlc_config_set (VLC_CONFIG_LIST, \
                    ARRAY_SIZE(list), \
                    VLC_CHECKED_TYPE(const char *const *, list), \
                    VLC_CHECKED_TYPE(const char *const *, list_text));

#define change_integer_list( list, list_text ) \
    vlc_config_set (VLC_CONFIG_LIST, \
                    ARRAY_SIZE(list), \
                    VLC_CHECKED_TYPE(const int *, list), \
                    VLC_CHECKED_TYPE(const char *const *, list_text));

#define change_integer_range( minv, maxv ) \
    vlc_config_set (VLC_CONFIG_RANGE, VLC_CHECKED_TYPE(int64_t, minv), \
                                      VLC_CHECKED_TYPE(int64_t, maxv));

#define change_float_range( minv, maxv ) \
    vlc_config_set (VLC_CONFIG_RANGE, VLC_CHECKED_TYPE(double, minv), \
                                      VLC_CHECKED_TYPE(double, maxv));

/* For options that are saved but hidden from the preferences panel */
#define change_private() \
    vlc_config_set (VLC_CONFIG_PRIVATE);

/* For options that cannot be saved in the configuration */
#define change_volatile() \
    change_private() \
    vlc_config_set (VLC_CONFIG_VOLATILE);

#define change_safe() \
    vlc_config_set (VLC_CONFIG_SAFE);

/* Configuration item choice enumerators */
#define VLC_CONFIG_INTEGER_ENUM(cb) \
EXTERN_SYMBOL DLL_SYMBOL \
int CDECL_SYMBOL VLC_SYMBOL(vlc_entry_cfg_int_enum)(const char *name, \
    int64_t **values, char ***descs) \
{ \
    return (cb)(name, values, descs); \
}

#define VLC_CONFIG_STRING_ENUM(cb) \
EXTERN_SYMBOL DLL_SYMBOL \
int CDECL_SYMBOL VLC_SYMBOL(vlc_entry_cfg_str_enum)(const char *name, \
    char ***values, char ***descs) \
{ \
    return (cb)(name, values, descs); \
}

/* Meta data plugin exports */
#define VLC_META_EXPORT( name, value ) \
    EXTERN_SYMBOL DLL_SYMBOL const char * CDECL_SYMBOL \
    VLC_SYMBOL(vlc_entry_ ## name)(void); \
    EXTERN_SYMBOL DLL_SYMBOL const char * CDECL_SYMBOL \
    VLC_SYMBOL(vlc_entry_ ## name)(void) \
    { \
         return value; \
    }

#define VLC_API_VERSION_EXPORT \
    VLC_META_EXPORT(api_version, VLC_API_VERSION_STRING)

#define VLC_COPYRIGHT_VIDEOLAN \
    "\x43\x6f\x70\x79\x72\x69\x67\x68\x74\x20\x28\x43\x29\x20\x74\x68" \
    "\x65\x20\x56\x69\x64\x65\x6f\x4c\x41\x4e\x20\x56\x4c\x43\x20\x6d" \
    "\x65\x64\x69\x61\x20\x70\x6c\x61\x79\x65\x72\x20\x64\x65\x76\x65" \
    "\x6c\x6f\x70\x65\x72\x73"
#define VLC_LICENSE_LGPL_2_1_PLUS \
    "\x4c\x69\x63\x65\x6e\x73\x65\x64\x20\x75\x6e\x64\x65\x72\x20\x74" \
    "\x68\x65\x20\x74\x65\x72\x6d\x73\x20\x6f\x66\x20\x74\x68\x65\x20" \
    "\x47\x4e\x55\x20\x4c\x65\x73\x73\x65\x72\x20\x47\x65\x6e\x65\x72" \
    "\x61\x6c\x20\x50\x75\x62\x6c\x69\x63\x20\x4c\x69\x63\x65\x6e\x73" \
    "\x65\x2c\x20\x76\x65\x72\x73\x69\x6f\x6e\x20\x32\x2e\x31\x20\x6f" \
    "\x72\x20\x6c\x61\x74\x65\x72\x2e"
#define VLC_LICENSE_GPL_2_PLUS \
    "\x4c\x69\x63\x65\x6e\x73\x65\x64\x20\x75\x6e\x64\x65\x72\x20\x74" \
    "\x68\x65\x20\x74\x65\x72\x6d\x73\x20\x6f\x66\x20\x74\x68\x65\x20" \
    "\x47\x4e\x55\x20\x47\x65\x6e\x65\x72\x61\x6c\x20\x50\x75\x62\x6c" \
    "\x69\x63\x20\x4c\x69\x63\x65\x6e\x73\x65\x2c\x20\x76\x65\x72\x73" \
    "\x69\x6f\x6e\x20\x32\x20\x6f\x72\x20\x6c\x61\x74\x65\x72\x2e"
#if defined (LIBVLC_INTERNAL_)
# define VLC_MODULE_COPYRIGHT VLC_COPYRIGHT_VIDEOLAN
# ifndef VLC_MODULE_LICENSE
#  define VLC_MODULE_LICENSE VLC_LICENSE_LGPL_2_1_PLUS
# endif
#endif

#ifdef VLC_MODULE_COPYRIGHT
# define VLC_COPYRIGHT_EXPORT VLC_META_EXPORT(copyright, VLC_MODULE_COPYRIGHT)
#else
# define VLC_COPYRIGHT_EXPORT
#endif
#ifdef VLC_MODULE_LICENSE
# define VLC_LICENSE_EXPORT VLC_META_EXPORT(license, VLC_MODULE_LICENSE)
#else
# define VLC_LICENSE_EXPORT
#endif

#define VLC_METADATA_EXPORTS \
    VLC_API_VERSION_EXPORT \
    VLC_COPYRIGHT_EXPORT \
    VLC_LICENSE_EXPORT

#endif
