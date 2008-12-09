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

#ifndef VLC_CONFIGURATION_H
#define VLC_CONFIGURATION_H 1

/**
 * \file
 * This file describes the programming interface for the configuration module.
 * It includes functions allowing to declare, get or set configuration options.
 */

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
   #define SUBCAT_INPUT_DEMUX 403
   #define SUBCAT_INPUT_VCODEC 404
   #define SUBCAT_INPUT_ACODEC 405
   #define SUBCAT_INPUT_SCODEC 406
   #define SUBCAT_INPUT_STREAM_FILTER 407

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
    vlc_callback_t pf_update_list; /*callback to initialize dropdownlists */

    /* Actions list */
    vlc_callback_t *ppf_action;    /* List of possible actions for a config */
    char          **ppsz_action_text;         /* Friendly names for actions */
    int            i_action;                           /* actions list size */

    /* Misc */
    vlc_mutex_t *p_lock;           /* Lock to use when modifying the config */
    bool        b_dirty;          /* Dirty flag to indicate a config change */
    bool        b_advanced;          /* Flag to indicate an advanced option */
    bool        b_internal;   /* Flag to indicate option is not to be shown */
    bool        b_restart;   /* Flag to indicate the option needs a restart */
                              /* to take effect */

    /* Deprecated */
    char        *psz_oldname;                          /* Old option name */
    bool        b_removed;

    /* Option values loaded from config file */
    bool        b_autosave;      /* Config will be auto-saved at exit time */
    bool        b_unsaveable;                /* Config should not be saved */

    bool        b_safe;
};

/*****************************************************************************
 * Prototypes - these methods are used to get, set or manipulate configuration
 * data.
 *****************************************************************************/
VLC_EXPORT( int,    __config_GetType,  (vlc_object_t *, const char *) LIBVLC_USED );
VLC_EXPORT( int,    __config_GetInt,   (vlc_object_t *, const char *) LIBVLC_USED );
VLC_EXPORT( void,   __config_PutInt,   (vlc_object_t *, const char *, int) );
VLC_EXPORT( float,  __config_GetFloat, (vlc_object_t *, const char *) LIBVLC_USED );
VLC_EXPORT( void,   __config_PutFloat, (vlc_object_t *, const char *, float) );
VLC_EXPORT( char *, __config_GetPsz,   (vlc_object_t *, const char *) LIBVLC_USED );
VLC_EXPORT( void,   __config_PutPsz,   (vlc_object_t *, const char *, const char *) );

#define config_SaveConfigFile(a,b) __config_SaveConfigFile(VLC_OBJECT(a),b)
VLC_EXPORT( int,    __config_SaveConfigFile, ( vlc_object_t *, const char * ) );
#define config_ResetAll(a) __config_ResetAll(VLC_OBJECT(a))
VLC_EXPORT( void,   __config_ResetAll, ( vlc_object_t * ) );

VLC_EXPORT( module_config_t *, config_FindConfig,( vlc_object_t *, const char * ) LIBVLC_USED );

VLC_EXPORT(const char *, config_GetDataDir, ( void ) LIBVLC_USED);
VLC_EXPORT(const char *, config_GetConfDir, ( void ) LIBVLC_USED);
VLC_EXPORT(const char *, config_GetHomeDir, ( void ) LIBVLC_USED);
VLC_EXPORT(char *, config_GetUserConfDir, ( void ) LIBVLC_USED);
VLC_EXPORT(char *, config_GetUserDataDir, ( void ) LIBVLC_USED);
VLC_EXPORT(char *, config_GetCacheDir, ( void ) LIBVLC_USED);

VLC_EXPORT( void,       __config_AddIntf,    ( vlc_object_t *, const char * ) );
VLC_EXPORT( void,       __config_RemoveIntf, ( vlc_object_t *, const char * ) );
VLC_EXPORT( bool, __config_ExistIntf,  ( vlc_object_t *, const char * ) LIBVLC_USED);

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

/****************************************************************************
 * config_chain_t:
 ****************************************************************************/
struct config_chain_t
{
    config_chain_t *p_next;     /**< Pointer on the next config_chain_t element */

    char        *psz_name;      /**< Option name */
    char        *psz_value;     /**< Option value */
};

/**
 * This function will
 * - create all options in the array ppsz_options (var_Create).
 * - parse the given linked list of config_chain_t and set the value (var_Set).
 *
 * The option names will be created by adding the psz_prefix prefix.
 */
#define config_ChainParse( a, b, c, d ) __config_ChainParse( VLC_OBJECT(a), b, c, d )
VLC_EXPORT( void,   __config_ChainParse, ( vlc_object_t *, const char *psz_prefix, const char *const *ppsz_options, config_chain_t * ) );

/**
 * This function will parse a configuration string (psz_string) and
 * - set the module name (*ppsz_name)
 * - set all options for this module in a chained list (*pp_cfg)
 * - returns a pointer on the next module if any.
 *
 * The string format is
 *   module{option=*,option=*}[:modulenext{option=*,...}]
 *
 * The options values are unescaped using config_StringUnescape.
 */
VLC_EXPORT( char *, config_ChainCreate, ( char **ppsz_name, config_chain_t **pp_cfg, const char *psz_string ) );

/**
 * This function will release a linked list of config_chain_t
 * (Including the head)
 */
VLC_EXPORT( void, config_ChainDestroy, ( config_chain_t * ) );

/**
 * This function will unescape a string in place and will return a pointer on
 * the given string.
 * No memory is allocated by it (unlike config_StringEscape).
 * If NULL is given as parameter nothing will be done (NULL will be returned).
 *
 * The following sequences will be unescaped (only one time):
 * \\ \' and \"
 */
VLC_EXPORT( char *, config_StringUnescape, ( char *psz_string ) );

/**
 * This function will escape a string that can be unescaped by
 * config_StringUnescape.
 * The returned value is allocated by it. You have to free it once you
 * do not need it anymore (unlike config_StringUnescape).
 * If NULL is given as parameter nothing will be done (NULL will be returned).
 *
 * The escaped characters are ' " and \
 */
VLC_EXPORT( char *, config_StringEscape, ( const char *psz_string ) LIBVLC_USED);

# ifdef __cplusplus
}
# endif

#endif /* _VLC_CONFIGURATION_H */
