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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _VLC_CONTROL_STRUCTURES_H
#define _VLC_CONTROL_STRUCTURES_H 1

# ifdef __cplusplus
extern "C" {
# endif

typedef enum  {
  mediacontrol_AbsolutePosition,
  mediacontrol_RelativePosition,
  mediacontrol_ModuloPosition
} mediacontrol_PositionOrigin;

typedef enum {
  mediacontrol_ByteCount,
  mediacontrol_SampleCount,
  mediacontrol_MediaTime
} mediacontrol_PositionKey;

typedef struct {
  mediacontrol_PositionOrigin origin;
  mediacontrol_PositionKey key;
  long value;
} mediacontrol_Position;


# ifdef __cplusplus
}
# endif

#endif
