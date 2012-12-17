/*****************************************************************************
 * fs.h: file system access plug-in common header
 *****************************************************************************
 * Copyright (C) 2009 Rémi Denis-Courmont
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

#include <dirent.h>

int FileOpen (vlc_object_t *);
void FileClose (vlc_object_t *);

int DirOpen (vlc_object_t *);
int DirInit (access_t *p_access, DIR *handle);
block_t *DirBlock (access_t *);
int DirControl (access_t *, int, va_list);
void DirClose (vlc_object_t *);
