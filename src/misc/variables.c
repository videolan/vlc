/*****************************************************************************
 * variables.c: routines for object variables handling
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: variables.c,v 1.2 2002/10/14 16:46:56 sam Exp $
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
 * Local prototypes
 *****************************************************************************/
static u32 HashString     ( const char * );
static int Insert         ( variable_t *, int, const char * );
static int InsertInner    ( variable_t *, int, u32 );
static int Lookup         ( variable_t *, int, const char * );
static int LookupInner    ( variable_t *, int, u32 );

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

    vlc_mutex_lock( &p_this->var_lock );

    if( (p_this->i_vars & 15) == 15 )
    {
        p_this->p_vars = realloc( p_this->p_vars,
                                  (p_this->i_vars+17) * sizeof(variable_t) );
    }

    i_new = Insert( p_this->p_vars, p_this->i_vars, psz_name );

    memmove( p_this->p_vars + i_new + 1,
             p_this->p_vars + i_new,
             (p_this->i_vars - i_new) * sizeof(variable_t) );

    p_this->p_vars[i_new].i_hash = HashString( psz_name );
    p_this->p_vars[i_new].psz_name = strdup( psz_name );

    p_this->p_vars[i_new].i_type = i_type;
    memset( &p_this->p_vars[i_new].val, 0, sizeof(vlc_value_t) );

    p_this->p_vars[i_new].b_set = VLC_FALSE;
    p_this->p_vars[i_new].b_active = VLC_TRUE;

    p_this->i_vars++;

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
    int i_del;

    vlc_mutex_lock( &p_this->var_lock );

    i_del = Lookup( p_this->p_vars, p_this->i_vars, psz_name );

    if( i_del < 0 )
    {
        msg_Err( p_this, "variable %s was not found", psz_name );
        vlc_mutex_unlock( &p_this->var_lock );
        return VLC_ENOVAR;
    }

    /* Free value if needed */
    switch( p_this->p_vars[i_del].i_type )
    {
        case VLC_VAR_STRING:
        case VLC_VAR_MODULE:
        case VLC_VAR_FILE:
            if( p_this->p_vars[i_del].b_set
                 && p_this->p_vars[i_del].val.psz_string )
            {
                free( p_this->p_vars[i_del].val.psz_string );
            }
            break;
    }

    free( p_this->p_vars[i_del].psz_name );

    memmove( p_this->p_vars + i_del,
             p_this->p_vars + i_del + 1,
             (p_this->i_vars - i_del - 1) * sizeof(variable_t) );

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
 * var_Type: request a variable's type, 0 if not found
 *****************************************************************************
 * 
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

    vlc_mutex_lock( &p_this->var_lock );

    i_var = Lookup( p_this->p_vars, p_this->i_vars, psz_name );

    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return VLC_ENOVAR;
    }

    /* Duplicate value if needed */
    switch( p_this->p_vars[i_var].i_type )
    {
        case VLC_VAR_STRING:
        case VLC_VAR_MODULE:
        case VLC_VAR_FILE:
            if( p_this->p_vars[i_var].b_set
                 && p_this->p_vars[i_var].val.psz_string )
            {
                free( p_this->p_vars[i_var].val.psz_string );
            }
            if( val.psz_string )
            {
                val.psz_string = strdup( val.psz_string );
            }
            break;
    }

    p_this->p_vars[i_var].val = val;
    p_this->p_vars[i_var].b_set = VLC_TRUE;

    /* XXX: callback stuff will go here */

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

    vlc_mutex_lock( &p_this->var_lock );

    i_var = Lookup( p_this->p_vars, p_this->i_vars, psz_name );

    if( i_var < 0 )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return VLC_ENOVAR;
    }

    if( !p_this->p_vars[i_var].b_set )
    {
        vlc_mutex_unlock( &p_this->var_lock );
        return VLC_EBADVAR;
    }

    /* Some variables trigger special behaviour. */
    switch( p_this->p_vars[i_var].i_type )
    {
        case VLC_VAR_COMMAND:
            if( p_this->p_vars[i_var].b_set )
            {
                int i_ret = ((int (*) (vlc_object_t *, char *, char *))
                                p_this->p_vars[i_var].val.p_address) (
                                    p_this,
                                    p_this->p_vars[i_var].psz_name,
                                    p_val->psz_string );
                vlc_mutex_unlock( &p_this->var_lock );
                return i_ret;
            }
            break;
    }

    /* Really set the variable */
    *p_val = p_this->p_vars[i_var].val;

    /* Duplicate value if needed */
    switch( p_this->p_vars[i_var].i_type )
    {
        case VLC_VAR_STRING:
        case VLC_VAR_MODULE:
        case VLC_VAR_FILE:
            if( p_val->psz_string )
            {
                p_val->psz_string = strdup( p_val->psz_string );
            }
            break;
    }

    vlc_mutex_unlock( &p_this->var_lock );

    return VLC_SUCCESS;
}

/* Following functions are local */

/*****************************************************************************
 * HashString: our cool hash function
 *****************************************************************************
 * This function is not intended to be crypto-secure, we only want it to be
 * fast and not suck too much. This one is pretty fast and did 0 collisions
 * in wenglish's dictionary.
 *****************************************************************************/
static u32 HashString( const char *psz_string )
{
    u32 i_hash = 0;

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

static int InsertInner( variable_t *p_vars, int i_count, u32 i_hash )
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
    u32 i_hash;
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

static int LookupInner( variable_t *p_vars, int i_count, u32 i_hash )
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

