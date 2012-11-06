/*****************************************************************************
 * rar.h: uncompressed RAR parser
 *****************************************************************************
 * Copyright (C) 2008-2010 Laurent Aimar
 * $Id$
 *
 * Author: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

typedef struct {
    char     *mrl;
    uint64_t offset;
    uint64_t size;
    uint64_t cummulated_size;
} rar_file_chunk_t;

typedef struct {
    char     *name;
    uint64_t size;
    bool     is_complete;

    int              chunk_count;
    rar_file_chunk_t **chunk;
    uint64_t         real_size;  /* Gathered size */
} rar_file_t;

int  RarProbe(stream_t *);
void RarFileDelete(rar_file_t *);
int  RarParse(stream_t *, int *, rar_file_t ***);

