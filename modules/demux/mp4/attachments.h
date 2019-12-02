/*****************************************************************************
 * attachments.h : MP4 attachments handling
 *****************************************************************************
 * Copyright (C) 2001-2015 VLC authors and VideoLAN
 *               2019 VideoLabs
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
#ifndef VLC_MP4_ATTACHMENTS_H_
#define VLC_MP4_ATTACHMENTS_H_

int MP4_GetAttachments( const MP4_Box_t *, input_attachment_t *** );
const MP4_Box_t *MP4_GetMetaRoot( const MP4_Box_t *, const char ** );
int MP4_GetCoverMetaURI( const MP4_Box_t *,  const MP4_Box_t *,
                         const char *, vlc_meta_t * );

#endif
