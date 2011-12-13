/*****************************************************************************
 * mediacontrol_internal.h: private header for mediacontrol
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

#ifndef _VLC_MEDIACONTROL_INTERNAL_H
#define _VLC_MEDIACONTROL_INTERNAL_H 1

# ifdef __cplusplus
extern "C" {
# endif

#include <vlc/vlc.h>
#include <vlc/mediacontrol_structures.h>
#include <vlc/libvlc_structures.h>
#include <vlc/libvlc.h>

struct mediacontrol_Instance {
    libvlc_instance_t * p_instance;
    libvlc_media_player_t * p_media_player;
};

libvlc_time_t private_mediacontrol_unit_convert( libvlc_media_player_t *p_media_player,
                                                 mediacontrol_PositionKey from,
                                                 mediacontrol_PositionKey to,
                                                 int64_t value );
libvlc_time_t private_mediacontrol_position2microsecond( libvlc_media_player_t *p_media_player,
                                                         const mediacontrol_Position *pos );

/**
 * Allocate a RGBPicture structure.
 * \param datasize: the size of the data
 */
mediacontrol_RGBPicture *private_mediacontrol_RGBPicture__alloc( int datasize );

mediacontrol_RGBPicture *private_mediacontrol_createRGBPicture( int, int, long, int64_t l_date, char *, int);


#define RAISE( c, m )  if( exception ) { exception->code = c;    \
                                         exception->message = strdup(m); }

#define RAISE_NULL( c, m ) do{ RAISE( c, m ); return NULL; } while(0)
#define RAISE_VOID( c, m ) do{ RAISE( c, m ); return;      } while(0)

#define HANDLE_LIBVLC_EXCEPTION_VOID( e )  if( libvlc_exception_raised( e ) ) {    \
    RAISE( mediacontrol_InternalException, libvlc_errmsg()); \
        libvlc_exception_clear( e ); \
        return; }

#define HANDLE_LIBVLC_EXCEPTION_NULL( e )  if( libvlc_exception_raised( e ) ) {     \
        RAISE( mediacontrol_InternalException, libvlc_errmsg()); \
        libvlc_exception_clear( e ); \
        return NULL; }

#define HANDLE_LIBVLC_EXCEPTION_ZERO( e )  if( libvlc_exception_raised( e ) ) { \
        RAISE( mediacontrol_InternalException, libvlc_errmsg()); \
        libvlc_exception_clear( e ); \
        return 0; }


# ifdef __cplusplus
}
# endif

#endif
