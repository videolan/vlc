/*****************************************************************************
 * control.h: global header for mediacontrol
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

#ifndef _VLC_CONTROL_H
#define _VLC_CONTROL_H 1

# ifdef __cplusplus
extern "C" {
# endif

#if defined( WIN32 )
#define WINDOWHANDLE HWND
#else
#define WINDOWHANDLE int
#endif

#include <vlc/control_structures.h>

/************************************************************************
 * Position Object Manipulation
 *************************************************************************/

typedef struct {
  int  width;
  int  height;
  long type;
  vlc_int64_t date;
  int  size;
  char *data;
} mediacontrol_RGBPicture;

typedef struct {
  int size;
  char **data;
} mediacontrol_PlaylistSeq;

typedef struct {
  int code;
  char *message;
} mediacontrol_Exception;

/* Exception codes */
#define mediacontrol_PositionKeyNotSupported    1
#define mediacontrol_PositionOriginNotSupported 2
#define mediacontrol_InvalidPosition            3
#define mediacontrol_PlaylistException          4
#define mediacontrol_InternalException          5

typedef mediacontrol_Instance;

/* Cf stream_control.h */
enum mediacontrol_PlayerStatusList
{
    mediacontrol_PlayingStatus, mediacontrol_PauseStatus,
    mediacontrol_ForwardStatus, mediacontrol_BackwardStatus,
    mediacontrol_InitStatus,    mediacontrol_EndStatus,
    mediacontrol_UndefinedStatus
};
typedef enum mediacontrol_PlayerStatusList mediacontrol_PlayerStatus;

typedef struct {
    mediacontrol_PlayerStatus streamstatus;
    char *url;         /* The URL of the current media stream */
    vlc_int64_t position;     /* actual location in the stream (in ms) */
    vlc_int64_t length;         /* total length of the stream (in ms) */
} mediacontrol_StreamInformation;

/**************************************************************************
 *  Helper functions
 ***************************************************************************/

mediacontrol_RGBPicture *mediacontrol_RGBPicture__alloc( int datasize );

void mediacontrol_RGBPicture__free( mediacontrol_RGBPicture *pic );

mediacontrol_RGBPicture *
  _mediacontrol_createRGBPicture( int, int, long, vlc_int64_t l_date,
                                  char *, int);

mediacontrol_PlaylistSeq *mediacontrol_PlaylistSeq__alloc( int size );

void mediacontrol_PlaylistSeq__free( mediacontrol_PlaylistSeq *ps );

mediacontrol_Exception *
  mediacontrol_exception_init( mediacontrol_Exception *exception );

void mediacontrol_exception_free(mediacontrol_Exception *exception);

/*****************************************************************************
 * Core functions
 *****************************************************************************/
mediacontrol_Instance *
  mediacontrol_new( char **args, mediacontrol_Exception *exception );

mediacontrol_Instance *
  mediacontrol_new_from_object( int vlc_object_id,
                                mediacontrol_Exception *exception );

mediacontrol_Position *
  mediacontrol_get_media_position(
                         mediacontrol_Instance *self,
                         const mediacontrol_PositionOrigin an_origin,
                         const mediacontrol_PositionKey a_key,
                         mediacontrol_Exception *exception );

void mediacontrol_set_media_position( mediacontrol_Instance *self,
                                      const mediacontrol_Position *a_position,
                                      mediacontrol_Exception *exception );

void mediacontrol_start( mediacontrol_Instance *self,
                         const mediacontrol_Position *a_position,
                         mediacontrol_Exception *exception );

void mediacontrol_pause( mediacontrol_Instance *self,
                         const mediacontrol_Position *a_position, 
                         mediacontrol_Exception *exception );

void mediacontrol_resume( mediacontrol_Instance *self,
                          const mediacontrol_Position *a_position,
                          mediacontrol_Exception *exception );

void mediacontrol_stop( mediacontrol_Instance *self,
                        const mediacontrol_Position *a_position,
                        mediacontrol_Exception *exception );

void mediacontrol_exit( mediacontrol_Instance *self );

void mediacontrol_playlist_add_item( mediacontrol_Instance *self,
                                     const char* psz_file,
                                     mediacontrol_Exception *exception );
void mediacontrol_playlist_clear( mediacontrol_Instance *self,
                                  mediacontrol_Exception *exception );
mediacontrol_PlaylistSeq *
  mediacontrol_playlist_get_list( mediacontrol_Instance *self,
                                  mediacontrol_Exception *exception );


/*****************************************************************************
 * A/V functions
 *****************************************************************************/
mediacontrol_RGBPicture *
  mediacontrol_snapshot( mediacontrol_Instance *self,
                         const mediacontrol_Position *a_position,
                         mediacontrol_Exception *exception );

/* Return a NULL terminated list */
mediacontrol_RGBPicture **
  mediacontrol_all_snapshots( mediacontrol_Instance *self,
                              mediacontrol_Exception *exception );

/* Displays the message string, between "begin" and "end" positions */
void mediacontrol_display_text( mediacontrol_Instance *self,
                                const char *message, 
                                const mediacontrol_Position *begin, 
                                const mediacontrol_Position *end,
                                mediacontrol_Exception *exception );

mediacontrol_StreamInformation *
  mediacontrol_get_stream_information( mediacontrol_Instance *self,
                                       mediacontrol_PositionKey a_key,
                                       mediacontrol_Exception *exception );

unsigned short
  mediacontrol_sound_get_volume( mediacontrol_Instance *self,
                                 mediacontrol_Exception *exception );
void mediacontrol_sound_set_volume( mediacontrol_Instance *self,
                                    const unsigned short volume,
                                    mediacontrol_Exception *exception );

vlc_bool_t mediacontrol_set_visual( mediacontrol_Instance *self,
                                    WINDOWHANDLE visual_id,
                                    mediacontrol_Exception *exception );

# ifdef __cplusplus
}
# endif

#endif
