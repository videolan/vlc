/*****************************************************************************
 * vlc_variables.h: variables handling
 *****************************************************************************
 * Copyright (C) 2002-2004 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_VARIABLES_H
#define VLC_VARIABLES_H 1

/**
 * \defgroup variables Variables
 * \ingroup vlc_object
 *
 * VLC object variables and callbacks
 *
 * @{
 * \file
 * VLC object variables and callbacks interface
 */

#define VLC_VAR_TYPE      0x00ff
#define VLC_VAR_CLASS     0x00f0
#define VLC_VAR_FLAGS     0xff00

/**
 * \defgroup var_type Variable types
 * These are the different types a vlc variable can have.
 * @{
 */
#define VLC_VAR_VOID      0x0010
#define VLC_VAR_BOOL      0x0020
#define VLC_VAR_INTEGER   0x0030
#define VLC_VAR_STRING    0x0040
#define VLC_VAR_FLOAT     0x0050
#define VLC_VAR_ADDRESS   0x0070
#define VLC_VAR_COORDS    0x00A0
/**@}*/

/** \defgroup var_flags Additive flags
 * These flags are added to the type field of the variable. Most as a result of
 * a var_Change() call, but some may be added at creation time
 * @{
 */
#define VLC_VAR_HASCHOICE 0x0100

#define VLC_VAR_ISCOMMAND 0x2000

/** Creation flag */
/* If the variable is not found on the current module
   search all parents and finally module config until found */
#define VLC_VAR_DOINHERIT 0x8000
/**@}*/

/**
 * \defgroup var_action Variable actions
 * These are the different actions that can be used with var_Change().
 * The parameters given are the meaning of the two last parameters of
 * var_Change() when this action is being used.
 * @{
 */

#define VLC_VAR_SETSTEP             0x0012

/**
 * Set the value of this variable without triggering any callbacks
 * \param p_val The new value
 * \param p_val2 Unused
 */
#define VLC_VAR_SETVALUE            0x0013

#define VLC_VAR_SETTEXT             0x0014
#define VLC_VAR_GETTEXT             0x0015

#define VLC_VAR_GETMIN              0x0016
#define VLC_VAR_GETMAX              0x0017
#define VLC_VAR_GETSTEP             0x0018

#define VLC_VAR_ADDCHOICE           0x0020
#define VLC_VAR_DELCHOICE           0x0021
#define VLC_VAR_CLEARCHOICES        0x0022
#define VLC_VAR_GETCHOICES          0x0024

#define VLC_VAR_CHOICESCOUNT        0x0026
#define VLC_VAR_SETMINMAX           0x0027

/**@}*/

/**
 * Variable actions.
 *
 * These are the different actions that can be used with var_GetAndSet().
 */
enum vlc_var_atomic_op {
    VLC_VAR_BOOL_TOGGLE, /**< Invert a boolean value (param ignored) */
    VLC_VAR_INTEGER_ADD, /**< Add parameter to an integer value */
    VLC_VAR_INTEGER_OR,  /**< Binary OR over an integer bits field */
    VLC_VAR_INTEGER_NAND,/**< Binary NAND over an integer bits field */
};

/**
 * Creates a VLC object variable.
 *
 * This function creates a named variable within a VLC object.
 * If a variable already exists with the same name within the same object, its
 * reference count is incremented instead.
 *
 * \param obj Object to hold the variable
 * \param name Variable name
 * \param type Variable type. Must be one of \ref var_type combined with
 *               zero or more \ref var_flags
 */
VLC_API int var_Create(vlc_object_t *obj, const char *name, int type);

/**
 * Destroys a VLC object variable.
 *
 * This function decrements the reference count of a named variable within a
 * VLC object. If the reference count reaches zero, the variable is destroyed.
 *
 * \param obj Object holding the variable
 * \param name Variable name
 */
VLC_API void var_Destroy(vlc_object_t *obj, const char *name);

/**
 * Performs a special action on a variable.
 *
 * \param obj Object holding the variable
 * \param name Variable name
 * \param action Action to perform. Must be one of \ref var_action
 */
VLC_API int var_Change(vlc_object_t *obj, const char *name, int action, ...);

/**
 * Get the type of a variable.
 *
 * \see var_type
 *
 * \return The variable type if it exists
 *         or 0 if the variable could not be found.
 */
VLC_API int var_Type(vlc_object_t *obj, const char *name) VLC_USED;

/**
 * Sets a variable value.
 *
 * \param obj Object holding the variable
 * \param name Variable name
 * \param val Variable value to set
 */
VLC_API int var_Set(vlc_object_t *obj, const char *name, vlc_value_t val);

/**
 * Gets a variable value.
 *
 * \param obj Object holding the variable
 * \param name Variable name
 * \param valp Pointer to a \ref vlc_value_t object to hold the value [OUT]
 */
VLC_API int var_Get(vlc_object_t *obj, const char *name, vlc_value_t *valp);

VLC_API int var_SetChecked( vlc_object_t *, const char *, int, vlc_value_t );
VLC_API int var_GetChecked( vlc_object_t *, const char *, int, vlc_value_t * );

/**
 * Perform an atomic read-modify-write of a variable.
 *
 * \param obj object holding the variable
 * \param name variable name
 * \param op read-modify-write operation to perform
 *           (see \ref vlc_var_atomic_op)
 * \param value value of the variable after the modification
 * \retval VLC_SUCCESS Operation successful
 * \retval VLC_ENOENT Variable not found
 *
 * \bug The modified value is returned rather than the original value.
 * As such, the original value cannot be known in the case of non-reversible
 * operation such as \ref VLC_VAR_INTEGER_OR and \ref VLC_VAR_INTEGER_NAND.
 */
VLC_API int var_GetAndSet(vlc_object_t *obj, const char *name, int op,
                          vlc_value_t *value);

/**
 * Finds the value of a variable.
 *
 * If the specified object does not hold a variable with the specified name,
 * try the parent object, and iterate until the top of the objects tree. If no
 * match is found, the value is read from the configuration.
 */
VLC_API int var_Inherit( vlc_object_t *, const char *, int, vlc_value_t * );


/*****************************************************************************
 * Variable callbacks
 *****************************************************************************
 * int MyCallback( vlc_object_t *p_this,
 *                 char const *psz_variable,
 *                 vlc_value_t oldvalue,
 *                 vlc_value_t newvalue,
 *                 void *p_data);
 *****************************************************************************/

/**
 * Registers a callback for a variable.
 *
 * We store a function pointer that will be called upon variable
 * modification.
 *
 * \param obj Object holding the variable
 * \param name Variable name
 * \param callback Callback function pointer
 * \param opaque Opaque data pointer for use by the callback.
 *
 * \warning The callback function is run in the thread that calls var_Set() on
 *          the variable. Use proper locking. This thread may not have much
 *          time to spare, so keep callback functions short.
 *
 * \bug It is not possible to atomically retrieve the current value and
 * register a callback. As a consequence, extreme care must be taken to ensure
 * that the variable value cannot change before the callback is registered.
 * Failure to do so will result in intractable race conditions.
 */
VLC_API void var_AddCallback(vlc_object_t *obj, const char *name,
                             vlc_callback_t callback, void *opaque);

/**
 * Deregisters a callback from a variable.
 *
 * The callback and opaque pointer must be supplied again, as the same callback
 * function might have been registered more than once.
 */
VLC_API void var_DelCallback(vlc_object_t *obj, const char *name,
                             vlc_callback_t callback, void *opaque);

/**
 * Triggers callbacks on a variable.
 *
 * This triggers any callbacks registered on the named variable without
 * actually modifying the variable value. This is primarily useful for
 * variables with \ref VLC_VAR_VOID type (which do not have a value).
 *
 * \param obj Object holding the variable
 * \param name Variable name
 */
VLC_API void var_TriggerCallback(vlc_object_t *obj, const char *name);

/**
 * Register a callback for a list variable
 *
 * The callback is triggered when an element is added/removed from the
 * list or when the list is cleared.
 *
 * See var_AddCallback().
 */
VLC_API void var_AddListCallback( vlc_object_t *, const char *, vlc_list_callback_t, void * );

/**
 * Remove a callback from a list variable
 *
 * See var_DelCallback().
 */
VLC_API void var_DelListCallback( vlc_object_t *, const char *, vlc_list_callback_t, void * );

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
static inline int var_SetInteger( vlc_object_t *p_obj, const char *psz_name,
                                  int64_t i )
{
    vlc_value_t val;
    val.i_int = i;
    return var_SetChecked( p_obj, psz_name, VLC_VAR_INTEGER, val );
}

/**
 * Set the value of an boolean variable
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 * \param b The new boolean value of this variable
 */
static inline int var_SetBool( vlc_object_t *p_obj, const char *psz_name, bool b )
{
    vlc_value_t val;
    val.b_bool = b;
    return var_SetChecked( p_obj, psz_name, VLC_VAR_BOOL, val );
}

static inline int var_SetCoords( vlc_object_t *obj, const char *name,
                                 int32_t x, int32_t y )
{
    vlc_value_t val;
    val.coords.x = x;
    val.coords.y = y;
    return var_SetChecked (obj, name, VLC_VAR_COORDS, val);
}

/**
 * Set the value of a float variable
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 * \param f The new float value of this variable
 */
static inline int var_SetFloat( vlc_object_t *p_obj, const char *psz_name, float f )
{
    vlc_value_t val;
    val.f_float = f;
    return var_SetChecked( p_obj, psz_name, VLC_VAR_FLOAT, val );
}

/**
 * Set the value of a string variable
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 * \param psz_string The new string value of this variable
 */
static inline int var_SetString( vlc_object_t *p_obj, const char *psz_name, const char *psz_string )
{
    vlc_value_t val;
    val.psz_string = (char *)psz_string;
    return var_SetChecked( p_obj, psz_name, VLC_VAR_STRING, val );
}

/**
 * Set the value of a pointer variable
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 * \param ptr The new pointer value of this variable
 */
static inline
int var_SetAddress( vlc_object_t *p_obj, const char *psz_name, void *ptr )
{
    vlc_value_t val;
    val.p_address = ptr;
    return var_SetChecked( p_obj, psz_name, VLC_VAR_ADDRESS, val );
}

/**
 * Get an integer value
*
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED
static inline int64_t var_GetInteger( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;
    if( !var_GetChecked( p_obj, psz_name, VLC_VAR_INTEGER, &val ) )
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
VLC_USED
static inline bool var_GetBool( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val; val.b_bool = false;

    if( !var_GetChecked( p_obj, psz_name, VLC_VAR_BOOL, &val ) )
        return val.b_bool;
    else
        return false;
}

static inline void var_GetCoords( vlc_object_t *obj, const char *name,
                                  int32_t *px, int32_t *py )
{
    vlc_value_t val;

    if (likely(!var_GetChecked (obj, name, VLC_VAR_COORDS, &val)))
    {
        *px = val.coords.x;
        *py = val.coords.y;
    }
    else
        *px = *py = 0;
}

/**
 * Get a float value
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED
static inline float var_GetFloat( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val; val.f_float = 0.0;
    if( !var_GetChecked( p_obj, psz_name, VLC_VAR_FLOAT, &val ) )
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
VLC_USED VLC_MALLOC
static inline char *var_GetString( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val; val.psz_string = NULL;
    if( var_GetChecked( p_obj, psz_name, VLC_VAR_STRING, &val ) )
        return NULL;
    else
        return val.psz_string;
}

VLC_USED VLC_MALLOC
static inline char *var_GetNonEmptyString( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;
    if( var_GetChecked( p_obj, psz_name, VLC_VAR_STRING, &val ) )
        return NULL;
    if( val.psz_string && *val.psz_string )
        return val.psz_string;
    free( val.psz_string );
    return NULL;
}

VLC_USED
static inline void *var_GetAddress( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;
    if( var_GetChecked( p_obj, psz_name, VLC_VAR_ADDRESS, &val ) )
        return NULL;
    else
        return val.p_address;
}

/**
 * Increment an integer variable
 * \param p_obj the object that holds the variable
 * \param psz_name the name of the variable
 */
static inline int64_t var_IncInteger( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;
    val.i_int = 1;
    if( var_GetAndSet( p_obj, psz_name, VLC_VAR_INTEGER_ADD, &val ) )
        return 0;
    return val.i_int;
}

/**
 * Decrement an integer variable
 * \param p_obj the object that holds the variable
 * \param psz_name the name of the variable
 */
static inline int64_t var_DecInteger( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;
    val.i_int = -1;
    if( var_GetAndSet( p_obj, psz_name, VLC_VAR_INTEGER_ADD, &val ) )
        return 0;
    return val.i_int;
}

static inline uint64_t var_OrInteger( vlc_object_t *obj, const char *name,
                                      unsigned v )
{
    vlc_value_t val;
    val.i_int = v;
    if( var_GetAndSet( obj, name, VLC_VAR_INTEGER_OR, &val ) )
        return 0;
    return val.i_int;
}

static inline uint64_t var_NAndInteger( vlc_object_t *obj, const char *name,
                                        unsigned v )
{
    vlc_value_t val;
    val.i_int = v;
    if( var_GetAndSet( obj, name, VLC_VAR_INTEGER_NAND, &val ) )
        return 0;
    return val.i_int;
}

/**
 * Create a integer variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED
static inline int64_t var_CreateGetInteger( vlc_object_t *p_obj, const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    return var_GetInteger( p_obj, psz_name );
}

/**
 * Create a boolean variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED
static inline bool var_CreateGetBool( vlc_object_t *p_obj, const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    return var_GetBool( p_obj, psz_name );
}

/**
 * Create a float variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED
static inline float var_CreateGetFloat( vlc_object_t *p_obj, const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    return var_GetFloat( p_obj, psz_name );
}

/**
 * Create a string variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED VLC_MALLOC
static inline char *var_CreateGetString( vlc_object_t *p_obj,
                                           const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    return var_GetString( p_obj, psz_name );
}

VLC_USED VLC_MALLOC
static inline char *var_CreateGetNonEmptyString( vlc_object_t *p_obj,
                                                   const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    return var_GetNonEmptyString( p_obj, psz_name );
}

/**
 * Create an address variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED
static inline void *var_CreateGetAddress( vlc_object_t *p_obj,
                                           const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_ADDRESS | VLC_VAR_DOINHERIT );
    return var_GetAddress( p_obj, psz_name );
}

/**
 * Create a integer command variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED
static inline int64_t var_CreateGetIntegerCommand( vlc_object_t *p_obj, const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_INTEGER | VLC_VAR_DOINHERIT
                                   | VLC_VAR_ISCOMMAND );
    return var_GetInteger( p_obj, psz_name );
}

/**
 * Create a boolean command variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED
static inline bool var_CreateGetBoolCommand( vlc_object_t *p_obj, const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_BOOL | VLC_VAR_DOINHERIT
                                   | VLC_VAR_ISCOMMAND );
    return var_GetBool( p_obj, psz_name );
}

/**
 * Create a float command variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED
static inline float var_CreateGetFloatCommand( vlc_object_t *p_obj, const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_FLOAT | VLC_VAR_DOINHERIT
                                   | VLC_VAR_ISCOMMAND );
    return var_GetFloat( p_obj, psz_name );
}

/**
 * Create a string command variable with inherit and get its value.
 *
 * \param p_obj The object that holds the variable
 * \param psz_name The name of the variable
 */
VLC_USED VLC_MALLOC
static inline char *var_CreateGetStringCommand( vlc_object_t *p_obj,
                                           const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_STRING | VLC_VAR_DOINHERIT
                                   | VLC_VAR_ISCOMMAND );
    return var_GetString( p_obj, psz_name );
}

VLC_USED VLC_MALLOC
static inline char *var_CreateGetNonEmptyStringCommand( vlc_object_t *p_obj,
                                                   const char *psz_name )
{
    var_Create( p_obj, psz_name, VLC_VAR_STRING | VLC_VAR_DOINHERIT
                                   | VLC_VAR_ISCOMMAND );
    return var_GetNonEmptyString( p_obj, psz_name );
}

VLC_USED
static inline int var_CountChoices( vlc_object_t *p_obj, const char *psz_name )
{
    size_t count;
    if( var_Change( p_obj, psz_name, VLC_VAR_CHOICESCOUNT, &count ) )
        return 0;
    return count;
}

static inline bool var_ToggleBool( vlc_object_t *p_obj, const char *psz_name )
{
    vlc_value_t val;
    if( var_GetAndSet( p_obj, psz_name, VLC_VAR_BOOL_TOGGLE, &val ) )
        return false;
    return val.b_bool;
}

VLC_USED
static inline bool var_InheritBool( vlc_object_t *obj, const char *name )
{
    vlc_value_t val;

    if( var_Inherit( obj, name, VLC_VAR_BOOL, &val ) )
        val.b_bool = false;
    return val.b_bool;
}

VLC_USED
static inline int64_t var_InheritInteger( vlc_object_t *obj, const char *name )
{
    vlc_value_t val;

    if( var_Inherit( obj, name, VLC_VAR_INTEGER, &val ) )
        val.i_int = 0;
    return val.i_int;
}

VLC_USED
static inline float var_InheritFloat( vlc_object_t *obj, const char *name )
{
    vlc_value_t val;

    if( var_Inherit( obj, name, VLC_VAR_FLOAT, &val ) )
        val.f_float = 0.;
    return val.f_float;
}

VLC_USED VLC_MALLOC
static inline char *var_InheritString( vlc_object_t *obj, const char *name )
{
    vlc_value_t val;

    if( var_Inherit( obj, name, VLC_VAR_STRING, &val ) )
        val.psz_string = NULL;
    else if( val.psz_string && !*val.psz_string )
    {
        free( val.psz_string );
        val.psz_string = NULL;
    }
    return val.psz_string;
}

VLC_USED
static inline void *var_InheritAddress( vlc_object_t *obj, const char *name )
{
    vlc_value_t val;

    if( var_Inherit( obj, name, VLC_VAR_ADDRESS, &val ) )
        val.p_address = NULL;
    return val.p_address;
}


/**
 * Inherit a string as a fractional value.
 *
 * This function inherits a string, and interprets it as an unsigned rational
 * number, i.e. a fraction. It also accepts a normally formatted floating point
 * number.
 *
 * \warning The caller shall perform any and all necessary boundary checks.
 *
 * \note The rational number is always reduced,
 * i.e. the returned numerator and denominator are always co-prime numbers.
 *
 * \note Fraction with zero as denominator are considered valid,
 * including the undefined form zero-by-zero.
 *
 * \return Zero on success, an error if parsing fails.
  */
VLC_API int var_InheritURational(vlc_object_t *obj, unsigned *num,
                                 unsigned *den, const char *name);

/**
 * Parses a string with multiple options.
 *
 * Parses a set of colon-separated or semicolon-separated
 * <code>name=value</code> pairs.
 * Some access (or access_demux) plugins uses this scheme
 * in media resource location.
 * @note Only trusted/safe variables are allowed. This is intended.
 *
 * @warning Only use this for plugins implementing VLC-specific resource
 * location schemes. This would not make any sense for standardized ones.
 *
 * @param obj VLC object on which to set variables (and emit error messages)
 * @param mrl string to parse
 * @param prefix prefix to prepend to option names in the string
 *
 * @return VLC_ENOMEM on error, VLC_SUCCESS on success.
 */
VLC_API int var_LocationParse(vlc_object_t *obj, const char *mrl, const char *prefix);

#ifndef DOC
#define var_Create(a,b,c) var_Create(VLC_OBJECT(a), b, c)
#define var_Destroy(a,b) var_Destroy(VLC_OBJECT(a), b)
#define var_Change(a,b,...) var_Change(VLC_OBJECT(a), b, __VA_ARGS__)
#define var_Type(a,b) var_Type(VLC_OBJECT(a), b)
#define var_Set(a,b,c) var_Set(VLC_OBJECT(a), b, c)
#define var_Get(a,b,c) var_Get(VLC_OBJECT(a), b, c)
#define var_SetChecked(o,n,t,v) var_SetChecked(VLC_OBJECT(o), n, t, v)
#define var_GetChecked(o,n,t,v) var_GetChecked(VLC_OBJECT(o), n, t, v)

#define var_AddCallback(a,b,c,d) var_AddCallback(VLC_OBJECT(a), b, c, d)
#define var_DelCallback(a,b,c,d) var_DelCallback(VLC_OBJECT(a), b, c, d)
#define var_TriggerCallback(a,b) var_TriggerCallback(VLC_OBJECT(a), b)
#define var_AddListCallback(a,b,c,d) \
        var_AddListCallback(VLC_OBJECT(a), b, c, d)
#define var_DelListCallback(a,b,c,d) \
        var_DelListCallback(VLC_OBJECT(a), b, c, d)

#define var_SetInteger(a,b,c) var_SetInteger(VLC_OBJECT(a), b, c)
#define var_SetBool(a,b,c) var_SetBool(VLC_OBJECT(a), b, c)
#define var_SetCoords(o,n,x,y) var_SetCoords(VLC_OBJECT(o), n, x, y)
#define var_SetFloat(a,b,c) var_SetFloat(VLC_OBJECT(a), b, c)
#define var_SetString(a,b,c) var_SetString(VLC_OBJECT(a), b, c)
#define var_SetAddress(o, n, p) var_SetAddress(VLC_OBJECT(o), n, p)

#define var_GetCoords(o,n,x,y) var_GetCoords(VLC_OBJECT(o), n, x, y)

#define var_IncInteger(a,b) var_IncInteger(VLC_OBJECT(a), b)
#define var_DecInteger(a,b) var_DecInteger(VLC_OBJECT(a), b)
#define var_OrInteger(a,b,c) var_OrInteger(VLC_OBJECT(a), b, c)
#define var_NAndInteger(a,b,c) var_NAndInteger(VLC_OBJECT(a), b, c)

#define var_CreateGetInteger(a,b) var_CreateGetInteger(VLC_OBJECT(a), b)
#define var_CreateGetBool(a,b) var_CreateGetBool(VLC_OBJECT(a), b)
#define var_CreateGetFloat(a,b) var_CreateGetFloat(VLC_OBJECT(a), b)
#define var_CreateGetString(a,b) var_CreateGetString(VLC_OBJECT(a), b)
#define var_CreateGetNonEmptyString(a,b) \
        var_CreateGetNonEmptyString(VLC_OBJECT(a), b)
#define var_CreateGetAddress(a,b) var_CreateGetAddress( VLC_OBJECT(a), b)

#define var_CreateGetIntegerCommand(a,b)   var_CreateGetIntegerCommand( VLC_OBJECT(a),b)
#define var_CreateGetBoolCommand(a,b)   var_CreateGetBoolCommand( VLC_OBJECT(a),b)
#define var_CreateGetFloatCommand(a,b)   var_CreateGetFloatCommand( VLC_OBJECT(a),b)
#define var_CreateGetStringCommand(a,b)   var_CreateGetStringCommand( VLC_OBJECT(a),b)
#define var_CreateGetNonEmptyStringCommand(a,b)   var_CreateGetNonEmptyStringCommand( VLC_OBJECT(a),b)

#define var_CountChoices(a,b) var_CountChoices(VLC_OBJECT(a),b)
#define var_ToggleBool(a,b) var_ToggleBool(VLC_OBJECT(a),b )

#define var_InheritBool(o, n) var_InheritBool(VLC_OBJECT(o), n)
#define var_InheritInteger(o, n) var_InheritInteger(VLC_OBJECT(o), n)
#define var_InheritFloat(o, n) var_InheritFloat(VLC_OBJECT(o), n)
#define var_InheritString(o, n) var_InheritString(VLC_OBJECT(o), n)
#define var_InheritAddress(o, n) var_InheritAddress(VLC_OBJECT(o), n)
#define var_InheritURational(a,b,c,d) var_InheritURational(VLC_OBJECT(a), b, c, d)

#define var_GetInteger(a,b) var_GetInteger(VLC_OBJECT(a),b)
#define var_GetBool(a,b) var_GetBool(VLC_OBJECT(a),b)
#define var_GetFloat(a,b) var_GetFloat(VLC_OBJECT(a),b)
#define var_GetString(a,b) var_GetString(VLC_OBJECT(a),b)
#define var_GetNonEmptyString(a,b) var_GetNonEmptyString( VLC_OBJECT(a),b)
#define var_GetAddress(a,b) var_GetAddress(VLC_OBJECT(a),b)

#define var_LocationParse(o, m, p) var_LocationParse(VLC_OBJECT(o), m, p)
#endif

/**
 * @}
 */
#endif /*  _VLC_VARIABLES_H */
