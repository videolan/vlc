/*****************************************************************************
 * libvlc_version.h
 *****************************************************************************
 * Copyright (C) 2010 Rémi Denis-Courmont
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

/**
 * \file
 * This file defines version macros for LibVLC.
 * Those macros are primilarly intended for conditional (pre)compilation.
 * To get the run-time LibVLC version, use libvlc_get_version() instead
 * (the run-time version may be more recent than build-time one, thanks to
 * backward binary compatibility).
 *
 * \version This header file is available in LibVLC 1.1.4 and higher.
 */

#ifndef LIBVLC_VERSION_H
# define LIBVLC_VERSION_H 1

/** LibVLC major version number */
# define LIBVLC_VERSION_MAJOR    (4)

/** LibVLC minor version number */
# define LIBVLC_VERSION_MINOR    (0)

/** LibVLC revision */
# define LIBVLC_VERSION_REVISION (0)

# define LIBVLC_VERSION_EXTRA    (0)

/** Makes a single integer from a LibVLC version numbers */
# define LIBVLC_VERSION(maj,min,rev,extra) \
         ((maj << 24) | (min << 16) | (rev << 8) | (extra))

/** LibVLC full version as a single integer (for comparison) */
# define LIBVLC_VERSION_INT \
         LIBVLC_VERSION(LIBVLC_VERSION_MAJOR, LIBVLC_VERSION_MINOR, \
                        LIBVLC_VERSION_REVISION, LIBVLC_VERSION_EXTRA)


/** LibVLC ABI major version number, updated when incompatible changes are added */
# define LIBVLC_ABI_VERSION_MAJOR  (12)

/** LibVLC ABI minor version number, updated when compatible changes are added */
# define LIBVLC_ABI_VERSION_MINOR  (0)

/** LibVLC ABI micro version number, updated with new releases */
# define LIBVLC_ABI_VERSION_MICRO  (0)

/** LibVLC full ABI version combining the major VLC version and the .so version:
 * - A 0xFF000000 mask gives the VLC major version,
 * - A 0x00FF0000 mask gives the LibVLC major ABI version,
 * - A 0x0000FF00 mask gives the LibVLC minor ABI version,
 * - A 0x000000FF mask gives the LibVLC ABI revision.
 *
 * LibVLC is considered compatible with your code if the VLC major and LibVLC
 * major values are equal and the minor ABI version is equal or higher than the
 * value you compiled with.
 */
# define LIBVLC_ABI_VERSION_INT \
         LIBVLC_VERSION(LIBVLC_VERSION_MAJOR, LIBVLC_ABI_VERSION_MAJOR, \
                        LIBVLC_ABI_VERSION_MINOR, LIBVLC_ABI_VERSION_MICRO )

#endif
