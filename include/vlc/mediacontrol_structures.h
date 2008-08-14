/*****************************************************************************
 * mediacontrol_structures.h: global header for mediacontrol
 *****************************************************************************
 * Copyright (C) 2005-2008 the VideoLAN team
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
 * \file
 * This file defines libvlc mediacontrol_* data structures
 */

/**
 * \defgroup mediacontrol_structures MediaControl Structures
 * Data structures used in the MediaControl API.
 *
 * @{
 */

#ifndef VLC_CONTROL_STRUCTURES_H
#define VLC_CONTROL_STRUCTURES_H 1

# ifdef __cplusplus
extern "C" {
# endif

#include <stdint.h>

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
 * Possible player status
 * Note the order of these enums must match exactly the order of
 * libvlc_state_t and input_state_e enums.
 */
typedef enum {
    mediacontrol_UndefinedStatus=0, mediacontrol_InitStatus,
    mediacontrol_BufferingStatus, mediacontrol_PlayingStatus,
    mediacontrol_PauseStatus,     mediacontrol_StopStatus,
    mediacontrol_ForwardStatus,   mediacontrol_BackwardStatus,
    mediacontrol_EndStatus,       mediacontrol_ErrorStatus,
} mediacontrol_PlayerStatus;

/**
 * MediaControl Position
 */
typedef struct {
    mediacontrol_PositionOrigin origin;
    mediacontrol_PositionKey key;
    int64_t value;
} mediacontrol_Position;

/**
 * RGBPicture structure
 * This generic structure holds a picture in an encoding specified by type.
 */
typedef struct {
    int  width;
    int  height;
    uint32_t type;
    int64_t date;
    int  size;
    char *data;
} mediacontrol_RGBPicture;

/**
 * Playlist sequence
 * A simple list of strings.
 */
typedef struct {
    int size;
    char **data;
} mediacontrol_PlaylistSeq;

typedef struct {
    int code;
    char *message;
} mediacontrol_Exception;

/**
 * Exception codes
 */
#define mediacontrol_PositionKeyNotSupported    1
#define mediacontrol_PositionOriginNotSupported 2
#define mediacontrol_InvalidPosition            3
#define mediacontrol_PlaylistException          4
#define mediacontrol_InternalException          5

/**
 * Stream information
 * This structure allows to quickly get various informations about the stream.
 */
typedef struct {
    mediacontrol_PlayerStatus streamstatus;
    char *url;         /* The URL of the current media stream */
    int64_t position;  /* actual location in the stream (in ms) */
    int64_t length;    /* total length of the stream (in ms) */
} mediacontrol_StreamInformation;


# ifdef __cplusplus
}
# endif

#endif

/** @} */
