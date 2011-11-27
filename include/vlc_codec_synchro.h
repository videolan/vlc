/*****************************************************************************
 * vlc_codec_synchro.h: frame-dropping structures
 *****************************************************************************
 * Copyright (C) 1999-2005 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Jean-Marc Dressler <polux@via.ecp.fr>
 *          Stéphane Borel <stef@via.ecp.fr>
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

/*****************************************************************************
 * decoder_synchro_t : timers for the video synchro
 *****************************************************************************/
/* Read the discussion on top of decoder_synchro.c for more information. */
/* Pictures types */
#define I_CODING_TYPE           1
#define P_CODING_TYPE           2
#define B_CODING_TYPE           3
#define D_CODING_TYPE           4 /* MPEG-1 ONLY */
/* other values are reserved */

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_API decoder_synchro_t * decoder_SynchroInit( decoder_t *, int ) VLC_USED;
VLC_API void decoder_SynchroRelease( decoder_synchro_t * );
VLC_API void decoder_SynchroReset( decoder_synchro_t * );
VLC_API bool decoder_SynchroChoose( decoder_synchro_t *, int, int, bool );
VLC_API void decoder_SynchroTrash( decoder_synchro_t * );
VLC_API void decoder_SynchroDecode( decoder_synchro_t * );
VLC_API void decoder_SynchroEnd( decoder_synchro_t *, int, bool );
VLC_API mtime_t decoder_SynchroDate( decoder_synchro_t * ) VLC_USED;
VLC_API void decoder_SynchroNewPicture( decoder_synchro_t *, int, int, mtime_t, mtime_t, bool );

