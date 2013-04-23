/*****************************************************************************
 * variables.c: routines for object variables handling
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SEARCH_H
# include <search.h>
#endif
#include <assert.h>
#include <math.h>
#include <limits.h>
#ifdef __GLIBC__
# include <dlfcn.h>
#endif

#include <vlc_common.h>
#include <vlc_charset.h>
#include "libvlc.h"
#include "variables.h"
#include "config/configuration.h"

/*****************************************************************************
 * Private types
 *****************************************************************************/
struct callback_entry_t
{
    vlc_callback_t pf_callback;
    void *         p_data;
};

/*****************************************************************************
 * Local comparison functions, returns 0 if v == w, < 0 if v < w, > 0 if v > w
 *****************************************************************************/
static int CmpBool( vlc_value_t v, vlc_value_t w )
{
    return v.b_bool ? w.b_bool ? 0 : 1 : w.b_bool ? -1 : 0;
}

static int CmpInt( vlc_value_t v, vlc_value_t w )
{
    return v.i_int == w.i_int ? 0 : v.i_int > w.i_int ? 1 : -1;
}

static int CmpTime( vlc_value_t v, vlc_value_t w )
{
    return v.i_time == w.i_time ? 0 : v.i_time > w.i_time ? 1 : -1;
}

static int CmpString( vlc_value_t v, vlc_value_t w )
{
    if( !v.psz_string )
        return !w.psz_string ? 0 : -1;
    else
        return !w.psz_string ? 1 : strcmp( v.psz_string, w.psz_string );
}
static int CmpFloat( vlc_value_t v, vlc_value_t w ) { return v.f_float == w.f_float ? 0 : v.f_float > w.f_float ? 1 : -1; }
static int CmpAddress( vlc_value_t v, vlc_value_t w ) { return v.p_address == w.p_address ? 0 : v.p_address > w.p_address ? 1 : -1; }

/*****************************************************************************
 * Local duplication functions, and local deallocation functions
 *****************************************************************************/
static void DupDummy( vlc_value_t *p_val ) { (void)p_val; /* unused */ }
static void DupString( vlc_value_t *p_val )
{
    p_val->psz_string = strdup( p_val->psz_string ? p_val->psz_string :  "" );
}

static void FreeDummy( vlc_value_t *p_val ) { (void)p_val; /* unused */ }
static void FreeString( vlc_value_t *p_val ) { free( p_val->psz_string ); }

static void FreeList( vlc_value_t *p_val )
{
    int i;
    for( i = 0; i < p_val->p_list->i_count; i++ )
    {
        switch( p_val->p_list->pi_types[i] & VLC_VAR_CLASS )
        {
        case VLC_VAR_STRING:
            FreeString( &p_val->p_list->p_values[i] );
            break;
        default:
            break;
        }
    }

    if( p_val->p_list->i_count )
    {
        free( p_val->p_list->p_values );
        free( p_val->p_list->pi_types );
    }
    free( p_val->p_list );
}

static const struct variable_ops_t
void_ops   = { NULL,       DupDummy,  FreeDummy,  },
addr_ops   = { CmpAddress, DupDummy,  FreeDummy,  },
bool_ops   = { CmpBool,    DupDummy,  FreeDummy,  },
float_ops  = { CmpFloat,   DupDummy,  FreeDummy,  },
int_ops    = { CmpInt,     DupDummy,  FreeDummy,  },
string_ops = { CmpString,  DupString, FreeString, },
time_ops   = { CmpTime,    DupDummy,  FreeDummy,  },
coords_ops = { NULL,       DupDummy,  FreeDummy,  };

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void     WaitUnused  ( vlc_object_t *, variable_t * );

static void     CheckValue  ( variable_t *, vlc_value_t * );

static int      TriggerCallback( vlc_object_t *, variable_t *, const char *,
                                 vlc_value_t );

static int varcmp( const void *a, const void *b )
{
    const variable_t *va = a, *vb = b;

    /* psz_name must be first */
    assert( va == (const void *)&va->psz_name );
    return strcmp( va->psz_name, vb->psz_name );
}

static variable_t *Lookup( vlc_object_t *obj, const char *psz_name )
{
    vlc_object_internals_t *priv = vlc_internals( obj );
    variable_t **pp_var;

    vlc_assert_locked( &priv->var_lock );
    pp_var = tfind( &psz_name, &priv->var_root, varcmp );
    return (pp_var != NULL) ? *pp_var : NULL;
}

static void Destroy( variable_t *p_var )
{
    p_var->ops->pf_free( &p_var->val );
    if( p_var->choices.i_count )
    {
        for( int i = 0 ; i < p_var->choices.i_count ; i++ )
        {
            p_var->ops->pf_free( &p_var->choices.p_values[i] );
            free( p_var->choices_text.p_values[i].psz_string );
        }
        free( p_var->choices.p_values );
        free( p_var->choices_text.p_values );
    }
#if 0 // ndef NDEBUG
    for (int i = 0; i < p_var->i_entries; i++)
    {
        const char *file = "?", *symbol = "?";
        const void *addr = p_var->p_entries[i].pf_callback;
# ifdef __GLIBC__
        Dl_info info;

        if (dladdr (addr, &info))
        {
            if (info.dli_fname) file = info.dli_fname;
            if (info.dli_sname) symbol = info.dli_sname;
        }
# endif
        fprintf (stderr, "Error: callback on \"%s\" dangling %s(%s)[%p]\n",
                 p_var->psz_name, file, symbol, addr);
    }
#endif

    free( p_var->psz_name );
    free( p_var->psz_text );
    free( p_var->p_entries );
    free( p_var );
}

#undef var_Create
/**
 * Initialize a vlc variable
 *
 * We hash the given string and insert it into the sorted list. The insertion
 * may require slow memory copies, but think about what we gain in the log(n)
 * lookup phase when setting/getting the variable value!
 *
 * \param p_this The object in which to create the variable
 * \param psz_name The name of the variable
 * \param i_type The variables type. Must be one of \ref var_type combined with
 *               zero or more \ref var_flags
 */
int var_Create( vlc_object_t *p_this, const char *psz_name, int i_type )
{
    assert( p_this );

    variable_t *p_var = calloc( 1, sizeof( *p_var ) );
    if( p_var == NULL )
        return VLC_ENOMEM;

    p_var->psz_name = strdup( psz_name );
    p_var->psz_text = NULL;

    p_var->i_type = i_type & ~VLC_VAR_DOINHERIT;

    p_var->i_usage = 1;

    p_var->i_default = -1;
    p_var->choices.i_count = 0;
    p_var->choices.p_values = NULL;
    p_var->choices_text.i_count = 0;
    p_var->choices_text.p_values = NULL;

    p_var->b_incallback = false;
    p_var->i_entries = 0;
    p_var->p_entries = NULL;

    /* Always initialize the variable, even if it is a list variable; this
     * will lead to errors if the variable is not initialized, but it will
     * not cause crashes in the variable handling. */
    switch( i_type & VLC_VAR_CLASS )
    {
        case VLC_VAR_BOOL:
            p_var->ops = &bool_ops;
            p_var->val.b_bool = false;
            break;
        case VLC_VAR_INTEGER:
            p_var->ops = &int_ops;
            p_var->val.i_int = 0;
            break;
        case VLC_VAR_STRING:
            p_var->ops = &string_ops;
            p_var->val.psz_string = NULL;
            break;
        case VLC_VAR_FLOAT:
            p_var->ops = &float_ops;
            p_var->val.f_float = 0.0;
            break;
        case VLC_VAR_TIME:
            p_var->ops = &time_ops;
            p_var->val.i_time = 0;
            break;
        case VLC_VAR_COORDS:
            p_var->ops = &coords_ops;
            p_var->val.coords.x = p_var->val.coords.y = 0;
            break;
        case VLC_VAR_ADDRESS:
            p_var->ops = &addr_ops;
            p_var->val.p_address = NULL;
            break;
        case VLC_VAR_VOID:
            p_var->ops = &void_ops;
            break;
        default:
            assert (0);
    }

    if( (i_type & VLC_VAR_DOINHERIT)
     && var_Inherit( p_this, psz_name, i_type, &p_var->val ) == 0 )
    {
        if( i_type & VLC_VAR_HASCHOICE )
        {
            /* We must add the inherited value to our choice list */
            p_var->i_default = 0;

            INSERT_ELEM( p_var->choices.p_values, p_var->choices.i_count,
                         0, p_var->val );
            INSERT_ELEM( p_var->choices_text.p_values,
                         p_var->choices_text.i_count, 0, p_var->val );
            p_var->ops->pf_dup( &p_var->choices.p_values[0] );
            p_var->choices_text.p_values[0].psz_string = NULL;
        }
    }

    vlc_object_internals_t *p_priv = vlc_internals( p_this );
    variable_t **pp_var, *p_oldvar;
    int ret = VLC_SUCCESS;

    vlc_mutex_lock( &p_priv->var_lock );

    pp_var = tsearch( p_var, &p_priv->var_root, varcmp );
    if( unlikely(pp_var == NULL) )
        ret = VLC_ENOMEM;
    else if( (p_oldvar = *pp_var) == p_var ) /* Variable create */
        p_var = NULL; /* Variable created */
    else /* Variable already exists */
    {
        assert (((i_type ^ p_oldvar->i_type) & VLC_VAR_CLASS) == 0);
        p_oldvar->i_usage++;
        p_oldvar->i_type |= i_type & (VLC_VAR_ISCOMMAND|VLC_VAR_HASCHOICE);
    }
    vlc_mutex_unlock( &p_priv->var_lock );

    /* If we did not need to create a new variable, free everything... */
    if( p_var != NULL )
        Destroy( p_var );
    return ret;
}

#undef var_Destroy
/**
 * Destroy a vlc variable
 *
 * Look for the variable and destroy it if it is found. As in var_Create we
 * do a call to memmove() but we have performance counterparts elsewhere.
 *
 * \param p_this The object that holds the variable
 * \param psz_name The name of the variable
 */
int var_Destroy( vlc_object_t *p_this, const char *psz_name )
{
    variable_t *p_var;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_ENOVAR;
    }

    WaitUnused( p_this, p_var );

    if( --p_var->i_usage == 0 )
        tdelete( p_var, &p_priv->var_root, varcmp );
    else
        p_var = NULL;
    vlc_mutex_unlock( &p_priv->var_lock );

    if( p_var != NULL )
        Destroy( p_var );
    return VLC_SUCCESS;
}

static void CleanupVar( void *var )
{
    Destroy( var );
}

void var_DestroyAll( vlc_object_t *obj )
{
    vlc_object_internals_t *priv = vlc_internals( obj );

    tdestroy( priv->var_root, CleanupVar );
    priv->var_root = NULL;
}

#undef var_Change
/**
 * Perform an action on a variable
 *
 * \param p_this The object that holds the variable
 * \param psz_name The name of the variable
 * \param i_action The action to perform. Must be one of \ref var_action
 * \param p_val First action parameter
 * \param p_val2 Second action parameter
 */
int var_Change( vlc_object_t *p_this, const char *psz_name,
                int i_action, vlc_value_t *p_val, vlc_value_t *p_val2 )
{
    int ret = VLC_SUCCESS;
    variable_t *p_var;
    vlc_value_t oldval;
    vlc_value_t newval;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_ENOVAR;
    }

    switch( i_action )
    {
        case VLC_VAR_SETMIN:
            if( p_var->i_type & VLC_VAR_HASMIN )
            {
                p_var->ops->pf_free( &p_var->min );
            }
            p_var->i_type |= VLC_VAR_HASMIN;
            p_var->min = *p_val;
            p_var->ops->pf_dup( &p_var->min );
            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_GETMIN:
            if( p_var->i_type & VLC_VAR_HASMIN )
                *p_val = p_var->min;
            else
                ret = VLC_EGENERIC;
            break;
        case VLC_VAR_SETMAX:
            if( p_var->i_type & VLC_VAR_HASMAX )
            {
                p_var->ops->pf_free( &p_var->max );
            }
            p_var->i_type |= VLC_VAR_HASMAX;
            p_var->max = *p_val;
            p_var->ops->pf_dup( &p_var->max );
            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_GETMAX:
            if( p_var->i_type & VLC_VAR_HASMAX )
                *p_val = p_var->max;
            else
                ret = VLC_EGENERIC;
            break;
        case VLC_VAR_SETSTEP:
            if( p_var->i_type & VLC_VAR_HASSTEP )
            {
                p_var->ops->pf_free( &p_var->step );
            }
            p_var->i_type |= VLC_VAR_HASSTEP;
            p_var->step = *p_val;
            p_var->ops->pf_dup( &p_var->step );
            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_GETSTEP:
            if( p_var->i_type & VLC_VAR_HASSTEP )
                *p_val = p_var->step;
            else
                ret = VLC_EGENERIC;
            break;
        case VLC_VAR_ADDCHOICE:
        {
            int i = p_var->choices.i_count;

            INSERT_ELEM( p_var->choices.p_values, p_var->choices.i_count,
                         i, *p_val );
            INSERT_ELEM( p_var->choices_text.p_values,
                         p_var->choices_text.i_count, i, *p_val );
            p_var->ops->pf_dup( &p_var->choices.p_values[i] );
            p_var->choices_text.p_values[i].psz_string =
                ( p_val2 && p_val2->psz_string ) ?
                strdup( p_val2->psz_string ) : NULL;

            CheckValue( p_var, &p_var->val );
            break;
        }
        case VLC_VAR_DELCHOICE:
        {
            int i;

            for( i = 0 ; i < p_var->choices.i_count ; i++ )
                if( p_var->ops->pf_cmp( p_var->choices.p_values[i], *p_val ) == 0 )
                    break;

            if( i == p_var->choices.i_count )
            {
                /* Not found */
                vlc_mutex_unlock( &p_priv->var_lock );
                return VLC_EGENERIC;
            }

            if( p_var->i_default > i )
                p_var->i_default--;
            else if( p_var->i_default == i )
                p_var->i_default = -1;

            p_var->ops->pf_free( &p_var->choices.p_values[i] );
            free( p_var->choices_text.p_values[i].psz_string );
            REMOVE_ELEM( p_var->choices.p_values, p_var->choices.i_count, i );
            REMOVE_ELEM( p_var->choices_text.p_values,
                         p_var->choices_text.i_count, i );

            CheckValue( p_var, &p_var->val );
            break;
        }
        case VLC_VAR_CHOICESCOUNT:
            p_val->i_int = p_var->choices.i_count;
            break;
        case VLC_VAR_CLEARCHOICES:
            for( int i = 0 ; i < p_var->choices.i_count ; i++ )
                p_var->ops->pf_free( &p_var->choices.p_values[i] );
            for( int i = 0 ; i < p_var->choices_text.i_count ; i++ )
                free( p_var->choices_text.p_values[i].psz_string );

            if( p_var->choices.i_count ) free( p_var->choices.p_values );
            if( p_var->choices_text.i_count ) free( p_var->choices_text.p_values );

            p_var->choices.i_count = 0;
            p_var->choices.p_values = NULL;
            p_var->choices_text.i_count = 0;
            p_var->choices_text.p_values = NULL;
            p_var->i_default = -1;
            break;
        case VLC_VAR_SETDEFAULT:
        {
            int i;
            /* FIXME: the list is sorted, dude. Use something cleverer. */
            for( i = 0 ; i < p_var->choices.i_count ; i++ )
                if( p_var->ops->pf_cmp( p_var->choices.p_values[i], *p_val ) == 0 )
                    break;

            if( i == p_var->choices.i_count )
                /* Not found */
                break;

            p_var->i_default = i;
            CheckValue( p_var, &p_var->val );
            break;
        }
        case VLC_VAR_SETVALUE:
            /* Duplicate data if needed */
            newval = *p_val;
            p_var->ops->pf_dup( &newval );
            /* Backup needed stuff */
            oldval = p_var->val;
            /* Check boundaries and list */
            CheckValue( p_var, &newval );
            /* Set the variable */
            p_var->val = newval;
            /* Free data if needed */
            p_var->ops->pf_free( &oldval );
            break;
        case VLC_VAR_GETCHOICES:
        case VLC_VAR_GETLIST:
            p_val->p_list = malloc( sizeof(vlc_list_t) );
            if( p_val2 ) p_val2->p_list = malloc( sizeof(vlc_list_t) );
            if( p_var->choices.i_count )
            {
                p_val->p_list->p_values = malloc( p_var->choices.i_count
                                                  * sizeof(vlc_value_t) );
                p_val->p_list->pi_types = malloc( p_var->choices.i_count
                                                  * sizeof(int) );
                if( p_val2 )
                {
                    p_val2->p_list->p_values =
                        malloc( p_var->choices.i_count * sizeof(vlc_value_t) );
                    p_val2->p_list->pi_types =
                        malloc( p_var->choices.i_count * sizeof(int) );
                }
            }
            p_val->p_list->i_count = p_var->choices.i_count;
            if( p_val2 ) p_val2->p_list->i_count = p_var->choices.i_count;
            for( int i = 0 ; i < p_var->choices.i_count ; i++ )
            {
                p_val->p_list->p_values[i] = p_var->choices.p_values[i];
                p_val->p_list->pi_types[i] = p_var->i_type;
                p_var->ops->pf_dup( &p_val->p_list->p_values[i] );
                if( p_val2 )
                {
                    p_val2->p_list->p_values[i].psz_string =
                        p_var->choices_text.p_values[i].psz_string ?
                    strdup(p_var->choices_text.p_values[i].psz_string) : NULL;
                    p_val2->p_list->pi_types[i] = VLC_VAR_STRING;
                }
            }
            break;
        case VLC_VAR_SETTEXT:
            free( p_var->psz_text );
            if( p_val && p_val->psz_string )
                p_var->psz_text = strdup( p_val->psz_string );
            else
                p_var->psz_text = NULL;
            break;
        case VLC_VAR_GETTEXT:
            p_val->psz_string = p_var->psz_text ? strdup( p_var->psz_text )
                                                : NULL;
            break;
        case VLC_VAR_SETISCOMMAND:
            p_var->i_type |= VLC_VAR_ISCOMMAND;
            break;

        default:
            break;
    }

    vlc_mutex_unlock( &p_priv->var_lock );

    return ret;
}

#undef var_GetAndSet
/**
 * Perform a Get and Set on a variable
 *
 * \param p_this: The object that hold the variable
 * \param psz_name: the name of the variable
 * \param i_action: the action to perform
 * \param p_val: The action parameter
 * \return vlc error codes
 */
int var_GetAndSet( vlc_object_t *p_this, const char *psz_name, int i_action,
                   vlc_value_t *p_val )
{
    int i_ret;
    variable_t *p_var;
    vlc_value_t oldval;

    assert( p_this );
    assert( p_val );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );
    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_ENOVAR;
    }

    WaitUnused( p_this, p_var );

    /* Duplicated data if needed */
    //p_var->ops->pf_dup( &val );

    /* Backup needed stuff */
    oldval = p_var->val;

    /* depending of the action requiered */
    switch( i_action )
    {
    case VLC_VAR_BOOL_TOGGLE:
        assert( ( p_var->i_type & VLC_VAR_BOOL ) == VLC_VAR_BOOL );
        p_var->val.b_bool = !p_var->val.b_bool;
        break;
    case VLC_VAR_INTEGER_ADD:
        assert( ( p_var->i_type & VLC_VAR_INTEGER ) == VLC_VAR_INTEGER );
        p_var->val.i_int += p_val->i_int;
        break;
    case VLC_VAR_INTEGER_OR:
        assert( ( p_var->i_type & VLC_VAR_INTEGER ) == VLC_VAR_INTEGER );
        p_var->val.i_int |= p_val->i_int;
        break;
    case VLC_VAR_INTEGER_NAND:
        assert( ( p_var->i_type & VLC_VAR_INTEGER ) == VLC_VAR_INTEGER );
        p_var->val.i_int &= ~p_val->i_int;
        break;
    default:
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_EGENERIC;
    }

    /*  Check boundaries */
    CheckValue( p_var, &p_var->val );
    *p_val = p_var->val;

    /* Deal with callbacks.*/
    i_ret = TriggerCallback( p_this, p_var, psz_name, oldval );

    vlc_mutex_unlock( &p_priv->var_lock );

    return i_ret;
}

#undef var_Type
/**
 * Request a variable's type
 *
 * \return The variable type if it exists, or 0 if the
 * variable could not be found.
 * \see \ref var_type
 */
int var_Type( vlc_object_t *p_this, const char *psz_name )
{
    variable_t *p_var;
    int i_type = 0;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    p_var = Lookup( p_this, psz_name );
    if( p_var != NULL )
        i_type = p_var->i_type;

    vlc_mutex_unlock( &p_priv->var_lock );

    return i_type;
}

#undef var_SetChecked
int var_SetChecked( vlc_object_t *p_this, const char *psz_name,
                    int expected_type, vlc_value_t val )
{
    int i_ret = VLC_SUCCESS;
    variable_t *p_var;
    vlc_value_t oldval;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_ENOVAR;
    }

    assert( expected_type == 0 ||
            (p_var->i_type & VLC_VAR_CLASS) == expected_type );
    assert ((p_var->i_type & VLC_VAR_CLASS) != VLC_VAR_VOID);

    WaitUnused( p_this, p_var );

    /* Duplicate data if needed */
    p_var->ops->pf_dup( &val );

    /* Backup needed stuff */
    oldval = p_var->val;

    /* Check boundaries and list */
    CheckValue( p_var, &val );

    /* Set the variable */
    p_var->val = val;

    /* Deal with callbacks */
    i_ret = TriggerCallback( p_this, p_var, psz_name, oldval );

    /* Free data if needed */
    p_var->ops->pf_free( &oldval );

    vlc_mutex_unlock( &p_priv->var_lock );

    return i_ret;
}

#undef var_Set
/**
 * Set a variable's value
 *
 * \param p_this The object that hold the variable
 * \param psz_name The name of the variable
 * \param val the value to set
 */
int var_Set( vlc_object_t *p_this, const char *psz_name, vlc_value_t val )
{
    return var_SetChecked( p_this, psz_name, 0, val );
}

#undef var_GetChecked
int var_GetChecked( vlc_object_t *p_this, const char *psz_name,
                    int expected_type, vlc_value_t *p_val )
{
    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );
    variable_t *p_var;
    int err = VLC_SUCCESS;

    vlc_mutex_lock( &p_priv->var_lock );

    p_var = Lookup( p_this, psz_name );
    if( p_var != NULL )
    {
        assert( expected_type == 0 ||
                (p_var->i_type & VLC_VAR_CLASS) == expected_type );
        assert ((p_var->i_type & VLC_VAR_CLASS) != VLC_VAR_VOID);

        /* Really get the variable */
        *p_val = p_var->val;

        /* Duplicate value if needed */
        p_var->ops->pf_dup( p_val );
    }
    else
        err = VLC_ENOVAR;

    vlc_mutex_unlock( &p_priv->var_lock );
    return err;
}

#undef var_Get
/**
 * Get a variable's value
 *
 * \param p_this The object that holds the variable
 * \param psz_name The name of the variable
 * \param p_val Pointer to a vlc_value_t that will hold the variable's value
 *              after the function is finished
 */
int var_Get( vlc_object_t *p_this, const char *psz_name, vlc_value_t *p_val )
{
    return var_GetChecked( p_this, psz_name, 0, p_val );
}

#undef var_AddCallback
/**
 * Register a callback in a variable
 *
 * We store a function pointer that will be called upon variable
 * modification.
 *
 * \param p_this The object that holds the variable
 * \param psz_name The name of the variable
 * \param pf_callback The function pointer
 * \param p_data A generic pointer that will be passed as the last
 *               argument to the callback function.
 *
 * \warning The callback function is run in the thread that calls var_Set on
 *          the variable. Use proper locking. This thread may not have much
 *          time to spare, so keep callback functions short.
 */
int var_AddCallback( vlc_object_t *p_this, const char *psz_name,
                     vlc_callback_t pf_callback, void *p_data )
{
    variable_t *p_var;
    callback_entry_t entry;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    entry.pf_callback = pf_callback;
    entry.p_data = p_data;

    vlc_mutex_lock( &p_priv->var_lock );

    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        msg_Err( p_this, "cannot add callback %p to nonexistent "
                         "variable '%s'", pf_callback, psz_name );
        return VLC_ENOVAR;
    }

    WaitUnused( p_this, p_var );
    INSERT_ELEM( p_var->p_entries,
                 p_var->i_entries,
                 p_var->i_entries,
                 entry );

    vlc_mutex_unlock( &p_priv->var_lock );

    return VLC_SUCCESS;
}

#undef var_DelCallback
/**
 * Remove a callback from a variable
 *
 * pf_callback and p_data have to be given again, because different objects
 * might have registered the same callback function.
 */
int var_DelCallback( vlc_object_t *p_this, const char *psz_name,
                     vlc_callback_t pf_callback, void *p_data )
{
    int i_entry;
    variable_t *p_var;
#ifndef NDEBUG
    bool b_found_similar = false;
#endif

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_ENOVAR;
    }

    WaitUnused( p_this, p_var );

    for( i_entry = p_var->i_entries ; i_entry-- ; )
    {
        if( p_var->p_entries[i_entry].pf_callback == pf_callback
            && p_var->p_entries[i_entry].p_data == p_data )
        {
            break;
        }
#ifndef NDEBUG
        else if( p_var->p_entries[i_entry].pf_callback == pf_callback )
            b_found_similar = true;
#endif
    }

    if( i_entry < 0 )
    {
#ifndef NDEBUG
        if( b_found_similar )
            fprintf( stderr, "Calling var_DelCallback for '%s' with the same "
                             "function but not the same data.", psz_name );
        assert( 0 );
#endif
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_EGENERIC;
    }

    REMOVE_ELEM( p_var->p_entries, p_var->i_entries, i_entry );

    vlc_mutex_unlock( &p_priv->var_lock );

    return VLC_SUCCESS;
}

#undef var_TriggerCallback
/**
 * Trigger callback on a variable
 *
 * \param p_this The object that hold the variable
 * \param psz_name The name of the variable
 */
int var_TriggerCallback( vlc_object_t *p_this, const char *psz_name )
{
    int i_ret;
    variable_t *p_var;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_ENOVAR;
    }

    WaitUnused( p_this, p_var );

    /* Deal with callbacks. Tell we're in a callback, release the lock,
     * call stored functions, retake the lock. */
    i_ret = TriggerCallback( p_this, p_var, psz_name, p_var->val );

    vlc_mutex_unlock( &p_priv->var_lock );
    return i_ret;
}

/** Parse a stringified option
 * This function parse a string option and create the associated object
 * variable
 * The option must be of the form "[no[-]]foo[=bar]" where foo is the
 * option name and bar is the value of the option.
 * \param p_obj the object in which the variable must be created
 * \param psz_option the option to parse
 * \param trusted whether the option is set by a trusted input or not
 * \return nothing
 */
void var_OptionParse( vlc_object_t *p_obj, const char *psz_option,
                      bool trusted )
{
    char *psz_name, *psz_value;
    int  i_type;
    bool b_isno = false;
    vlc_value_t val;

    val.psz_string = NULL;

    /* It's too much of a hassle to remove the ':' when we parse
     * the cmd line :) */
    if( psz_option[0] == ':' )
        psz_option++;

    if( !psz_option[0] )
        return;

    psz_name = strdup( psz_option );
    if( psz_name == NULL )
        return;

    psz_value = strchr( psz_name, '=' );
    if( psz_value != NULL )
        *psz_value++ = '\0';

    i_type = config_GetType( p_obj, psz_name );
    if( !i_type && !psz_value )
    {
        /* check for "no-foo" or "nofoo" */
        if( !strncmp( psz_name, "no-", 3 ) )
        {
            memmove( psz_name, psz_name + 3, strlen(psz_name) + 1 - 3 );
        }
        else if( !strncmp( psz_name, "no", 2 ) )
        {
            memmove( psz_name, psz_name + 2, strlen(psz_name) + 1 - 2 );
        }
        else goto cleanup;           /* Option doesn't exist */

        b_isno = true;
        i_type = config_GetType( p_obj, psz_name );
    }
    if( !i_type ) goto cleanup; /* Option doesn't exist */

    if( ( i_type != VLC_VAR_BOOL ) &&
        ( !psz_value || !*psz_value ) ) goto cleanup; /* Invalid value */

    /* check if option is unsafe */
    if( !trusted && !config_IsSafe( psz_name ) )
    {
        msg_Err( p_obj, "unsafe option \"%s\" has been ignored for "
                        "security reasons", psz_name );
        free( psz_name );
        return;
    }

    /* Create the variable in the input object.
     * Children of the input object will be able to retreive this value
     * thanks to the inheritance property of the object variables. */
    var_Create( p_obj, psz_name, i_type );

    switch( i_type )
    {
    case VLC_VAR_BOOL:
        val.b_bool = !b_isno;
        break;

    case VLC_VAR_INTEGER:
        val.i_int = strtoll( psz_value, NULL, 0 );
        break;

    case VLC_VAR_FLOAT:
        val.f_float = us_atof( psz_value );
        break;

    case VLC_VAR_STRING:
        val.psz_string = psz_value;
        break;

    default:
        goto cleanup;
    }

    var_Set( p_obj, psz_name, val );

cleanup:
    free( psz_name );
}

#undef var_LocationParse
/**
 * Parses a set of colon-separated or semicolon-separated
 * <variable name>=<value> pairs.
 * Some access (or access_demux) plugins uses this scheme
 * in media resource location.
 * @note Only trusted/safe variables are allowed. This is intended.
 *
 * @warning Only use this for plugins implementing VLC-specific resource
 * location schemes. This would not make any sense for standardized ones.
 *
 * @param obj VLC object on which to set variables (and emit error messages)
 * @param mrl string to parse
 * @param pref prefix to prepend to option names in the string
 *
 * @return VLC_ENOMEM on error, VLC_SUCCESS on success.
 */
int var_LocationParse (vlc_object_t *obj, const char *mrl, const char *pref)
{
    int ret = VLC_SUCCESS;
    size_t preflen = strlen (pref) + 1;

    assert(mrl != NULL);
    while (*mrl != '\0')
    {
        mrl += strspn (mrl, ":;"); /* skip leading colon(s) */

        size_t len = strcspn (mrl, ":;");
        char *buf = malloc (preflen + len);

        if (likely(buf != NULL))
        {
            /* NOTE: this does not support the "no-<varname>" bool syntax. */
            /* DO NOT use asprintf() here; it won't work! Think again. */
            snprintf (buf, preflen + len, "%s%s", pref, mrl);
            var_OptionParse (obj, buf, false);
            free (buf);
        }
        else
            ret = VLC_ENOMEM;
        mrl += len;
    }

    return ret;
}

/**
 * Waits until the variable is inactive (i.e. not executing a callback)
 */
static void WaitUnused( vlc_object_t *p_this, variable_t *p_var )
{
    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    mutex_cleanup_push( &p_priv->var_lock );
    while( p_var->b_incallback )
        vlc_cond_wait( &p_priv->var_wait, &p_priv->var_lock );
    vlc_cleanup_pop( );
}

/*****************************************************************************
 * CheckValue: check that a value is valid wrt. a variable
 *****************************************************************************
 * This function checks p_val's value against p_var's limitations such as
 * minimal and maximal value, step, in-list position, and modifies p_val if
 * necessary.
 ****************************************************************************/
static void CheckValue ( variable_t *p_var, vlc_value_t *p_val )
{
    /* Check that our variable is in the list */
    if( p_var->i_type & VLC_VAR_HASCHOICE && p_var->choices.i_count )
    {
        int i;

        /* This list is not sorted so go throug it (this is a small list) */
        for( i = p_var->choices.i_count ; i-- ; )
        {
            if( p_var->ops->pf_cmp( *p_val, p_var->choices.p_values[i] ) == 0 )
            {
                break;
            }
        }

        /* If not found, change it to anything vaguely valid */
        if( i < 0 )
        {
            /* Free the old variable, get the new one, dup it */
            p_var->ops->pf_free( p_val );
            *p_val = p_var->choices.p_values[p_var->i_default >= 0
                                          ? p_var->i_default : 0 ];
            p_var->ops->pf_dup( p_val );
        }
    }

    /* Check that our variable is within the bounds */
    switch( p_var->i_type & VLC_VAR_TYPE )
    {
        case VLC_VAR_INTEGER:
            if( p_var->i_type & VLC_VAR_HASSTEP && p_var->step.i_int
                 && (p_val->i_int % p_var->step.i_int) )
            {
                p_val->i_int = (p_val->i_int + (p_var->step.i_int / 2))
                               / p_var->step.i_int * p_var->step.i_int;
            }
            if( p_var->i_type & VLC_VAR_HASMIN
                 && p_val->i_int < p_var->min.i_int )
            {
                p_val->i_int = p_var->min.i_int;
            }
            if( p_var->i_type & VLC_VAR_HASMAX
                 && p_val->i_int > p_var->max.i_int )
            {
                p_val->i_int = p_var->max.i_int;
            }
            break;
        case VLC_VAR_FLOAT:
            if( p_var->i_type & VLC_VAR_HASSTEP && p_var->step.f_float )
            {
                float f_round = p_var->step.f_float * (float)(int)( 0.5 +
                                        p_val->f_float / p_var->step.f_float );
                if( p_val->f_float != f_round )
                {
                    p_val->f_float = f_round;
                }
            }
            if( p_var->i_type & VLC_VAR_HASMIN
                 && p_val->f_float < p_var->min.f_float )
            {
                p_val->f_float = p_var->min.f_float;
            }
            if( p_var->i_type & VLC_VAR_HASMAX
                 && p_val->f_float > p_var->max.f_float )
            {
                p_val->f_float = p_var->max.f_float;
            }
            break;
        case VLC_VAR_TIME:
            /* FIXME: TODO */
            break;
    }
}

/**
 * Finds the value of a variable. If the specified object does not hold a
 * variable with the specified name, try the parent object, and iterate until
 * the top of the tree. If no match is found, the value is read from the
 * configuration.
 */
int var_Inherit( vlc_object_t *p_this, const char *psz_name, int i_type,
                 vlc_value_t *p_val )
{
    i_type &= VLC_VAR_CLASS;
    for( vlc_object_t *obj = p_this; obj != NULL; obj = obj->p_parent )
    {
        if( var_GetChecked( obj, psz_name, i_type, p_val ) == VLC_SUCCESS )
            return VLC_SUCCESS;
    }

    /* else take value from config */
    switch( i_type & VLC_VAR_CLASS )
    {
        case VLC_VAR_STRING:
            p_val->psz_string = config_GetPsz( p_this, psz_name );
            if( !p_val->psz_string ) p_val->psz_string = strdup("");
            break;
        case VLC_VAR_FLOAT:
            p_val->f_float = config_GetFloat( p_this, psz_name );
            break;
        case VLC_VAR_INTEGER:
            p_val->i_int = config_GetInt( p_this, psz_name );
            break;
        case VLC_VAR_BOOL:
            p_val->b_bool = config_GetInt( p_this, psz_name );
            break;
        default:
            assert(0);
        case VLC_VAR_ADDRESS:
            return VLC_ENOOBJ;
    }
    return VLC_SUCCESS;
}


/**
 * It inherits a string as an unsigned rational number (it also accepts basic
 * float number).
 *
 * It returns an error if the rational number cannot be parsed (0/0 is valid).
 * The rational is already reduced.
 */
int (var_InheritURational)(vlc_object_t *object,
                           unsigned *num, unsigned *den,
                           const char *var)
{
    /* */
    *num = 0;
    *den = 0;

    /* */
    char *tmp = var_InheritString(object, var);
    if (!tmp)
        goto error;

    char *next;
    unsigned n = strtol(tmp,  &next, 0);
    unsigned d = strtol(*next ? &next[1] : "0", NULL, 0);

    if (*next == '.') {
        /* Interpret as a float number */
        double r = us_atof(tmp);
        double c = ceil(r);
        if (c >= UINT_MAX)
            goto error;
        unsigned m = c;
        if (m > 0) {
            d = UINT_MAX / m;
            n = r * d;
        } else {
            n = 0;
            d = 0;
        }
    }

    if (n > 0 && d > 0)
        vlc_ureduce(num, den, n, d, 0);

    free(tmp);
    return VLC_SUCCESS;

error:
    free(tmp);
    return VLC_EGENERIC;
}

/**********************************************************************
 * Trigger the callbacks.
 * Tell we're in a callback, release the lock, call stored functions,
 * retake the lock.
 **********************************************************************/
static int TriggerCallback( vlc_object_t *p_this, variable_t *p_var,
                            const char *psz_name, vlc_value_t oldval )
{
    assert( p_this );

    int i_entries = p_var->i_entries;
    if( i_entries == 0 )
        return VLC_SUCCESS;

    callback_entry_t *p_entries = p_var->p_entries;
    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    assert( !p_var->b_incallback );
    p_var->b_incallback = true;
    vlc_mutex_unlock( &p_priv->var_lock );

    /* The real calls */
    for( ; i_entries-- ; )
    {
        p_entries[i_entries].pf_callback( p_this, psz_name, oldval, p_var->val,
                                          p_entries[i_entries].p_data );
    }

    vlc_mutex_lock( &p_priv->var_lock );
    p_var->b_incallback = false;
    vlc_cond_broadcast( &p_priv->var_wait );

    return VLC_SUCCESS;
}

/**
 * Free a list and the associated strings
 * @param p_val: the list variable
 * @param p_val2: the variable associated or NULL
 */
void var_FreeList( vlc_value_t *p_val, vlc_value_t *p_val2 )
{
    FreeList( p_val );
    if( p_val2 && p_val2->p_list )
    {
        for( int i = 0; i < p_val2->p_list->i_count; i++ )
            free( p_val2->p_list->p_values[i].psz_string );
        if( p_val2->p_list->i_count )
        {
            free( p_val2->p_list->p_values );
            free( p_val2->p_list->pi_types );
        }
        free( p_val2->p_list );
    }
}
