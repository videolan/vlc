/*****************************************************************************
 * configuration.h management of the modules configuration
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
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

#if defined(__PLUGIN__) || defined(__BUILTIN__) || !defined(__LIBVLC__)
# error This header file can only be included from LibVLC.
#endif

#ifndef LIBVLC_CONFIGURATION_H
# define LIBVLC_CONFIGURATION_H 1

# ifdef __cplusplus
extern "C" {
# endif

/* Internal configuration prototypes and structures */

int  config_CreateDir( vlc_object_t *, const char * );
int  config_AutoSaveConfigFile( vlc_object_t * );

void config_Free( module_t * );

void config_SetCallbacks( module_config_t *, module_config_t *, size_t );
void config_UnsetCallbacks( module_config_t *, size_t );

#define config_LoadCmdLine(a,b,c,d) __config_LoadCmdLine(VLC_OBJECT(a),b,c,d)
#define config_LoadConfigFile(a,b) __config_LoadConfigFile(VLC_OBJECT(a),b)

int __config_LoadCmdLine   ( vlc_object_t *, int *, const char *[], bool );
char *config_GetCustomConfigFile( libvlc_int_t * );
int __config_LoadConfigFile( vlc_object_t *, const char * );

int IsConfigStringType( int type );
int IsConfigIntegerType (int type);
static inline int IsConfigFloatType (int type)
{
    return type == CONFIG_ITEM_FLOAT;
}

int ConfigStringToKey( const char * );

/* The configuration file and directory */
#if defined (SYS_BEOS)
#  define CONFIG_DIR                    "config/settings/VideoLAN Client"
#elif defined (__APPLE__)
#  define CONFIG_DIR                    "Library/Preferences/VLC"
#elif defined( WIN32 ) || defined( UNDER_CE )
#  define CONFIG_DIR                    "vlc"
#else
#  define CONFIG_DIR                    ".vlc"
#endif
#define CONFIG_FILE                     "vlcrc"

# ifdef __cplusplus
}
# endif
#endif
