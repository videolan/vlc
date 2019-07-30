/*****************************************************************************
 * vlc_configuration.h : configuration management module
 * This file describes the programming interface for the configuration module.
 * It includes functions allowing to declare, get or set configuration options.
 *****************************************************************************
 * Copyright (C) 1999-2006 VLC authors and VideoLAN
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
 * \defgroup config User settings
 * \ingroup interface
 * VLC provides a simple name-value dictionary for user settings.
 *
 * Those settings are per-user per-system - they are shared by all LibVLC
 * instances in a single process, and potentially other processes as well.
 *
 * Each name-value pair is called a configuration item.
 * @{
 */

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

typedef int (*vlc_string_list_cb)(const char *, char ***, char ***);
typedef int (*vlc_integer_list_cb)(const char *, int64_t **, char ***);

/**
 * Configuration item
 *
 * This is the internal reprensation of a configuration item.
 * See also config_FindConfig().
 */
struct module_config_t
{
    uint8_t     i_type; /**< Configuration type */
    char        i_short; /**< Optional short option name */
    unsigned    b_internal:1; /**< Hidden from preferences and help */
    unsigned    b_unsaveable:1; /**< Not stored in configuration */
    unsigned    b_safe:1; /**< Safe for web plugins and playlist files */
    unsigned    b_removed:1; /**< Obsolete */

    const char *psz_type; /**< Configuration subtype */
    const char *psz_name; /**< Option name */
    const char *psz_text; /**< Short comment on the configuration option */
    const char *psz_longtext; /**< Long comment on the configuration option */

    module_value_t value; /**< Current value */
    module_value_t orig; /**< Default value */
    module_value_t min; /**< Minimum value (for scalars only) */
    module_value_t max; /**< Maximum value (for scalars only) */

    /* Values list */
    uint16_t list_count; /**< Choices count */
    union
    {
        const char **psz; /**< Table of possible string choices */
        const int  *i; /**< Table of possible integer choices */
    } list; /**< Possible choices */
    const char **list_text; /**< Human-readable names for list values */
    void *owner; /**< Origin run-time linker module handle */
};

/**
 * Gets a configuration item type
 *
 * This function checks the type of configuration item by name.
 * \param name Configuration item name
 * \return The configuration item type or 0 if not found.
 */
VLC_API int config_GetType(const char *name) VLC_USED;

/**
 * Gets an integer configuration item.
 *
 * This function retrieves the current value of a configuration item of
 * integral type (\ref CONFIG_ITEM_INTEGER and \ref CONFIG_ITEM_BOOL).
 *
 * \warning The behaviour is undefined if the configuration item exists but is
 * not of integer or boolean type.
 *
 * \param name Configuration item name
 * \return The configuration item value or -1 if not found.
 * \bug A legitimate integer value of -1 cannot be distinguished from an error.
 */
VLC_API int64_t config_GetInt(const char *name) VLC_USED;

/**
 * Sets an integer configuration item.
 *
 * This function changes the current value of a configuration item of
 * integral type (\ref CONFIG_ITEM_INTEGER and \ref CONFIG_ITEM_BOOL).
 *
 * \warning The behaviour is undefined if the configuration item exists but is
 * not of integer or boolean type.
 *
 * \note If no configuration item by the specified exist, the function has no
 * effects.
 *
 * \param name Configuration item name
 * \param val New value
 */
VLC_API void config_PutInt(const char *name, int64_t val);

/**
 * Gets an floating point configuration item.
 *
 * This function retrieves the current value of a configuration item of
 * floating point type (\ref CONFIG_ITEM_FLOAT).
 *
 * \warning The behaviour is undefined if the configuration item exists but is
 * not of floating point type.
 *
 * \param name Configuration item name
 * \return The configuration item value or -1 if not found.
 * \bug A legitimate floating point value of -1 cannot be distinguished from an
 * error.
 */
VLC_API float config_GetFloat(const char *name) VLC_USED;

/**
 * Sets an integer configuration item.
 *
 * This function changes the current value of a configuration item of
 * integral type (\ref CONFIG_ITEM_FLOAT).
 *
 * \warning The behaviour is undefined if the configuration item exists but is
 * not of floating point type.
 *
 * \note If no configuration item by the specified exist, the function has no
 * effects.
 *
 * \param name Configuration item name
 * \param val New value
 */
VLC_API void config_PutFloat(const char *name, float val);

/**
 * Gets an string configuration item.
 *
 * This function retrieves the current value of a configuration item of
 * string type (\ref CONFIG_ITEM_STRING).
 *
 * \note The caller must free() the returned pointer (if non-NULL), which is a
 * duplicate of the current value. It is not safe to return a pointer to the
 * current value internally as it can be modified at any time by any other
 * thread.
 *
 * \warning The behaviour is undefined if the configuration item exists but is
 * not of string type.
 *
 * \param name Configuration item name
 * \return Normally, a heap-allocated copy of the configuration item value.
 * If the value is the empty string, if the configuration does not exist,
 * or if an error occurs, NULL is returned.
 * \bug The empty string value cannot be distinguished from an error.
 */
VLC_API char *config_GetPsz(const char *name) VLC_USED VLC_MALLOC;

/**
 * Sets an string configuration item.
 *
 * This function changes the current value of a configuration item of
 * string type (e.g. \ref CONFIG_ITEM_STRING).
 *
 * \warning The behaviour is undefined if the configuration item exists but is
 * not of a string type.
 *
 * \note If no configuration item by the specified exist, the function has no
 * effects.
 *
 * \param name Configuration item name
 * \param val New value (will be copied)
 * \bug This function allocates memory but errors cannot be detected.
 */
VLC_API void config_PutPsz(const char *name, const char *val);

/**
 * Enumerates integer configuration choices.
 *
 * Determines a list of suggested values for an integer configuration item.
 * \param values pointer to a table of integer values [OUT]
 * \param texts pointer to a table of descriptions strings [OUT]
 * \return number of choices, or -1 on error
 * \note the caller is responsible for calling free() on all descriptions and
 * on both tables. In case of error, both pointers are set to NULL.
 */
VLC_API ssize_t config_GetIntChoices(const char *, int64_t **values,
                                     char ***texts) VLC_USED;

/**
 * Determines a list of suggested values for a string configuration item.
 * \param values pointer to a table of value strings [OUT]
 * \param texts pointer to a table of descriptions strings [OUT]
 * \return number of choices, or -1 on error
 * \note the caller is responsible for calling free() on all values, on all
 * descriptions and on both tables.
 * In case of error, both pointers are set to NULL.
 */
VLC_API ssize_t config_GetPszChoices(const char *,
                                     char ***values, char ***texts) VLC_USED;

VLC_API int config_SaveConfigFile( vlc_object_t * );
#define config_SaveConfigFile(a) config_SaveConfigFile(VLC_OBJECT(a))

/**
 * Resets the configuration.
 *
 * This function resets all configuration items to their respective
 * compile-time default value.
 */
VLC_API void config_ResetAll(void);

/**
 * Looks up a configuration item.
 *
 * This function looks for the internal representation of a configuration item.
 * Where possible, this should be avoided in favor of more specific function
 * calls.
 *
 * \param name Configuration item name
 * \return The internal structure, or NULL if not found.
 */
VLC_API module_config_t *config_FindConfig(const char *name) VLC_USED;

/**
 * System directory identifiers
 */
typedef enum vlc_system_dir
{
    VLC_PKG_DATA_DIR, /**< Package-specific architecture-independent read-only
                           data directory (e.g. /usr/local/data/vlc). */
    VLC_PKG_LIB_DIR, /**< Package-specific architecture-dependent read-only
                          data directory (e.g. /usr/local/lib/vlc). */
    VLC_PKG_LIBEXEC_DIR, /**< Package-specific executable read-only directory
                              (e.g. /usr/local/libexec/vlc). */
    VLC_PKG_INCLUDE_DIR_RESERVED,
    VLC_SYSDATA_DIR, /**< Global architecture-independent read-only
                          data directory (e.g. /usr/local/data).
                          Available only on some platforms. */
    VLC_LIB_DIR, /**< Global architecture-dependent read-only directory
                      (e.g. /usr/local/lib). */
    VLC_LIBEXEC_DIR, /**< Global executable read-only directory
                          (e.g. /usr/local/libexec). */
    VLC_INCLUDE_DIR_RESERVED,
    VLC_LOCALE_DIR, /**< Base directory for package read-only locale data. */
} vlc_sysdir_t;

/**
 * Gets an installation directory.
 *
 * This function determines one of the installation directory.
 *
 * @param dir identifier of the directory (see \ref vlc_sysdir_t)
 * @param filename name of a file or other object within the directory
 *                 (or NULL to obtain the plain directory)
 *
 * @return a heap-allocated string (use free() to release it), or NULL on error
 */
VLC_API char *config_GetSysPath(vlc_sysdir_t dir, const char *filename)
VLC_USED VLC_MALLOC;

typedef enum vlc_user_dir
{
    VLC_HOME_DIR, /* User's home */
    VLC_CONFIG_DIR, /* VLC-specific configuration directory */
    VLC_USERDATA_DIR, /* VLC-specific data directory */
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

VLC_API void config_AddIntf(const char *);
VLC_API void config_RemoveIntf(const char *);
VLC_API bool config_ExistIntf(const char *) VLC_USED;

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
VLC_API void config_ChainParse( vlc_object_t *, const char *psz_prefix, const char *const *ppsz_options, const config_chain_t * );
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

/** @} */

#endif /* _VLC_CONFIGURATION_H */
