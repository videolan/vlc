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

int config_LoadCmdLine   ( vlc_object_t *, int, const char *[], int * );
int config_LoadConfigFile( vlc_object_t *, const char * );
#define config_LoadCmdLine(a,b,c,d) config_LoadCmdLine(VLC_OBJECT(a),b,c,d)
#define config_LoadConfigFile(a,b) config_LoadConfigFile(VLC_OBJECT(a),b)

int config_SortConfig (void);
void config_UnsortConfig (void);

char *config_GetDataDirDefault( void );

int IsConfigStringType( int type );
int IsConfigIntegerType (int type);
static inline int IsConfigFloatType (int type)
{
    return type == CONFIG_ITEM_FLOAT;
}

uint_fast32_t ConfigStringToKey( const char * );
char *ConfigKeyToString( uint_fast32_t );

extern vlc_rwlock_t config_lock;

/* The configuration file */
#define CONFIG_FILE                     "vlcrc"

# ifdef __cplusplus
}
# endif
#endif
