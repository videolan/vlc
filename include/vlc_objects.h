/*****************************************************************************
 * vlc_objects.h: vlc_object_t definition.
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: vlc_objects.h,v 1.4 2002/06/07 14:59:40 sam Exp $
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
#define VLC_OBJECT_MODULE     (-2)
#define VLC_OBJECT_INTF       (-3)
#define VLC_OBJECT_PLAYLIST   (-4)
#define VLC_OBJECT_ITEM       (-5)
#define VLC_OBJECT_INPUT      (-6)
#define VLC_OBJECT_DECODER    (-7)
#define VLC_OBJECT_VOUT       (-8)
#define VLC_OBJECT_AOUT       (-9)
#define VLC_OBJECT_PRIVATE  (-666)

/* Object search mode */
#define FIND_PARENT         0x0001
#define FIND_CHILD          0x0002
#define FIND_ANYWHERE       0x0003

#define FIND_STRICT         0x0010

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( void *, __vlc_object_create, ( vlc_object_t *, int ) );
VLC_EXPORT( void, __vlc_object_destroy, ( vlc_object_t * ) );
VLC_EXPORT( void *, __vlc_object_find, ( vlc_object_t *, int, int ) );
VLC_EXPORT( void, __vlc_object_yield, ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_release, ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_detach, ( vlc_object_t *, vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_detach_all, ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_attach, ( vlc_object_t *, vlc_object_t * ) );
#if 0
//VLC_EXPORT( void, __vlc_object_setchild, ( vlc_object_t *, vlc_object_t * ) );
#endif

VLC_EXPORT( void, __vlc_dumpstructure, ( vlc_object_t * ) );

#define vlc_object_create(a,b) \
    __vlc_object_create( CAST_TO_VLC_OBJECT(a), b )

#define vlc_object_destroy(a) do { \
    __vlc_object_destroy( CAST_TO_VLC_OBJECT(a) ); \
    (a) = NULL; } while(0)

#define vlc_object_find(a,b,c) \
    __vlc_object_find( CAST_TO_VLC_OBJECT(a),b,c)

#define vlc_object_yield(a) \
    __vlc_object_yield( CAST_TO_VLC_OBJECT(a) )

#define vlc_object_release(a) \
    __vlc_object_release( CAST_TO_VLC_OBJECT(a) )

#define vlc_object_detach(a,b) \
    __vlc_object_detach( CAST_TO_VLC_OBJECT(a), CAST_TO_VLC_OBJECT(b) )

#define vlc_object_detach_all(a) \
    __vlc_object_detach_all( CAST_TO_VLC_OBJECT(a) )

#define vlc_object_attach(a,b) \
    __vlc_object_attach( CAST_TO_VLC_OBJECT(a), CAST_TO_VLC_OBJECT(b) )

#if 0
#define vlc_object_setchild(a,b) \
    __vlc_object_setchild( CAST_TO_VLC_OBJECT(a), CAST_TO_VLC_OBJECT(b) )
#endif

#define vlc_dumpstructure(a) \
    __vlc_dumpstructure( CAST_TO_VLC_OBJECT(a) )

