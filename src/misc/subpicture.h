/*****************************************************************************
 * subpicture.h: Private subpicture definitions
 *****************************************************************************
 * Copyright (C) 2010 Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

struct subpicture_region_private_t {
    video_format_t fmt;
    picture_t      *p_picture;
};

subpicture_region_t * subpicture_region_NewInternal( const video_format_t *p_fmt );

subpicture_region_private_t *subpicture_region_private_New(video_format_t *);
void subpicture_region_private_Delete(subpicture_region_private_t *);

