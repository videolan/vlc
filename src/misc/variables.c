/*****************************************************************************
 * variables.c: routines for object variables handling
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
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
#include <vlc/vlc.h>

#ifdef HAVE_STDLIB_H
#   include <stdlib.h>                                          /* realloc() */
#endif

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
static int CmpString( vlc_value_t v, vlc_value_t w ) { return strcmp( v.psz_string, w.psz_string ); }
static int CmpFloat( vlc_value_t v, vlc_value_t w ) { return v.f_float == w.f_float ? 0 : v.f_float > w.f_float ? 1 : -1; }
static int CmpAddress( vlc_value_t v, vlc_value_t w ) { return v.p_address == w.p_address ? 0 : v.p_address > w.p_address ? 1 : -1; }

/*****************************************************************************
 * Local duplication functions, and local deallocation functions
 *****************************************************************************/
static void DupDummy( vlc_value_t *p_val ) { (void)p_val; /* unused */ }
static void DupString( vlc_value_t *p_val ) { p_val->psz_string = strdup( p_val->psz_string ); }

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
        switch( p_val->p_list->pi_types[i] & VLC_VAR_TYPE )
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
        switch( p_val->p_list->pi_types[i] & VLC_VAR_TYPE )
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

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int      GetUnused   ( vlc_object_t *, const char * );
static uint32_t HashString  ( const char * );
static int      Insert      ( variable_t *, int, const char * );
static int      InsertInner ( variable_t *, int, uint32_t );
static int      Lookup      ( variable_t *, int, const char * );
static int      LookupInner ( variable_t *, int, uint32_t );

static void     CheckValue  ( variable_t *, vlc_value_t * );

static int      InheritValue( vlc_object_t *, const char *, vlc_value_t *,
                              int );

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
    int i_new;
    variable_t *p_var;
    static vlc_list_t dummy_null_list = {0, NULL, NULL};

    vlc_mutex_lock( &p_this->var_lock );

    /* FIXME: if the variable already exists, we don't duplicate it. But we
     * duplicate the lookups. It's not that serious, but if anyone finds some
     * time to rework Insert() so that only one lookup has to be done, feel
     * free to do so. */
    i_new = Lookup( p_this->p_vars, p_this->i_vars, psz_name );

    if( i_new >= 0 )
    {
        /* If the types differ, variable creation failed. */
        if( (i_type & ~VLC_VAR_DOINHERIT) != p_this->p_vars[i_new].i_type )
        {
            vlc_mutex_unlock( &p_this->var_lock );
            return VLC_EBADVAR;
        }

        p_this->p_vars[i_new].i_usage++;
        vlc_mutex_unlock( &p_this->var_lock );
        return VLC_SUCCESS;
    }

    i_new = Insert( p_this->p_vars, p_this->i_vars, psz_name );

    if( (p_this->i_vars & 15) == 15 )
    {
        p_this->p_vars = realloc( p_this->p_vars,
                                  (p_this->i_vars+17) * sizeof(variable_t) );
    }

    memmove( p_this->p_vars + i_new + 1,
             p_this->p_vars + i_new,
             (p_this->i_vars - i_new) * sizeof(variable_t) );

    p_this->i_vars++;

    p_var = &p_this->p_vars[i_new];
    memset( p_var, 0, sizeof(*p_var) );

    p_var->i_hash = HashString( psz_name );
    p_var->psz_name = strdup( psz_name );
    p_var->psz_text = NULL;

    p_var->i_type = i_type & ~VLC_VAR_DOINHERIT;
    memset( &p_var->val, 0, sizeof(vlc_value_t) );

    p_var->pf_dup = DupDummy;
    p_var->pf_free = FreeDummy;

    p_var->i_usage = 1;

    p_var->i_default = -1;
    p_var->choices.i_count = 0;
    p_var->choices.p_values = NULL;
    p_var->choices_text.i_count = 0;
    p_var->choices_text.p_values = NULL;

    p_var->b_incallback = VLC_FALSE;
    p_var->i_entries = 0;
    p_var->p_entries = NULL;

    /* Always initialize the variable, even if it is a list variable; this
     * will lead to errors if the variable is not initialized, but it will
     * not cause crashes in the variable handling. */
    switch( i_type & VLC_VAR_TYPE )
    {
        case VLC_VAR_BOOL:
            p_var->pf_cmp = CmpBool;
            p_var->val.b_bool = VLC_FALSE;
            break;
        case VLC_VAR_INTEGER:
        case VLC_VAR_HOTKEY:
            p_var->pf_cmp = CmpInt;
            p_var->val.i_int = 0;
            break;
        case VLC_VAR_STRING:
        case VLC_VAR_MODULE:
        case VLC_VAR_FILE:
        case VLC_VAR_DIRECTORY:
        case VLC_VAR_VARIABLE:
            p_var->pf_cmp = CmpString;
            p_var->pf_dup = DupString;
            p_var->pf_free = FreeString;
            p_var->val.psz_string = "";
            break;
        case VLC_VAR_FLOAT:
            p_var->pf_cmp = CmpFloat;
            p_var->val.f_float = 0.0;
            break;
        case VLC_VAR_TIME:
            p_var->pf_cmp = CmpTime;
            p_var->val.i_time = 0;
            break;
        case VLC_VAR_ADDRESS:
            p_var->pf_cmp = CmpAddress;
            p_var->val.p_address = NULL;
            break;
        case VLC_VAR_MUTEX:
            p_var->pf_cmp = CmpAddress;
            p_var->pf_free = FreeMutex;
            p_var->val.p_address = malloc( sizeof(vlc_mutex_t) );
            vlc_mutex_init( p_this, (vlc_mutex_t*)p_var->val.p_address );
            break;
        case VLC_VAR_LIST:
            p_var->pf_cmp = CmpAddress;
            p_var->pf_dup = DupList;
            p_var->pf_free = FreeList;
            p_var->val.p_list = &dummy_null_list;
            break;
    }

    /* Duplicate the default data we stored. */
    p_var->pf_dup( &p_var->val );

    if( i_type & VLC_VAR_DOINHERIT )
    {
        vlc_value_t val;

        if( InheritValue( p_this, psz_name, &val, p_var->i_type )
            == VLC_SUCCESS );
        {
            /* Free data if needed */
            p_var->pf_free( &p_var->val );
            /* Set the variable */
            p_var->val = val;

            if( i_type & VLC_VAR_HASCHOICE )
            {
                /* We must add the inherited value to our choice list */
                p_var->i_default = 0;

                INSERT_ELEM( p_var->choices.p_values, p_var->choices.i_count,
                             0, val );
                INSERT_ELEM( p_var->choices_text.p_values,
                             p_var->choices_text.i_count, 0, val );
                p_var->pf_dup( &p_var->choices.p_values[0] );
                p_var->choices_text.p_values[0].psz_string = NULL;
            }
        }
    }

    vlc_mutex_unlock( &p_this->var_lock );

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

    vlc_mutex_lock( &p_this->var_lock );

    i_var = GetUnused( p_this, psz_name );
    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return i_var;
    }

    p_var = &p_this->p_vars[i_var];

    if( p_var->i_usage > 1 )
    {
        p_var->i_usage--;
        vlc_mutex_unlock( &p_this->var_lock );
        return VLC_SUCCESS;
    }

    /* Free value if needed */
    p_var->pf_free( &p_var->val );

    /* Free choice list if needed */
    if( p_var->choices.i_count )
    {
        for( i = 0 ; i < p_var->choices.i_count ; i++ )
        {
            p_var->pf_free( &p_var->choices.p_values[i] );
            if( p_var->choices_text.p_values[i].psz_string )
                free( p_var->choices_text.p_values[i].psz_string );
        }
        free( p_var->choices.p_values );
        free( p_var->choices_text.p_values );
    }

    /* Free callbacks if needed */
    if( p_var->p_entries )
    {
        free( p_var->p_entries );
    }

    free( p_var->psz_name );
    if( p_var->psz_text ) free( p_var->psz_text );

    memmove( p_this->p_vars + i_var,
             p_this->p_vars + i_var + 1,
             (p_this->i_vars - i_var - 1) * sizeof(variable_t) );

    if( (p_this->i_vars & 15) == 0 )
    {
        p_this->p_vars = realloc( p_this->p_vars,
                          (p_this->i_vars) * sizeof( variable_t ) );
    }

    p_this->i_vars--;

    vlc_mutex_unlock( &p_this->var_lock );

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

    vlc_mutex_lock( &p_this->var_lock );

    i_var = Lookup( p_this->p_vars, p_this->i_vars, psz_name );

    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return VLC_ENOVAR;
    }

    p_var = &p_this->p_vars[i_var];

    switch( i_action )
    {
        case VLC_VAR_SETMIN:
            if( p_var->i_type & VLC_VAR_HASMIN )
            {
                p_var->pf_free( &p_var->min );
            }
            p_var->i_type |= VLC_VAR_HASMIN;
            p_var->min = *p_val;
            p_var->pf_dup( &p_var->min );
            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_SETMAX:
            if( p_var->i_type & VLC_VAR_HASMAX )
            {
                p_var->pf_free( &p_var->max );
            }
            p_var->i_type |= VLC_VAR_HASMAX;
            p_var->max = *p_val;
            p_var->pf_dup( &p_var->max );
            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_SETSTEP:
            if( p_var->i_type & VLC_VAR_HASSTEP )
            {
                p_var->pf_free( &p_var->step );
            }
            p_var->i_type |= VLC_VAR_HASSTEP;
            p_var->step = *p_val;
            p_var->pf_dup( &p_var->step );
            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_ADDCHOICE:
            /* FIXME: the list is sorted, dude. Use something cleverer. */
            for( i = p_var->choices.i_count ; i-- ; )
            {
                if( p_var->pf_cmp( p_var->choices.p_values[i], *p_val ) < 0 )
                {
                    break;
                }
            }

            /* The new place is i+1 */
            i++;

            if( p_var->i_default >= i )
            {
                p_var->i_default++;
            }

            INSERT_ELEM( p_var->choices.p_values, p_var->choices.i_count,
                         i, *p_val );
            INSERT_ELEM( p_var->choices_text.p_values,
                         p_var->choices_text.i_count, i, *p_val );
            p_var->pf_dup( &p_var->choices.p_values[i] );
            p_var->choices_text.p_values[i].psz_string =
                ( p_val2 && p_val2->psz_string ) ?
                strdup( p_val2->psz_string ) : NULL;

            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_DELCHOICE:
            /* FIXME: the list is sorted, dude. Use something cleverer. */
            for( i = 0 ; i < p_var->choices.i_count ; i++ )
            {
                if( p_var->pf_cmp( p_var->choices.p_values[i], *p_val ) == 0 )
                {
                    break;
                }
            }

            if( i == p_var->choices.i_count )
            {
                /* Not found */
                vlc_mutex_unlock( &p_this->var_lock );
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

            p_var->pf_free( &p_var->choices.p_values[i] );
            if( p_var->choices_text.p_values[i].psz_string )
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
                p_var->pf_free( &p_var->choices.p_values[i] );
            }
            for( i = 0 ; i < p_var->choices_text.i_count ; i++ )
            {
                if( p_var->choices_text.p_values[i].psz_string )
                    free( p_var->choices_text.p_values[i].psz_string );
            }
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
                if( p_var->pf_cmp( p_var->choices.p_values[i], *p_val ) == 0 )
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
            p_var->pf_dup( p_val );
            /* Backup needed stuff */
            oldval = p_var->val;
            /* Check boundaries and list */
            CheckValue( p_var, p_val );
            /* Set the variable */
            p_var->val = *p_val;
            /* Free data if needed */
            p_var->pf_free( &oldval );
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
                p_var->pf_dup( &p_val->p_list->p_values[i] );
                if( p_val2 )
                {
                    p_val2->p_list->p_values[i].psz_string =
                        p_var->choices_text.p_values[i].psz_string ?
                    strdup(p_var->choices_text.p_values[i].psz_string) : NULL;
                    p_val2->p_list->pi_types[i] = VLC_VAR_STRING;
                }
            }
            break;
        case VLC_VAR_FREELIST:
            FreeList( p_val );
            if( p_val2 && p_val2->p_list )
            {
                for( i = 0; i < p_val2->p_list->i_count; i++ )
                    if( p_val2->p_list->p_values[i].psz_string )
                        free( p_val2->p_list->p_values[i].psz_string );
                if( p_val2->p_list->i_count )
                {
                    free( p_val2->p_list->p_values );
                    free( p_val2->p_list->pi_types );
                }
                free( p_val2->p_list );
            }
            break;
        case VLC_VAR_SETTEXT:
            if( p_var->psz_text ) free( p_var->psz_text );
            if( p_val && p_val->psz_string )
                p_var->psz_text = strdup( p_val->psz_string );
            break;
        case VLC_VAR_GETTEXT:
            p_val->psz_string = NULL;
            if( p_var->psz_text )
            {
                p_val->psz_string = strdup( p_var->psz_text );
            }
            break;
        case VLC_VAR_INHERITVALUE:
            {
                vlc_value_t val;

                if( InheritValue( p_this, psz_name, &val, p_var->i_type )
                    == VLC_SUCCESS );
                {
                    /* Duplicate already done */

                    /* Backup needed stuff */
                    oldval = p_var->val;
                    /* Check boundaries and list */
                    CheckValue( p_var, &val );
                    /* Set the variable */
                    p_var->val = val;
                    /* Free data if needed */
                    p_var->pf_free( &oldval );
                }

                if( p_val )
                {
                    *p_val = p_var->val;
                    p_var->pf_dup( p_val );
                }
            }
            break;
        case VLC_VAR_TRIGGER_CALLBACKS:
            {
                /* Deal with callbacks. Tell we're in a callback, release the lock,
                 * call stored functions, retake the lock. */
                if( p_var->i_entries )
                {
                    int i_var;
                    int i_entries = p_var->i_entries;
                    callback_entry_t *p_entries = p_var->p_entries;

                    p_var->b_incallback = VLC_TRUE;
                    vlc_mutex_unlock( &p_this->var_lock );

                    /* The real calls */
                    for( ; i_entries-- ; )
                    {
                        p_entries[i_entries].pf_callback( p_this, psz_name, p_var->val, p_var->val,
                                                          p_entries[i_entries].p_data );
                    }

                    vlc_mutex_lock( &p_this->var_lock );

                    i_var = Lookup( p_this->p_vars, p_this->i_vars, psz_name );
                    if( i_var < 0 )
                    {
                        msg_Err( p_this, "variable %s has disappeared", psz_name );
                        vlc_mutex_unlock( &p_this->var_lock );
                        return VLC_ENOVAR;
                    }

                    p_var = &p_this->p_vars[i_var];
                    p_var->b_incallback = VLC_FALSE;
                }
            }
            break;

        default:
            break;
    }

    vlc_mutex_unlock( &p_this->var_lock );

    return VLC_SUCCESS;
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

    vlc_mutex_lock( &p_this->var_lock );

    i_var = Lookup( p_this->p_vars, p_this->i_vars, psz_name );

    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return 0;
    }

    i_type = p_this->p_vars[i_var].i_type;

    vlc_mutex_unlock( &p_this->var_lock );

    return i_type;
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
    int i_var;
    variable_t *p_var;
    vlc_value_t oldval;

    vlc_mutex_lock( &p_this->var_lock );

    i_var = GetUnused( p_this, psz_name );
    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return i_var;
    }

    p_var = &p_this->p_vars[i_var];

    /* Duplicate data if needed */
    p_var->pf_dup( &val );

    /* Backup needed stuff */
    oldval = p_var->val;

    /* Check boundaries and list */
    CheckValue( p_var, &val );

    /* Set the variable */
    p_var->val = val;

    /* Deal with callbacks. Tell we're in a callback, release the lock,
     * call stored functions, retake the lock. */
    if( p_var->i_entries )
    {
        int i_var;
        int i_entries = p_var->i_entries;
        callback_entry_t *p_entries = p_var->p_entries;

        p_var->b_incallback = VLC_TRUE;
        vlc_mutex_unlock( &p_this->var_lock );

        /* The real calls */
        for( ; i_entries-- ; )
        {
            p_entries[i_entries].pf_callback( p_this, psz_name, oldval, val,
                                              p_entries[i_entries].p_data );
        }

        vlc_mutex_lock( &p_this->var_lock );

        i_var = Lookup( p_this->p_vars, p_this->i_vars, psz_name );
        if( i_var < 0 )
        {
            msg_Err( p_this, "variable %s has disappeared", psz_name );
            vlc_mutex_unlock( &p_this->var_lock );
            return VLC_ENOVAR;
        }

        p_var = &p_this->p_vars[i_var];
        p_var->b_incallback = VLC_FALSE;
    }

    /* Free data if needed */
    p_var->pf_free( &oldval );

    vlc_mutex_unlock( &p_this->var_lock );

    return VLC_SUCCESS;
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
    int i_var;
    variable_t *p_var;

    vlc_mutex_lock( &p_this->var_lock );

    i_var = Lookup( p_this->p_vars, p_this->i_vars, psz_name );

    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return VLC_ENOVAR;
    }

    p_var = &p_this->p_vars[i_var];

    /* Really get the variable */
    *p_val = p_var->val;

    /* Duplicate value if needed */
    p_var->pf_dup( p_val );

    vlc_mutex_unlock( &p_this->var_lock );

    return VLC_SUCCESS;
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

    entry.pf_callback = pf_callback;
    entry.p_data = p_data;

    vlc_mutex_lock( &p_this->var_lock );

    i_var = GetUnused( p_this, psz_name );
    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return i_var;
    }

    p_var = &p_this->p_vars[i_var];

    INSERT_ELEM( p_var->p_entries,
                 p_var->i_entries,
                 p_var->i_entries,
                 entry );

    vlc_mutex_unlock( &p_this->var_lock );

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

    vlc_mutex_lock( &p_this->var_lock );

    i_var = GetUnused( p_this, psz_name );
    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return i_var;
    }

    p_var = &p_this->p_vars[i_var];

    for( i_entry = p_var->i_entries ; i_entry-- ; )
    {
        if( p_var->p_entries[i_entry].pf_callback == pf_callback
            && p_var->p_entries[i_entry].p_data == p_data )
        {
            break;
        }
    }

    if( i_entry < 0 )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return VLC_EGENERIC;
    }

    REMOVE_ELEM( p_var->p_entries, p_var->i_entries, i_entry );

    vlc_mutex_unlock( &p_this->var_lock );

    return VLC_SUCCESS;
}

/** Parse a stringified option
 * This function parse a string option and create the associated object
 * variable
 * The option must be of the form "[no[-]]foo[=bar]" where foo is the
 * option name and bar is the value of the option.
 * \param p_obj the object in which the variable must be created
 * \param psz_option the option to parse
 * \return nothing
 */
void __var_OptionParse( vlc_object_t *p_obj,const char *psz_option )
{
    char *psz_name = (char *)psz_option;
    char *psz_value = strchr( psz_option, '=' );
    int  i_name_len, i_type;
    vlc_bool_t b_isno = VLC_FALSE;
    vlc_value_t val;

    if( psz_value ) i_name_len = psz_value - psz_option;
    else i_name_len = strlen( psz_option );

    /* It's too much of an hassle to remove the ':' when we parse
     * the cmd line :) */
    if( i_name_len && *psz_name == ':' )
    {
        psz_name++;
        i_name_len--;
    }

    if( i_name_len == 0 ) return;

    psz_name = strndup( psz_name, i_name_len );
    if( psz_value ) psz_value++;

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

        b_isno = VLC_TRUE;
        i_type = config_GetType( p_obj, psz_name );

        if( !i_type ) goto cleanup;  /* Option doesn't exist */
    }
    else if( !i_type ) goto cleanup; /* Option doesn't exist */

    if( ( i_type != VLC_VAR_BOOL ) &&
        ( !psz_value || !*psz_value ) ) goto cleanup; /* Invalid value */

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
        val.i_int = strtol( psz_value, NULL, 0 );
        break;

    case VLC_VAR_FLOAT:
        val.f_float = atof( psz_value );
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
        if( psz_orig ) free( psz_orig );
        break;
    }

    default:
        goto cleanup;
        break;
    }

    var_Set( p_obj, psz_name, val );

  cleanup:
    if( psz_name ) free( psz_name );
    return;
}


/* Following functions are local */

/*****************************************************************************
 * GetUnused: find an unused variable from its name
 *****************************************************************************
 * We do i_tries tries before giving up, just in case the variable is being
 * modified and called from a callback.
 *****************************************************************************/
static int GetUnused( vlc_object_t *p_this, const char *psz_name )
{
    int i_var, i_tries = 0;

    while( VLC_TRUE )
    {
        i_var = Lookup( p_this->p_vars, p_this->i_vars, psz_name );
        if( i_var < 0 )
        {
            return VLC_ENOVAR;
        }

        if( ! p_this->p_vars[i_var].b_incallback )
        {
            return i_var;
        }

        if( i_tries++ > 100 )
        {
            msg_Err( p_this, "caught in a callback deadlock?" );
            return VLC_ETIMEOUT;
        }

        vlc_mutex_unlock( &p_this->var_lock );
        msleep( THREAD_SLEEP );
        vlc_mutex_lock( &p_this->var_lock );
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

/*****************************************************************************
 * Lookup: find an existing variable given its name
 *****************************************************************************
 * We use a recursive inner function indexed on the hash. Care is taken of
 * possible hash collisions.
 * XXX: does this really need to be written recursively?
 *****************************************************************************/
static int Lookup( variable_t *p_vars, int i_count, const char *psz_name )
{
    uint32_t i_hash;
    int i, i_pos;

    if( i_count == 0 )
    {
        return -1;
    }

    i_hash = HashString( psz_name );

    i_pos = LookupInner( p_vars, i_count, i_hash );

    /* Hash not found */
    if( i_hash != p_vars[i_pos].i_hash )
    {
        return -1;
    }

    /* Hash found, entry found */
    if( !strcmp( psz_name, p_vars[i_pos].psz_name ) )
    {
        return i_pos;
    }

    /* Hash collision! This should be very rare, but we cannot guarantee
     * it will never happen. Just do an exhaustive search amongst all
     * entries with the same hash. */
    for( i = i_pos - 1 ; i > 0 && i_hash == p_vars[i].i_hash ; i-- )
    {
        if( !strcmp( psz_name, p_vars[i].psz_name ) )
        {
            return i;
        }
    }

    for( i = i_pos + 1 ; i < i_count && i_hash == p_vars[i].i_hash ; i++ )
    {
        if( !strcmp( psz_name, p_vars[i].psz_name ) )
        {
            return i;
        }
    }

    /* Hash found, but entry not found */
    return -1;
}

static int LookupInner( variable_t *p_vars, int i_count, uint32_t i_hash )
{
    int i_middle;

    if( i_hash <= p_vars[0].i_hash )
    {
        return 0;
    }

    if( i_hash >= p_vars[i_count-1].i_hash )
    {
        return i_count - 1;
    }

    i_middle = i_count / 2;

    /* We know that 0 < i_middle */
    if( i_hash < p_vars[i_middle].i_hash )
    {
        return LookupInner( p_vars, i_middle, i_hash );
    }

    /* We know that i_middle + 1 < i_count */
    if( i_hash > p_vars[i_middle].i_hash )
    {
        return i_middle + LookupInner( p_vars + i_middle,
                                       i_count - i_middle,
                                       i_hash );
    }

    return i_middle;
}

/*****************************************************************************
 * CheckValue: check that a value is valid wrt. a variable
 *****************************************************************************
 * This function checks p_val's value against p_var's limitations such as
 * minimal and maximal value, step, in-list position, and modifies p_val if
 * necessary.
 *****************************************************************************/
static void CheckValue ( variable_t *p_var, vlc_value_t *p_val )
{
    /* Check that our variable is in the list */
    if( p_var->i_type & VLC_VAR_HASCHOICE && p_var->choices.i_count )
    {
        int i;

        /* FIXME: the list is sorted, dude. Use something cleverer. */
        for( i = p_var->choices.i_count ; i-- ; )
        {
            if( p_var->pf_cmp( *p_val, p_var->choices.p_values[i] ) == 0 )
            {
                break;
            }
        }

        /* If not found, change it to anything vaguely valid */
        if( i < 0 )
        {
            /* Free the old variable, get the new one, dup it */
            p_var->pf_free( p_val );
            *p_val = p_var->choices.p_values[p_var->i_default >= 0
                                          ? p_var->i_default : 0 ];
            p_var->pf_dup( p_val );
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
 * InheritValue: try to inherit the value of this variable from the same one
 *               in our closest parent.
 *****************************************************************************/
static int InheritValue( vlc_object_t *p_this, const char *psz_name,
                         vlc_value_t *p_val, int i_type )
{
    int i_var;
    variable_t *p_var;

    /* No need to take the structure lock,
     * we are only looking for our parents */

    if( !p_this->p_parent )
    {
        switch( i_type & VLC_VAR_TYPE )
        {
        case VLC_VAR_FILE:
        case VLC_VAR_DIRECTORY:
        case VLC_VAR_STRING:
        case VLC_VAR_MODULE:
            p_val->psz_string = config_GetPsz( p_this, psz_name );
            if( !p_val->psz_string ) p_val->psz_string = strdup("");
            break;
        case VLC_VAR_FLOAT:
            p_val->f_float = config_GetFloat( p_this, psz_name );
            break;
        case VLC_VAR_INTEGER:
        case VLC_VAR_HOTKEY:
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
            if( psz_orig ) free( psz_orig );
            break;
        }
        default:
            return VLC_ENOOBJ;
            break;
        }

        return VLC_SUCCESS;
    }

    /* Look for the variable */
    vlc_mutex_lock( &p_this->p_parent->var_lock );

    i_var = Lookup( p_this->p_parent->p_vars, p_this->p_parent->i_vars,
                    psz_name );

    if( i_var >= 0 )
    {
        /* We found it! */
        p_var = &p_this->p_parent->p_vars[i_var];

        /* Really get the variable */
        *p_val = p_var->val;

        /* Duplicate value if needed */
        p_var->pf_dup( p_val );

        vlc_mutex_unlock( &p_this->p_parent->var_lock );
        return VLC_SUCCESS;
    }

    vlc_mutex_unlock( &p_this->p_parent->var_lock );

    /* We're still not there */

    return InheritValue( p_this->p_parent, psz_name, p_val, i_type );
}
