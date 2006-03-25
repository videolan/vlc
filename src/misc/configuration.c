/*****************************************************************************
 * configuration.c management of the modules configuration
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
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

#include <vlc/vlc.h>
#include "vlc_keys.h"
#include "charset.h"

#include <stdio.h>                                              /* sprintf() */
#include <stdlib.h>                                      /* free(), strtol() */
#include <string.h>                                              /* strdup() */
#include <errno.h>                                                  /* errno */

#ifdef HAVE_LIMITS_H
#   include <limits.h>
#endif

#ifdef HAVE_UNISTD_H
#    include <unistd.h>                                          /* getuid() */
#endif

#ifdef HAVE_GETOPT_LONG
#   ifdef HAVE_GETOPT_H
#       include <getopt.h>                                       /* getopt() */
#   endif
#else
#   include "../extras/getopt.h"
#endif

#if defined(HAVE_GETPWUID)
#   include <pwd.h>                                            /* getpwuid() */
#endif

#if defined( HAVE_SYS_STAT_H )
#   include <sys/stat.h>
#endif
#if defined( HAVE_SYS_TYPES_H )
#   include <sys/types.h>
#endif
#if defined( WIN32 )
#   if !defined( UNDER_CE )
#       include <direct.h>
#   endif
#include <tchar.h>
#endif

#if defined( WIN32 ) || defined( UNDER_CE )
#   define DIR_SEP "\\"
#else
#   define DIR_SEP "/"
#endif

static int ConfigStringToKey( char * );
static char *ConfigKeyToString( int );

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
    if( (p_config->i_type!=CONFIG_ITEM_INTEGER) &&
        (p_config->i_type!=CONFIG_ITEM_KEY) &&
        (p_config->i_type!=CONFIG_ITEM_BOOL) )
    {
        msg_Err( p_this, "option %s does not refer to an int", psz_name );
        return -1;
    }

    return p_config->i_value;
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
    if( p_config->i_type != CONFIG_ITEM_FLOAT )
    {
        msg_Err( p_this, "option %s does not refer to a float", psz_name );
        return -1;
    }

    return p_config->f_value;
}

/*****************************************************************************
 * config_GetPsz: get the string value of a string variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by a string (CONFIG_ITEM_STRING, CONFIG_ITEM_FILE,
 * CONFIG_ITEM_DIRECTORY, and CONFIG_ITEM_MODULE).
 *
 * Important note: remember to free() the returned char* because it's a
 *   duplicate of the actual value. It isn't safe to return a pointer to the
 *   actual value as it can be modified at any time.
 *****************************************************************************/
char * __config_GetPsz( vlc_object_t *p_this, const char *psz_name )
{
    module_config_t *p_config;
    char *psz_value = NULL;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return NULL;
    }
    if( (p_config->i_type!=CONFIG_ITEM_STRING) &&
        (p_config->i_type!=CONFIG_ITEM_FILE) &&
        (p_config->i_type!=CONFIG_ITEM_DIRECTORY) &&
        (p_config->i_type!=CONFIG_ITEM_MODULE_LIST) &&
        (p_config->i_type!=CONFIG_ITEM_MODULE_LIST_CAT) &&
        (p_config->i_type!=CONFIG_ITEM_MODULE_CAT) &&
        (p_config->i_type!=CONFIG_ITEM_MODULE) )
    {
        msg_Err( p_this, "option %s does not refer to a string", psz_name );
        return NULL;
    }


    /* return a copy of the string */
    vlc_mutex_lock( p_config->p_lock );
    if( p_config->psz_value ) psz_value = strdup( p_config->psz_value );
    vlc_mutex_unlock( p_config->p_lock );

    return psz_value;
}

/*****************************************************************************
 * config_PutPsz: set the string value of a string variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by a string (CONFIG_ITEM_STRING, CONFIG_ITEM_FILE,
 * CONFIG_ITEM_DIRECTORY, and CONFIG_ITEM_MODULE).
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
    if( (p_config->i_type!=CONFIG_ITEM_STRING) &&
        (p_config->i_type!=CONFIG_ITEM_FILE) &&
        (p_config->i_type!=CONFIG_ITEM_DIRECTORY) &&
        (p_config->i_type!=CONFIG_ITEM_MODULE_LIST) &&
        (p_config->i_type!=CONFIG_ITEM_MODULE_CAT) &&
        (p_config->i_type!=CONFIG_ITEM_MODULE_LIST_CAT) &&
        (p_config->i_type!=CONFIG_ITEM_MODULE) )
    {
        msg_Err( p_this, "option %s does not refer to a string", psz_name );
        return;
    }

    vlc_mutex_lock( p_config->p_lock );

    /* backup old value */
    oldval.psz_string = p_config->psz_value;

    if( psz_value && *psz_value ) p_config->psz_value = strdup( psz_value );
    else p_config->psz_value = NULL;

    p_config->b_dirty = VLC_TRUE;

    val.psz_string = p_config->psz_value;

    vlc_mutex_unlock( p_config->p_lock );

    if( p_config->pf_callback )
    {
        p_config->pf_callback( p_this, psz_name, oldval, val,
                               p_config->p_callback_data );
    }

    /* free old string */
    if( oldval.psz_string ) free( oldval.psz_string );
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
    if( (p_config->i_type!=CONFIG_ITEM_INTEGER) &&
        (p_config->i_type!=CONFIG_ITEM_KEY) &&
        (p_config->i_type!=CONFIG_ITEM_BOOL) )
    {
        msg_Err( p_this, "option %s does not refer to an int", psz_name );
        return;
    }

    /* backup old value */
    oldval.i_int = p_config->i_value;

    /* if i_min == i_max == 0, then do not use them */
    if ((p_config->i_min == 0) && (p_config->i_max == 0))
    {
        p_config->i_value = i_value;
    }
    else if (i_value < p_config->i_min)
    {
        p_config->i_value = p_config->i_min;
    }
    else if (i_value > p_config->i_max)
    {
        p_config->i_value = p_config->i_max;
    }
    else
    {
        p_config->i_value = i_value;
    }

    p_config->b_dirty = VLC_TRUE;

    val.i_int = p_config->i_value;

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
    if( p_config->i_type != CONFIG_ITEM_FLOAT )
    {
        msg_Err( p_this, "option %s does not refer to a float", psz_name );
        return;
    }

    /* backup old value */
    oldval.f_float = p_config->f_value;

    /* if f_min == f_max == 0, then do not use them */
    if ((p_config->f_min == 0) && (p_config->f_max == 0))
    {
        p_config->f_value = f_value;
    }
    else if (f_value < p_config->f_min)
    {
        p_config->f_value = p_config->f_min;
    }
    else if (f_value > p_config->f_max)
    {
        p_config->f_value = p_config->f_max;
    }
    else
    {
        p_config->f_value = f_value;
    }

    p_config->b_dirty = VLC_TRUE;

    val.f_float = p_config->f_value;

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
 *****************************************************************************/
module_config_t *config_FindConfig( vlc_object_t *p_this, const char *psz_name )
{
    vlc_list_t *p_list;
    module_t *p_parser;
    module_config_t *p_item;
    int i_index;

    if( !psz_name ) return NULL;

    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !p_parser->i_config_items )
            continue;

        for( p_item = p_parser->p_config;
             p_item->i_type != CONFIG_HINT_END;
             p_item++ )
        {
            if( p_item->i_type & CONFIG_HINT )
                /* ignore hints */
                continue;
            if( !strcmp( psz_name, p_item->psz_name ) )
            {
                vlc_list_release( p_list );
                return p_item;
            }
        }
    }

    vlc_list_release( p_list );

    return NULL;
}

/*****************************************************************************
 * config_FindModule: find a specific module structure.
 *****************************************************************************/
module_t *config_FindModule( vlc_object_t *p_this, const char *psz_name )
{
    vlc_list_t *p_list;
    module_t *p_module, *p_result = NULL;
    int i_index;

    if( !psz_name ) return NULL;

    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object;
        if( !strcmp( p_module->psz_object_name, psz_name ) )
        {
             p_result = p_module;
             break;
        }
    }

    vlc_list_release( p_list );

    return p_result;
}

/*****************************************************************************
 * config_Duplicate: creates a duplicate of a module's configuration data.
 *****************************************************************************
 * Unfortunatly we cannot work directly with the module's config data as
 * this module might be unloaded from memory at any time (remember HideModule).
 * This is why we need to create an exact copy of the config data.
 *****************************************************************************/
void config_Duplicate( module_t *p_module, module_config_t *p_orig )
{
    int i, j, i_lines = 1;
    module_config_t *p_item;

    /* Calculate the structure length */
    p_module->i_config_items = 0;
    p_module->i_bool_items = 0;

    for( p_item = p_orig; p_item->i_type != CONFIG_HINT_END; p_item++ )
    {
        i_lines++;

        if( p_item->i_type & CONFIG_ITEM )
        {
            p_module->i_config_items++;
        }

        if( p_item->i_type == CONFIG_ITEM_BOOL )
        {
            p_module->i_bool_items++;
        }
    }

    /* Allocate memory */
    p_module->p_config = (module_config_t *)malloc( sizeof(module_config_t)
                                                     * i_lines );
    if( p_module->p_config == NULL )
    {
        msg_Err( p_module, "config error: can't duplicate p_config" );
        return;
    }

    /* Do the duplication job */
    for( i = 0; i < i_lines ; i++ )
    {
        p_module->p_config[i] = p_orig[i];

        p_module->p_config[i].i_value_orig = p_orig[i].i_value;
        p_module->p_config[i].f_value_orig = p_orig[i].f_value;
        p_module->p_config[i].i_value_saved = p_orig[i].i_value;
        p_module->p_config[i].f_value_saved = p_orig[i].f_value;
        p_module->p_config[i].psz_value_saved = 0;

        p_module->p_config[i].psz_type = p_orig[i].psz_type ?
                                   strdup( p_orig[i].psz_type ) : NULL;
        p_module->p_config[i].psz_name = p_orig[i].psz_name ?
                                   strdup( p_orig[i].psz_name ) : NULL;
        p_module->p_config[i].psz_current = p_orig[i].psz_current?
                                   strdup( p_orig[i].psz_current ) : NULL;
        p_module->p_config[i].psz_text = p_orig[i].psz_text ?
                                   strdup( _(p_orig[i].psz_text) ) : NULL;
        p_module->p_config[i].psz_longtext = p_orig[i].psz_longtext ?
                                   strdup( _(p_orig[i].psz_longtext) ) : NULL;
        p_module->p_config[i].psz_value = p_orig[i].psz_value ?
                                   strdup( p_orig[i].psz_value ) : NULL;
        p_module->p_config[i].psz_value_orig = p_orig[i].psz_value ?
                                   strdup( p_orig[i].psz_value ) : NULL;

        p_module->p_config[i].p_lock = &p_module->object_lock;

        /* duplicate the string list */
        if( p_orig[i].i_list )
        {
            if( p_orig[i].ppsz_list )
            {
                p_module->p_config[i].ppsz_list =
                    malloc( (p_orig[i].i_list + 1) * sizeof(char *) );
                if( p_module->p_config[i].ppsz_list )
                {
                    for( j = 0; j < p_orig[i].i_list; j++ )
                        p_module->p_config[i].ppsz_list[j] = p_orig[i].ppsz_list[j] ?
                            strdup( p_orig[i].ppsz_list[j] ) : NULL ;
                    p_module->p_config[i].ppsz_list[j] = NULL;
                }
            }
            if( p_orig[i].ppsz_list_text )
            {
                p_module->p_config[i].ppsz_list_text =
                    malloc( (p_orig[i].i_list + 1) * sizeof(char *) );
                if( p_module->p_config[i].ppsz_list_text )
                {
                    for( j = 0; j < p_orig[i].i_list; j++ )
                        p_module->p_config[i].ppsz_list_text[j] = _(p_orig[i].ppsz_list_text[j]) ?
                            strdup( _(p_orig[i].ppsz_list_text[j] ) ) : NULL ;
                    p_module->p_config[i].ppsz_list_text[j] = NULL;
                }
            }
            if( p_orig[i].pi_list )
            {
                p_module->p_config[i].pi_list =
                    malloc( (p_orig[i].i_list + 1) * sizeof(int) );
                if( p_module->p_config[i].pi_list )
                {
                    for( j = 0; j < p_orig[i].i_list; j++ )
                        p_module->p_config[i].pi_list[j] =
                            p_orig[i].pi_list[j];
                }
            }
        }

        /* duplicate the actions list */
        if( p_orig[i].i_action )
        {
            int j;

            p_module->p_config[i].ppf_action =
                malloc( p_orig[i].i_action * sizeof(void *) );
            p_module->p_config[i].ppsz_action_text =
                malloc( p_orig[i].i_action * sizeof(char *) );

            for( j = 0; j < p_orig[i].i_action; j++ )
            {
                p_module->p_config[i].ppf_action[j] =
                    p_orig[i].ppf_action[j];
                p_module->p_config[i].ppsz_action_text[j] =
                    p_orig[i].ppsz_action_text[j] ?
                    strdup( p_orig[i].ppsz_action_text[j] ) : NULL;
            }
        }

        p_module->p_config[i].pf_callback = p_orig[i].pf_callback;
    }
}

/*****************************************************************************
 * config_Free: frees a duplicated module's configuration data.
 *****************************************************************************
 * This function frees all the data duplicated by config_Duplicate.
 *****************************************************************************/
void config_Free( module_t *p_module )
{
    module_config_t *p_item = p_module->p_config;
    int i;

    if( p_item == NULL )
    {
        return;
    }

    for( ; p_item->i_type != CONFIG_HINT_END ; p_item++ )
    {
        if( p_item->psz_type )
            free( p_item->psz_type );

        if( p_item->psz_name )
            free( p_item->psz_name );

        if( p_item->psz_current )
            free( p_item->psz_current );

        if( p_item->psz_text )
            free( p_item->psz_text );

        if( p_item->psz_longtext )
            free( p_item->psz_longtext );

        if( p_item->psz_value )
            free( p_item->psz_value );

        if( p_item->psz_value_orig )
            free( p_item->psz_value_orig );

        if( p_item->psz_value_saved )
            free( p_item->psz_value_saved );

        if( p_item->i_list )
        {
            for( i = 0; i < p_item->i_list; i++ )
            {
                if( p_item->ppsz_list && p_item->ppsz_list[i] )
                    free( p_item->ppsz_list[i] );
                if( p_item->ppsz_list_text && p_item->ppsz_list_text[i] )
                    free( p_item->ppsz_list_text[i] );
            }
            if( p_item->ppsz_list ) free( p_item->ppsz_list );
            if( p_item->ppsz_list_text ) free( p_item->ppsz_list_text );
            if( p_item->pi_list ) free( p_item->pi_list );
        }

        if( p_item->i_action )
        {
            for( i = 0; i < p_item->i_action; i++ )
            {
                if( p_item->ppsz_action_text[i] )
                    free( p_item->ppsz_action_text[i] );
            }
            if( p_item->ppf_action ) free( p_item->ppf_action );
            if( p_item->ppsz_action_text ) free( p_item->ppsz_action_text );
        }
    }

    free( p_module->p_config );
    p_module->p_config = NULL;
}

/*****************************************************************************
 * config_SetCallbacks: sets callback functions in the duplicate p_config.
 *****************************************************************************
 * Unfortunatly we cannot work directly with the module's config data as
 * this module might be unloaded from memory at any time (remember HideModule).
 * This is why we need to duplicate callbacks each time we reload the module.
 *****************************************************************************/
void config_SetCallbacks( module_config_t *p_new, module_config_t *p_orig )
{
    while( p_new->i_type != CONFIG_HINT_END )
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
void config_UnsetCallbacks( module_config_t *p_new )
{
    while( p_new->i_type != CONFIG_HINT_END )
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
    int i_index, i;
    vlc_list_t *p_list;
    module_t *p_module;

    /* Acquire config file lock */
    vlc_mutex_lock( &p_this->p_vlc->config_lock );

    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object ;
        if( p_module->b_submodule ) continue;

        for( i = 0; p_module->p_config[i].i_type != CONFIG_HINT_END; i++ )
        {
            p_module->p_config[i].i_value = p_module->p_config[i].i_value_orig;
            p_module->p_config[i].f_value = p_module->p_config[i].f_value_orig;
            if( p_module->p_config[i].psz_value )
                free( p_module->p_config[i].psz_value );
            p_module->p_config[i].psz_value =
                p_module->p_config[i].psz_value_orig ?
                strdup( p_module->p_config[i].psz_value_orig ) : NULL;
        }
    }

    vlc_list_release( p_list );
    vlc_mutex_unlock( &p_this->p_vlc->config_lock );
}

/*****************************************************************************
 * config_LoadConfigFile: loads the configuration file.
 *****************************************************************************
 * This function is called to load the config options stored in the config
 * file.
 *****************************************************************************/
int __config_LoadConfigFile( vlc_object_t *p_this, const char *psz_module_name )
{
    vlc_list_t *p_list;
    module_t *p_parser;
    module_config_t *p_item;
    FILE *file;
    char line[1024];
    char *p_index, *psz_option_name, *psz_option_value;
    char *psz_filename, *psz_homedir, *psz_configfile;
    int i_index;

    psz_configfile = p_this->p_vlc->psz_configfile;
    if( !psz_configfile || !psz_configfile )
    {
        psz_homedir = p_this->p_vlc->psz_homedir;
        if( !psz_homedir )
        {
            msg_Err( p_this, "psz_homedir is null" );
            return -1;
        }
        psz_filename = (char *)malloc( sizeof("/" CONFIG_DIR "/" CONFIG_FILE) +
                                       strlen(psz_homedir) );
        if( psz_filename )
            sprintf( psz_filename,
                     "%s" DIR_SEP CONFIG_DIR DIR_SEP CONFIG_FILE,
                     psz_homedir );
    }
    else
    {
        psz_filename = strdup( psz_configfile );
    }

    if( !psz_filename )
    {
        msg_Err( p_this, "out of memory" );
        return -1;
    }

    msg_Dbg( p_this, "opening config file %s", psz_filename );

    /* Acquire config file lock */
    vlc_mutex_lock( &p_this->p_vlc->config_lock );

    file = utf8_fopen( psz_filename, "rt" );
    if( !file )
    {
        msg_Warn( p_this, "config file %s does not exist yet", psz_filename );
        free( psz_filename );
        vlc_mutex_unlock( &p_this->p_vlc->config_lock );
        return -1;
    }

    /* Look for the selected module, if NULL then save everything */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( psz_module_name
             && strcmp( psz_module_name, p_parser->psz_object_name ) )
        {
            continue;
        }

        /* The config file is organized in sections, one per module. Look for
         * the interesting section ( a section is of the form [foo] ) */
        fseek( file, 0L, SEEK_SET );
        while( fgets( line, 1024, file ) )
        {
            if( (line[0] == '[')
               && (p_index = strchr(line,']'))
               && (p_index - &line[1]
                    == (int)strlen(p_parser->psz_object_name))
               && !memcmp( &line[1], p_parser->psz_object_name,
                           strlen(p_parser->psz_object_name) ) )
            {
#if 0
                msg_Dbg( p_this, "loading config for module \"%s\"",
                                 p_parser->psz_object_name );
#endif

                break;
            }
        }
        /* either we found the section or we're at the EOF */

        /* Now try to load the options in this section */
        while( fgets( line, 1024, file ) )
        {
            if( line[0] == '[' ) break; /* end of section */

            /* ignore comments or empty lines */
            if( (line[0] == '#') || (line[0] == '\n') || (line[0] == (char)0) )
                continue;

            /* get rid of line feed */
            if( line[strlen(line)-1] == '\n' )
                line[strlen(line)-1] = (char)0;

            /* look for option name */
            psz_option_name = line;
            psz_option_value = NULL;
            p_index = strchr( line, '=' );
            if( !p_index ) break; /* this ain't an option!!! */

            *p_index = (char)0;
            psz_option_value = p_index + 1;

            if( !p_parser->i_config_items )
            {
                continue;
            }

            /* try to match this option with one of the module's options */
            for( p_item = p_parser->p_config;
                 p_item->i_type != CONFIG_HINT_END;
                 p_item++ )
            {
                if( p_item->i_type & CONFIG_HINT )
                    /* ignore hints */
                    continue;

                if( !strcmp( p_item->psz_name, psz_option_name ) )
                {
                    /* We found it */
                    switch( p_item->i_type )
                    {
                    case CONFIG_ITEM_BOOL:
                    case CONFIG_ITEM_INTEGER:
                        if( !*psz_option_value )
                            break;                    /* ignore empty option */
                        p_item->i_value = strtol( psz_option_value, 0, 0 );
                        p_item->i_value_saved = p_item->i_value;
#if 0
                        msg_Dbg( p_this, "option \"%s\", value %i",
                                 p_item->psz_name, p_item->i_value );
#endif
                        break;

                    case CONFIG_ITEM_FLOAT:
                        if( !*psz_option_value )
                            break;                    /* ignore empty option */
                        p_item->f_value = (float)i18n_atof( psz_option_value);
                        p_item->f_value_saved = p_item->f_value;
#if 0
                        msg_Dbg( p_this, "option \"%s\", value %f",
                                 p_item->psz_name, (double)p_item->f_value );
#endif
                        break;
                    case CONFIG_ITEM_KEY:
                        if( !*psz_option_value )
                            break;                    /* ignore empty option */
                        p_item->i_value = ConfigStringToKey(psz_option_value);
                        p_item->i_value_saved = p_item->i_value;
                        break;

                    default:
                        vlc_mutex_lock( p_item->p_lock );

                        /* free old string */
                        if( p_item->psz_value )
                            free( p_item->psz_value );

                        p_item->psz_value = *psz_option_value ?
                            strdup( psz_option_value ) : NULL;

                        if( p_item->psz_value_saved )
                            free( p_item->psz_value_saved );
                        p_item->psz_value_saved = 0;
                        if( !p_item->psz_value || !p_item->psz_value_orig ||
                            (p_item->psz_value && p_item->psz_value_orig &&
                             strcmp(p_item->psz_value,p_item->psz_value_orig)))
                            p_item->psz_value_saved = p_item->psz_value ?
                                strdup( p_item->psz_value ) : 0;

                        vlc_mutex_unlock( p_item->p_lock );

#if 0
                        msg_Dbg( p_this, "option \"%s\", value \"%s\"",
                                 p_item->psz_name,
                                 p_item->psz_value ? p_item->psz_value : "" );
#endif
                        break;
                    }
                }
            }
        }

    }

    vlc_list_release( p_list );

    fclose( file );
    free( psz_filename );

    vlc_mutex_unlock( &p_this->p_vlc->config_lock );

    return 0;
}

/*****************************************************************************
 * config_CreateDir: Create configuration directory if it doesn't exist.
 *****************************************************************************/
int config_CreateDir( vlc_object_t *p_this, const char *psz_dirname )
{
    if( !psz_dirname && !*psz_dirname ) return -1;

    if( utf8_mkdir( psz_dirname ) && ( errno != EEXIST ) )
    {
        msg_Err( p_this, "could not create %s (%s)",
                 psz_dirname, strerror(errno) );
        return -1;
    }

    return 0;
}

/*****************************************************************************
 * config_SaveConfigFile: Save a module's config options.
 *****************************************************************************
 * This will save the specified module's config options to the config file.
 * If psz_module_name is NULL then we save all the modules config options.
 * It's no use to save the config options that kept their default values, so
 * we'll try to be a bit clever here.
 *
 * When we save we mustn't delete the config options of the modules that
 * haven't been loaded. So we cannot just create a new config file with the
 * config structures we've got in memory.
 * I don't really know how to deal with this nicely, so I will use a completly
 * dumb method ;-)
 * I will load the config file in memory, but skipping all the sections of the
 * modules we want to save. Then I will create a brand new file, dump the file
 * loaded in memory and then append the sections of the modules we want to
 * save.
 * Really stupid no ?
 *****************************************************************************/
static int SaveConfigFile( vlc_object_t *p_this, const char *psz_module_name,
                           vlc_bool_t b_autosave )
{
    module_t *p_parser;
    vlc_list_t *p_list;
    module_config_t *p_item;
    FILE *file;
    char p_line[1024], *p_index2;
    int i_sizebuf = 0;
    char *p_bigbuffer, *p_index;
    vlc_bool_t b_backup;
    char *psz_filename, *psz_homedir, *psz_configfile;
    int i_index;

    /* Acquire config file lock */
    vlc_mutex_lock( &p_this->p_vlc->config_lock );

    psz_configfile = p_this->p_vlc->psz_configfile;
    if( !psz_configfile || !psz_configfile )
    {
        psz_homedir = p_this->p_vlc->psz_homedir;
        if( !psz_homedir )
        {
            msg_Err( p_this, "psz_homedir is null" );
            vlc_mutex_unlock( &p_this->p_vlc->config_lock );
            return -1;
        }
        psz_filename = (char *)malloc( sizeof("/" CONFIG_DIR "/" CONFIG_FILE) +
                                       strlen(psz_homedir) );

        if( psz_filename )
            sprintf( psz_filename, "%s" DIR_SEP CONFIG_DIR, psz_homedir );

        if( !psz_filename )
        {
            msg_Err( p_this, "out of memory" );
            vlc_mutex_unlock( &p_this->p_vlc->config_lock );
            return -1;
        }

        config_CreateDir( p_this, psz_filename );

        strcat( psz_filename, DIR_SEP CONFIG_FILE );
    }
    else
    {
        psz_filename = strdup( psz_configfile );
        if( !psz_filename )
        {
            msg_Err( p_this, "out of memory" );
            vlc_mutex_unlock( &p_this->p_vlc->config_lock );
            return -1;
        }
    }

    msg_Dbg( p_this, "opening config file %s", psz_filename );

    file = utf8_fopen( psz_filename, "rt" );
    if( !file )
    {
        msg_Warn( p_this, "config file %s does not exist yet", psz_filename );
    }
    else
    {
        /* look for file size */
        fseek( file, 0L, SEEK_END );
        i_sizebuf = ftell( file );
        fseek( file, 0L, SEEK_SET );
    }

    p_bigbuffer = p_index = malloc( i_sizebuf+1 );
    if( !p_bigbuffer )
    {
        msg_Err( p_this, "out of memory" );
        if( file ) fclose( file );
        free( psz_filename );
        vlc_mutex_unlock( &p_this->p_vlc->config_lock );
        return -1;
    }
    p_bigbuffer[0] = 0;

    /* List all available modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /* backup file into memory, we only need to backup the sections we won't
     * save later on */
    b_backup = 0;
    while( file && fgets( p_line, 1024, file ) )
    {
        if( (p_line[0] == '[') && (p_index2 = strchr(p_line,']')))
        {

            /* we found a section, check if we need to do a backup */
            for( i_index = 0; i_index < p_list->i_count; i_index++ )
            {
                p_parser = (module_t *)p_list->p_values[i_index].p_object ;

                if( ((p_index2 - &p_line[1])
                       == (int)strlen(p_parser->psz_object_name) )
                    && !memcmp( &p_line[1], p_parser->psz_object_name,
                                strlen(p_parser->psz_object_name) ) )
                {
                    if( !psz_module_name )
                        break;
                    else if( !strcmp( psz_module_name,
                                      p_parser->psz_object_name ) )
                        break;
                }
            }

            if( i_index == p_list->i_count )
            {
                /* we don't have this section in our list so we need to back
                 * it up */
                *p_index2 = 0;
#if 0
                msg_Dbg( p_this, "backing up config for unknown module \"%s\"",
                                 &p_line[1] );
#endif
                *p_index2 = ']';

                b_backup = 1;
            }
            else
            {
                b_backup = 0;
            }
        }

        /* save line if requested and line is valid (doesn't begin with a
         * space, tab, or eol) */
        if( b_backup && (p_line[0] != '\n') && (p_line[0] != ' ')
            && (p_line[0] != '\t') )
        {
            strcpy( p_index, p_line );
            p_index += strlen( p_line );
        }
    }
    if( file ) fclose( file );


    /*
     * Save module config in file
     */

    file = utf8_fopen( psz_filename, "wt" );
    if( !file )
    {
        msg_Warn( p_this, "could not open config file %s for writing",
                          psz_filename );
        free( psz_filename );
        vlc_list_release( p_list );
        vlc_mutex_unlock( &p_this->p_vlc->config_lock );
        return -1;
    }

    fprintf( file, "###\n###  " COPYRIGHT_MESSAGE "\n###\n\n"
       "###\n### lines begining with a '#' character are comments\n###\n\n" );

    /* Look for the selected module, if NULL then save everything */
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( psz_module_name && strcmp( psz_module_name,
                                       p_parser->psz_object_name ) )
            continue;

        if( !p_parser->i_config_items )
            continue;

        if( psz_module_name )
            msg_Dbg( p_this, "saving config for module \"%s\"",
                     p_parser->psz_object_name );

        fprintf( file, "[%s]", p_parser->psz_object_name );
        if( p_parser->psz_longname )
            fprintf( file, " # %s\n\n", p_parser->psz_longname );
        else
            fprintf( file, "\n\n" );

        for( p_item = p_parser->p_config;
             p_item->i_type != CONFIG_HINT_END;
             p_item++ )
        {
            char  *psz_key;
            int   i_value = p_item->i_value;
            float f_value = p_item->f_value;
            char  *psz_value = p_item->psz_value;

            if( p_item->i_type & CONFIG_HINT )
                /* ignore hints */
                continue;
            /* Ignore deprecated options */
            if( p_item->psz_current )
                continue;
            if( b_autosave && !p_item->b_autosave )
            {
                i_value = p_item->i_value_saved;
                f_value = p_item->f_value_saved;
                psz_value = p_item->psz_value_saved;
                if( !psz_value ) psz_value = p_item->psz_value_orig;
            }
            else
            {
                p_item->b_dirty = VLC_FALSE;
            }

            switch( p_item->i_type )
            {
            case CONFIG_ITEM_BOOL:
            case CONFIG_ITEM_INTEGER:
                if( p_item->psz_text )
                    fprintf( file, "# %s (%s)\n", p_item->psz_text,
                             (p_item->i_type == CONFIG_ITEM_BOOL) ?
                             _("boolean") : _("integer") );
                if( i_value == p_item->i_value_orig )
                    fprintf( file, "#" );
                fprintf( file, "%s=%i\n", p_item->psz_name, i_value );

                p_item->i_value_saved = i_value;
                break;

            case CONFIG_ITEM_KEY:
                if( p_item->psz_text )
                    fprintf( file, "# %s (%s)\n", p_item->psz_text,
                             _("key") );
                if( i_value == p_item->i_value_orig )
                    fprintf( file, "#" );
                psz_key = ConfigKeyToString( i_value );
                fprintf( file, "%s=%s\n", p_item->psz_name,
                         psz_key ? psz_key : "" );
                if ( psz_key ) free( psz_key );

                p_item->i_value_saved = i_value;
                break;

            case CONFIG_ITEM_FLOAT:
                if( p_item->psz_text )
                    fprintf( file, "# %s (%s)\n", p_item->psz_text,
                             _("float") );
                if( f_value == p_item->f_value_orig )
                    fprintf( file, "#" );
                fprintf( file, "%s=%f\n", p_item->psz_name, (double)f_value );

                p_item->f_value_saved = f_value;
                break;

            default:
                if( p_item->psz_text )
                    fprintf( file, "# %s (%s)\n", p_item->psz_text,
                             _("string") );
                if( (!psz_value && !p_item->psz_value_orig) ||
                    (psz_value && p_item->psz_value_orig &&
                     !strcmp( psz_value, p_item->psz_value_orig )) )
                    fprintf( file, "#" );
                fprintf( file, "%s=%s\n", p_item->psz_name,
                         psz_value ? psz_value : "" );

                if( b_autosave && !p_item->b_autosave ) break;

                if( p_item->psz_value_saved ) free( p_item->psz_value_saved );
                p_item->psz_value_saved = 0;
                if( (psz_value && p_item->psz_value_orig &&
                     strcmp( psz_value, p_item->psz_value_orig )) ||
                    !psz_value || !p_item->psz_value_orig)
                    p_item->psz_value_saved = psz_value ? strdup(psz_value):0;
            }
        }

        fprintf( file, "\n" );
    }

    vlc_list_release( p_list );

    /*
     * Restore old settings from the config in file
     */
    fputs( p_bigbuffer, file );
    free( p_bigbuffer );

    fclose( file );
    free( psz_filename );
    vlc_mutex_unlock( &p_this->p_vlc->config_lock );

    return 0;
}

int config_AutoSaveConfigFile( vlc_object_t *p_this )
{
    vlc_list_t *p_list;
    module_t *p_parser;
    module_config_t *p_item;
    int i_index, i_count;

    /* Check if there's anything to save */
    vlc_mutex_lock( &p_this->p_vlc->config_lock );
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    i_count = p_list->i_count;
    for( i_index = 0; i_index < i_count; i_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( !p_parser->i_config_items ) continue;

        for( p_item = p_parser->p_config;
             p_item->i_type != CONFIG_HINT_END;
             p_item++ )
        {
            if( p_item->b_autosave && p_item->b_dirty ) break;
        }
        if( p_item->i_type != CONFIG_HINT_END ) break;
    }
    vlc_list_release( p_list );
    vlc_mutex_unlock( &p_this->p_vlc->config_lock );

    if( i_index == i_count ) return VLC_SUCCESS;
    return SaveConfigFile( p_this, 0, VLC_TRUE );
}

int __config_SaveConfigFile( vlc_object_t *p_this, const char *psz_module_name )
{
    return SaveConfigFile( p_this, psz_module_name, VLC_FALSE );
}

/*****************************************************************************
 * config_LoadCmdLine: parse command line
 *****************************************************************************
 * Parse command line for configuration options.
 * Now that the module_bank has been initialized, we can dynamically
 * generate the longopts structure used by getops. We have to do it this way
 * because we don't know (and don't want to know) in advance the configuration
 * options used (ie. exported) by each module.
 *****************************************************************************/
int __config_LoadCmdLine( vlc_object_t *p_this, int *pi_argc, char *ppsz_argv[],
                          vlc_bool_t b_ignore_errors )
{
    int i_cmd, i_index, i_opts, i_shortopts, flag, i_verbose = 0;
    module_t *p_parser;
    vlc_list_t *p_list;
    module_config_t *p_item;
    struct option *p_longopts;
    int i_modules_index;

    /* Short options */
    module_config_t *pp_shortopts[256];
    char *psz_shortopts;

    /* Set default configuration and copy arguments */
    p_this->p_vlc->i_argc    = *pi_argc;
    p_this->p_vlc->ppsz_argv = ppsz_argv;

#ifdef __APPLE__
    /* When vlc.app is run by double clicking in Mac OS X, the 2nd arg
     * is the PSN - process serial number (a unique PID-ish thingie)
     * still ok for real Darwin & when run from command line */
    if ( (*pi_argc > 1) && (strncmp( ppsz_argv[ 1 ] , "-psn" , 4 ) == 0) )
                                        /* for example -psn_0_9306113 */
    {
        /* GDMF!... I can't do this or else the MacOSX window server will
         * not pick up the PSN and not register the app and we crash...
         * hence the following kludge otherwise we'll get confused w/ argv[1]
         * being an input file name */
#if 0
        ppsz_argv[ 1 ] = NULL;
#endif
        *pi_argc = *pi_argc - 1;
        pi_argc--;
        return 0;
    }
#endif

    /* List all modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /*
     * Generate the longopts and shortopts structures used by getopt_long
     */

    i_opts = 0;
    for( i_modules_index = 0; i_modules_index < p_list->i_count;
         i_modules_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_modules_index].p_object ;

        /* count the number of exported configuration options (to allocate
         * longopts). We also need to allocate space for too options when
         * dealing with boolean to allow for --foo and --no-foo */
        i_opts += p_parser->i_config_items
                     + 2 * p_parser->i_bool_items;
    }

    p_longopts = malloc( sizeof(struct option) * (i_opts + 1) );
    if( p_longopts == NULL )
    {
        msg_Err( p_this, "out of memory" );
        vlc_list_release( p_list );
        return -1;
    }

    psz_shortopts = malloc( sizeof( char ) * (2 * i_opts + 1) );
    if( psz_shortopts == NULL )
    {
        msg_Err( p_this, "out of memory" );
        free( p_longopts );
        vlc_list_release( p_list );
        return -1;
    }

    /* If we are requested to ignore errors, then we must work on a copy
     * of the ppsz_argv array, otherwise getopt_long will reorder it for
     * us, ignoring the arity of the options */
    if( b_ignore_errors )
    {
        ppsz_argv = (char**)malloc( *pi_argc * sizeof(char *) );
        if( ppsz_argv == NULL )
        {
            msg_Err( p_this, "out of memory" );
            free( psz_shortopts );
            free( p_longopts );
            vlc_list_release( p_list );
            return -1;
        }
        memcpy( ppsz_argv, p_this->p_vlc->ppsz_argv,
                *pi_argc * sizeof(char *) );
    }

    i_shortopts = 0;
    for( i_index = 0; i_index < 256; i_index++ )
    {
        pp_shortopts[i_index] = NULL;
    }

    /* Fill the p_longopts and psz_shortopts structures */
    i_index = 0;
    for( i_modules_index = 0; i_modules_index < p_list->i_count;
         i_modules_index++ )
    {
        p_parser = (module_t *)p_list->p_values[i_modules_index].p_object ;

        if( !p_parser->i_config_items )
            continue;

        for( p_item = p_parser->p_config;
             p_item->i_type != CONFIG_HINT_END;
             p_item++ )
        {
            /* Ignore hints */
            if( p_item->i_type & CONFIG_HINT )
                continue;

            /* Add item to long options */
            p_longopts[i_index].name = strdup( p_item->psz_name );
            if( p_longopts[i_index].name == NULL ) continue;
            p_longopts[i_index].has_arg =
                (p_item->i_type == CONFIG_ITEM_BOOL)?
                                               no_argument : required_argument;
            p_longopts[i_index].flag = &flag;
            p_longopts[i_index].val = 0;
            i_index++;

            /* When dealing with bools we also need to add the --no-foo
             * option */
            if( p_item->i_type == CONFIG_ITEM_BOOL )
            {
                char *psz_name = malloc( strlen(p_item->psz_name) + 3 );
                if( psz_name == NULL ) continue;
                strcpy( psz_name, "no" );
                strcat( psz_name, p_item->psz_name );

                p_longopts[i_index].name = psz_name;
                p_longopts[i_index].has_arg = no_argument;
                p_longopts[i_index].flag = &flag;
                p_longopts[i_index].val = 1;
                i_index++;

                psz_name = malloc( strlen(p_item->psz_name) + 4 );
                if( psz_name == NULL ) continue;
                strcpy( psz_name, "no-" );
                strcat( psz_name, p_item->psz_name );

                p_longopts[i_index].name = psz_name;
                p_longopts[i_index].has_arg = no_argument;
                p_longopts[i_index].flag = &flag;
                p_longopts[i_index].val = 1;
                i_index++;
            }

            /* If item also has a short option, add it */
            if( p_item->i_short )
            {
                pp_shortopts[(int)p_item->i_short] = p_item;
                psz_shortopts[i_shortopts] = p_item->i_short;
                i_shortopts++;
                if( p_item->i_type != CONFIG_ITEM_BOOL )
                {
                    psz_shortopts[i_shortopts] = ':';
                    i_shortopts++;

                    if( p_item->i_short == 'v' )
                    {
                        psz_shortopts[i_shortopts] = ':';
                        i_shortopts++;
                    }
                }
            }
        }
    }

    /* We don't need the module list anymore */
    vlc_list_release( p_list );

    /* Close the longopts and shortopts structures */
    memset( &p_longopts[i_index], 0, sizeof(struct option) );
    psz_shortopts[i_shortopts] = '\0';

    /*
     * Parse the command line options
     */
    opterr = 0;
    optind = 1;
    while( ( i_cmd = getopt_long( *pi_argc, ppsz_argv, psz_shortopts,
                                  p_longopts, &i_index ) ) != EOF )
    {
        /* A long option has been recognized */
        if( i_cmd == 0 )
        {
            module_config_t *p_conf;
            char *psz_name = (char *)p_longopts[i_index].name;

            /* Check if we deal with a --nofoo or --no-foo long option */
            if( flag ) psz_name += psz_name[2] == '-' ? 3 : 2;

            /* Store the configuration option */
            p_conf = config_FindConfig( p_this, psz_name );
            if( p_conf )
            {
                /* Check if the option is deprecated */
                if( p_conf->psz_current )
                {
                    if( !strcmp(p_conf->psz_current,"SUPPRESSED") )
                    {
                        if( !b_ignore_errors )
                        {
                            fprintf(stderr,
                                    "Warning: option --%s is no longer used.\n",
                                    p_conf->psz_name);
                        }
                       continue;
                    }
                    if( !b_ignore_errors )
                    {
                        if( p_conf->b_strict )
                        {
                            fprintf( stderr,
                                     "Error: option --%s is deprecated. "
                                     "Use --%s instead.\n",
                                     p_conf->psz_name, p_conf->psz_current);
                            /*free */
                            for( i_index = 0; p_longopts[i_index].name; i_index++ )
                                free( (char *)p_longopts[i_index].name );

                            free( p_longopts );
                            free( psz_shortopts );
                            return -1;
                        }
                        fprintf(stderr,
                                "Warning: option --%s is deprecated. "
                                "You should use --%s instead.\n",
                                p_conf->psz_name, p_conf->psz_current);
                    }
                    psz_name=p_conf->psz_current;
                    p_conf = config_FindConfig( p_this, psz_name );
                }

            switch( p_conf->i_type )
            {
                case CONFIG_ITEM_STRING:
                case CONFIG_ITEM_FILE:
                case CONFIG_ITEM_DIRECTORY:
                case CONFIG_ITEM_MODULE:
                case CONFIG_ITEM_MODULE_LIST:
                case CONFIG_ITEM_MODULE_LIST_CAT:
                case CONFIG_ITEM_MODULE_CAT:
                    config_PutPsz( p_this, psz_name, optarg );
                    break;
                case CONFIG_ITEM_INTEGER:
                    config_PutInt( p_this, psz_name, strtol(optarg, 0, 0));
                    break;
                case CONFIG_ITEM_FLOAT:
                    config_PutFloat( p_this, psz_name, (float)atof(optarg) );
                    break;
                case CONFIG_ITEM_KEY:
                    config_PutInt( p_this, psz_name, ConfigStringToKey( optarg ) );
                    break;
                case CONFIG_ITEM_BOOL:
                    config_PutInt( p_this, psz_name, !flag );
                    break;
            }

            continue;
        }
    }
    /* A short option has been recognized */
    if( pp_shortopts[i_cmd] != NULL )
    {
        switch( pp_shortopts[i_cmd]->i_type )
        {
            case CONFIG_ITEM_STRING:
            case CONFIG_ITEM_FILE:
            case CONFIG_ITEM_DIRECTORY:
            case CONFIG_ITEM_MODULE:
            case CONFIG_ITEM_MODULE_CAT:
            case CONFIG_ITEM_MODULE_LIST:
            case CONFIG_ITEM_MODULE_LIST_CAT:
                config_PutPsz( p_this, pp_shortopts[i_cmd]->psz_name, optarg );
                break;
            case CONFIG_ITEM_INTEGER:
                if( i_cmd == 'v' )
                {
                    if( optarg )
                    {
                        if( *optarg == 'v' ) /* eg. -vvv */
                        {
                            i_verbose++;
                            while( *optarg == 'v' )
                            {
                                i_verbose++;
                                optarg++;
                            }
                        }
                        else
                        {
                            i_verbose += atoi( optarg ); /* eg. -v2 */
                        }
                    }
                    else
                    {
                        i_verbose++; /* -v */
                    }
                    config_PutInt( p_this, pp_shortopts[i_cmd]->psz_name,
                                           i_verbose );
                }
                else
                {
                    config_PutInt( p_this, pp_shortopts[i_cmd]->psz_name,
                                           strtol(optarg, 0, 0) );
                }
                break;
            case CONFIG_ITEM_BOOL:
                config_PutInt( p_this, pp_shortopts[i_cmd]->psz_name, 1 );
                break;
            }

            continue;
        }

        /* Internal error: unknown option */
        if( !b_ignore_errors )
        {
            fprintf( stderr, "%s: unknown option"
                     " or missing mandatory argument ",
                     p_this->p_vlc->psz_object_name );
            if( optopt )
            {
                fprintf( stderr, "`-%c'\n", optopt );
            }
            else
            {
                fprintf( stderr, "`%s'\n", ppsz_argv[optind-1] );
            }
            fprintf( stderr, "Try `%s --help' for more information.\n",
                             p_this->p_vlc->psz_object_name );

            for( i_index = 0; p_longopts[i_index].name; i_index++ )
                free( (char *)p_longopts[i_index].name );
            free( p_longopts );
            free( psz_shortopts );
            return -1;
        }
    }

    /* Free allocated resources */
    for( i_index = 0; p_longopts[i_index].name; i_index++ )
        free( (char *)p_longopts[i_index].name );
    free( p_longopts );
    free( psz_shortopts );
    if( b_ignore_errors ) free( ppsz_argv );

    return 0;
}

/**
 * config_GetDataDir: find directory where shared data is installed
 *
 * @return a string (always succeeds).
 */
const char *config_GetDataDir( const vlc_object_t *p_this )
{
#if defined (WIN32) || defined (UNDER_CE)
    return p_this->p_libvlc->psz_vlcpath;
#elif defined(__APPLE__) || defined (SYS_BEOS)
    static char path[PATH_MAX] = "";

    if( *path == '\0' )
    {
        snprintf( path, sizeof( path ), "%s/share",
                  p_this->p_libvlc->psz_vlcpath );
        path[sizeof( path ) - 1] = '\0';
    }
    return path;
#else
    return DATA_PATH;
#endif
}

/*****************************************************************************
 * config_GetHomeDir, config_GetUserDir: find the user's home directory.
 *****************************************************************************
 * This function will try by different ways to find the user's home path.
 * Note that this function is not reentrant, it should be called only once
 * at the beginning of main where the result will be stored for later use.
 *****************************************************************************/
static char *GetDir( vlc_bool_t b_appdata )
{
    char *psz_localhome = NULL;

#if defined(HAVE_GETPWUID)
    struct passwd *p_pw = NULL;
#endif

#if defined(WIN32) && !defined(UNDER_CE)
    typedef HRESULT (WINAPI *SHGETFOLDERPATH)( HWND, int, HANDLE, DWORD,
                                               LPSTR );
#ifndef CSIDL_FLAG_CREATE
#   define CSIDL_FLAG_CREATE 0x8000
#endif
#ifndef CSIDL_APPDATA
#   define CSIDL_APPDATA 0x1A
#endif
#ifndef CSIDL_PROFILE
#   define CSIDL_PROFILE 0x28
#endif
#ifndef SHGFP_TYPE_CURRENT
#   define SHGFP_TYPE_CURRENT 0
#endif

    HINSTANCE shfolder_dll;
    SHGETFOLDERPATH SHGetFolderPath ;

    /* load the shfolder dll to retrieve SHGetFolderPath */
    if( ( shfolder_dll = LoadLibrary( _T("SHFolder.dll") ) ) != NULL )
    {
        SHGetFolderPath = (void *)GetProcAddress( shfolder_dll,
                                                  _T("SHGetFolderPathA") );
        if ( SHGetFolderPath != NULL )
        {
            char psz_ACPhome[MAX_PATH];

            /* get the "Application Data" folder for the current user */
            if( S_OK == SHGetFolderPath( NULL,
                                         (b_appdata ? CSIDL_APPDATA :
                                           CSIDL_PROFILE) | CSIDL_FLAG_CREATE,
                                         NULL, SHGFP_TYPE_CURRENT,
                                         psz_ACPhome ) )
            {
                FreeLibrary( shfolder_dll );
                return FromLocaleDup( psz_ACPhome );
            }
        }
        FreeLibrary( shfolder_dll );
    }

#elif defined(UNDER_CE)

#ifndef CSIDL_APPDATA
#   define CSIDL_APPDATA 0x1A
#endif

    wchar_t p_whomedir[MAX_PATH];

    /* get the "Application Data" folder for the current user */
    if( SHGetSpecialFolderPath( NULL, p_whomedir, CSIDL_APPDATA, 1 ) )
    {
        char psz_ACPhome[2 * MAX_PATH];

        sprintf( psz_ACPhome, "%ls", p_whomedir );
        return FromLocaleDup( psz_ACPhome );
    }
#endif

#if defined(HAVE_GETPWUID)
    if( ( p_pw = getpwuid( getuid() ) ) == NULL )
#endif
    {
        psz_localhome = getenv( "HOME" );
        if( psz_localhome == NULL )
        {
            psz_localhome = getenv( "TMP" );
            if( psz_localhome == NULL )
                psz_localhome = "/tmp";
        }
    }
#if defined(HAVE_GETPWUID)
    else
        psz_localhome = p_pw->pw_dir;
#endif

    return FromLocaleDup( psz_localhome );
}

char *config_GetHomeDir( void )
{
    return GetDir( VLC_TRUE );
}

char *config_GetUserDir( void )
{
    return GetDir( VLC_FALSE );
}


static int ConfigStringToKey( char *psz_key )
{
    int i_key = 0;
    unsigned int i;
    char *psz_parser = strchr( psz_key, '-' );
    while( psz_parser && psz_parser != psz_key )
    {
        for( i = 0; i < sizeof(vlc_modifiers) / sizeof(key_descriptor_t); i++ )
        {
            if( !strncasecmp( vlc_modifiers[i].psz_key_string, psz_key,
                              strlen( vlc_modifiers[i].psz_key_string ) ) )
            {
                i_key |= vlc_modifiers[i].i_key_code;
            }
        }
        psz_key = psz_parser + 1;
        psz_parser = strchr( psz_key, '-' );
    }
    for( i = 0; i < sizeof(vlc_keys) / sizeof( key_descriptor_t ); i++ )
    {
        if( !strcasecmp( vlc_keys[i].psz_key_string, psz_key ) )
        {
            i_key |= vlc_keys[i].i_key_code;
            break;
        }
    }
    return i_key;
}

static char *ConfigKeyToString( int i_key )
{
    char *psz_key = malloc( 100 );
    char *p;
    size_t index;

    if ( !psz_key )
    {
        return NULL;
    }
    *psz_key = '\0';
    p = psz_key;
    for( index = 0; index < (sizeof(vlc_modifiers) / sizeof(key_descriptor_t));
         index++ )
    {
        if( i_key & vlc_modifiers[index].i_key_code )
        {
            p += sprintf( p, "%s-", vlc_modifiers[index].psz_key_string );
        }
    }
    for( index = 0; index < (sizeof(vlc_keys) / sizeof( key_descriptor_t));
         index++)
    {
        if( (int)( i_key & ~KEY_MODIFIER ) == vlc_keys[index].i_key_code )
        {
            p += sprintf( p, "%s", vlc_keys[index].psz_key_string );
            break;
        }
    }
    return psz_key;
}
