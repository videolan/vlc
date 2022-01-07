/*****************************************************************************
 * importer.h
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#ifndef VLC_GL_IMPORTER_H
#define VLC_GL_IMPORTER_H

typedef struct picture_t picture_t;

/**
 * An importer uses an interop to convert picture_t to a valid vlc_gl_picture,
 * with all necessary transformations computed.
 */
struct vlc_gl_importer;
struct vlc_gl_interop;

struct vlc_gl_importer *
vlc_gl_importer_New(struct vlc_gl_interop *interop);

void
vlc_gl_importer_Delete(struct vlc_gl_importer *importer);

int
vlc_gl_importer_Update(struct vlc_gl_importer *importer, picture_t *picture);

#endif
