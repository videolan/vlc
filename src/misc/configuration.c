/*****************************************************************************
 * configuration.c management of the modules configuration
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: configuration.c,v 1.2 2002/02/26 22:08:57 gbazin Exp $
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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

#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                              /* strdup() */

#include <videolan/vlc.h>

/* TODO: implement locking for config_PutPszVariable and config_GetPszVariable
 * because they are not thread safe */

/*****************************************************************************
 * config_GetIntVariable: get the value of an int variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by an integer (MODULE_CONFIG_ITEM_INTEGER and
 * MODULE_CONFIG_ITEM_BOOL).
 *****************************************************************************/
int config_GetIntVariable( const char *psz_name )
{
    module_config_t *p_config;

    p_config = config_FindConfig( psz_name );

    /* sanity checks */
    if( !p_config )
    {
        intf_ErrMsg( "config_GetIntVariable: option %s doesn't exist",
                     psz_name );
        return -1;
    }
    if( (p_config->i_type!=MODULE_CONFIG_ITEM_INTEGER) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_BOOL) )
    {
        intf_ErrMsg( "config_GetIntVariable: option %s doesn't refer to an int"
                     , psz_name );
        return -1;
    }

    return p_config->i_value;
}

/*****************************************************************************
 * config_GetPszVariable: get the string value of a string variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by a string (MODULE_CONFIG_ITEM_STRING, MODULE_CONFIG_ITEM_FILE,
 * and MODULE_CONFIG_ITEM_PLUGIN).
 *
 * Important note: remember to free() the returned char* because it a duplicate
 *   of the actual value. It isn't safe to return a pointer to the actual value
 *   as it can be modified at any time.
 *****************************************************************************/
char * config_GetPszVariable( const char *psz_name )
{
    module_config_t *p_config;
    char *psz_value = NULL;

    p_config = config_FindConfig( psz_name );

    /* sanity checks */
    if( !p_config )
    {
        intf_ErrMsg( "config_GetPszVariable: option %s doesn't exist",
                     psz_name );
        return NULL;
    }
    if( (p_config->i_type!=MODULE_CONFIG_ITEM_STRING) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_FILE) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_PLUGIN) )
    {
        intf_ErrMsg( "config_GetPszVariable: option %s doesn't refer to a "
                     "string", psz_name );
        return NULL;
    }

    /* return a copy of the string */
    if( p_config->psz_value ) psz_value = strdup( p_config->psz_value );

    return psz_value;
}

/*****************************************************************************
 * config_PutPszVariable: set the string value of a string variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by a string (MODULE_CONFIG_ITEM_STRING, MODULE_CONFIG_ITEM_FILE,
 * and MODULE_CONFIG_ITEM_PLUGIN).
 *****************************************************************************/
void config_PutPszVariable( const char *psz_name, char *psz_value )
{
    module_config_t *p_config;
    char *psz_tmp;

    p_config = config_FindConfig( psz_name );

    /* sanity checks */
    if( !p_config )
    {
        intf_ErrMsg( "config_PutPszVariable: option %s doesn't exist",
                     psz_name );
        return;
    }
    if( (p_config->i_type!=MODULE_CONFIG_ITEM_STRING) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_FILE) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_PLUGIN) )
    {
        intf_ErrMsg( "config_PutPszVariable: option %s doesn't refer to a "
                     "string", psz_name );
        return;
    }

    psz_tmp = p_config->psz_value;
    if( psz_value ) p_config->psz_value = strdup( psz_value );
    else p_config->psz_value = NULL;

    /* free old string */
    if( psz_tmp ) free( psz_tmp );

}

/*****************************************************************************
 * config_PutIntVariable: set the integer value of an int variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by an integer (MODULE_CONFIG_ITEM_INTEGER and
 * MODULE_CONFIG_ITEM_BOOL).
 *****************************************************************************/
void config_PutIntVariable( const char *psz_name, int i_value )
{
    module_config_t *p_config;

    p_config = config_FindConfig( psz_name );

    /* sanity checks */
    if( !p_config )
    {
        intf_ErrMsg( "config_PutIntVariable: option %s doesn't exist",
                     psz_name );
        return;
    }
    if( (p_config->i_type!=MODULE_CONFIG_ITEM_INTEGER) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_BOOL) )
    {
        intf_ErrMsg( "config_PutIntVariable: option %s doesn't refer to an int"
                     , psz_name );
        return;
    }

    p_config->i_value = i_value;
}

/*****************************************************************************
 * config_FindConfig: find the config structure associated with an option.
 *****************************************************************************
 * FIXME: This function really needs to be optimized.
 *****************************************************************************/
module_config_t *config_FindConfig( const char *psz_name )
{
    module_t *p_module;
    int i;

    if( !psz_name ) return NULL;

    for( p_module = p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        for( i = 0; i < (p_module->i_config_options -1); i++ )
        {
            if( (p_module->p_config[i].i_type ==
                     MODULE_CONFIG_ITEM_CATEGORY)||
                (p_module->p_config[i].i_type ==
                     MODULE_CONFIG_ITEM_SUBCATEGORY)||
                (p_module->p_config[i].i_type ==
                     MODULE_CONFIG_ITEM_SUBCATEGORY_END) )
                continue;
            if( !strcmp( psz_name, p_module->p_config[i].psz_name ) )
                return &p_module->p_config[i];
        }
    }

    return NULL;
}

/*****************************************************************************
 * config_Duplicate: creates a duplicate of a module's configuration data.
 *****************************************************************************
 * Unfortunatly we cannot work directly with the module's config data as
 * this module might be unloaded from memory at any time (remember HideModule).
 * This is why we need to create an exact copy of the config data.
 *****************************************************************************/
module_config_t *config_Duplicate( module_config_t *p_config_orig,
                                   int i_config_options )
{
    int i;
    module_config_t *p_config;

    /* allocate memory */
    p_config = (module_config_t *)malloc( sizeof(module_config_t)
                                          * i_config_options );
    if( p_config == NULL )
    {
        intf_ErrMsg( "config_Duplicate error: can't allocate p_config" );
        return( NULL );
    }

    for( i = 0; i < i_config_options ; i++ )
    {
        p_config[i].i_type = p_config_orig[i].i_type;
        p_config[i].i_value = p_config_orig[i].i_value;
        p_config[i].b_dirty = p_config_orig[i].b_dirty;
        if( p_config_orig[i].psz_name )
            p_config[i].psz_name = strdup( p_config_orig[i].psz_name );
        else p_config[i].psz_name = NULL;
        if( p_config_orig[i].psz_text )
            p_config[i].psz_text = strdup( p_config_orig[i].psz_text );
        else p_config[i].psz_text = NULL;
        if( p_config_orig[i].psz_longtext )
            p_config[i].psz_longtext = strdup( p_config_orig[i].psz_longtext );
        else p_config[i].psz_longtext = NULL;
        if( p_config_orig[i].psz_value )
            p_config[i].psz_value = strdup( p_config_orig[i].psz_value );
        else p_config[i].psz_value = NULL;

        /* the callback pointer is only valid when the module is loaded so this
         * value is set in ActivateModule() and reset in DeactivateModule() */
        p_config_orig[i].p_callback = NULL;
    }

    return p_config;
}
