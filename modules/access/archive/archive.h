/*****************************************************************************
 * archive.h: libarchive access & stream filter
 *****************************************************************************
 * Copyright (C) 2014 Videolan Team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

int AccessOpen(vlc_object_t *object);
void AccessClose(vlc_object_t *object);

int StreamOpen(vlc_object_t *object);
void StreamClose(vlc_object_t *object);

bool ProbeArchiveFormat(stream_t *p_stream);

struct archive;
void EnableArchiveFormats(struct archive *p_archive);

#define ARCHIVE_READ_SIZE 8192
#define ARCHIVE_SEP_CHAR '|'
