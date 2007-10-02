/*****************************************************************************
 * vout_synchro.h: frame-dropping structures
 * Only used in libmpeg2 decoder at the moment
 *****************************************************************************
 * Copyright (C) 1999-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Marc Dressler <polux@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
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
 * vout_synchro_t : timers for the video synchro
 *****************************************************************************/
/* Read the discussion on top of vout_synchro.c for more information. */
/* Pictures types */
#define I_CODING_TYPE           1
#define P_CODING_TYPE           2
#define B_CODING_TYPE           3
#define D_CODING_TYPE           4 /* MPEG-1 ONLY */
/* other values are reserved */

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
#define vout_SynchroInit(a,b) __vout_SynchroInit(VLC_OBJECT(a),b)
VLC_EXPORT( vout_synchro_t *, __vout_SynchroInit, ( vlc_object_t *, int ) );
VLC_EXPORT( void, vout_SynchroRelease,        ( vout_synchro_t * ) );
VLC_EXPORT( void, vout_SynchroReset,          ( vout_synchro_t * ) );
VLC_EXPORT( vlc_bool_t, vout_SynchroChoose,   ( vout_synchro_t *, int, int, vlc_bool_t ) );
VLC_EXPORT( void, vout_SynchroTrash,          ( vout_synchro_t * ) );
VLC_EXPORT( void, vout_SynchroDecode,         ( vout_synchro_t * ) );
VLC_EXPORT( void, vout_SynchroEnd,            ( vout_synchro_t *, int, vlc_bool_t ) );
VLC_EXPORT( mtime_t, vout_SynchroDate,        ( vout_synchro_t * ) );
VLC_EXPORT( void, vout_SynchroNewPicture,     ( vout_synchro_t *, int, int, mtime_t, mtime_t, int, vlc_bool_t ) );

