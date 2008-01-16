/*****************************************************************************
 * configuration.h : configuration management module
 * This file describes the programming interface for the configuration module.
 * It includes functions allowing to declare, get or set configuration options.
 *****************************************************************************
 * Copyright (C) 1999-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#if !defined( __LIBVLC__ )
  #error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef _VLC_CONFIGURATION_H
#define _VLC_CONFIGURATION_H 1


# ifdef __cplusplus
extern "C" {
# endif

/*****************************************************************************
 * Macros used to build the configuration structure.
 *****************************************************************************/

/* Configuration hint types */


#define CONFIG_HINT_CATEGORY                0x0002  /* Start of new category */
#define CONFIG_HINT_SUBCATEGORY             0x0003  /* Start of sub-category */
#define CONFIG_HINT_SUBCATEGORY_END         0x0004  /* End of sub-category */
#define CONFIG_HINT_USAGE                   0x0005  /* Usage information */

#define CONFIG_CATEGORY                     0x0006 /* Set category */
#define CONFIG_SUBCATEGORY                  0x0007 /* Set subcategory */
#define CONFIG_SECTION                      0x0008 /* Start of new section */

#define CONFIG_HINT                         0x000F

/* Configuration item types */
#define CONFIG_ITEM_STRING                  0x0010  /* String option */
#define CONFIG_ITEM_FILE                    0x0020  /* File option */
#define CONFIG_ITEM_MODULE                  0x0030  /* Module option */
#define CONFIG_ITEM_INTEGER                 0x0040  /* Integer option */
#define CONFIG_ITEM_BOOL                    0x0050  /* Bool option */
#define CONFIG_ITEM_FLOAT                   0x0060  /* Float option */
#define CONFIG_ITEM_DIRECTORY               0x0070  /* Directory option */
#define CONFIG_ITEM_KEY                     0x0080  /* Hot key option */
#define CONFIG_ITEM_MODULE_CAT              0x0090  /* Module option */
#define CONFIG_ITEM_MODULE_LIST             0x00A0  /* Module option */
#define CONFIG_ITEM_MODULE_LIST_CAT         0x00B0  /* Module option */
#define CONFIG_ITEM_FONT                    0x00C0  /* Font option */
#define CONFIG_ITEM_PASSWORD                0x00D0  /* Password option (*) */

#define CONFIG_ITEM                         0x00F0

/*******************************************************************
 * All predefined categories and subcategories
 *******************************************************************/
#define CAT_INTERFACE 1
   #define SUBCAT_INTERFACE_GENERAL 101
   #define SUBCAT_INTERFACE_MAIN 102
   #define SUBCAT_INTERFACE_CONTROL 103
   #define SUBCAT_INTERFACE_HOTKEYS 104

#define CAT_AUDIO 2
   #define SUBCAT_AUDIO_GENERAL 201
   #define SUBCAT_AUDIO_AOUT 202
   #define SUBCAT_AUDIO_AFILTER 203
   #define SUBCAT_AUDIO_VISUAL 204
   #define SUBCAT_AUDIO_MISC 205

#define CAT_VIDEO 3
   #define SUBCAT_VIDEO_GENERAL 301
   #define SUBCAT_VIDEO_VOUT 302
   #define SUBCAT_VIDEO_VFILTER 303
   #define SUBCAT_VIDEO_TEXT 304
   #define SUBCAT_VIDEO_SUBPIC 305
   #define SUBCAT_VIDEO_VFILTER2 306

#define CAT_INPUT 4
   #define SUBCAT_INPUT_GENERAL 401
   #define SUBCAT_INPUT_ACCESS 402
   #define SUBCAT_INPUT_ACCESS_FILTER 403
   #define SUBCAT_INPUT_DEMUX 404
   #define SUBCAT_INPUT_VCODEC 405
   #define SUBCAT_INPUT_ACODEC 406
   #define SUBCAT_INPUT_SCODEC 407

#define CAT_SOUT 5
   #define SUBCAT_SOUT_GENERAL 501
   #define SUBCAT_SOUT_STREAM 502
   #define SUBCAT_SOUT_MUX 503
   #define SUBCAT_SOUT_ACO 504
   #define SUBCAT_SOUT_PACKETIZER 505
   #define SUBCAT_SOUT_SAP 506
   #define SUBCAT_SOUT_VOD 507

#define CAT_ADVANCED 6
   #define SUBCAT_ADVANCED_CPU 601
   #define SUBCAT_ADVANCED_MISC 602
   #define SUBCAT_ADVANCED_NETWORK 603
   #define SUBCAT_ADVANCED_XML 604

#define CAT_PLAYLIST 7
   #define SUBCAT_PLAYLIST_GENERAL 701
   #define SUBCAT_PLAYLIST_SD 702
   #define SUBCAT_PLAYLIST_EXPORT 703

#define CAT_OSD 8
   #define SUBCAT_OSD_IMPORT 801

struct config_category_t
{
    int         i_id;
    const char *psz_name;
    const char *psz_help;
};

typedef union
{
    char       *psz;
    int         i;
    float       f;
} module_value_t;

typedef union
{
    int         i;
    float       f;
} module_nvalue_t;

struct module_config_t
{
    int          i_type;                               /* Configuration type */
    char        *psz_type;                          /* Configuration subtype */
    char        *psz_name;                                    /* Option name */
    char         i_short;                      /* Optional short option name */
    char        *psz_text;      /* Short comment on the configuration option */
    char        *psz_longtext;   /* Long comment on the configuration option */
    module_value_t value;                                    /* Option value */
    module_value_t orig;
    module_value_t saved;
    module_nvalue_t min;
    module_nvalue_t max;

    /* Function to call when commiting a change */
    vlc_callback_t pf_callback;
    void          *p_callback_data;

    /* Values list */
    char **      ppsz_list;       /* List of possible values for the option */
    int         *pi_list;                              /* Idem for integers */
    char       **ppsz_list_text;          /* Friendly names for list values */
    int          i_list;                               /* Options list size */

    /* Actions list */
    vlc_callback_t *ppf_action;    /* List of possible actions for a config */
    char          **ppsz_action_text;         /* Friendly names for actions */
    int            i_action;                           /* actions list size */

    /* Misc */
    vlc_mutex_t *p_lock;            /* Lock to use when modifying the config */
    vlc_bool_t   b_dirty;          /* Dirty flag to indicate a config change */
    vlc_bool_t   b_advanced;          /* Flag to indicate an advanced option */
    vlc_bool_t   b_internal;   /* Flag to indicate option is not to be shown */
    vlc_bool_t   b_restart;   /* Flag to indicate the option needs a restart */
                              /* to take effect */

    /* Deprecated */
    char          *psz_oldname;                          /* Old option name */
    vlc_bool_t     b_removed;

    /* Option values loaded from config file */
    vlc_bool_t   b_autosave;      /* Config will be auto-saved at exit time */
    vlc_bool_t   b_unsaveable;                    /* Config should be saved */

    vlc_bool_t   b_unsafe;
};

/*****************************************************************************
 * Prototypes - these methods are used to get, set or manipulate configuration
 * data.
 *****************************************************************************/
VLC_EXPORT( int,    __config_GetType,  (vlc_object_t *, const char *) );
VLC_EXPORT( int,    __config_GetInt,   (vlc_object_t *, const char *) );
VLC_EXPORT( void,   __config_PutInt,   (vlc_object_t *, const char *, int) );
VLC_EXPORT( float,  __config_GetFloat, (vlc_object_t *, const char *) );
VLC_EXPORT( void,   __config_PutFloat, (vlc_object_t *, const char *, float) );
VLC_EXPORT( char *, __config_GetPsz,   (vlc_object_t *, const char *) );
VLC_EXPORT( void,   __config_PutPsz,   (vlc_object_t *, const char *, const char *) );

#define config_SaveConfigFile(a,b) __config_SaveConfigFile(VLC_OBJECT(a),b)
VLC_EXPORT( int,    __config_SaveConfigFile, ( vlc_object_t *, const char * ) );
#define config_ResetAll(a) __config_ResetAll(VLC_OBJECT(a))
VLC_EXPORT( void,   __config_ResetAll, ( vlc_object_t * ) );

VLC_EXPORT( module_config_t *, config_FindConfig,( vlc_object_t *, const char * ) );

VLC_EXPORT(const char *, config_GetDataDir, ( void ));

VLC_EXPORT( void,       __config_AddIntf,    ( vlc_object_t *, const char * ) );
VLC_EXPORT( void,       __config_RemoveIntf, ( vlc_object_t *, const char * ) );
VLC_EXPORT( vlc_bool_t, __config_ExistIntf,  ( vlc_object_t *, const char * ) );

#define config_GetType(a,b) __config_GetType(VLC_OBJECT(a),b)
#define config_GetInt(a,b) __config_GetInt(VLC_OBJECT(a),b)
#define config_PutInt(a,b,c) __config_PutInt(VLC_OBJECT(a),b,c)
#define config_GetFloat(a,b) __config_GetFloat(VLC_OBJECT(a),b)
#define config_PutFloat(a,b,c) __config_PutFloat(VLC_OBJECT(a),b,c)
#define config_GetPsz(a,b) __config_GetPsz(VLC_OBJECT(a),b)
#define config_PutPsz(a,b,c) __config_PutPsz(VLC_OBJECT(a),b,c)

#define config_AddIntf(a,b) __config_AddIntf(VLC_OBJECT(a),b)
#define config_RemoveIntf(a,b) __config_RemoveIntf(VLC_OBJECT(a),b)
#define config_ExistIntf(a,b) __config_ExistIntf(VLC_OBJECT(a),b)

enum vlc_config_properties
{
    /* DO NOT EVER REMOVE, INSERT OR REPLACE ANY ITEM! It would break the ABI!
     * Append new items at the end ONLY. */

    VLC_CONFIG_NAME,
    /* command line name (args=const char *, vlc_callback_t) */

    VLC_CONFIG_DESC,
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

    VLC_CONFIG_LIST,
    /* possible values list
     * (args=size_t, const <type> *, const char *const *) */

    VLC_CONFIG_ADD_ACTION,
    /* add value change callback (args=vlc_callback_t, const char *) */

    VLC_CONFIG_OLDNAME,
    /* former option name (args=const char *) */

    VLC_CONFIG_UNSAFE,
    /* tag as modifiable by untrusted input item "sources" (args=none) */
};


VLC_EXPORT( module_config_t *, vlc_config_create, (module_t *, int type) );
VLC_EXPORT( int, vlc_config_set, (module_config_t *, int, ...) );

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
    p_config = vlc_config_create (p_module, type)

#define add_typedesc_inner( type, text, longtext ) \
    add_type_inner( type ); \
    vlc_config_set (p_config, VLC_CONFIG_DESC, \
                    (const char *)(text), (const char *)(longtext))

#define add_typeadv_inner( type, text, longtext, advc ) \
    add_typedesc_inner( type, text, longtext ); \
    if (advc) vlc_config_set (p_config, VLC_CONFIG_ADVANCED)

#define add_typename_inner( type, name, text, longtext, advc, cb ) \
    add_typeadv_inner( type, text, longtext, advc ); \
    vlc_config_set (p_config, VLC_CONFIG_NAME, \
                    (const char *)(name), (vlc_callback_t)(cb))

#define add_string_inner( type, name, text, longtext, advc, cb, v ) \
    add_typename_inner( type, name, text, longtext, advc, cb ); \
    vlc_config_set (p_config, VLC_CONFIG_VALUE, (const char *)(v))

#define add_int_inner( type, name, text, longtext, advc, cb, v ) \
    add_typename_inner( type, name, text, longtext, advc, cb ); \
    vlc_config_set (p_config, VLC_CONFIG_VALUE, (int)(v))


#define set_category( i_id ) \
    add_type_inner( CONFIG_CATEGORY ); \
    vlc_config_set (p_config, VLC_CONFIG_VALUE, (int)(i_id))

#define set_subcategory( i_id ) \
    add_type_inner( CONFIG_SUBCATEGORY ); \
    vlc_config_set (p_config, VLC_CONFIG_VALUE, (int)(i_id))

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
    add_string_inner( CONFIG_ITEM_STRING, name, text, longtext, advc, p_callback, value )

#define add_password( name, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_PASSWORD, name, text, longtext, advc, p_callback, value )

#define add_file( name, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_FILE, name, text, longtext, advc, p_callback, value )

#define add_directory( name, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_DIRECTORY, name, text, longtext, advc, p_callback, value )

#define add_module( name, psz_caps, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE, name, text, longtext, advc, p_callback, value ); \
    vlc_config_set (p_config, VLC_CONFIG_CAPABILITY, (const char *)(psz_caps))

#define add_module_list( name, psz_caps, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE_LIST, name, text, longtext, advc, p_callback, value ); \
    vlc_config_set (p_config, VLC_CONFIG_CAPABILITY, (const char *)(psz_caps))

#ifndef __PLUGIN__
#define add_module_cat( name, i_subcategory, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE_CAT, name, text, longtext, advc, p_callback, value ); \
    p_config->min.i = i_subcategory /* gruik */

#define add_module_list_cat( name, i_subcategory, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE_LIST_CAT, name, text, longtext, advc, p_callback, value ); \
    p_config->min.i = i_subcategory /* gruik */
#endif

#define add_integer( name, value, p_callback, text, longtext, advc ) \
    add_int_inner( CONFIG_ITEM_INTEGER, name, text, longtext, advc, p_callback, value )

#define add_key( name, value, p_callback, text, longtext, advc ) \
    add_int_inner( CONFIG_ITEM_KEY, name, text, longtext, advc, p_callback, value )

#define add_integer_with_range( name, value, i_min, i_max, p_callback, text, longtext, advc ) \
    add_integer( name, value, p_callback, text, longtext, advc ); \
    change_integer_range( i_min, i_max )

#define add_float( name, v, p_callback, text, longtext, advc ) \
    add_typename_inner( CONFIG_ITEM_FLOAT, name, text, longtext, advc, p_callback ); \
    vlc_config_set (p_config, VLC_CONFIG_VALUE, (double)(v))

#define add_float_with_range( name, value, f_min, f_max, p_callback, text, longtext, advc ) \
    add_float( name, value, p_callback, text, longtext, advc ); \
    change_float_range( f_min, f_max )

#define add_bool( name, v, p_callback, text, longtext, advc ) \
    add_typename_inner( CONFIG_ITEM_BOOL, name, text, longtext, advc, p_callback ); \
    if (v) vlc_config_set (p_config, VLC_CONFIG_VALUE, (int)VLC_TRUE)

/* For removed option */
#define add_obsolete_inner( name, type ) \
    add_type_inner( type ); \
    vlc_config_set (p_config, VLC_CONFIG_NAME, \
                    (const char *)(name), (vlc_callback_t)NULL); \
    vlc_config_set (p_config, VLC_CONFIG_REMOVED)

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
    vlc_config_set (p_config, VLC_CONFIG_SHORTCUT, (int)(ch))

#define change_string_list( list, list_text, list_update_func ) \
    vlc_config_set (p_config, VLC_CONFIG_LIST, \
                    (size_t)(sizeof (list) / sizeof (char *)), \
                    (const char *const *)(list), \
                    (const char *const *)(list_text))

#define change_integer_list( list, list_text, list_update_func ) \
    vlc_config_set (p_config, VLC_CONFIG_LIST, \
                    (size_t)(sizeof (list) / sizeof (int)), \
                    (const int *)(list), \
                    (const char *const *)(list_text))

#define change_float_list( list, list_text, list_update_func ) \
    vlc_config_set (p_config, VLC_CONFIG_LIST, \
                    (size_t)(sizeof (list) / sizeof (float)), \
                    (const float *)(list), \
                    (const char *const *)(list_text))

#define change_integer_range( minv, maxv ) \
    vlc_config_set (p_config, VLC_CONFIG_RANGE, (int)(minv), (int)(maxv))

#define change_float_range( minv, maxv ) \
    vlc_config_set (p_config, VLC_CONFIG_RANGE, \
                    (double)(minv), (double)(maxv))

#define change_action_add( pf_action, text ) \
    vlc_config_set (p_config, VLC_CONFIG_ADD_ACTION, \
                    (vlc_callback_t)(pf_action), (const char *)(text))

#define change_internal() \
    vlc_config_set (p_config, VLC_CONFIG_PRIVATE)

#define change_need_restart() \
    vlc_config_set (p_config, VLC_CONFIG_RESTART)

#define change_autosave() \
    vlc_config_set (p_config, VLC_CONFIG_PERSISTENT)

#define change_unsaveable() \
    vlc_config_set (p_config, VLC_CONFIG_VOLATILE)

#define change_unsafe() \
    vlc_config_set (p_config, VLC_CONFIG_UNSAFE)

/****************************************************************************
 * config_chain_t:
 ****************************************************************************/
struct config_chain_t
{
    config_chain_t *p_next;

    char        *psz_name;
    char        *psz_value;
};

#define config_ChainParse( a, b, c, d ) __config_ChainParse( VLC_OBJECT(a), b, c, d )
VLC_EXPORT( void,   __config_ChainParse, ( vlc_object_t *, const char *psz_prefix, const char *const *ppsz_options, config_chain_t * ) );
VLC_EXPORT( char *, config_ChainCreate, ( char **, config_chain_t **, const char * ) );
VLC_EXPORT( void, config_ChainDestroy, ( config_chain_t * ) );

# ifdef __cplusplus
}
# endif

#endif /* _VLC_CONFIGURATION_H */
