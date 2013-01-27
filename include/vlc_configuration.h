/*****************************************************************************
 * vlc_configuration.h : configuration management module
 * This file describes the programming interface for the configuration module.
 * It includes functions allowing to declare, get or set configuration options.
 *****************************************************************************
 * Copyright (C) 1999-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#ifndef VLC_CONFIGURATION_H
#define VLC_CONFIGURATION_H 1

/**
 * \file
 * This file describes the programming interface for the configuration module.
 * It includes functions allowing to declare, get or set configuration options.
 */

#include <sys/types.h>  /* for ssize_t */

# ifdef __cplusplus
extern "C" {
# endif

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

typedef int (*vlc_string_list_cb)(vlc_object_t *, const char *,
                                  char ***, char ***);
typedef int (*vlc_integer_list_cb)(vlc_object_t *, const char *,
                                   int64_t **, char ***);

struct module_config_t
{
    uint8_t     i_type;                        /* Configuration type */
    char        i_short;               /* Optional short option name */
    unsigned    b_advanced:1;                     /* Advanced option */
    unsigned    b_internal:1;          /* Hidden from prefs and help */
    unsigned    b_unsaveable:1;       /* Not stored in configuration */
    unsigned    b_safe:1;       /* Safe in web plugins and playlists */
    unsigned    b_removed:1;                           /* Deprecated */

    char *psz_type;                                 /* Configuration subtype */
    char *psz_name;                                           /* Option name */
    char *psz_text;             /* Short comment on the configuration option */
    char *psz_longtext;          /* Long comment on the configuration option */

    module_value_t value;                                    /* Option value */
    module_value_t orig;
    module_value_t min;
    module_value_t max;

    /* Values list */
    uint16_t list_count;                                /* Options list size */
    union
    {
        char **psz;               /* List of possible values for the option */
        int   *i;
        vlc_string_list_cb psz_cb;
        vlc_integer_list_cb i_cb;
    } list;
    char **list_text;                      /* Friendly names for list values */
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
VLC_API ssize_t config_GetIntChoices(vlc_object_t *, const char *,
                                     int64_t **, char ***) VLC_USED;
VLC_API ssize_t config_GetPszChoices(vlc_object_t *, const char *,
                                     char ***, char ***) VLC_USED;

VLC_API int config_SaveConfigFile( vlc_object_t * );
#define config_SaveConfigFile(a) config_SaveConfigFile(VLC_OBJECT(a))

VLC_API void config_ResetAll( vlc_object_t * );
#define config_ResetAll(a) config_ResetAll(VLC_OBJECT(a))

VLC_API module_config_t * config_FindConfig( vlc_object_t *, const char * ) VLC_USED;
VLC_API char * config_GetDataDir(void) VLC_USED VLC_MALLOC;
VLC_API char *config_GetLibDir(void) VLC_USED;

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
 * This function will parse a configuration string (psz_opts) and
 * - set all options for this module in a chained list (*pp_cfg)
 * - returns a pointer on the next module if any.
 *
 * The string format is
 *   module{option=*,option=*}
 *
 * The options values are unescaped using config_StringUnescape.
 */
VLC_API const char *config_ChainParseOptions( config_chain_t **pp_cfg, const char *ppsz_opts );

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
