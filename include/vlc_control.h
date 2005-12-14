/*****************************************************************************
 * control.h: private header for mediacontrol
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: vlc.h 10101 2005-03-02 16:47:31Z robux4 $
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

#ifndef _VLC_PRIVATE_CONTROL_H
#define _VLC_PRIVATE_CONTROL_H 1

# ifdef __cplusplus
extern "C" {
# endif

#include <vlc/vlc.h>
#include "vlc/control_structures.h"


typedef struct {
  vlc_object_t  *p_vlc;
  playlist_t    *p_playlist;
  intf_thread_t *p_intf;
  int           vlc_object_id;
} mediacontrol_Instance;

vlc_int64_t mediacontrol_unit_convert( input_thread_t *p_input,
                                       mediacontrol_PositionKey from,
                                       mediacontrol_PositionKey to,
                                       vlc_int64_t value );
vlc_int64_t mediacontrol_position2microsecond(
                                     input_thread_t *p_input,
                                     const mediacontrol_Position *pos );


# ifdef __cplusplus
}
# endif

#endif
