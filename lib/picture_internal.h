/*****************************************************************************
 * picture_internal.h:  libvlc API picture management
 *****************************************************************************
 * Copyright (C) 1998-2018 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#ifndef PICTURE_INTERNAL_H
#define PICTURE_INTERNAL_H

#include <vlc_picture.h>

/**
 * \brief libvlc_picture_new Wraps a libvlccore's picture_t to a libvlc_picture_t
 * \param p_obj A vlc object
 * \param p_input Input picture
 * \param i_type Desired converted picture type
 * \param i_width Converted picture width
 * \param i_height Converted picture height
 * \param b_crop Should the picture be cropped to preserve aspect ratio
 * \return An opaque libvlc_picture_t
 *
 * The picture refcount is left untouched by this function, but is converted to
 * the required format and stored as a block_t
 * The returned picture must be released through libvlc_picture_release
 */
libvlc_picture_t* libvlc_picture_new( vlc_object_t* p_obj, picture_t* p_pic,
                                      libvlc_picture_type_t i_format,
                                      unsigned int i_width, unsigned int i_height,
                                      bool b_crop );

#endif /* PICTURE_INTERNAL_H */
