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

picture_t * subpicture_region_cache_GetPicture( subpicture_region_t * );
void subpicture_region_cache_Invalidate( subpicture_region_t * );
const video_format_t * subpicture_region_cache_GetFormat( const subpicture_region_t * );
int subpicture_region_cache_Assign( subpicture_region_t *p_region, picture_t * );
bool subpicture_region_cache_IsValid(const subpicture_region_t *);
