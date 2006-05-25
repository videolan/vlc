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

/*****************************************************************************
 * Macros used to build the configuration structure.
 *****************************************************************************/

/* Configuration hint types */


#define CONFIG_HINT_END                     0x0001  /* End of config */
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

struct config_category_t
{
    int         i_id;
    char       *psz_name;
    char       *psz_help;
};

struct module_config_t
{
    int          i_type;                               /* Configuration type */
    char        *psz_type;                          /* Configuration subtype */
    char        *psz_name;                                    /* Option name */
    char         i_short;                      /* Optional short option name */
    char        *psz_text;      /* Short comment on the configuration option */
    char        *psz_longtext;   /* Long comment on the configuration option */
    char        *psz_value;                                  /* Option value */
    int          i_value;                                    /* Option value */
    float        f_value;                                    /* Option value */
    int         i_min;                               /* Option minimum value */
    int         i_max;                               /* Option maximum value */
    float       f_min;                               /* Option minimum value */
    float       f_max;                               /* Option maximum value */

    /* Function to call when commiting a change */
    vlc_callback_t pf_callback;
    void          *p_callback_data;

    /* Values list */
    char       **ppsz_list;        /* List of possible values for the option */
    int         *pi_list;          /* Idem for integers */
    char       **ppsz_list_text;   /* Friendly names for list values */
    int          i_list;           /* Options list size */

    /* Actions list */
    vlc_callback_t *ppf_action;    /* List of possible actions for a config */
    char           **ppsz_action_text;         /* Friendly names for actions */
    int            i_action;                            /* actions list size */

    /* Deprecated */
    char           *psz_current;   /* Good option name */
    vlc_bool_t     b_strict;      /* Transitionnal or strict */
    /* Misc */
    vlc_mutex_t *p_lock;            /* Lock to use when modifying the config */
    vlc_bool_t   b_dirty;          /* Dirty flag to indicate a config change */
    vlc_bool_t   b_advanced;          /* Flag to indicate an advanced option */
    vlc_bool_t   b_internal;   /* Flag to indicate option is not to be shows */

    /* Original option values */
    char        *psz_value_orig;
    int          i_value_orig;
    float        f_value_orig;

    /* Option values loaded from config file */
    char        *psz_value_saved;
    int          i_value_saved;
    float        f_value_saved;
    vlc_bool_t   b_autosave;       /* Config will be auto-saved at exit time */
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

VLC_EXPORT( int,    __config_LoadCmdLine,  ( vlc_object_t *, int *, char *[], vlc_bool_t ) );
VLC_EXPORT( char *,   config_GetHomeDir,     ( void ) );
VLC_EXPORT( char *,   config_GetUserDir,     ( void ) );
VLC_EXPORT( const char *, config_GetDataDir, ( const vlc_object_t * ) );
VLC_EXPORT( int,    __config_LoadConfigFile, ( vlc_object_t *, const char * ) );
VLC_EXPORT( int,    __config_SaveConfigFile, ( vlc_object_t *, const char * ) );
VLC_EXPORT( void,   __config_ResetAll, ( vlc_object_t * ) );

VLC_EXPORT( module_config_t *, config_FindConfig,( vlc_object_t *, const char * ) );
VLC_EXPORT( module_t *, config_FindModule,( vlc_object_t *, const char * ) );

VLC_EXPORT( void, config_Duplicate, ( module_t *, module_config_t * ) );
            void  config_Free       ( module_t * );

VLC_EXPORT( void, config_SetCallbacks, ( module_config_t *, module_config_t * ) );
VLC_EXPORT( void, config_UnsetCallbacks, ( module_config_t * ) );

#define config_GetType(a,b) __config_GetType(VLC_OBJECT(a),b)
#define config_GetInt(a,b) __config_GetInt(VLC_OBJECT(a),b)
#define config_PutInt(a,b,c) __config_PutInt(VLC_OBJECT(a),b,c)
#define config_GetFloat(a,b) __config_GetFloat(VLC_OBJECT(a),b)
#define config_PutFloat(a,b,c) __config_PutFloat(VLC_OBJECT(a),b,c)
#define config_GetPsz(a,b) __config_GetPsz(VLC_OBJECT(a),b)
#define config_PutPsz(a,b,c) __config_PutPsz(VLC_OBJECT(a),b,c)

#define config_LoadCmdLine(a,b,c,d) __config_LoadCmdLine(VLC_OBJECT(a),b,c,d)
#define config_LoadConfigFile(a,b) __config_LoadConfigFile(VLC_OBJECT(a),b)
#define config_SaveConfigFile(a,b) __config_SaveConfigFile(VLC_OBJECT(a),b)
#define config_ResetAll(a) __config_ResetAll(VLC_OBJECT(a))

/* internal only */
int config_CreateDir( vlc_object_t *, const char * );
int config_AutoSaveConfigFile( vlc_object_t * );

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

#define add_config_inner( ) \
    i_config++; \
    if( (i_config % 10) == 0 ) \
        p_config = (module_config_t *) \
             realloc(p_config, (i_config+11) * sizeof(module_config_t)); \
    memset( p_config + i_config, 0, sizeof( *p_config ) )

#define add_type_inner( type ) \
    add_config_inner( ); \
    p_config[i_config].i_type = type

#define add_typedesc_inner( type, text, longtext ) \
    add_type_inner( type ); \
    p_config[i_config].psz_text = text; \
    p_config[i_config].psz_longtext = longtext

#define add_typeadv_inner( type, text, longtext, advc ) \
    add_typedesc_inner( type, text, longtext ); \
    p_config[i_config].b_advanced = advc

#define add_typename_inner( type, name, text, longtext, advc, cb ) \
    add_typeadv_inner( type, text, longtext, advc ); \
    p_config[i_config].psz_name = name; \
    p_config[i_config].pf_callback = cb

#define add_string_inner( type, name, text, longtext, advc, cb, value ) \
    add_typename_inner( type, name, text, longtext, advc, cb ); \
    p_config[i_config].psz_value = value

#define add_int_inner( type, name, text, longtext, advc, cb, value ) \
    add_typename_inner( type, name, text, longtext, advc, cb ); \
    p_config[i_config].i_value = value


#define set_category( i_id ) \
    add_type_inner( CONFIG_CATEGORY ); \
    p_config[i_config].i_value = i_id

#define set_subcategory( i_id ) \
    add_type_inner( CONFIG_SUBCATEGORY ); \
    p_config[i_config].i_value = i_id

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

#define add_file( name, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_FILE, name, text, longtext, advc, p_callback, value )

#define add_directory( name, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_DIRECTORY, name, text, longtext, advc, p_callback, value )

#define add_module( name, psz_caps, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE, name, text, longtext, advc, p_callback, value ); \
    p_config[i_config].psz_type = psz_caps

#define add_module_cat( name, i_subcategory, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE_CAT, name, text, longtext, advc, p_callback, value ); \
    p_config[i_config].i_min = i_subcategory /* gruik */

#define add_module_list( name, psz_caps, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE_LIST, name, text, longtext, advc, p_callback, value ); \
    p_config[i_config].psz_type = psz_caps

#define add_module_list_cat( name, i_subcategory, value, p_callback, text, longtext, advc ) \
    add_string_inner( CONFIG_ITEM_MODULE_LIST_CAT, name, text, longtext, advc, p_callback, value ); \
    p_config[i_config].i_min = i_subcategory /* gruik */

#define add_integer( name, value, p_callback, text, longtext, advc ) \
    add_int_inner( CONFIG_ITEM_INTEGER, name, text, longtext, advc, p_callback, value )

#define add_key( name, value, p_callback, text, longtext, advc ) \
    add_int_inner( CONFIG_ITEM_KEY, name, text, longtext, advc, p_callback, value )

#define add_integer_with_range( name, value, i_min, i_max, p_callback, text, longtext, advc ) \
    add_integer( name, value, p_callback, text, longtext, advc ); \
    change_integer_range( i_min, i_max )

#define add_float( name, value, p_callback, text, longtext, advc ) \
    add_typename_inner( CONFIG_ITEM_FLOAT, name, text, longtext, advc, p_callback ); \
    p_config[i_config].f_value = value

#define add_float_with_range( name, value, f_min, f_max, p_callback, text, longtext, advc ) \
    add_float( name, value, p_callback, text, longtext, advc ); \
    change_float_range( f_min, f_max )

#define add_bool( name, value, p_callback, text, longtext, advc ) \
    add_typename_inner( CONFIG_ITEM_BOOL, name, text, longtext, advc, p_callback ); \
    p_config[i_config].i_value = value

/* For option renamed */
#define add_deprecated( name, strict ) \
    add_config_inner( ); \
    p_config[ i_config ].i_type = p_config[ i_config -1 ].i_type; \
    p_config[ i_config ].psz_name = name; \
    p_config[i_config].b_strict = strict; \
    p_config[ i_config ].psz_current = p_config[ i_config-1].psz_current \
        ? p_config[ i_config-1 ].psz_current \
        : p_config[ i_config-1 ].psz_name;

/* For option suppressed*/
#define add_suppressed_inner( name, type ) \
    add_type_inner( type ); \
    p_config[ i_config ].psz_name = name; \
    p_config[ i_config ].psz_current = "SUPPRESSED";

#define add_suppressed_bool( name ) \
        add_suppressed_inner( name, CONFIG_ITEM_BOOL )

#define add_suppressed_integer( name ) \
        add_suppressed_inner( name, CONFIG_ITEM_INTEGER )

#define add_suppressed_float( name ) \
        add_suppressed_inner( name, CONFIG_ITEM_FLOAT )

#define add_suppressed_string( name ) \
        add_suppressed_inner( name, CONFIG_ITEM_STRING )

/* Modifier macros for the config options (used for fine tuning) */
#define change_short( ch ) \
    p_config[i_config].i_short = ch;

#define change_string_list( list, list_text, list_update_func ) \
    p_config[i_config].i_list = sizeof(list)/sizeof(char *); \
    p_config[i_config].ppsz_list = list; \
    p_config[i_config].ppsz_list_text = list_text;

#define change_integer_list( list, list_text, list_update_func ) \
    p_config[i_config].i_list = sizeof(list)/sizeof(int); \
    p_config[i_config].pi_list = list; \
    p_config[i_config].ppsz_list_text = list_text;

#define change_integer_range( min, max ) \
    p_config[i_config].i_min = min; \
    p_config[i_config].i_max = max;

#define change_float_range( min, max ) \
    p_config[i_config].f_min = min; \
    p_config[i_config].f_max = max;

#define change_action_add( pf_action, action_text ) \
    if( !p_config[i_config].i_action ) \
    { p_config[i_config].ppsz_action_text = 0; \
      p_config[i_config].ppf_action = 0; } \
    p_config[i_config].ppf_action = (vlc_callback_t *) \
      realloc( p_config[i_config].ppf_action, \
      (p_config[i_config].i_action + 1) * sizeof(void *) ); \
    p_config[i_config].ppsz_action_text = (char **)\
      realloc( p_config[i_config].ppsz_action_text, \
      (p_config[i_config].i_action + 1) * sizeof(void *) ); \
    p_config[i_config].ppf_action[p_config[i_config].i_action] = pf_action; \
    p_config[i_config].ppsz_action_text[p_config[i_config].i_action] = \
      action_text; \
    p_config[i_config].i_action++;

#define change_internal() \
    p_config[i_config].b_internal = VLC_TRUE;

#define change_autosave() \
    p_config[i_config].b_autosave = VLC_TRUE;
