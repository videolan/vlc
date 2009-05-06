/*****************************************************************************
 * core.c management of the modules configuration
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "vlc_keys.h"
#include "vlc_charset.h"
#include "vlc_configuration.h"

#include <assert.h>

#include "configuration.h"
#include "modules/modules.h"

static inline char *strdupnull (const char *src)
{
    return src ? strdup (src) : NULL;
}

/* Item types that use a string value (i.e. serialized in the module cache) */
int IsConfigStringType (int type)
{
    static const unsigned char config_types[] =
    {
        CONFIG_ITEM_STRING, CONFIG_ITEM_FILE, CONFIG_ITEM_MODULE,
        CONFIG_ITEM_DIRECTORY, CONFIG_ITEM_MODULE_CAT, CONFIG_ITEM_PASSWORD,
        CONFIG_ITEM_MODULE_LIST, CONFIG_ITEM_MODULE_LIST_CAT
    };

    /* NOTE: this needs to be changed if we ever get more than 255 types */
    return memchr (config_types, type, sizeof (config_types)) != NULL;
}


int IsConfigIntegerType (int type)
{
    static const unsigned char config_types[] =
    {
        CONFIG_ITEM_INTEGER, CONFIG_ITEM_KEY, CONFIG_ITEM_BOOL,
        CONFIG_CATEGORY, CONFIG_SUBCATEGORY
    };

    return memchr (config_types, type, sizeof (config_types)) != NULL;
}


/*****************************************************************************
 * config_GetType: get the type of a variable (bool, int, float, string)
 *****************************************************************************
 * This function is used to get the type of a variable from its name.
 * Beware, this is quite slow.
 *****************************************************************************/
int __config_GetType( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;
    int i_type;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        return 0;
    }

    switch( p_config->i_type )
    {
    case CONFIG_ITEM_BOOL:
        i_type = VLC_VAR_BOOL;
        break;

    case CONFIG_ITEM_INTEGER:
    case CONFIG_ITEM_KEY:
        i_type = VLC_VAR_INTEGER;
        break;

    case CONFIG_ITEM_FLOAT:
        i_type = VLC_VAR_FLOAT;
        break;

    case CONFIG_ITEM_MODULE:
    case CONFIG_ITEM_MODULE_CAT:
    case CONFIG_ITEM_MODULE_LIST:
    case CONFIG_ITEM_MODULE_LIST_CAT:
        i_type = VLC_VAR_MODULE;
        break;

    case CONFIG_ITEM_STRING:
        i_type = VLC_VAR_STRING;
        break;

    case CONFIG_ITEM_PASSWORD:
        i_type = VLC_VAR_STRING;
        break;

    case CONFIG_ITEM_FILE:
        i_type = VLC_VAR_FILE;
        break;

    case CONFIG_ITEM_DIRECTORY:
        i_type = VLC_VAR_DIRECTORY;
        break;

    default:
        i_type = 0;
        break;
    }

    return i_type;
}

/*****************************************************************************
 * config_GetInt: get the value of an int variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by an integer (CONFIG_ITEM_INTEGER and
 * CONFIG_ITEM_BOOL).
 *****************************************************************************/
int __config_GetInt( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return -1;
    }

    if (!IsConfigIntegerType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to an int", psz_name );
        return -1;
    }

    return p_config->value.i;
}

/*****************************************************************************
 * config_GetFloat: get the value of a float variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by a float (CONFIG_ITEM_FLOAT).
 *****************************************************************************/
float __config_GetFloat( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return -1;
    }

    if (!IsConfigFloatType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a float", psz_name );
        return -1;
    }

    return p_config->value.f;
}

/*****************************************************************************
 * config_GetPsz: get the string value of a string variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by a string (CONFIG_ITEM_STRING, CONFIG_ITEM_FILE,
 * CONFIG_ITEM_DIRECTORY, CONFIG_ITEM_PASSWORD, and CONFIG_ITEM_MODULE).
 *
 * Important note: remember to free() the returned char* because it's a
 *   duplicate of the actual value. It isn't safe to return a pointer to the
 *   actual value as it can be modified at any time.
 *****************************************************************************/
char * __config_GetPsz( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return NULL;
    }

    if (!IsConfigStringType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a string", psz_name );
        return NULL;
    }

    /* return a copy of the string */
    vlc_mutex_lock( p_config->p_lock );
    char *psz_value = strdupnull (p_config->value.psz);
    vlc_mutex_unlock( p_config->p_lock );

    return psz_value;
}

/*****************************************************************************
 * config_PutPsz: set the string value of a string variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by a string (CONFIG_ITEM_STRING, CONFIG_ITEM_FILE,
 * CONFIG_ITEM_DIRECTORY, CONFIG_ITEM_PASSWORD, and CONFIG_ITEM_MODULE).
 *****************************************************************************/
void __config_PutPsz( vlc_object_t *p_this,
                      const char *psz_name, const char *psz_value )
{
    module_config_t *p_config;
    vlc_value_t oldval, val;

    p_config = config_FindConfig( p_this, psz_name );


    /* sanity checks */
    if( !p_config )
    {
        msg_Warn( p_this, "option %s does not exist", psz_name );
        return;
    }

    if (!IsConfigStringType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a string", psz_name );
        return;
    }

    vlc_mutex_lock( p_config->p_lock );

    /* backup old value */
    oldval.psz_string = (char *)p_config->value.psz;

    if ((psz_value != NULL) && *psz_value)
        p_config->value.psz = strdup (psz_value);
    else
        p_config->value.psz = NULL;

    p_config->b_dirty = true;

    val.psz_string = (char *)p_config->value.psz;

    vlc_mutex_unlock( p_config->p_lock );

    if( p_config->pf_callback )
    {
        p_config->pf_callback( p_this, psz_name, oldval, val,
                               p_config->p_callback_data );
    }

    /* free old string */
    free( oldval.psz_string );
}

/*****************************************************************************
 * config_PutInt: set the integer value of an int variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by an integer (CONFIG_ITEM_INTEGER and
 * CONFIG_ITEM_BOOL).
 *****************************************************************************/
void __config_PutInt( vlc_object_t *p_this, const char *psz_name, int i_value )
{
    module_config_t *p_config;
    vlc_value_t oldval, val;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Warn( p_this, "option %s does not exist", psz_name );
        return;
    }

    if (!IsConfigIntegerType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to an int", psz_name );
        return;
    }

    /* backup old value */
    oldval.i_int = p_config->value.i;

    /* if i_min == i_max == 0, then do not use them */
    if ((p_config->min.i == 0) && (p_config->max.i == 0))
    {
        p_config->value.i = i_value;
    }
    else if (i_value < p_config->min.i)
    {
        p_config->value.i = p_config->min.i;
    }
    else if (i_value > p_config->max.i)
    {
        p_config->value.i = p_config->max.i;
    }
    else
    {
        p_config->value.i = i_value;
    }

    p_config->b_dirty = true;

    val.i_int = p_config->value.i;

    if( p_config->pf_callback )
    {
        p_config->pf_callback( p_this, psz_name, oldval, val,
                               p_config->p_callback_data );
    }
}

/*****************************************************************************
 * config_PutFloat: set the value of a float variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by a float (CONFIG_ITEM_FLOAT).
 *****************************************************************************/
void __config_PutFloat( vlc_object_t *p_this,
                        const char *psz_name, float f_value )
{
    module_config_t *p_config;
    vlc_value_t oldval, val;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Warn( p_this, "option %s does not exist", psz_name );
        return;
    }

    if (!IsConfigFloatType (p_config->i_type))
    {
        msg_Err( p_this, "option %s does not refer to a float", psz_name );
        return;
    }

    /* backup old value */
    oldval.f_float = p_config->value.f;

    /* if f_min == f_max == 0, then do not use them */
    if ((p_config->min.f == 0) && (p_config->max.f == 0))
    {
        p_config->value.f = f_value;
    }
    else if (f_value < p_config->min.f)
    {
        p_config->value.f = p_config->min.f;
    }
    else if (f_value > p_config->max.f)
    {
        p_config->value.f = p_config->max.f;
    }
    else
    {
        p_config->value.f = f_value;
    }

    p_config->b_dirty = true;

    val.f_float = p_config->value.f;

    if( p_config->pf_callback )
    {
        p_config->pf_callback( p_this, psz_name, oldval, val,
                               p_config->p_callback_data );
    }
}

/*****************************************************************************
 * config_FindConfig: find the config structure associated with an option.
 *****************************************************************************
 * FIXME: This function really needs to be optimized.
 * FIXME: And now even more.
 * FIXME: remove p_this pointer parameter (or use it)
 *****************************************************************************/
module_config_t *config_FindConfig( vlc_object_t *p_this, const char *psz_name )
{
    module_t *p_parser;

    if( !psz_name ) return NULL;

    module_t **list = module_list_get (NULL);
    if (list == NULL)
        return NULL;

    for (size_t i = 0; (p_parser = list[i]) != NULL; i++)
    {
        module_config_t *p_item, *p_end;

        if( !p_parser->i_config_items )
            continue;

        for( p_item = p_parser->p_config, p_end = p_item + p_parser->confsize;
             p_item < p_end;
             p_item++ )
        {
            if( p_item->i_type & CONFIG_HINT )
                /* ignore hints */
                continue;
            if( !strcmp( psz_name, p_item->psz_name )
             || ( p_item->psz_oldname
              && !strcmp( psz_name, p_item->psz_oldname ) ) )
            {
                module_list_free (list);
                return p_item;
            }
        }
    }

    module_list_free (list);
    return NULL;
}

/*****************************************************************************
 * config_Free: frees a duplicated module's configuration data.
 *****************************************************************************
 * This function frees all the data duplicated by config_Duplicate.
 *****************************************************************************/
void config_Free( module_t *p_module )
{
    int i;

    for (size_t j = 0; j < p_module->confsize; j++)
    {
        module_config_t *p_item = p_module->p_config + j;

        free( p_item->psz_type );
        free( p_item->psz_name );
        free( p_item->psz_text );
        free( p_item->psz_longtext );
        free( p_item->psz_oldname );

        if (IsConfigStringType (p_item->i_type))
        {
            free (p_item->value.psz);
            free (p_item->orig.psz);
            free (p_item->saved.psz);
        }

        if( p_item->ppsz_list )
            for( i = 0; i < p_item->i_list; i++ )
                free( p_item->ppsz_list[i] );
        if( p_item->ppsz_list_text )
            for( i = 0; i < p_item->i_list; i++ )
                free( p_item->ppsz_list_text[i] );
        free( p_item->ppsz_list );
        free( p_item->ppsz_list_text );
        free( p_item->pi_list );

        if( p_item->i_action )
        {
            for( i = 0; i < p_item->i_action; i++ )
            {
                free( p_item->ppsz_action_text[i] );
            }
            free( p_item->ppf_action );
            free( p_item->ppsz_action_text );
        }
    }

    free (p_module->p_config);
    p_module->p_config = NULL;
}

/*****************************************************************************
 * config_SetCallbacks: sets callback functions in the duplicate p_config.
 *****************************************************************************
 * Unfortunatly we cannot work directly with the module's config data as
 * this module might be unloaded from memory at any time (remember HideModule).
 * This is why we need to duplicate callbacks each time we reload the module.
 *****************************************************************************/
void config_SetCallbacks( module_config_t *p_new, module_config_t *p_orig,
                          size_t n )
{
    for (size_t i = 0; i < n; i++)
    {
        p_new->pf_callback = p_orig->pf_callback;
        p_new++;
        p_orig++;
    }
}

/*****************************************************************************
 * config_UnsetCallbacks: unsets callback functions in the duplicate p_config.
 *****************************************************************************
 * We simply undo what we did in config_SetCallbacks.
 *****************************************************************************/
void config_UnsetCallbacks( module_config_t *p_new, size_t n )
{
    for (size_t i = 0; i < n; i++)
    {
        p_new->pf_callback = NULL;
        p_new++;
    }
}

/*****************************************************************************
 * config_ResetAll: reset the configuration data for all the modules.
 *****************************************************************************/
void __config_ResetAll( vlc_object_t *p_this )
{
    module_t *p_module;
    module_t **list = module_list_get (NULL);

    for (size_t j = 0; (p_module = list[j]) != NULL; j++)
    {
        if( p_module->b_submodule ) continue;

        for (size_t i = 0; i < p_module->confsize; i++ )
        {
            module_config_t *p_config = p_module->p_config + i;

            vlc_mutex_lock (p_config->p_lock);
            if (IsConfigIntegerType (p_config->i_type))
                p_config->value.i = p_config->orig.i;
            else
            if (IsConfigFloatType (p_config->i_type))
                p_config->value.f = p_config->orig.f;
            else
            if (IsConfigStringType (p_config->i_type))
            {
                free ((char *)p_config->value.psz);
                p_config->value.psz =
                        strdupnull (p_config->orig.psz);
            }
            vlc_mutex_unlock (p_config->p_lock);
        }
    }

    module_list_free (list);
}
