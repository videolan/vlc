/*****************************************************************************
 * vlc_objects.h: vlc_object_t definition.
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: vlc_objects.h,v 1.15 2003/01/27 17:41:01 ipkiss Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/* Object types */
#define VLC_OBJECT_ROOT       (-1)
#define VLC_OBJECT_VLC        (-2)
#define VLC_OBJECT_MODULE     (-3)
#define VLC_OBJECT_INTF       (-4)
#define VLC_OBJECT_PLAYLIST   (-5)
#define VLC_OBJECT_ITEM       (-6)
#define VLC_OBJECT_INPUT      (-7)
#define VLC_OBJECT_DECODER    (-8)
#define VLC_OBJECT_VOUT       (-9)
#define VLC_OBJECT_AOUT      (-10)
#define VLC_OBJECT_SOUT      (-11)
#define VLC_OBJECT_GENERIC  (-666)

/* Object search mode */
#define FIND_PARENT         0x0001
#define FIND_CHILD          0x0002
#define FIND_ANYWHERE       0x0003

#define FIND_STRICT         0x0010

/*****************************************************************************
 * The vlc_object_t type. Yes, it's that simple :-)
 *****************************************************************************/
struct vlc_object_t
{
    VLC_COMMON_MEMBERS
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( void *, __vlc_object_create, ( vlc_object_t *, int ) );
VLC_EXPORT( void, __vlc_object_destroy, ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_attach, ( vlc_object_t *, vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_detach, ( vlc_object_t * ) );
VLC_EXPORT( void *, __vlc_object_get, ( vlc_object_t *, int ) );
VLC_EXPORT( void *, __vlc_object_find, ( vlc_object_t *, int, int ) );
VLC_EXPORT( void, __vlc_object_yield, ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_release, ( vlc_object_t * ) );
VLC_EXPORT( vlc_list_t *, __vlc_list_find, ( vlc_object_t *, int, int ) );
VLC_EXPORT( void, vlc_list_release, ( vlc_list_t * ) );

#define vlc_object_create(a,b) \
    __vlc_object_create( VLC_OBJECT(a), b )

#define vlc_object_destroy(a) do { \
    __vlc_object_destroy( VLC_OBJECT(a) ); \
    (a) = NULL; } while(0)

#define vlc_object_detach(a) \
    __vlc_object_detach( VLC_OBJECT(a) )

#define vlc_object_attach(a,b) \
    __vlc_object_attach( VLC_OBJECT(a), VLC_OBJECT(b) )

#define vlc_object_get(a,b) \
    __vlc_object_get( VLC_OBJECT(a),b)

#define vlc_object_find(a,b,c) \
    __vlc_object_find( VLC_OBJECT(a),b,c)

#define vlc_object_yield(a) \
    __vlc_object_yield( VLC_OBJECT(a) )

#define vlc_object_release(a) \
    __vlc_object_release( VLC_OBJECT(a) )

#define vlc_list_find(a,b,c) \
    __vlc_list_find( VLC_OBJECT(a),b,c)

