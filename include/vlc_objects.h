/*****************************************************************************
 * vlc_objects.h: vlc_object_t definition.
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: vlc_objects.h,v 1.7 2002/08/12 22:12:50 massiot Exp $
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
#define VLC_OBJECT_SOUT       (-10)
#define VLC_OBJECT_GENERIC  (-666)

/* Object search mode */
#define FIND_PARENT         0x0001
#define FIND_CHILD          0x0002
#define FIND_ANYWHERE       0x0003

#define FIND_STRICT         0x0010

/* Object cast */


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( void *, __vlc_object_create, ( vlc_object_t *, int ) );
VLC_EXPORT( void, __vlc_object_destroy, ( vlc_object_t * ) );
VLC_EXPORT( void *, __vlc_object_find, ( vlc_object_t *, int, int ) );
VLC_EXPORT( void, __vlc_object_yield, ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_release, ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_detach, ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_attach, ( vlc_object_t *, vlc_object_t * ) );
#if 0
//VLC_EXPORT( void, __vlc_object_setchild, ( vlc_object_t *, vlc_object_t * ) );
#endif

VLC_EXPORT( void, __vlc_dumpstructure, ( vlc_object_t * ) );

#define vlc_object_create(a,b) \
    __vlc_object_create( VLC_OBJECT(a), b )

#define vlc_object_destroy(a) do { \
    __vlc_object_destroy( VLC_OBJECT(a) ); \
    (a) = NULL; } while(0)

#define vlc_object_find(a,b,c) \
    __vlc_object_find( VLC_OBJECT(a),b,c)

#define vlc_object_yield(a) \
    __vlc_object_yield( VLC_OBJECT(a) )

#define vlc_object_release(a) \
    __vlc_object_release( VLC_OBJECT(a) )

#define vlc_object_detach(a) \
    __vlc_object_detach( VLC_OBJECT(a) )

#define vlc_object_attach(a,b) \
    __vlc_object_attach( VLC_OBJECT(a), VLC_OBJECT(b) )

#if 0
#define vlc_object_setchild(a,b) \
    __vlc_object_setchild( VLC_OBJECT(a), VLC_OBJECT(b) )
#endif

#define vlc_dumpstructure(a) \
    __vlc_dumpstructure( VLC_OBJECT(a) )

