/*****************************************************************************
 * variables.h: variables handling
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
 * \defgroup variables Variables
 *
 * Functions for using the object variables in vlc.
 *
 * Vlc have a very powerful "object variable" infrastructure useful
 * for many things.
 *
 * @{
 */

typedef struct callback_entry_t callback_entry_t;

/**
 * The structure describing a variable.
 * \note vlc_value_t is the common union for variable values
 */
struct variable_t
{
    /** The variable's exported value */
    vlc_value_t  val;

    char *       psz_name; /**< The variable unique name */
    uint32_t     i_hash;   /**< (almost) unique hashed value */
    int          i_type;   /**< The type of the variable */

    /** The variable display name, mainly for use by the interfaces */
    char *       psz_text;

    /** A pointer to a comparison function */
    int      ( * pf_cmp ) ( vlc_value_t, vlc_value_t );
    /** A pointer to a duplication function */
    void     ( * pf_dup ) ( vlc_value_t * );
    /** A pointer to a deallocation function */
    void     ( * pf_free ) ( vlc_value_t * );

    /** Creation count: we only destroy the variable if it reaches 0 */
    int          i_usage;

    /** If the variable has min/max/step values */
    vlc_value_t  min, max, step;

    /** Index of the default choice, if the variable is to be chosen in
     * a list */
    int          i_default;
    /** List of choices */
    vlc_list_t   choices;
    /** List of friendly names for the choices */
    vlc_list_t   choices_text;

    /** Set to TRUE if the variable is in a callback */
    vlc_bool_t   b_incallback;

    /** Number of registered callbacks */
    int                i_entries;
    /** Array of registered callbacks */
    callback_entry_t * p_entries;
};

/*****************************************************************************
 * Variable types - probably very incomplete
 *****************************************************************************/
#define VLC_VAR_TYPE      0x00ff
#define VLC_VAR_FLAGS     0xff00

/** \defgroup var_flags Additive flags
 * These flags are added to the type field of the variable. Most as a result of
 * a __var_Change() call, but some may be added at creation time
 * @{
 */
#define VLC_VAR_HASCHOICE 0x0100
#define VLC_VAR_HASMIN    0x0200
#define VLC_VAR_HASMAX    0x0400
#define VLC_VAR_HASSTEP   0x0800

#define VLC_VAR_ISLIST    0x1000
#define VLC_VAR_ISCOMMAND 0x2000
#define VLC_VAR_ISCONFIG  0x2000

/** Creation flag */
#define VLC_VAR_DOINHERIT 0x8000
/**@}*/

/**
 * \defgroup var_action Variable actions
 * These are the different actions that can be used with __var_Change().
 * The parameters given are the meaning of the two last parameters of
 * __var_Change() when this action is being used.
 * @{
 */

/**
 * Set the minimum value of this variable
 * \param p_val The new minimum value
 * \param p_val2 Unused
 */
#define VLC_VAR_SETMIN              0x0010
/**
 * Set the maximum value of this variable
 * \param p_val The new maximum value
 * \param p_val2 Unused
 */
#define VLC_VAR_SETMAX              0x0011
#define VLC_VAR_SETSTEP             0x0012

/**
 * Set the value of this variable without triggering any callbacks
 * \param p_val The new value
 * \param p_val2 Unused
 */
#define VLC_VAR_SETVALUE            0x0013

#define VLC_VAR_SETTEXT             0x0014
#define VLC_VAR_GETTEXT             0x0015

#define VLC_VAR_ADDCHOICE           0x0020
#define VLC_VAR_DELCHOICE           0x0021
#define VLC_VAR_CLEARCHOICES        0x0022
#define VLC_VAR_SETDEFAULT          0x0023
#define VLC_VAR_GETCHOICES          0x0024
#define VLC_VAR_FREECHOICES         0x0025
#define VLC_VAR_GETLIST             0x0026
#define VLC_VAR_FREELIST            0x0027
#define VLC_VAR_CHOICESCOUNT        0x0028

#define VLC_VAR_INHERITVALUE        0x0030
#define VLC_VAR_TRIGGER_CALLBACKS   0x0035
/**@}*/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
VLC_EXPORT( int, __var_Create, ( vlc_object_t *, const char *, int ) );
VLC_EXPORT( int, __var_Destroy, ( vlc_object_t *, const char * ) );

VLC_EXPORT( int, __var_Change, ( vlc_object_t *, const char *, int, vlc_value_t *, vlc_value_t * ) );

VLC_EXPORT( int, __var_Type, ( vlc_object_t *, const char * ) );
VLC_EXPORT( int, __var_Set, ( vlc_object_t *, const char *, vlc_value_t ) );
VLC_EXPORT( int, __var_Get, ( vlc_object_t *, const char *, vlc_value_t * ) );

#define var_OptionParse(a,b) __var_OptionParse( VLC_OBJECT( a ) , b )
VLC_EXPORT( void, __var_OptionParse, ( vlc_object_t *, const char * ) );

/**
 * __var_Create() with automatic casting.
 */
#define var_Create(a,b,c) __var_Create( VLC_OBJECT(a), b, c )
/**
 * __var_Destroy() with automatic casting
 */
#define var_Destroy(a,b) __var_Destroy( VLC_OBJECT(a), b )

/**
 * __var_Change() with automatic casting
 */
#define var_Change(a,b,c,d,e) __var_Change( VLC_OBJECT(a), b, c, d, e )

/**
 * __var_Type() with automatic casting
 */
#define var_Type(a,b) __var_Type( VLC_OBJECT(a), b )
/**
 * __var_Set() with automatic casting
 */
#define var_Set(a,b,c) __var_Set( VLC_OBJECT(a), b, c )
/**
 * __var_Get() with automatic casting
 */
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

/**
 * __var_AddCallback() with automatic casting
 */
#define var_AddCallback(a,b,c,d) __var_AddCallback( VLC_OBJECT(a), b, c, d )

/**
 * __var_DelCallback() with automatic casting
 */
#define var_DelCallback(a,b,c,d) __var_DelCallback( VLC_OBJECT(a), b, c, d )

/*****************************************************************************
 * helpers functions
 *****************************************************************************/

/**
 * Set the value of an integer variable
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 * \param i The new integer value of this variable
 */
static inline int __var_SetInteger( vlc_object_t *p_obj, const char *psz_name, int i )
{
    vlc_value_t val;
    val.i_int = i;
    return __var_Set( p_obj, psz_name, val );
}

/**
 * Set the value of an boolean variable
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 * \param b The new boolean value of this variable
 */
static inline int __var_SetBool( vlc_object_t *p_obj, const char *psz_name, vlc_bool_t b )
{
    vlc_value_t val;
    val.b_bool = b;
    return __var_Set( p_obj, psz_name, val );
}

/**
 * Set the value of a time variable
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 * \param i The new time value of this variable
 */
static inline int __var_SetTime( vlc_object_t *p_obj, const char *psz_name, int64_t i )
{
    vlc_value_t val;
    val.i_time = i;
    return __var_Set( p_obj, psz_name, val );
}

/**
 * Set the value of a float variable
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 * \param f The new float value of this variable
 */
static inline int __var_SetFloat( vlc_object_t *p_obj, const char *psz_name, float f )
{
    vlc_value_t val;
    val.f_float = f;
    return __var_Set( p_obj, psz_name, val );
}

/**
 * Set the value of a string variable
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 * \param psz_string The new string value of this variable
 */
static inline int __var_SetString( vlc_object_t *p_obj, const char *psz_name, char *psz_string )
{
    vlc_value_t val;
    val.psz_string = psz_string;
    return __var_Set( p_obj, psz_name, val );
}

/**
 * Trigger the callbacks on a void variable
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline int __var_SetVoid( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;
    val.b_bool = VLC_TRUE;
    return __var_Set( p_obj, psz_name, val );
}

/**
 * __var_SetInteger() with automatic casting
 */
#define var_SetInteger(a,b,c)   __var_SetInteger( VLC_OBJECT(a),b,c)
/**
 * __var_SetBool() with automatic casting
 */
#define var_SetBool(a,b,c)   __var_SetBool( VLC_OBJECT(a),b,c)

/**
 * __var_SetTime() with automatic casting
 */
#define var_SetTime(a,b,c)      __var_SetTime( VLC_OBJECT(a),b,c)
/**
 * __var_SetFloat() with automatic casting
 */
#define var_SetFloat(a,b,c)     __var_SetFloat( VLC_OBJECT(a),b,c)
/**
 * __var_SetString() with automatic casting
 */
#define var_SetString(a,b,c)     __var_SetString( VLC_OBJECT(a),b,c)
/**
 * __var_SetVoid() with automatic casting
 */
#define var_SetVoid(a,b)        __var_SetVoid( VLC_OBJECT(a),b)

/**
 * Get a integer value
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline int __var_GetInteger( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;val.i_int = 0;
    if( !__var_Get( p_obj, psz_name, &val ) )
        return val.i_int;
    else
        return 0;
}

/**
 * Get a boolean value
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline int __var_GetBool( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val; val.b_bool = VLC_FALSE;
    if( !__var_Get( p_obj, psz_name, &val ) )
        return val.b_bool;
    else
        return VLC_FALSE;
}

/**
 * Get a time value
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline int64_t __var_GetTime( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val; val.i_time = 0L;
    if( !__var_Get( p_obj, psz_name, &val ) )
        return val.i_time;
    else
        return 0;
}

/**
 * Get a float value
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline float __var_GetFloat( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val; val.f_float = 0.0;
    if( !__var_Get( p_obj, psz_name, &val ) )
        return val.f_float;
    else
        return 0.0;
}

/**
 * Get a string value
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline char *__var_GetString( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val; val.psz_string = NULL;
    if( !__var_Get( p_obj, psz_name, &val ) )
        return val.psz_string;
    else
        return strdup( "" );
}

/**
 * __var_GetInteger() with automatic casting
 */
#define var_GetInteger(a,b)   __var_GetInteger( VLC_OBJECT(a),b)
/**
 * __var_GetBool() with automatic casting
 */
#define var_GetBool(a,b)   __var_GetBool( VLC_OBJECT(a),b)
/**
 * __var_GetTime() with automatic casting
 */
#define var_GetTime(a,b)   __var_GetTime( VLC_OBJECT(a),b)
/**
 * __var_GetFloat() with automatic casting
 */
#define var_GetFloat(a,b)   __var_GetFloat( VLC_OBJECT(a),b)
/**
 * __var_GetString() with automatic casting
 */
#define var_GetString(a,b)   __var_GetString( VLC_OBJECT(a),b)


/**
 * Create a integer variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline int __var_CreateGetInteger( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;

    __var_Create( p_obj, psz_name, VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    if( !__var_Get( p_obj, psz_name, &val ) )
        return val.i_int;
    else
        return 0;
}

/**
 * Create a boolean variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline int __var_CreateGetBool( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;

    __var_Create( p_obj, psz_name, VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    if( !__var_Get( p_obj, psz_name, &val ) )
        return val.b_bool;
    else
        return VLC_FALSE;
}

/**
 * Create a time variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline int64_t __var_CreateGetTime( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;

    __var_Create( p_obj, psz_name, VLC_VAR_TIME | VLC_VAR_DOINHERIT );
    if( !__var_Get( p_obj, psz_name, &val ) )
        return val.i_time;
    else
        return 0;
}

/**
 * Create a float variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline float __var_CreateGetFloat( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;

    __var_Create( p_obj, psz_name, VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    if( !__var_Get( p_obj, psz_name, &val ) )
        return val.f_float;
    else
        return 0.0;
}

/**
 * Create a string variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
static inline char *__var_CreateGetString( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;

    __var_Create( p_obj, psz_name, VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    if( !__var_Get( p_obj, psz_name, &val ) )
        return val.psz_string;
    else
        return strdup( "" );
}

/**
 * __var_CreateGetInteger() with automatic casting
 */
#define var_CreateGetInteger(a,b)   __var_CreateGetInteger( VLC_OBJECT(a),b)
/**
 * __var_CreateGetBool() with automatic casting
 */
#define var_CreateGetBool(a,b)   __var_CreateGetBool( VLC_OBJECT(a),b)
/**
 * __var_CreateGetTime() with automatic casting
 */
#define var_CreateGetTime(a,b)   __var_CreateGetTime( VLC_OBJECT(a),b)
/**
 * __var_CreateGetFloat() with automatic casting
 */
#define var_CreateGetFloat(a,b)   __var_CreateGetFloat( VLC_OBJECT(a),b)
/**
 * __var_CreateGetString() with automatic casting
 */
#define var_CreateGetString(a,b)   __var_CreateGetString( VLC_OBJECT(a),b)

/**
 * @}
 */

