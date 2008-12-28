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

/**
 * \file
 * This file defines functions for modules in vlc
 */

/*****************************************************************************
 * Exported functions.
 *****************************************************************************/

#define module_need(a,b,c,d) __module_need(VLC_OBJECT(a),b,c,d)
VLC_EXPORT( module_t *, __module_need, ( vlc_object_t *, const char *, const char *, bool ) );
#define module_unneed(a,b) __module_unneed(VLC_OBJECT(a),b)
VLC_EXPORT( void, __module_unneed, ( vlc_object_t *, module_t * ) );
VLC_EXPORT( bool,  module_exists, (const char *) );
VLC_EXPORT( module_t *, module_find, (const char *) );

VLC_EXPORT( module_config_t *, module_config_get, ( const module_t *, unsigned * ) );
VLC_EXPORT( void, module_config_free, ( module_config_t * ) );

VLC_EXPORT( module_t *, module_hold, (module_t *module) );
VLC_EXPORT( void, module_release, (module_t *module) );
VLC_EXPORT( void, module_list_free, (module_t **) );
VLC_EXPORT( module_t **, module_list_get, (size_t *n) );

VLC_EXPORT( bool, module_provides, ( const module_t *m, const char *cap ) );
VLC_EXPORT( const char *, module_get_object, ( const module_t *m ) );
VLC_EXPORT( const char *, module_get_name, ( const module_t *m, bool long_name ) );
#define module_GetLongName( m ) module_get_name( m, true )
VLC_EXPORT( const char *, module_get_help, ( const module_t *m ) );
VLC_EXPORT( const char *, module_get_capability, ( const module_t *m ) );
VLC_EXPORT( int, module_get_score, ( const module_t *m ) );

static inline module_t *module_get_main (void)
{
    return module_find ("main");
}
#define module_get_main(a) module_get_main()

static inline bool module_is_main( const module_t * p_module )
{
    return !strcmp( module_get_object( p_module ), "main" );
}
