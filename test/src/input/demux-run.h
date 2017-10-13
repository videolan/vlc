/**
 * @file demux-run.c
 */
/*****************************************************************************
 * Copyright © 2016 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Rémi Denis-Courmont reserves the right to redistribute this file under
 * the terms of the GNU Lesser General Public License as published by the
 * the Free Software Foundation; either version 2.1 or the License, or
 * (at his option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "common.h"

int vlc_demux_process_url(const struct vlc_run_args *, const char *url);
int vlc_demux_process_path(const struct vlc_run_args *, const char *path);
int vlc_demux_process_memory(const struct vlc_run_args *,
                             const unsigned char *buf, size_t length);
