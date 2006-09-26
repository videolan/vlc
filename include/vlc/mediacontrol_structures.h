/*****************************************************************************
 * control_structures.h: global header for mediacontrol
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <olivier.aubert@liris.univ-lyon1.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
/**
 * \defgroup mediacontrol_structures MediaControl Structures
 * Data structures used in the MediaControl API.
 *
 * @{
 */

#ifndef _VLC_CONTROL_STRUCTURES_H
#define _VLC_CONTROL_STRUCTURES_H 1

# ifdef __cplusplus
extern "C" {
# endif

/**
 * A position may have different origins: 
 *  - absolute counts from the movie start
 *  - relative counts from the current position
 *  - modulo counts from the current position and wraps at the end of the movie
 */
typedef enum  {
    mediacontrol_AbsolutePosition,
    mediacontrol_RelativePosition,
    mediacontrol_ModuloPosition
} mediacontrol_PositionOrigin;

/**
 * Units available in mediacontrol Positions
 *  - ByteCount number of bytes
 *  - SampleCount number of frames
 *  - MediaTime time in milliseconds
 */
typedef enum {
    mediacontrol_ByteCount,
    mediacontrol_SampleCount,
    mediacontrol_MediaTime
} mediacontrol_PositionKey;

/**
 * MediaControl Position
 */
typedef struct {
    mediacontrol_PositionOrigin origin;
    mediacontrol_PositionKey key;
    long value;
} mediacontrol_Position;

# ifdef __cplusplus
}
# endif

#endif

/** @} */
