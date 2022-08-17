/*****************************************************************************
 * configuration.h management of the modules configuration
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
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

#ifndef LIBVLC_CONFIGURATION_H
# define LIBVLC_CONFIGURATION_H 1

/* Internal configuration prototypes and structures */

struct vlc_plugin_t;

struct vlc_param {
    union {
        _Atomic int64_t i; /**< Current value (if integer or boolean) */
        _Atomic float f; /**< Current value (if floating point) */
        char *_Atomic str; /**< Current value (if character string) */
    } value;

    struct vlc_plugin_t *owner;
    unsigned char shortname; /**< Optional short option name */
    unsigned internal:1; /**< Hidden from preferences and help */
    unsigned unsaved:1; /**< Not stored in persistent configuration */
    unsigned safe:1; /**< Safe for untrusted provisioning (playlists) */
    unsigned obsolete:1; /**< Ignored for backward compatibility */
    struct module_config_t item;
};

/**
 * Looks up a configuration parameter by name.
 *
 * \return the configuration parameter, or NULL if not found
 */
struct vlc_param *vlc_param_Find(const char *name);

int vlc_param_SetString(struct vlc_param *param, const char *value);

int  config_AutoSaveConfigFile( vlc_object_t * );

void config_Free(struct vlc_param *, size_t);

void config_CmdLineEarlyScan( vlc_object_t *, int, const char *[] );
#define config_CmdLineEarlyScan(a,b,c) config_CmdLineEarlyScan(VLC_OBJECT(a),b,c)

int config_LoadCmdLine   ( vlc_object_t *, int, const char *[], int * );
int config_LoadConfigFile( vlc_object_t * );
#define config_LoadCmdLine(a,b,c,d) config_LoadCmdLine(VLC_OBJECT(a),b,c,d)
#define config_LoadConfigFile(a) config_LoadConfigFile(VLC_OBJECT(a))
bool config_PrintHelp (vlc_object_t *);
void config_Lock(void);
void config_Unlock(void);

int config_SortConfig (void);
void config_UnsortConfig (void);

bool config_IsSafe (const char *);

/**
 * Gets the arch-specific installation directory.
 *
 * This function determines the directory containing the architecture-specific
 * installed asset files (such as executable plugins and compiled byte code).
 *
 * @return a heap-allocated string (use free() to release it), or NULL on error
 */
char *config_GetLibDir(void) VLC_USED VLC_MALLOC;

/* The configuration file */
#define CONFIG_FILE                     "vlcrc"

#endif
