/*****************************************************************************
 * iomx.h: Interface for the IOMX OpenMAX wrapper
 *****************************************************************************
 * Copyright (C) 2011 VLC authors and VideoLAN
 *
 * Authors: Martin Storsjo <martin@martin.st>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

void* iomx_dlopen(const char *name);
void iomx_dlclose(void *handle);
void* iomx_dlsym(void *handle, const char *name);

#ifdef __cplusplus
}
#endif
