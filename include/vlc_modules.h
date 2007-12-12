/*****************************************************************************
 * modules.h : Module descriptor and load functions
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Exported functions.
 *****************************************************************************/
#define module_Need(a,b,c,d) __module_Need(VLC_OBJECT(a),b,c,d)
VLC_EXPORT( module_t *, __module_Need, ( vlc_object_t *, const char *, const char *, vlc_bool_t ) );
#define module_Unneed(a,b) __module_Unneed(VLC_OBJECT(a),b)
VLC_EXPORT( void, __module_Unneed, ( vlc_object_t *, module_t * ) );
#define module_Exists(a,b) __module_Exists(VLC_OBJECT(a),b)
VLC_EXPORT( vlc_bool_t,  __module_Exists, ( vlc_object_t *, const char * ) );

/* Use only if you know what you're doing... */
#define module_FindName(a,b) __module_FindName(VLC_OBJECT(a),b)
VLC_EXPORT( module_t *, __module_FindName, ( vlc_object_t *, const char * ) );
VLC_EXPORT( void, module_Put, ( module_t *module ) );

VLC_EXPORT( module_config_t *, module_GetConfig, ( const module_t *, unsigned * ) );
VLC_EXPORT( void, module_PutConfig, ( module_config_t * ) );


/* Return a NULL terminated array with the names of the modules that have a
 * certain capability.
 * Free after uses both the string and the table. */
 #define module_GetModulesNamesForCapability(a,b,c) \
                    __module_GetModulesNamesForCapability(VLC_OBJECT(a),b,c)
VLC_EXPORT(char **, __module_GetModulesNamesForCapability,
                    ( vlc_object_t *p_this, const char * psz_capability,
                      char ***psz_longname ) );

VLC_EXPORT( module_t *, vlc_module_create, ( vlc_object_t * ) );
VLC_EXPORT( module_t *, vlc_submodule_create, ( module_t * ) );
VLC_EXPORT( int, vlc_module_set, (module_t *module, int propid, void *value) );

enum vlc_module_properties
{
    /* DO NOT EVER REMOVE, INSERT OR REPLACE ANY ITEM! It would break the ABI!
     * Append new items at the end ONLY. */
    VLC_MODULE_CPU_REQUIREMENT,
    VLC_MODULE_SHORTCUT,
    VLC_MODULE_SHORTNAME,
    VLC_MODULE_DESCRIPTION,
    VLC_MODULE_HELP,
    VLC_MODULE_CAPABILITY,
    VLC_MODULE_SCORE,
    VLC_MODULE_PROGRAM,
    VLC_MODULE_CB_OPEN,
    VLC_MODULE_CB_CLOSE,
    VLC_MODULE_UNLOADABLE,
    VLC_MODULE_NAME
};

VLC_EXPORT( vlc_bool_t, module_IsCapable, ( const module_t *m, const char *cap ) );
VLC_EXPORT( const char *, module_GetObjName, ( const module_t *m ) );
VLC_EXPORT( const char *, module_GetName, ( const module_t *m, vlc_bool_t long_name ) );
#define module_GetLongName( m ) module_GetName( m, VLC_TRUE )
VLC_EXPORT( const char *, module_GetHelp, ( const module_t *m ) );
