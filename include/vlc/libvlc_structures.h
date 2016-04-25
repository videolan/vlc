/*****************************************************************************
 * libvlc_structures.h:  libvlc_* new external API structures
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * $Id $
 *
 * Authors: Filippo Carone <littlejohn@videolan.org>
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

#ifndef LIBVLC_STRUCTURES_H
#define LIBVLC_STRUCTURES_H 1

/**
 * \file
 * This file defines libvlc_* new external API structures
 */

#include <stdint.h>

# ifdef __cplusplus
extern "C" {
# endif

/**
 * \ingroup libvlc_core
 * @{
 */

/** This structure is opaque. It represents a libvlc instance */
typedef struct libvlc_instance_t libvlc_instance_t;

typedef int64_t libvlc_time_t;

/**@} */

# ifdef __cplusplus
}
# endif

#endif
