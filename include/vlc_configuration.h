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

/* Configuration item types */
#define CONFIG_ITEM_STRING                  0x0010  /* String option */
/* unused 0x0020 */
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
#define CONFIG_ITEM_LOADFILE                0x00E0  /* Read file option */
#define CONFIG_ITEM_SAVEFILE                0x00F0  /* Written file option */

#define CONFIG_ITEM(x) (((x) & 0xF0) != 0)

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
    int64_t     i;
    float       f;
} module_value_t;

struct module_config_t
{
    char        *psz_type;                          /* Configuration subtype */
    char        *psz_name;                                    /* Option name */
    char        *psz_text;      /* Short comment on the configuration option */
    char        *psz_longtext;   /* Long comment on the configuration option */
    module_value_t value;                                    /* Option value */
    module_value_t orig;
    module_value_t min;
    module_value_t max;

    /* Values list */
    char **      ppsz_list;       /* List of possible values for the option */
    int         *pi_list;                              /* Idem for integers */
    char       **ppsz_list_text;          /* Friendly names for list values */
    int          i_list;                               /* Options list size */
    vlc_callback_t pf_update_list; /* Callback to initialize dropdown lists */
    uint8_t      i_type;                              /* Configuration type */
    char         i_short;                     /* Optional short option name */

    /* Misc */
    unsigned    b_dirty:1;        /* Dirty flag to indicate a config change */
    unsigned    b_advanced:1;        /* Flag to indicate an advanced option */
    unsigned    b_internal:1; /* Flag to indicate option is not to be shown */
    unsigned    b_unsaveable:1;               /* Config should not be saved */
    unsigned    b_safe:1;       /* Safe to use in web plugins and playlists */

    /* Actions list */
    int            i_action;                           /* actions list size */
    vlc_callback_t *ppf_action;    /* List of possible actions for a config */
    char          **ppsz_action_text;         /* Friendly names for actions */

    /* Deprecated */
    char        *psz_oldname;                          /* Old option name */
    bool        b_removed;
};

/*****************************************************************************
 * Prototypes - these methods are used to get, set or manipulate configuration
 * data.
 *****************************************************************************/
VLC_API int config_GetType(vlc_object_t *, const char *) VLC_USED;
VLC_API int64_t config_GetInt(vlc_object_t *, const char *) VLC_USED;
VLC_API void config_PutInt(vlc_object_t *, const char *, int64_t);
VLC_API float config_GetFloat(vlc_object_t *, const char *) VLC_USED;
VLC_API void config_PutFloat(vlc_object_t *, const char *, float);
VLC_API char * config_GetPsz(vlc_object_t *, const char *) VLC_USED VLC_MALLOC;
VLC_API void config_PutPsz(vlc_object_t *, const char *, const char *);

VLC_API int config_SaveConfigFile( vlc_object_t * );
#define config_SaveConfigFile(a) config_SaveConfigFile(VLC_OBJECT(a))

VLC_API void config_ResetAll( vlc_object_t * );
#define config_ResetAll(a) config_ResetAll(VLC_OBJECT(a))

VLC_API module_config_t * config_FindConfig( vlc_object_t *, const char * ) VLC_USED;
VLC_API char * config_GetDataDir( vlc_object_t * ) VLC_USED VLC_MALLOC;
#define config_GetDataDir(a) config_GetDataDir(VLC_OBJECT(a))
VLC_API const char * config_GetLibDir( void ) VLC_USED;
VLC_API const char * config_GetConfDir( void ) VLC_USED;

typedef enum vlc_userdir
{
    VLC_HOME_DIR, /* User's home */
    VLC_CONFIG_DIR, /* VLC-specific configuration directory */
    VLC_DATA_DIR, /* VLC-specific data directory */
    VLC_CACHE_DIR, /* VLC-specific user cached data directory */
    /* Generic directories (same as XDG) */
    VLC_DESKTOP_DIR=0x80,
    VLC_DOWNLOAD_DIR,
    VLC_TEMPLATES_DIR,
    VLC_PUBLICSHARE_DIR,
    VLC_DOCUMENTS_DIR,
    VLC_MUSIC_DIR,
    VLC_PICTURES_DIR,
    VLC_VIDEOS_DIR,
} vlc_userdir_t;

VLC_API char * config_GetUserDir( vlc_userdir_t ) VLC_USED VLC_MALLOC;

VLC_API void config_AddIntf( vlc_object_t *, const char * );
VLC_API void config_RemoveIntf( vlc_object_t *, const char * );
VLC_API bool config_ExistIntf( vlc_object_t *, const char * ) VLC_USED;

#define config_GetType(a,b) config_GetType(VLC_OBJECT(a),b)
#define config_GetInt(a,b) config_GetInt(VLC_OBJECT(a),b)
#define config_PutInt(a,b,c) config_PutInt(VLC_OBJECT(a),b,c)
#define config_GetFloat(a,b) config_GetFloat(VLC_OBJECT(a),b)
#define config_PutFloat(a,b,c) config_PutFloat(VLC_OBJECT(a),b,c)
#define config_GetPsz(a,b) config_GetPsz(VLC_OBJECT(a),b)
#define config_PutPsz(a,b,c) config_PutPsz(VLC_OBJECT(a),b,c)

#define config_AddIntf(a,b) config_AddIntf(VLC_OBJECT(a),b)
#define config_RemoveIntf(a,b) config_RemoveIntf(VLC_OBJECT(a),b)
#define config_ExistIntf(a,b) config_ExistIntf(VLC_OBJECT(a),b)

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
VLC_API void config_ChainParse( vlc_object_t *, const char *psz_prefix, const char *const *ppsz_options, config_chain_t * );
#define config_ChainParse( a, b, c, d ) config_ChainParse( VLC_OBJECT(a), b, c, d )

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
VLC_API char *config_ChainCreate( char **ppsz_name, config_chain_t **pp_cfg, const char *psz_string ) VLC_USED VLC_MALLOC;

/**
 * This function will release a linked list of config_chain_t
 * (Including the head)
 */
VLC_API void config_ChainDestroy( config_chain_t * );

/**
 * This function will duplicate a linked list of config_chain_t
 */
VLC_API config_chain_t * config_ChainDuplicate( const config_chain_t * ) VLC_USED VLC_MALLOC;

/**
 * This function will unescape a string in place and will return a pointer on
 * the given string.
 * No memory is allocated by it (unlike config_StringEscape).
 * If NULL is given as parameter nothing will be done (NULL will be returned).
 *
 * The following sequences will be unescaped (only one time):
 * \\ \' and \"
 */
VLC_API char * config_StringUnescape( char *psz_string );

/**
 * This function will escape a string that can be unescaped by
 * config_StringUnescape.
 * The returned value is allocated by it. You have to free it once you
 * do not need it anymore (unlike config_StringUnescape).
 * If NULL is given as parameter nothing will be done (NULL will be returned).
 *
 * The escaped characters are ' " and \
 */
VLC_API char * config_StringEscape( const char *psz_string ) VLC_USED VLC_MALLOC;

# ifdef __cplusplus
}
# endif

#endif /* _VLC_CONFIGURATION_H */
