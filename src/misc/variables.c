/*****************************************************************************
 * variables.c: routines for object variables handling
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: variables.c,v 1.17 2002/12/10 18:22:01 gbazin Exp $
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
static int CmpString( vlc_value_t v, vlc_value_t w ) { return strcmp( v.psz_string, w.psz_string ); }
static int CmpFloat( vlc_value_t v, vlc_value_t w ) { return v.f_float == w.f_float ? 0 : v.f_float > w.f_float ? 1 : -1; }
static int CmpAddress( vlc_value_t v, vlc_value_t w ) { return v.p_address == w.p_address ? 0 : v.p_address > w.p_address ? 1 : -1; }

/*****************************************************************************
 * Local duplication functions, and local deallocation functions
 *****************************************************************************/
static void DupDummy( vlc_value_t *p_val ) { (void)p_val; /* unused */ }
static void DupString( vlc_value_t *p_val ) { p_val->psz_string = strdup( p_val->psz_string ); }

static void FreeDummy( vlc_value_t *p_val ) { (void)p_val; /* unused */ }
static void FreeString( vlc_value_t *p_val ) { free( p_val->psz_string ); }
static void FreeMutex( vlc_value_t *p_val ) { vlc_mutex_destroy( (vlc_mutex_t*)p_val->p_address ); free( p_val->p_address ); }

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

/*****************************************************************************
 * var_Create: initialize a vlc variable
 *****************************************************************************
 * We hash the given string and insert it into the sorted list. The insertion
 * may require slow memory copies, but think about what we gain in the log(n)
 * lookup phase when setting/getting the variable value!
 *****************************************************************************/
int __var_Create( vlc_object_t *p_this, const char *psz_name, int i_type )
{
    int i_new;
    variable_t *p_var;

    vlc_mutex_lock( &p_this->var_lock );

    /* FIXME: if the variable already exists, we don't duplicate it. But we
     * duplicate the lookups. It's not that serious, but if anyone finds some
     * time to rework Insert() so that only one lookup has to be done, feel
     * free to do so. */
    i_new = Lookup( p_this->p_vars, p_this->i_vars, psz_name );

    if( i_new >= 0 )
    {
        /* If the types differ, variable creation failed. */
        if( i_type != p_this->p_vars[i_new].i_type )
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

    p_var->i_hash = HashString( psz_name );
    p_var->psz_name = strdup( psz_name );

    p_var->i_type = i_type;
    memset( &p_var->val, 0, sizeof(vlc_value_t) );

    p_var->pf_dup = DupDummy;
    p_var->pf_free = FreeDummy;

    p_var->i_usage = 1;

    p_var->i_default = -1;
    p_var->i_choices = 0;
    p_var->pp_choices = NULL;

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
            p_var->pf_cmp = CmpInt;
            p_var->val.i_int = 0;
            break;
        case VLC_VAR_STRING:
        case VLC_VAR_MODULE:
        case VLC_VAR_FILE:
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
            /* FIXME: TODO */
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
    }

    /* Duplicate the default data we stored. */
    p_var->pf_dup( &p_var->val );

    vlc_mutex_unlock( &p_this->var_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * var_Destroy: destroy a vlc variable
 *****************************************************************************
 * Look for the variable and destroy it if it is found. As in var_Create we
 * do a call to memmove() but we have performance counterparts elsewhere.
 *****************************************************************************/
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
    if( p_var->pp_choices )
    {
        for( i = 0 ; i < p_var->i_choices ; i++ )
        {
            p_var->pf_free( &p_var->pp_choices[i] );
        }
        free( p_var->pp_choices );
    }

    /* Free callbacks if needed */
    if( p_var->p_entries )
    {
        free( p_var->p_entries );
    }

    free( p_var->psz_name );

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

/*****************************************************************************
 * var_Change: perform an action on a variable
 *****************************************************************************
 *
 *****************************************************************************/
int __var_Change( vlc_object_t *p_this, const char *psz_name,
                  int i_action, vlc_value_t *p_val )
{
    int i_var, i;
    variable_t *p_var;

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
            for( i = p_var->i_choices ; i-- ; )
            {
                if( p_var->pf_cmp( p_var->pp_choices[i], *p_val ) < 0 )
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

            INSERT_ELEM( p_var->pp_choices, p_var->i_choices, i, *p_val );
            p_var->pf_dup( &p_var->pp_choices[i] );

            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_DELCHOICE:
            /* FIXME: the list is sorted, dude. Use something cleverer. */
            for( i = 0 ; i < p_var->i_choices ; i++ )
            {
                if( p_var->pf_cmp( p_var->pp_choices[i], *p_val ) == 0 )
                {
                    break;
                }
            }

            if( i == p_var->i_choices )
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

            p_var->pf_free( &p_var->pp_choices[i] );
            REMOVE_ELEM( p_var->pp_choices, p_var->i_choices, i );

            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_SETDEFAULT:
            /* FIXME: the list is sorted, dude. Use something cleverer. */
            for( i = 0 ; i < p_var->i_choices ; i++ )
            {
                if( p_var->pf_cmp( p_var->pp_choices[i], *p_val ) == 0 )
                {
                    break;
                }
            }

            if( i == p_var->i_choices )
            {
                /* Not found */
                break;
            }

            p_var->i_default = i;
            CheckValue( p_var, &p_var->val );
            break;

        case VLC_VAR_GETLIST:
            p_val->p_address = malloc( (1 + p_var->i_choices)
                                        * sizeof(vlc_value_t) );
            ((vlc_value_t*)p_val->p_address)[0].i_int = p_var->i_choices;
            for( i = 0 ; i < p_var->i_choices ; i++ )
            {
                ((vlc_value_t*)p_val->p_address)[i+1] = p_var->pp_choices[i];
                p_var->pf_dup( &((vlc_value_t*)p_val->p_address)[i+1] );
            }
            break;
        case VLC_VAR_FREELIST:
            for( i = ((vlc_value_t*)p_val->p_address)[0].i_int ; i-- ; )
            {
                p_var->pf_free( &((vlc_value_t*)p_val->p_address)[i+1] );
            }
            free( p_val->p_address );
            break;

        default:
            break;
    }

    vlc_mutex_unlock( &p_this->var_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * var_Type: request a variable's type
 *****************************************************************************
 * This function returns the variable type if it exists, or 0 if the
 * variable could not be found.
 *****************************************************************************/
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

/*****************************************************************************
 * var_Set: set a variable's value
 *****************************************************************************
 *
 *****************************************************************************/
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
            msg_Err( p_this, "variable %s has disappeared" );
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

/*****************************************************************************
 * var_Get: get a variable's value
 *****************************************************************************
 *
 *****************************************************************************/
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

/*****************************************************************************
 * var_AddCallback: register a callback in a variable
 *****************************************************************************
 * We store a function pointer pf_callback that will be called upon variable
 * modification. p_data is a generic pointer that will be passed as additional
 * argument to the callback function.
 *****************************************************************************/
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

/*****************************************************************************
 * var_DelCallback: remove a callback from a variable
 *****************************************************************************
 * pf_callback and p_data have to be given again, because different objects
 * might have registered the same callback function.
 *****************************************************************************/
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
            || p_var->p_entries[i_entry].p_data == p_data )
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
    if( p_var->i_type & VLC_VAR_HASCHOICE && p_var->i_choices )
    {
        int i;

        /* FIXME: the list is sorted, dude. Use something cleverer. */
        for( i = p_var->i_choices ; i-- ; )
        {
            if( p_var->pf_cmp( *p_val, p_var->pp_choices[i] ) == 0 )
            {
                break;
            }
        }

        /* If not found, change it to anything vaguely valid */
        if( i < 0 )
        {
            /* Free the old variable, get the new one, dup it */
            p_var->pf_free( p_val );
            *p_val = p_var->pp_choices[p_var->i_default >= 0
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

