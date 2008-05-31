/*****************************************************************************
 * version.c: LibVLC version infos
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Rémi Denis-Courmont <rem # videolan : org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

/*****************************************************************************
 * VLC_Version: return the libvlc version.
 *****************************************************************************
 * This function returns full version string (numeric version and codename).
 *****************************************************************************/
char const * VLC_Version( void )
{
    return VERSION_MESSAGE;
}

/*****************************************************************************
 * VLC_CompileBy, VLC_CompileHost, VLC_CompileDomain,
 * VLC_Compiler, VLC_Changeset
 *****************************************************************************/
#define DECLARE_VLC_VERSION( func, var )                                    \
char const * VLC_##func ( void )                                            \
{                                                                           \
    return VLC_##var ;                                                      \
}

DECLARE_VLC_VERSION( CompileBy, COMPILE_BY );
DECLARE_VLC_VERSION( CompileHost, COMPILE_HOST );
DECLARE_VLC_VERSION( CompileDomain, COMPILE_DOMAIN );
DECLARE_VLC_VERSION( Compiler, COMPILER );

extern const char psz_vlc_changeset[];
const char* VLC_Changeset( void )
{
    return psz_vlc_changeset;
}
