/*****************************************************************************
 * variables.h: variables handling
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: variables.h,v 1.7 2002/10/28 20:57:01 sam Exp $
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

    /* A pointer to a comparison function, a duplication function, and
     * a deallocation function */
    int      ( * pf_cmp ) ( vlc_value_t, vlc_value_t );
    void     ( * pf_dup ) ( vlc_value_t * );
    void     ( * pf_free ) ( vlc_value_t * );

    /* Creation count: we only destroy the variable if it reaches 0 */
    int          i_usage;

    /* If the variable has min/max/step values */
    vlc_value_t  min, max, step;

    /* If the variable is to be chosen in a list */
    int          i_default;
    int          i_choices;
    vlc_value_t *pp_choices;

    /* Set to TRUE if the variable is in a callback */
    vlc_bool_t   b_incallback;

    /* Registered callbacks */
    int                i_entries;
    callback_entry_t * p_entries;
};

/*****************************************************************************
 * Variable types - probably very incomplete
 *****************************************************************************/
#define VLC_VAR_TYPE      0x00ff
#define VLC_VAR_FLAGS     0xff00

/* Different types */
#define VLC_VAR_BOOL      0x0010
#define VLC_VAR_INTEGER   0x0020
#define VLC_VAR_STRING    0x0030
#define VLC_VAR_MODULE    0x0031
#define VLC_VAR_FILE      0x0032
#define VLC_VAR_FLOAT     0x0040
#define VLC_VAR_TIME      0x0050
#define VLC_VAR_ADDRESS   0x0060
#define VLC_VAR_COMMAND   0x0070
#define VLC_VAR_MUTEX     0x0080

/* Additive flags */
#define VLC_VAR_ISLIST    0x0100
#define VLC_VAR_HASMIN    0x0200
#define VLC_VAR_HASMAX    0x0400
#define VLC_VAR_HASSTEP   0x0800

/*****************************************************************************
 * Variable actions
 *****************************************************************************/
#define VLC_VAR_SETMIN        0x0010
#define VLC_VAR_SETMAX        0x0011
#define VLC_VAR_SETSTEP       0x0012

#define VLC_VAR_ADDCHOICE     0x0020
#define VLC_VAR_DELCHOICE     0x0021
#define VLC_VAR_SETDEFAULT    0x0022
#define VLC_VAR_GETLIST       0x0023
#define VLC_VAR_FREELIST      0x0024

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( int, __var_Create, ( vlc_object_t *, const char *, int ) );
VLC_EXPORT( int, __var_Destroy, ( vlc_object_t *, const char * ) );

VLC_EXPORT( int, __var_Change, ( vlc_object_t *, const char *, int, vlc_value_t * ) );

VLC_EXPORT( int, __var_Type, ( vlc_object_t *, const char * ) );
VLC_EXPORT( int, __var_Set, ( vlc_object_t *, const char *, vlc_value_t ) );
VLC_EXPORT( int, __var_Get, ( vlc_object_t *, const char *, vlc_value_t * ) );

#define var_Create(a,b,c) __var_Create( VLC_OBJECT(a), b, c )
#define var_Destroy(a,b) __var_Destroy( VLC_OBJECT(a), b )

#define var_Change(a,b,c,d) __var_Change( VLC_OBJECT(a), b, c, d )

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

