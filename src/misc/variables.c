/*****************************************************************************
 * variables.c: routines for object variables handling
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_charset.h>
#include "variables.h"

#include "libvlc.h"

#include <assert.h>

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
static int CmpBool( vlc_value_t v, vlc_value_t w ) { return v.b_bool ? w.b_bool ? 0 : 1 : w.b_bool ? -1 : 0; }
static int CmpInt( vlc_value_t v, vlc_value_t w ) { return v.i_int == w.i_int ? 0 : v.i_int > w.i_int ? 1 : -1; }
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

static void DupList( vlc_value_t *p_val )
{
    int i;
    vlc_list_t *p_list = malloc( sizeof(vlc_list_t) );

    p_list->i_count = p_val->p_list->i_count;
    if( p_val->p_list->i_count )
    {
        p_list->p_values = malloc( p_list->i_count * sizeof(vlc_value_t) );
        p_list->pi_types = malloc( p_list->i_count * sizeof(int) );
    }
    else
    {
        p_list->p_values = NULL;
        p_list->pi_types = NULL;
    }

    for( i = 0; i < p_list->i_count; i++ )
    {
        p_list->p_values[i] = p_val->p_list->p_values[i];
        p_list->pi_types[i] = p_val->p_list->pi_types[i];
        switch( p_val->p_list->pi_types[i] & VLC_VAR_CLASS )
        {
        case VLC_VAR_STRING:

            DupString( &p_list->p_values[i] );
            break;
        default:
            break;
        }
    }

    p_val->p_list = p_list;
}

static void FreeDummy( vlc_value_t *p_val ) { (void)p_val; /* unused */ }
static void FreeString( vlc_value_t *p_val ) { free( p_val->psz_string ); }
static void FreeMutex( vlc_value_t *p_val ) { vlc_mutex_destroy( (vlc_mutex_t*)p_val->p_address ); free( p_val->p_address ); }

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
        case VLC_VAR_MUTEX:
            FreeMutex( &p_val->p_list->p_values[i] );
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
list_ops   = { CmpAddress, DupList,   FreeList,   },
mutex_ops  = { CmpAddress, DupDummy,  FreeMutex,  },
string_ops = { CmpString,  DupString, FreeString, },
time_ops   = { CmpTime,    DupDummy,  FreeDummy,  };

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      GetUnused   ( vlc_object_t *, const char * );
static uint32_t HashString  ( const char * );
static int      Insert      ( variable_t *, int, const char * );
static int      InsertInner ( variable_t *, int, uint32_t );
static int      Lookup      ( variable_t *, size_t, const char * );

static void     CheckValue  ( variable_t *, vlc_value_t * );

static int      InheritValue( vlc_object_t *, const char *, vlc_value_t *,
                              int );
static int      TriggerCallback( vlc_object_t *, variable_t **, const char *,
                                 vlc_value_t );

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
int __var_Create( vlc_object_t *p_this, const char *psz_name, int i_type )
{
    variable_t var, *p_var = &var;
    static vlc_list_t dummy_null_list = {0, NULL, NULL};

    assert( p_this );

    memset( p_var, 0, sizeof(*p_var) );

    p_var->i_hash = HashString( psz_name );
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
        case VLC_VAR_ADDRESS:
            p_var->ops = &addr_ops;
            p_var->val.p_address = NULL;
            break;
        case VLC_VAR_MUTEX:
            p_var->ops = &mutex_ops;
            p_var->val.p_address = malloc( sizeof(vlc_mutex_t) );
            vlc_mutex_init( (vlc_mutex_t*)p_var->val.p_address );
            break;
        case VLC_VAR_LIST:
            p_var->ops = &list_ops;
            p_var->val.p_list = &dummy_null_list;
            break;
        default:
            p_var->ops = &void_ops;
            break;
    }

    if( i_type & VLC_VAR_DOINHERIT )
    {
        if( InheritValue( p_this, psz_name, &p_var->val, p_var->i_type ) )
            msg_Err( p_this, "cannot inherit value for %s", psz_name );
        else if( i_type & VLC_VAR_HASCHOICE )
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
    int i_new;

    vlc_mutex_lock( &p_priv->var_lock );

    /* FIXME: if the variable already exists, we don't duplicate it. But we
     * duplicate the lookups. It's not that serious, but if anyone finds some
     * time to rework Insert() so that only one lookup has to be done, feel
     * free to do so. */
    i_new = Lookup( p_priv->p_vars, p_priv->i_vars, psz_name );

    if( i_new >= 0 )
    {
        /* If the types differ, variable creation failed. */
        if( (i_type & VLC_VAR_CLASS) != (p_priv->p_vars[i_new].i_type & VLC_VAR_CLASS) )
        {
            msg_Err( p_this, "Variable '%s' (0x%04x) already exist but with a different type (0x%04x)",
                     psz_name, p_priv->p_vars[i_new].i_type, i_type );
            vlc_mutex_unlock( &p_priv->var_lock );
            return VLC_EBADVAR;
        }

        p_priv->p_vars[i_new].i_usage++;
        p_priv->p_vars[i_new].i_type |= ( i_type & VLC_VAR_ISCOMMAND );
        p_priv->p_vars[i_new].i_type |= ( i_type & VLC_VAR_HASCHOICE );
        vlc_mutex_unlock( &p_priv->var_lock );

        /* We did not need to create a new variable, free everything... */
        p_var->ops->pf_free( &p_var->val );
        free( p_var->psz_name );
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
        return VLC_SUCCESS;
    }

    i_new = Insert( p_priv->p_vars, p_priv->i_vars, psz_name );

    if( (p_priv->i_vars & 15) == 15 )
    {
        p_priv->p_vars = xrealloc( p_priv->p_vars,
                                  (p_priv->i_vars+17) * sizeof(variable_t) );
    }

    memmove( p_priv->p_vars + i_new + 1,
             p_priv->p_vars + i_new,
             (p_priv->i_vars - i_new) * sizeof(variable_t) );

    p_priv->i_vars++;

    p_priv->p_vars[i_new] = var;
    vlc_mutex_unlock( &p_priv->var_lock );

    return VLC_SUCCESS;
}

/**
 * Destroy a vlc variable
 *
 * Look for the variable and destroy it if it is found. As in var_Create we
 * do a call to memmove() but we have performance counterparts elsewhere.
 *
 * \param p_this The object that holds the variable
 * \param psz_name The name of the variable
 */
int __var_Destroy( vlc_object_t *p_this, const char *psz_name )
{
    int i_var, i;
    variable_t *p_var;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    i_var = GetUnused( p_this, psz_name );
    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return i_var;
    }

    p_var = &p_priv->p_vars[i_var];

    if( p_var->i_usage > 1 )
    {
        p_var->i_usage--;
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_SUCCESS;
    }

    /* Free value if needed */
    p_var->ops->pf_free( &p_var->val );

    /* Free choice list if needed */
    if( p_var->choices.i_count )
    {
        for( i = 0 ; i < p_var->choices.i_count ; i++ )
        {
            p_var->ops->pf_free( &p_var->choices.p_values[i] );
            free( p_var->choices_text.p_values[i].psz_string );
        }
        free( p_var->choices.p_values );
        free( p_var->choices_text.p_values );
    }

    /* Free callbacks if needed */
    free( p_var->p_entries );

    free( p_var->psz_name );
    free( p_var->psz_text );

    memmove( p_priv->p_vars + i_var,
             p_priv->p_vars + i_var + 1,
             (p_priv->i_vars - i_var - 1) * sizeof(variable_t) );

    if( (p_priv->i_vars & 15) == 0 )
    {
        variable_t *p_vars = realloc( p_priv->p_vars,
                                    (p_priv->i_vars) * sizeof( variable_t ) );
        if( p_vars )
            p_priv->p_vars = p_vars;
    }

    p_priv->i_vars--;

    vlc_mutex_unlock( &p_priv->var_lock );

    return VLC_SUCCESS;
}

/**
 * Perform an action on a variable
 *
 * \param p_this The object that holds the variable
 * \param psz_name The name of the variable
 * \param i_action The action to perform. Must be one of \ref var_action
 * \param p_val First action parameter
 * \param p_val2 Second action parameter
 */
int __var_Change( vlc_object_t *p_this, const char *psz_name,
                  int i_action, vlc_value_t *p_val, vlc_value_t *p_val2 )
{
    int i_var, i;
    variable_t *p_var;
    vlc_value_t oldval;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    i_var = Lookup( p_priv->p_vars, p_priv->i_vars, psz_name );

    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_ENOVAR;
    }

    p_var = &p_priv->p_vars[i_var];

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
            {
                *p_val = p_var->min;
            }
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
            {
                *p_val = p_var->max;
            }
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
            {
                *p_val = p_var->step;
            }
            break;
        case VLC_VAR_ADDCHOICE:
            i = p_var->choices.i_count;

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
        case VLC_VAR_DELCHOICE:
            for( i = 0 ; i < p_var->choices.i_count ; i++ )
            {
                if( p_var->ops->pf_cmp( p_var->choices.p_values[i], *p_val ) == 0 )
                {
                    break;
                }
            }

            if( i == p_var->choices.i_count )
            {
                /* Not found */
                vlc_mutex_unlock( &p_priv->var_lock );
                return VLC_EGENERIC;
            }

            if( p_var->i_default > i )
            {
                p_var->i_default--;
            }
            else if( p_var->i_default == i )
            {
                p_var->i_default = -1;
            }

            p_var->ops->pf_free( &p_var->choices.p_values[i] );
            free( p_var->choices_text.p_values[i].psz_string );
            REMOVE_ELEM( p_var->choices.p_values, p_var->choices.i_count, i );
            REMOVE_ELEM( p_var->choices_text.p_values,
                         p_var->choices_text.i_count, i );

            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_CHOICESCOUNT:
            p_val->i_int = p_var->choices.i_count;
            break;
        case VLC_VAR_CLEARCHOICES:
            for( i = 0 ; i < p_var->choices.i_count ; i++ )
            {
                p_var->ops->pf_free( &p_var->choices.p_values[i] );
            }
            for( i = 0 ; i < p_var->choices_text.i_count ; i++ )
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
            /* FIXME: the list is sorted, dude. Use something cleverer. */
            for( i = 0 ; i < p_var->choices.i_count ; i++ )
            {
                if( p_var->ops->pf_cmp( p_var->choices.p_values[i], *p_val ) == 0 )
                {
                    break;
                }
            }

            if( i == p_var->choices.i_count )
            {
                /* Not found */
                break;
            }

            p_var->i_default = i;
            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_SETVALUE:
            /* Duplicate data if needed */
            p_var->ops->pf_dup( p_val );
            /* Backup needed stuff */
            oldval = p_var->val;
            /* Check boundaries and list */
            CheckValue( p_var, p_val );
            /* Set the variable */
            p_var->val = *p_val;
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
            for( i = 0 ; i < p_var->choices.i_count ; i++ )
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

    return VLC_SUCCESS;
}


/**
 * Perform a Get and Set on a variable
 *
 * \param p_this: The object that hold the variable
 * \param psz_name: the name of the variable
 * \param i_action: the action to perform
 * \param p_val: The action parameter
 * \return vlc error codes
 */
int __var_GetAndSet( vlc_object_t *p_this, const char *psz_name, int i_action,
                     vlc_value_t val )
{
    int i_var;
    int i_ret = VLC_SUCCESS;
    variable_t *p_var;
    vlc_value_t oldval;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );
    i_var = GetUnused( p_this, psz_name );
    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return i_var;
    }

    p_var = &p_priv->p_vars[i_var];

    /* Duplicated data if needed */
    //p_var->ops->pf_dup( &val );

    /* Backup needed stuff */
    oldval = p_var->val;

    /* depending of the action requiered */
    switch( i_action )
    {
    case VLC_VAR_TOGGLE_BOOL:
        assert( ( p_var->i_type & VLC_VAR_BOOL ) == VLC_VAR_BOOL );
        p_var->val.b_bool = !p_var->val.b_bool;
        break;
    case VLC_VAR_INTEGER_INCDEC:
        assert( ( p_var->i_type & VLC_VAR_INTEGER ) == VLC_VAR_INTEGER );
        p_var->val.i_int += val.i_int;
        break;
    default:
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_EGENERIC;
    }

    /*  Check boundaries */
    CheckValue( p_var, &p_var->val );

    /* Deal with callbacks.*/
    if( p_var->i_entries )
        i_ret = TriggerCallback( p_this, &p_var, psz_name, oldval );

    vlc_mutex_unlock( &p_priv->var_lock );

    return i_ret;
}


/**
 * Request a variable's type
 *
 * \return The variable type if it exists, or 0 if the
 * variable could not be found.
 * \see \ref var_type
 */
int __var_Type( vlc_object_t *p_this, const char *psz_name )
{
    int i_var, i_type;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    i_var = Lookup( p_priv->p_vars, p_priv->i_vars, psz_name );

    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return 0;
    }

    i_type = p_priv->p_vars[i_var].i_type;

    vlc_mutex_unlock( &p_priv->var_lock );

    return i_type;
}

int var_SetChecked( vlc_object_t *p_this, const char *psz_name,
                    int expected_type, vlc_value_t val )
{
    int i_var;
    int i_ret = VLC_SUCCESS;
    variable_t *p_var;
    vlc_value_t oldval;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    i_var = GetUnused( p_this, psz_name );
    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return i_var;
    }

    p_var = &p_priv->p_vars[i_var];
    assert( expected_type == 0 ||
            (p_var->i_type & VLC_VAR_CLASS) == expected_type );

    /* Duplicate data if needed */
    p_var->ops->pf_dup( &val );

    /* Backup needed stuff */
    oldval = p_var->val;

    /* Check boundaries and list */
    CheckValue( p_var, &val );

    /* Set the variable */
    p_var->val = val;

    /* Deal with callbacks */
    if( p_var->i_entries )
        i_ret = TriggerCallback( p_this, &p_var, psz_name, oldval );

    /* Free data if needed */
    p_var->ops->pf_free( &oldval );

    vlc_mutex_unlock( &p_priv->var_lock );

    return i_ret;
}


/**
 * Set a variable's value
 *
 * \param p_this The object that hold the variable
 * \param psz_name The name of the variable
 * \param val the value to set
 */
int __var_Set( vlc_object_t *p_this, const char *psz_name, vlc_value_t val )
{
    return var_SetChecked( p_this, psz_name, 0, val );
}

int var_GetChecked( vlc_object_t *p_this, const char *psz_name,
                    int expected_type, vlc_value_t *p_val )
{
    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );
    int i_var, err = VLC_SUCCESS;

    vlc_mutex_lock( &p_priv->var_lock );

    i_var = Lookup( p_priv->p_vars, p_priv->i_vars, psz_name );
    if( i_var >= 0 )
    {
        variable_t *p_var = &p_priv->p_vars[i_var];

        assert( expected_type == 0 ||
                (p_var->i_type & VLC_VAR_CLASS) == expected_type );

        /* Really get the variable */
        *p_val = p_var->val;

#ifndef NDEBUG
        /* Alert if the type is VLC_VAR_VOID */
        if( ( p_var->i_type & VLC_VAR_TYPE ) == VLC_VAR_VOID )
            msg_Warn( p_this, "Calling var_GetVoid on the void variable '%s' (0x%04x)", psz_name, p_var->i_type );
#endif

        /* Duplicate value if needed */
        p_var->ops->pf_dup( p_val );
    }
    else
        err = VLC_ENOVAR;

    vlc_mutex_unlock( &p_priv->var_lock );
    return err;
}

/**
 * Get a variable's value
 *
 * \param p_this The object that holds the variable
 * \param psz_name The name of the variable
 * \param p_val Pointer to a vlc_value_t that will hold the variable's value
 *              after the function is finished
 */
int __var_Get( vlc_object_t *p_this, const char *psz_name, vlc_value_t *p_val )
{
    return var_GetChecked( p_this, psz_name, 0, p_val );
}

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
int __var_AddCallback( vlc_object_t *p_this, const char *psz_name,
                       vlc_callback_t pf_callback, void *p_data )
{
    int i_var;
    variable_t *p_var;
    callback_entry_t entry;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    entry.pf_callback = pf_callback;
    entry.p_data = p_data;

    vlc_mutex_lock( &p_priv->var_lock );

    i_var = GetUnused( p_this, psz_name );
    if( i_var < 0 )
    {
#ifndef NDEBUG
        msg_Warn( p_this, "Failed to add a callback to the non-existing "
                          "variable '%s'", psz_name );
#endif
        vlc_mutex_unlock( &p_priv->var_lock );
        return i_var;
    }

    p_var = &p_priv->p_vars[i_var];

    INSERT_ELEM( p_var->p_entries,
                 p_var->i_entries,
                 p_var->i_entries,
                 entry );

    vlc_mutex_unlock( &p_priv->var_lock );

    return VLC_SUCCESS;
}

/**
 * Remove a callback from a variable
 *
 * pf_callback and p_data have to be given again, because different objects
 * might have registered the same callback function.
 */
int __var_DelCallback( vlc_object_t *p_this, const char *psz_name,
                       vlc_callback_t pf_callback, void *p_data )
{
    int i_entry, i_var;
    variable_t *p_var;
#ifndef NDEBUG
    bool b_found_similar = false;
#endif

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    i_var = GetUnused( p_this, psz_name );
    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return i_var;
    }

    p_var = &p_priv->p_vars[i_var];

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

/**
 * Trigger callback on a variable
 *
 * \param p_this The object that hold the variable
 * \param psz_name The name of the variable
 */
int __var_TriggerCallback( vlc_object_t *p_this, const char *psz_name )
{
    int i_var;
    int i_ret = VLC_SUCCESS;
    variable_t *p_var;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    vlc_mutex_lock( &p_priv->var_lock );

    i_var = GetUnused( p_this, psz_name );
    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return i_var;
    }

    p_var = &p_priv->p_vars[i_var];

    /* Deal with callbacks. Tell we're in a callback, release the lock,
     * call stored functions, retake the lock. */
    if( p_var->i_entries )
        i_ret = TriggerCallback( p_this, &p_var, psz_name, p_var->val );

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

    /* FIXME: :programs should be handled generically */
    if( !strcmp( psz_name, "programs" ) )
        i_type = VLC_VAR_LIST;
    else
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
    if( !trusted )
    {
        module_config_t *p_config = config_FindConfig( p_obj, psz_name );
        if( !p_config || !p_config->b_safe )
        {
            msg_Err( p_obj, "unsafe option \"%s\" has been ignored for "
                            "security reasons", psz_name );
            free( psz_name );
            return;
        }
    }

    /* Create the variable in the input object.
     * Children of the input object will be able to retreive this value
     * thanks to the inheritance property of the object variables. */
    __var_Create( p_obj, psz_name, i_type );

    switch( i_type )
    {
    case VLC_VAR_BOOL:
        val.b_bool = !b_isno;
        break;

    case VLC_VAR_INTEGER:
        val.i_int = strtol( psz_value, NULL, 0 );
        break;

    case VLC_VAR_FLOAT:
        val.f_float = us_atof( psz_value );
        break;

    case VLC_VAR_STRING:
    case VLC_VAR_MODULE:
    case VLC_VAR_FILE:
    case VLC_VAR_DIRECTORY:
        val.psz_string = psz_value;
        break;

    case VLC_VAR_LIST:
    {
        char *psz_orig, *psz_var;
        vlc_list_t *p_list = malloc(sizeof(vlc_list_t));
        val.p_list = p_list;
        p_list->i_count = 0;

        psz_var = psz_orig = strdup(psz_value);
        while( psz_var && *psz_var )
        {
            char *psz_item = psz_var;
            vlc_value_t val2;
            while( *psz_var && *psz_var != ',' ) psz_var++;
            if( *psz_var == ',' )
            {
                *psz_var = '\0';
                psz_var++;
            }
            val2.i_int = strtol( psz_item, NULL, 0 );
            INSERT_ELEM( p_list->p_values, p_list->i_count,
                         p_list->i_count, val2 );
            /* p_list->i_count is incremented twice by INSERT_ELEM */
            p_list->i_count--;
            INSERT_ELEM( p_list->pi_types, p_list->i_count,
                         p_list->i_count, VLC_VAR_INTEGER );
        }
        free( psz_orig );
        break;
    }

    default:
        goto cleanup;
    }

    __var_Set( p_obj, psz_name, val );

    /* If that's a list, remove all elements allocated */
    if( i_type == VLC_VAR_LIST )
        FreeList( &val );

cleanup:
    free( psz_name );
}


/* Following functions are local */

/*****************************************************************************
 * GetUnused: find an unused (not in callback) variable from its name
 *****************************************************************************
 * We do i_tries tries before giving up, just in case the variable is being
 * modified and called from a callback.
 *****************************************************************************/
static int GetUnused( vlc_object_t *p_this, const char *psz_name )
{
    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    while( true )
    {
        int i_var;

        i_var = Lookup( p_priv->p_vars, p_priv->i_vars, psz_name );
        if( i_var < 0 )
        {
            return VLC_ENOVAR;
        }

        if( ! p_priv->p_vars[i_var].b_incallback )
        {
            return i_var;
        }

        mutex_cleanup_push( &p_priv->var_lock );
        vlc_cond_wait( &p_priv->var_wait, &p_priv->var_lock );
        vlc_cleanup_pop( );
    }
}

/*****************************************************************************
 * HashString: our cool hash function
 *****************************************************************************
 * This function is not intended to be crypto-secure, we only want it to be
 * fast and not suck too much. This one is pretty fast and did 0 collisions
 * in wenglish's dictionary.
 *****************************************************************************/
static uint32_t HashString( const char *psz_string )
{
    uint32_t i_hash = 0;

    while( *psz_string )
    {
        i_hash += *psz_string++;
        i_hash += i_hash << 10;
        i_hash ^= i_hash >> 8;
    }

    return i_hash;
}

/*****************************************************************************
 * Insert: find an empty slot to insert a new variable
 *****************************************************************************
 * We use a recursive inner function indexed on the hash. This function does
 * nothing in the rare cases where a collision may occur, see Lookup()
 * to see how we handle them.
 * XXX: does this really need to be written recursively?
 *****************************************************************************/
static int Insert( variable_t *p_vars, int i_count, const char *psz_name )
{
    if( i_count == 0 )
    {
        return 0;
    }

    return InsertInner( p_vars, i_count, HashString( psz_name ) );
}

static int InsertInner( variable_t *p_vars, int i_count, uint32_t i_hash )
{
    int i_middle;

    if( i_hash <= p_vars[0].i_hash )
    {
        return 0;
    }

    if( i_hash >= p_vars[i_count - 1].i_hash )
    {
        return i_count;
    }

    i_middle = i_count / 2;

    /* We know that 0 < i_middle */
    if( i_hash < p_vars[i_middle].i_hash )
    {
        return InsertInner( p_vars, i_middle, i_hash );
    }

    /* We know that i_middle + 1 < i_count */
    if( i_hash > p_vars[i_middle + 1].i_hash )
    {
        return i_middle + 1 + InsertInner( p_vars + i_middle + 1,
                                           i_count - i_middle - 1,
                                           i_hash );
    }

    return i_middle + 1;
}

static int u32cmp( const void *key, const void *data )
{
    const variable_t *p_var = data;
    uint32_t hash = *(const uint32_t *)key ;

    if( hash > p_var->i_hash )
        return 1;
    if( hash < p_var->i_hash )
        return -1;
    return 0;
}

/*****************************************************************************
 * Lookup: find an existing variable given its name
 *****************************************************************************
 * We use a recursive inner function indexed on the hash. Care is taken of
 * possible hash collisions.
 *****************************************************************************/
static int Lookup( variable_t *p_vars, size_t i_count, const char *psz_name )
{
    variable_t *p_var;
    uint32_t i_hash;

    i_hash = HashString( psz_name );
    p_var = bsearch( &i_hash, p_vars, i_count, sizeof( *p_var ), u32cmp );

    /* Hash not found */
    if( p_var == NULL )
        return -1;

    assert( i_count > 0 );

    /* Find the first entry with the right hash */
    while( (p_var > p_vars) && (i_hash == p_var[-1].i_hash) )
        p_var--;

    assert( p_var->i_hash == i_hash );

    /* Hash collision should be very unlikely, but we cannot guarantee
     * it will never happen. So we do an exhaustive search amongst all
     * entries with the same hash. Typically, there is only one anyway. */
    for( variable_t *p_end = p_vars + i_count;
         (p_var < p_end) && (i_hash == p_var->i_hash);
         p_var++ )
    {
        if( !strcmp( psz_name, p_var->psz_name ) )
            return p_var - p_vars;
    }

    /* Hash found, but entry not found */
    return -1;
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

/*****************************************************************************
 * InheritValue: try to inherit the value of this variable from the closest
 * ancestor objects or ultimately from the configuration.
 * The function should always be entered with the object var_lock locked.
 *****************************************************************************/
static int InheritValue( vlc_object_t *p_this, const char *psz_name,
                         vlc_value_t *p_val, int i_type )
{
    i_type &= VLC_VAR_CLASS;
    for( vlc_object_t *obj = p_this; obj != NULL; obj = obj->p_parent )
        if( var_GetChecked( p_this, psz_name, i_type, p_val ) == VLC_SUCCESS )
            return VLC_SUCCESS;

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
        case VLC_VAR_LIST:
        {
            char *psz_orig, *psz_var;
            vlc_list_t *p_list = malloc(sizeof(vlc_list_t));
            p_val->p_list = p_list;
            p_list->i_count = 0;

            psz_var = psz_orig = config_GetPsz( p_this, psz_name );
            while( psz_var && *psz_var )
            {
                char *psz_item = psz_var;
                vlc_value_t val;
                while( *psz_var && *psz_var != ',' ) psz_var++;
                if( *psz_var == ',' )
                {
                    *psz_var = '\0';
                    psz_var++;
                }
                val.i_int = strtol( psz_item, NULL, 0 );
                INSERT_ELEM( p_list->p_values, p_list->i_count,
                             p_list->i_count, val );
                /* p_list->i_count is incremented twice by INSERT_ELEM */
                p_list->i_count--;
                INSERT_ELEM( p_list->pi_types, p_list->i_count,
                             p_list->i_count, VLC_VAR_INTEGER );
            }
            free( psz_orig );
            break;
        }
        default:
            msg_Warn( p_this, "Could not inherit value for var %s "
                              "from config. Invalid Type", psz_name );
            return VLC_ENOOBJ;
            break;
    }
    /*msg_Dbg( p_this, "Inherited value for var %s from config", psz_name );*/
    return VLC_SUCCESS;
}


/**********************************************************************
 * Trigger the callbacks.
 * Tell we're in a callback, release the lock, call stored functions,
 * retake the lock.
 **********************************************************************/
static int TriggerCallback( vlc_object_t *p_this, variable_t **pp_var,
                            const char *psz_name, vlc_value_t oldval )
{
    int i_var;
    int i_entries = (*pp_var)->i_entries;
    callback_entry_t *p_entries = (*pp_var)->p_entries;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    (*pp_var)->b_incallback = true;
    vlc_mutex_unlock( &p_priv->var_lock );

    /* The real calls */
    for( ; i_entries-- ; )
    {
        p_entries[i_entries].pf_callback( p_this, psz_name, oldval, (*pp_var)->val,
                                          p_entries[i_entries].p_data );
    }

    vlc_mutex_lock( &p_priv->var_lock );

    i_var = Lookup( p_priv->p_vars, p_priv->i_vars, psz_name );
    if( i_var < 0 )
    {
        msg_Err( p_this, "variable %s has disappeared", psz_name );
        return VLC_ENOVAR;
     }

     *pp_var = &p_priv->p_vars[i_var];
     (*pp_var)->b_incallback = false;
     vlc_cond_broadcast( &p_priv->var_wait );

     return VLC_SUCCESS;
}


/**********************************************************************
 * Execute a var command on an object identified by its name
 **********************************************************************/
int __var_Command( vlc_object_t *p_this, const char *psz_name,
                   const char *psz_cmd, const char *psz_arg, char **psz_msg )
{
    vlc_object_t *p_obj = vlc_object_find_name( p_this->p_libvlc,
                                                psz_name, FIND_CHILD );
    int i_type, i_ret;

    if( !p_obj )
    {
        if( psz_msg )
            *psz_msg = strdup( "Unknown destination object." );
        return VLC_ENOOBJ;
    }

    i_type = var_Type( p_obj, psz_cmd );
    if( !( i_type&VLC_VAR_ISCOMMAND ) )
    {
        vlc_object_release( p_obj );
        if( psz_msg )
            *psz_msg = strdup( "Variable doesn't exist or isn't a command." );
        return VLC_EGENERIC;
    }

    i_type &= VLC_VAR_CLASS;
    switch( i_type )
    {
        case VLC_VAR_INTEGER:
            i_ret = var_SetInteger( p_obj, psz_cmd, atoi( psz_arg ) );
            break;
        case VLC_VAR_FLOAT:
            i_ret = var_SetFloat( p_obj, psz_cmd, us_atof( psz_arg ) );
            break;
        case VLC_VAR_STRING:
            i_ret = var_SetString( p_obj, psz_cmd, psz_arg );
            break;
        case VLC_VAR_BOOL:
            i_ret = var_SetBool( p_obj, psz_cmd, atoi( psz_arg ) );
            break;
        default:
            i_ret = VLC_EGENERIC;
            break;
    }

    vlc_object_release( p_obj );

    if( psz_msg )
    {
        if( asprintf( psz_msg, "%s on object %s returned %i (%s)",
                  psz_cmd, psz_name, i_ret, vlc_error( i_ret ) ) == -1)
            *psz_msg = NULL;
    }

    return i_ret;
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
