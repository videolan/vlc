/*****************************************************************************
 * variables.h: variables handling
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: variables.h,v 1.4 2002/10/16 19:39:42 sam Exp $
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

typedef struct callback_entry_t callback_entry_t;

/*****************************************************************************
 * vlc_value_t is the common union for variable values; variable_t is the
 * structure describing a variable. 
 *****************************************************************************/
struct variable_t
{
    /* The variable's exported value */
    vlc_value_t  val;

    /* The variable unique name, (almost) unique hashed value, and type */
    char *       psz_name;
    u32          i_hash;
    int          i_type;

    /* Lots of other things that can be added */
    int          i_usage;

    int                i_entries;
    callback_entry_t * p_entries;
};

/*****************************************************************************
 * Variable types - probably very incomplete
 *****************************************************************************/
#define VLC_VAR_BOOL      0x0100
#define VLC_VAR_INTEGER   0x0200
#define VLC_VAR_STRING    0x0300
#define VLC_VAR_MODULE    0x0301
#define VLC_VAR_FILE      0x0302
#define VLC_VAR_FLOAT     0x0400
#define VLC_VAR_TIME      0x0500
#define VLC_VAR_ADDRESS   0x0600
#define VLC_VAR_COMMAND   0x0700
#define VLC_VAR_MUTEX     0x0800

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( int, __var_Create, ( vlc_object_t *, const char *, int ) );
VLC_EXPORT( int, __var_Destroy, ( vlc_object_t *, const char * ) );

VLC_EXPORT( int, __var_Type, ( vlc_object_t *, const char * ) );
VLC_EXPORT( int, __var_Set, ( vlc_object_t *, const char *, vlc_value_t ) );
VLC_EXPORT( int, __var_Get, ( vlc_object_t *, const char *, vlc_value_t * ) );

#define var_Create(a,b,c) __var_Create( VLC_OBJECT(a), b, c )
#define var_Destroy(a,b) __var_Destroy( VLC_OBJECT(a), b )

#define var_Type(a,b) __var_Type( VLC_OBJECT(a), b )
#define var_Set(a,b,c) __var_Set( VLC_OBJECT(a), b, c )
#define var_Get(a,b,c) __var_Get( VLC_OBJECT(a), b, c )

/*****************************************************************************
 * Variable callbacks
 *****************************************************************************
 * int MyCallback( vlc_object_t *p_this,
 *                 char const *psz_variable,
 *                 vlc_value_t oldvalue,
 *                 vlc_value_t newvalue,
 *                 void *p_data);
 *****************************************************************************/
VLC_EXPORT( int, __var_AddCallback, ( vlc_object_t *, const char *, vlc_callback_t, void * ) );
VLC_EXPORT( int, __var_DelCallback, ( vlc_object_t *, const char *, vlc_callback_t, void * ) );

#define var_AddCallback(a,b,c,d) __var_AddCallback( VLC_OBJECT(a), b, c, d )
#define var_DelCallback(a,b,c,d) __var_DelCallback( VLC_OBJECT(a), b, c, d )

