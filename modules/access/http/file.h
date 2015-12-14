/*****************************************************************************
 * file.h: HTTP read-only file
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
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

#include <stdint.h>

struct vlc_http_file;
struct vlc_http_mgr;
struct block_t;

struct vlc_http_file *vlc_http_file_create(struct vlc_http_mgr *mgr,
                                           const char *url, const char *ua,
                                           const char *ref);
void vlc_http_file_destroy(struct vlc_http_file *file);

int vlc_http_file_get_status(struct vlc_http_file *file);
char *vlc_http_file_get_redirect(struct vlc_http_file *file);

uintmax_t vlc_http_file_get_size(struct vlc_http_file *file);
bool vlc_http_file_can_seek(struct vlc_http_file *file);
char *vlc_http_file_get_type(struct vlc_http_file *file);
int vlc_http_file_seek(struct vlc_http_file *file, uintmax_t offset);
struct block_t *vlc_http_file_read(struct vlc_http_file *file);
