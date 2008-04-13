/*****************************************************************************
 * info.h : CD digital audio input information routine headers
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Rocky Bernstein <rocky@panix.com>
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

/*
 Fills out playlist information.
 */
int      CDDAFixupPlaylist( access_t *p_access, cdda_data_t *p_cdda,
                            bool b_single_track );

/*
 Sets CDDA Meta Information. In the Control routine,
 we handle Meta Information requests and basically copy what we've
 saved here.
 */
void     CDDAMetaInfo( access_t *p_access, track_t i_track );

/*
 Saves Meta Information about the CD-DA.

 Saves information that CDDAMetaInfo uses. Should be called before
 CDDAMetaInfo is called.
 */
void     CDDAMetaInfoInit( access_t *p_access );

char *CDDAFormatTitle( const access_t *p_access, track_t i_track );

