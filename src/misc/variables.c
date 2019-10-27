/*****************************************************************************
 * variables.c: routines for object variables handling
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
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
#include <float.h>
#include <math.h>
#include <limits.h>

#include <vlc_common.h>
#include <vlc_arrays.h>
#include <vlc_charset.h>
#include "libvlc.h"
#include "variables.h"
#include "config/configuration.h"

typedef struct callback_entry_t
{
    struct callback_entry_t *next;
    union
    {
        vlc_callback_t       pf_value_callback;
        vlc_list_callback_t  pf_list_callback;
        void *               p_callback;
    };
    void *         p_data;
} callback_entry_t;

typedef struct variable_ops_t
{
    int  (*pf_cmp) ( vlc_value_t, vlc_value_t );
    void (*pf_dup) ( vlc_value_t * );
    void (*pf_free) ( vlc_value_t * );
} variable_ops_t;

/**
 * The structure describing a variable.
 * \note vlc_value_t is the common union for variable values
 */
struct variable_t
{
    char *       psz_name; /**< The variable unique name (must be first) */

    /** The variable's exported value */
    vlc_value_t  val;

    /** The variable display name, mainly for use by the interfaces */
    char *       psz_text;

    const variable_ops_t *ops;

    int          i_type;   /**< The type of the variable */
    unsigned     i_usage;  /**< Reference count */

    /** If the variable has min/max/step values */
    vlc_value_t  min, max, step;

    /** List of choices */
    vlc_value_t *choices;
    /** List of friendly names for the choices */
    char       **choices_text;
    size_t       choices_count;

    /** Set to TRUE if the variable is in a callback */
    bool   b_incallback;

    /** Registered value callbacks */
    callback_entry_t    *value_callbacks;
    /** Registered list callbacks */
    callback_entry_t    *list_callbacks;
};

static int CmpBool( vlc_value_t v, vlc_value_t w )
{
    return v.b_bool ? w.b_bool ? 0 : 1 : w.b_bool ? -1 : 0;
}

static int CmpInt( vlc_value_t v, vlc_value_t w )
{
    return v.i_int == w.i_int ? 0 : v.i_int > w.i_int ? 1 : -1;
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

static void DupDummy( vlc_value_t *p_val ) { (void)p_val; /* unused */ }
static void DupString( vlc_value_t *p_val )
{
    p_val->psz_string = strdup( p_val->psz_string ? p_val->psz_string :  "" );
}

static void FreeDummy( vlc_value_t *p_val ) { (void)p_val; /* unused */ }
static void FreeString( vlc_value_t *p_val ) { free( p_val->psz_string ); }

static const struct variable_ops_t
void_ops   = { NULL,       DupDummy,  FreeDummy,  },
addr_ops   = { CmpAddress, DupDummy,  FreeDummy,  },
bool_ops   = { CmpBool,    DupDummy,  FreeDummy,  },
float_ops  = { CmpFloat,   DupDummy,  FreeDummy,  },
int_ops    = { CmpInt,     DupDummy,  FreeDummy,  },
string_ops = { CmpString,  DupString, FreeString, },
coords_ops = { NULL,       DupDummy,  FreeDummy,  };

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
    void **pp_var;

    vlc_mutex_lock(&priv->var_lock);
    pp_var = tfind( &psz_name, &priv->var_root, varcmp );
    return (pp_var != NULL) ? *pp_var : NULL;
}

static void Destroy( variable_t *p_var )
{
    p_var->ops->pf_free( &p_var->val );

    for (size_t i = 0, count = p_var->choices_count; i < count; i++)
    {
        p_var->ops->pf_free(&p_var->choices[i]);
        free(p_var->choices_text[i]);
    }
    free(p_var->choices);
    free(p_var->choices_text);

    free( p_var->psz_name );
    free( p_var->psz_text );
    while (unlikely(p_var->value_callbacks != NULL))
    {
        callback_entry_t *next = p_var->value_callbacks->next;

        free(p_var->value_callbacks);
        p_var->value_callbacks = next;
    }
    assert(p_var->list_callbacks == NULL);
    free( p_var );
}

/**
 * Adjusts a value to fit the constraints for a certain variable:
 * - If the value is lower than the minimum, use the minimum.
 * - If the value is higher than the maximum, use the maximum.
 * - If the variable has steps, round the value to the nearest step.
 */
static void CheckValue(variable_t *var, vlc_value_t *val)
{
    /* Check that our variable is within the bounds */
    switch (var->i_type & VLC_VAR_TYPE)
    {
        case VLC_VAR_INTEGER:
            if (val->i_int < var->min.i_int)
               val->i_int = var->min.i_int;
            else if (val->i_int > var->max.i_int)
                val->i_int = var->max.i_int;
            if (var->step.i_int != 0 && (val->i_int % var->step.i_int))
            {
                if (val->i_int > 0)
                    val->i_int = (val->i_int + (var->step.i_int / 2))
                                 / var->step.i_int * var->step.i_int;
                else
                    val->i_int = (val->i_int - (var->step.i_int / 2))
                                 / var->step.i_int * var->step.i_int;
            }
            break;

        case VLC_VAR_FLOAT:
            if (isless(val->f_float, var->min.f_float))
                val->f_float = var->min.f_float;
            else if (isgreater(val->f_float, var->max.f_float))
                val->f_float = var->max.f_float;
            if (var->step.f_float != 0.f)
                val->f_float = var->step.f_float
                              * roundf(val->f_float / var->step.f_float);
            break;
    }
}

/**
 * Waits until the variable is inactive (i.e. not executing a callback)
 */
static void WaitUnused(vlc_object_t *obj, variable_t *var)
{
    vlc_object_internals_t *priv = vlc_internals(obj);

    mutex_cleanup_push(&priv->var_lock);
    while (var->b_incallback)
        vlc_cond_wait(&priv->var_wait, &priv->var_lock);
    vlc_cleanup_pop();
}

static void TriggerCallback(vlc_object_t *obj, variable_t *var,
                            const char *name, vlc_value_t prev)
{
    assert(obj != NULL);

    callback_entry_t *entry = var->value_callbacks;
    if (entry == NULL)
        return;

    vlc_object_internals_t *priv = vlc_internals(obj);

    assert(!var->b_incallback);
    var->b_incallback = true;
    vlc_mutex_unlock(&priv->var_lock);

    do
    {
        entry->pf_value_callback(obj, name, prev, var->val, entry->p_data);
        entry = entry->next;
    }
    while (entry != NULL);

    vlc_mutex_lock(&priv->var_lock);
    var->b_incallback = false;
    vlc_cond_broadcast(&priv->var_wait);
}

static void TriggerListCallback(vlc_object_t *obj, variable_t *var,
                                const char *name, int action, vlc_value_t *val)
{
    assert(obj != NULL);

    callback_entry_t *entry = var->list_callbacks;
    if (entry == NULL)
        return;

    vlc_object_internals_t *priv = vlc_internals(obj);

    assert(!var->b_incallback);
    var->b_incallback = true;
    vlc_mutex_unlock(&priv->var_lock);

    do
    {
        entry->pf_list_callback(obj, name, action, val, entry->p_data);
        entry = entry->next;
    }
    while (entry != NULL);

    vlc_mutex_lock(&priv->var_lock);
    var->b_incallback = false;
    vlc_cond_broadcast(&priv->var_wait);
}

int (var_Create)( vlc_object_t *p_this, const char *psz_name, int i_type )
{
    assert( p_this );

    variable_t *p_var = calloc( 1, sizeof( *p_var ) );
    if( p_var == NULL )
        return VLC_ENOMEM;

    p_var->psz_name = strdup( psz_name );
    p_var->psz_text = NULL;

    p_var->i_type = i_type & ~VLC_VAR_DOINHERIT;

    p_var->i_usage = 1;

    p_var->choices_count = 0;
    p_var->choices = NULL;
    p_var->choices_text = NULL;

    p_var->b_incallback = false;
    p_var->value_callbacks = NULL;

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
            p_var->min.i_int = INT64_MIN;
            p_var->max.i_int = INT64_MAX;
            break;
        case VLC_VAR_STRING:
            p_var->ops = &string_ops;
            p_var->val.psz_string = NULL;
            break;
        case VLC_VAR_FLOAT:
            p_var->ops = &float_ops;
            p_var->val.f_float = 0.f;
            p_var->min.f_float = -FLT_MAX;
            p_var->max.f_float = FLT_MAX;
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
            vlc_assert_unreachable ();
    }

    if (i_type & VLC_VAR_DOINHERIT)
        var_Inherit(p_this, psz_name, i_type, &p_var->val);

    vlc_object_internals_t *p_priv = vlc_internals( p_this );
    void **pp_var;
    variable_t *p_oldvar;
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
        p_oldvar->i_type |= i_type & VLC_VAR_ISCOMMAND;
    }
    vlc_mutex_unlock( &p_priv->var_lock );

    /* If we did not need to create a new variable, free everything... */
    if( p_var != NULL )
        Destroy( p_var );
    return ret;
}

void (var_Destroy)(vlc_object_t *p_this, const char *psz_name)
{
    variable_t *p_var;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
        msg_Dbg( p_this, "attempt to destroy nonexistent variable \"%s\"",
                 psz_name );
    else if( --p_var->i_usage == 0 )
    {
        assert(!p_var->b_incallback);
        tdelete( p_var, &p_priv->var_root, varcmp );
    }
    else
    {
        assert(p_var->i_usage != -1u);
        p_var = NULL;
    }
    vlc_mutex_unlock( &p_priv->var_lock );

    if( p_var != NULL )
        Destroy( p_var );
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

int (var_Change)(vlc_object_t *p_this, const char *psz_name, int i_action, ...)
{
    va_list ap;
    int ret = VLC_SUCCESS;
    variable_t *p_var;
    vlc_value_t oldval;
    vlc_value_t newval;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        return VLC_ENOVAR;
    }

    va_start(ap, i_action);
    switch( i_action )
    {
        case VLC_VAR_GETMIN:
            *va_arg(ap, vlc_value_t *) = p_var->min;
            break;
        case VLC_VAR_GETMAX:
            *va_arg(ap, vlc_value_t *) = p_var->max;
            break;
        case VLC_VAR_SETMINMAX:
            assert(p_var->ops->pf_free == FreeDummy);
            p_var->min = va_arg(ap, vlc_value_t);
            p_var->max = va_arg(ap, vlc_value_t);
            break;
        case VLC_VAR_SETSTEP:
            assert(p_var->ops->pf_free == FreeDummy);
            p_var->step = va_arg(ap, vlc_value_t);
            CheckValue( p_var, &p_var->val );
            break;
        case VLC_VAR_GETSTEP:
            switch (p_var->i_type & VLC_VAR_TYPE)
            {
                case VLC_VAR_INTEGER:
                    if (p_var->step.i_int == 0)
                        ret = VLC_EGENERIC;
                    break;
                case VLC_VAR_FLOAT:
                    if (p_var->step.f_float == 0.f)
                        ret = VLC_EGENERIC;
                    break;
                default:
                    ret = VLC_EGENERIC;
            }
            if (ret == VLC_SUCCESS)
                *va_arg(ap, vlc_value_t *) = p_var->step;
            break;
        case VLC_VAR_ADDCHOICE:
        {
            vlc_value_t val = va_arg(ap, vlc_value_t);
            const char *text = va_arg(ap, const char *);
            size_t count = p_var->choices_count;

            TAB_APPEND(p_var->choices_count, p_var->choices, val);
            p_var->ops->pf_dup(&p_var->choices[count]);
            TAB_APPEND(count, p_var->choices_text, NULL);
            assert(count == p_var->choices_count);
            if (text != NULL)
                p_var->choices_text[count - 1] = strdup(text);

            TriggerListCallback(p_this, p_var, psz_name, VLC_VAR_ADDCHOICE,
                                &val);
            break;
        }
        case VLC_VAR_DELCHOICE:
        {
            vlc_value_t val = va_arg(ap, vlc_value_t);
            size_t count = p_var->choices_count, i;

            for (i = 0; i < count; i++)
                if (p_var->ops->pf_cmp(p_var->choices[i], val) == 0)
                    break;

            if (i == count)
            {   /* Not found */
                ret = VLC_EGENERIC;
                break;
            }

            p_var->ops->pf_free(&p_var->choices[i]);
            free(p_var->choices_text[i]);
            TAB_ERASE(p_var->choices_count, p_var->choices, i);
            TAB_ERASE(count, p_var->choices_text, i);
            assert(count == p_var->choices_count);

            TriggerListCallback(p_this, p_var, psz_name, VLC_VAR_DELCHOICE,
                                &val);
            break;
        }
        case VLC_VAR_CHOICESCOUNT:
            *va_arg(ap, size_t *) = p_var->choices_count;
            break;
        case VLC_VAR_CLEARCHOICES:
            for (size_t i = 0; i < p_var->choices_count; i++)
                p_var->ops->pf_free(&p_var->choices[i]);
            for (size_t i = 0; i < p_var->choices_count; i++)
                free(p_var->choices_text[i]);
            TAB_CLEAN(p_var->choices_count, p_var->choices);
            free(p_var->choices_text);
            p_var->choices_text = NULL;

            TriggerListCallback(p_this, p_var, psz_name, VLC_VAR_CLEARCHOICES, NULL);
            break;
        case VLC_VAR_SETVALUE:
            /* Duplicate data if needed */
            newval = va_arg(ap, vlc_value_t);
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
        {
            size_t *count = va_arg(ap, size_t *);
            vlc_value_t **values = va_arg(ap, vlc_value_t **);
            char ***texts = va_arg(ap, char ***);

            *count = p_var->choices_count;
            *values = xmalloc(p_var->choices_count * sizeof (**values));

            for (size_t i = 0; i < p_var->choices_count; i++)
            {
                vlc_value_t *val = (*values) + i;
                *val = p_var->choices[i];
                p_var->ops->pf_dup(val);
            }

            if( texts != NULL )
            {
                char **tab = xmalloc(p_var->choices_count * sizeof (*tab));
                *texts = tab;

                for (size_t i = 0; i < p_var->choices_count; i++)
                    tab[i] = (p_var->choices_text[i] != NULL)
                        ? strdup(p_var->choices_text[i]) : NULL;
            }
            break;
        }
        case VLC_VAR_SETTEXT:
        {
            const char *text = va_arg(ap, const char *);

            free( p_var->psz_text );
            p_var->psz_text = (text != NULL) ? strdup(text) : NULL;
            break;
        }
        case VLC_VAR_GETTEXT:
            *va_arg(ap, char **) = (p_var->psz_text != NULL)
                ? strdup(p_var->psz_text) : NULL;
            break;
        default:
            break;
    }
    va_end(ap);
    vlc_mutex_unlock( &p_priv->var_lock );
    return ret;
}

int (var_GetAndSet)(vlc_object_t *p_this, const char *psz_name, int i_action,
                    vlc_value_t *p_val)
{
    variable_t *p_var;
    vlc_value_t oldval;

    assert( p_this );
    assert( p_val );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

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
    TriggerCallback( p_this, p_var, psz_name, oldval );

    vlc_mutex_unlock( &p_priv->var_lock );
    return VLC_SUCCESS;
}

int (var_Type)(vlc_object_t *p_this, const char *psz_name)
{
    variable_t *p_var;
    int i_type = 0;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    p_var = Lookup( p_this, psz_name );
    if( p_var != NULL )
    {
        i_type = p_var->i_type;
        if (p_var->choices_count > 0)
            i_type |= VLC_VAR_HASCHOICE;
    }
    vlc_mutex_unlock( &p_priv->var_lock );

    return i_type;
}

int (var_SetChecked)(vlc_object_t *p_this, const char *psz_name,
                     int expected_type, vlc_value_t val)
{
    variable_t *p_var;
    vlc_value_t oldval;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

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
    TriggerCallback( p_this, p_var, psz_name, oldval );

    /* Free data if needed */
    p_var->ops->pf_free( &oldval );

    vlc_mutex_unlock( &p_priv->var_lock );
    return VLC_SUCCESS;
}

int (var_Set)(vlc_object_t *p_this, const char *psz_name, vlc_value_t val)
{
    return var_SetChecked( p_this, psz_name, 0, val );
}

int (var_GetChecked)(vlc_object_t *p_this, const char *psz_name,
                     int expected_type, vlc_value_t *p_val)
{
    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );
    variable_t *p_var;
    int err = VLC_SUCCESS;

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

int (var_Get)(vlc_object_t *p_this, const char *psz_name, vlc_value_t *p_val)
{
    return var_GetChecked( p_this, psz_name, 0, p_val );
}

typedef enum
{
    vlc_value_callback,
    vlc_list_callback
} vlc_callback_type_t;

static void AddCallback( vlc_object_t *p_this, const char *psz_name,
                         callback_entry_t *restrict entry,
                         vlc_callback_type_t i_type )
{
    variable_t *p_var;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        msg_Err( p_this, "cannot add callback %p to nonexistent variable '%s'",
                 entry->p_callback, psz_name );
        free( entry );
        return;
    }

    WaitUnused( p_this, p_var );

    callback_entry_t **pp;

    if (i_type == vlc_value_callback)
        pp = &p_var->value_callbacks;
    else
        pp = &p_var->list_callbacks;

    entry->next = *pp;
    *pp = entry;

    vlc_mutex_unlock( &p_priv->var_lock );
}

void (var_AddCallback)(vlc_object_t *p_this, const char *psz_name,
                       vlc_callback_t pf_callback, void *p_data)
{
    callback_entry_t *entry = xmalloc(sizeof (*entry));

    entry->pf_value_callback = pf_callback;
    entry->p_data = p_data;
    AddCallback(p_this, psz_name, entry, vlc_value_callback);
}

static void DelCallback( vlc_object_t *p_this, const char *psz_name,
                         const callback_entry_t *restrict match,
                         vlc_callback_type_t i_type )
{
    callback_entry_t **pp, *entry;
    variable_t *p_var;

    assert( p_this );

    vlc_object_internals_t *p_priv = vlc_internals( p_this );

    p_var = Lookup( p_this, psz_name );
    if( p_var == NULL )
    {
        vlc_mutex_unlock( &p_priv->var_lock );
        msg_Err( p_this, "cannot delete callback %p from nonexistent "
                 "variable '%s'", match->p_callback, psz_name );
        return;
    }

    WaitUnused( p_this, p_var );

    if (i_type == vlc_value_callback)
        pp = &p_var->value_callbacks;
    else
        pp = &p_var->list_callbacks;

    entry = *pp;
    assert(entry != NULL);

    while (entry->p_callback != match->p_callback
        || entry->p_data != match->p_data)
    {
        pp = &entry->next;
        entry = *pp;
        assert(entry != NULL);
    }

    *pp = entry->next;
    vlc_mutex_unlock( &p_priv->var_lock );
    free(entry);
}

void (var_DelCallback)(vlc_object_t *p_this, const char *psz_name,
                       vlc_callback_t pf_callback, void *p_data)
{
    callback_entry_t entry;
    entry.pf_value_callback = pf_callback;
    entry.p_data = p_data;

    DelCallback(p_this, psz_name, &entry, vlc_value_callback);
}

void (var_TriggerCallback)(vlc_object_t *p_this, const char *psz_name)
{
    vlc_object_internals_t *p_priv = vlc_internals( p_this );
    variable_t *p_var = Lookup( p_this, psz_name );
    if( p_var != NULL )
    {
        WaitUnused( p_this, p_var );

        /* Deal with callbacks. Tell we're in a callback, release the lock,
         * call stored functions, retake the lock. */
        TriggerCallback( p_this, p_var, psz_name, p_var->val );
    }
    vlc_mutex_unlock( &p_priv->var_lock );
}

void (var_AddListCallback)(vlc_object_t *p_this, const char *psz_name,
                           vlc_list_callback_t pf_callback, void *p_data)
{
    callback_entry_t *entry = xmalloc(sizeof (*entry));

    entry->pf_list_callback = pf_callback;
    entry->p_data = p_data;
    AddCallback(p_this, psz_name, entry, vlc_list_callback);
}

void (var_DelListCallback)(vlc_object_t *p_this, const char *psz_name,
                           vlc_list_callback_t pf_callback, void *p_data)
{
    callback_entry_t entry;

    entry.pf_list_callback = pf_callback;
    entry.p_data = p_data;
    DelCallback(p_this, psz_name, &entry, vlc_list_callback);
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

    i_type = config_GetType( psz_name );
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
        i_type = config_GetType( psz_name );
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
     * Children of the input object will be able to retrieve this value
     * thanks to the inheritance property of the object variables. */
    var_Create( p_obj, psz_name, i_type );

    switch( i_type )
    {
    case VLC_VAR_BOOL:
        if( psz_value )
        {
            char *endptr;
            long long int value = strtoll( psz_value, &endptr, 0 );
            if( endptr == psz_value ) /* Not an integer */
                val.b_bool = strcasecmp( psz_value, "true" ) == 0
                          || strcasecmp( psz_value, "yes" ) == 0;
            else
                val.b_bool = value != 0;
        }
        else
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

int (var_LocationParse)(vlc_object_t *obj, const char *mrl, const char *pref)
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

int var_Inherit( vlc_object_t *p_this, const char *psz_name, int i_type,
                 vlc_value_t *p_val )
{
    i_type &= VLC_VAR_CLASS;
    for (vlc_object_t *obj = p_this; obj != NULL; obj = vlc_object_parent(obj))
    {
        if( var_GetChecked( obj, psz_name, i_type, p_val ) == VLC_SUCCESS )
            return VLC_SUCCESS;
    }

    /* else take value from config */
    switch( i_type & VLC_VAR_CLASS )
    {
        case VLC_VAR_STRING:
            p_val->psz_string = config_GetPsz( psz_name );
            if( !p_val->psz_string ) p_val->psz_string = strdup("");
            break;
        case VLC_VAR_FLOAT:
            p_val->f_float = config_GetFloat( psz_name );
            break;
        case VLC_VAR_INTEGER:
            p_val->i_int = config_GetInt( psz_name );
            break;
        case VLC_VAR_BOOL:
            p_val->b_bool = config_GetInt( psz_name ) > 0;
            break;
        default:
            vlc_assert_unreachable();
        case VLC_VAR_ADDRESS:
            return VLC_ENOOBJ;
    }
    return VLC_SUCCESS;
}

int (var_InheritURational)(vlc_object_t *object,
                           unsigned *num, unsigned *den,
                           const char *var)
{
    char *str = var_InheritString(object, var);
    if (str == NULL)
        goto error;

    char *sep;
    unsigned n = strtoul(str, &sep, 10);
    unsigned d;

    switch (*sep) {
        case '\0':
            /* Decimal integer */
            d = 1;
            break;

        case ':':
        case '/':
            /* Decimal fraction */
            d = strtoul(sep + 1, &sep, 10);
            if (*sep != '\0')
                goto error;
            break;

        case '.': {
            /* Decimal number */
            unsigned char c;

            d = 1;
            while ((c = *(++sep)) != '\0') {
                c -= '0';

                if (c >= 10)
                    goto error;

                n = n * 10 + c;
                d *= 10;
            }
            break;
        }

        default:
            goto error;
    }

    free(str);

    if (n == 0) {
        *num = 0;
        *den = d ? 1 : 0;
    } else if (d == 0) {
        *num = 1;
        *den = 0;
    } else
        vlc_ureduce(num, den, n, d, 0);

    return VLC_SUCCESS;

error:
    free(str);
    *num = 0;
    *den = 0;
    return VLC_EGENERIC;
}

static thread_local void *twalk_ctx;

static void TwalkGetNames(const void *data, const VISIT which, const int depth)
{
    if (which != postorder && which != leaf)
        return;
    (void) depth;

    const variable_t *var = *(const variable_t **)data;
    DECL_ARRAY(char *) *names = twalk_ctx;
    char *dup = strdup(var->psz_name);
    if (dup != NULL)
        ARRAY_APPEND(*names, dup);
}

char **var_GetAllNames(vlc_object_t *obj)
{
    vlc_object_internals_t *priv = vlc_internals(obj);

    DECL_ARRAY(char *) names;
    ARRAY_INIT(names);

    twalk_ctx = &names;
    vlc_mutex_lock(&priv->var_lock);
    twalk(priv->var_root, TwalkGetNames);
    vlc_mutex_unlock(&priv->var_lock);

    if (names.i_size == 0)
        return NULL;
    ARRAY_APPEND(names, NULL);
    return names.p_elems;
}
