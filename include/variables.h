/*****************************************************************************
 * variables.h: variables handling
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: variables.h,v 1.1 2002/10/11 11:05:52 sam Exp $
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

/* Variable types - probably very incomplete */
#define VLC_VAR_BOOL      0x0100
#define VLC_VAR_INTEGER   0x0200
#define VLC_VAR_STRING    0x0300
#define VLC_VAR_MODULE    0x0301
#define VLC_VAR_FILE      0x0302
#define VLC_VAR_FLOAT     0x0400
#define VLC_VAR_TIME      0x0500
#define VLC_VAR_ADDRESS   0x0600

/*****************************************************************************
 * vlc_value_t is the common union for variable values; variable_t is the
 * structure describing a variable. 
 *****************************************************************************/
struct variable_t
{
    u32          i_hash;
    char *       psz_name;

    int          i_type;
    vlc_value_t  val;

    /* Lots of other things that can be added */
    vlc_bool_t   b_set;
    vlc_bool_t   b_active;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( void, __var_Create, ( vlc_object_t *, const char *, int ) );
VLC_EXPORT( void, __var_Destroy, ( vlc_object_t *, const char * ) );

VLC_EXPORT( int, __var_Set, ( vlc_object_t *, const char *, vlc_value_t ) );
VLC_EXPORT( int, __var_Get, ( vlc_object_t *, const char *, vlc_value_t * ) );

#define var_Create(a,b,c) \
    __var_Create( VLC_OBJECT(a), b, c )

#define var_Destroy(a,b) \
    __var_Destroy( VLC_OBJECT(a), b )

#define var_Set(a,b,c) \
    __var_Set( VLC_OBJECT(a), b, c )

#define var_Get(a,b,c) \
    __var_Get( VLC_OBJECT(a), b, c )

