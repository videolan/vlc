/*****************************************************************************
 * vlc_update.h: VLC update download
 *****************************************************************************
 * Copyright © 2005-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
 *          Rafaël Carré <funman@videolanorg>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either release 2 of the License, or
 * (at your option) any later release.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef VLC_UPDATE_H
#define VLC_UPDATE_H

/**
 * \file
 * This file defines update API in vlc
 */

/**
 * \defgroup update Update
 *
 * @{
 */

#ifdef UPDATE_CHECK

/**
 * Describes an update VLC release number
 */
struct update_release_t
{
    int i_major;        ///< Version major
    int i_minor;        ///< Version minor
    int i_revision;     ///< Version revision
    unsigned char extra;///< Version extra
    char* psz_url;      ///< Download URL
    char* psz_desc;     ///< Release description
};

#endif /* UPDATE_CHECK */

typedef struct update_release_t update_release_t;

#define update_New( a ) __update_New( VLC_OBJECT( a ) )

VLC_EXPORT( update_t *, __update_New, ( vlc_object_t * ) );
VLC_EXPORT( void, update_Delete, ( update_t * ) );
VLC_EXPORT( void, update_Check, ( update_t *, void (*callback)( void*, bool ), void * ) );
VLC_EXPORT( bool, update_NeedUpgrade, ( update_t * ) );
VLC_EXPORT( void, update_Download, ( update_t *, const char* ) );
VLC_EXPORT( update_release_t*, update_GetRelease, ( update_t * ) );
VLC_EXPORT( void, update_WaitDownload, ( update_t * ) );

/**
 * @}
 */

#endif /* _VLC_UPDATE_H */
