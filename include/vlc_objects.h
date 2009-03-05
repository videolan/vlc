/*****************************************************************************
 * vlc_objects.h: vlc_object_t definition and manipulation methods
 *****************************************************************************
 * Copyright (C) 2002-2008 the VideoLAN team
 * $Id$
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * \file
 * This file defines the vlc_object_t structure and object types.
 */

/**
 * \defgroup vlc_object Objects
 * @{
 */

/* Object types */
#define VLC_OBJECT_INPUT       (-7)
#define VLC_OBJECT_DECODER     (-8)
#define VLC_OBJECT_VOUT        (-9)
#define VLC_OBJECT_AOUT        (-10)
/* Please add new object types below -34 */
/* Please do not add new object types anyway */
#define VLC_OBJECT_GENERIC     (-666)

/* Object search mode */
#define FIND_PARENT         0x0001
#define FIND_CHILD          0x0002
#define FIND_ANYWHERE       0x0003

#define FIND_STRICT         0x0010

/* Object flags */
#define OBJECT_FLAGS_NODBG       0x0001
#define OBJECT_FLAGS_QUIET       0x0002
#define OBJECT_FLAGS_NOINTERACT  0x0004

/* Types */
typedef void (*vlc_destructor_t)(struct vlc_object_t *);

/*****************************************************************************
 * The vlc_object_t type. Yes, it's that simple :-)
 *****************************************************************************/
/** The main vlc_object_t structure */
struct vlc_object_t
{
    VLC_COMMON_MEMBERS
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( void *, __vlc_object_create, ( vlc_object_t *, int ) );
VLC_EXPORT( void, __vlc_object_set_destructor, ( vlc_object_t *, vlc_destructor_t ) );
VLC_EXPORT( void, __vlc_object_attach, ( vlc_object_t *, vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_detach, ( vlc_object_t * ) );
#if defined (__GNUC__) && !defined __cplusplus
__attribute__((deprecated))
#endif
VLC_EXPORT( void *, __vlc_object_find, ( vlc_object_t *, int, int ) );
VLC_EXPORT( vlc_object_t *, vlc_object_find_name, ( vlc_object_t *, const char *, int ) );
VLC_EXPORT( void *, __vlc_object_hold, ( vlc_object_t * ) );
VLC_EXPORT( void, __vlc_object_release, ( vlc_object_t * ) );
VLC_EXPORT( vlc_list_t *, __vlc_list_children, ( vlc_object_t * ) );
VLC_EXPORT( void, vlc_list_release, ( vlc_list_t * ) );

/*}@*/

#define vlc_object_create(a,b) \
    __vlc_object_create( VLC_OBJECT(a), b )

#define vlc_object_set_destructor(a,b) \
    __vlc_object_set_destructor( VLC_OBJECT(a), b )

#define vlc_object_detach(a) \
    __vlc_object_detach( VLC_OBJECT(a) )

#define vlc_object_attach(a,b) \
    __vlc_object_attach( VLC_OBJECT(a), VLC_OBJECT(b) )

#define vlc_object_find(a,b,c) \
    __vlc_object_find( VLC_OBJECT(a),b,c)

#define vlc_object_find_name(a,b,c) \
    vlc_object_find_name( VLC_OBJECT(a),b,c)

#define vlc_object_hold(a) \
    __vlc_object_hold( VLC_OBJECT(a) )

#define vlc_object_release(a) \
    __vlc_object_release( VLC_OBJECT(a) )

#define vlc_list_children(a) \
    __vlc_list_children( VLC_OBJECT(a) )

/* Objects and threading */
VLC_EXPORT( void, __vlc_object_kill, ( vlc_object_t * ) );
#define vlc_object_kill(a) \
    __vlc_object_kill( VLC_OBJECT(a) )

static inline bool vlc_object_alive (const vlc_object_t *obj)
{
    barrier ();
    return !obj->b_die;
}

#define vlc_object_alive(a) vlc_object_alive( VLC_OBJECT(a) )
