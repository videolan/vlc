/*****************************************************************************
 * configuration.h : configuration management module
 * This file describes the programming interface for the configuration module.
 * It includes functions allowing to declare, get or set configuration options.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: configuration.h,v 1.1 2002/02/24 20:51:09 gbazin Exp $
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

/*****************************************************************************
 * Macros used to build the configuration structure.
 *****************************************************************************/

/* Mandatory last part of the structure */
#define MODULE_CONFIG_ITEM_END              0x000   /* End of config */

/* Configuration widgets */
#define MODULE_CONFIG_ITEM_CATEGORY         0x0001  /* Start of new category */
#define MODULE_CONFIG_ITEM_SUBCATEGORY      0x0002  /* Start of sub-category */
#define MODULE_CONFIG_ITEM_SUBCATEGORY_END  0x0003  /* End of sub-category */
#define MODULE_CONFIG_ITEM_STRING           0x0004  /* String option */
#define MODULE_CONFIG_ITEM_FILE             0x0005  /* File option */
#define MODULE_CONFIG_ITEM_PLUGIN           0x0006  /* Plugin option */
#define MODULE_CONFIG_ITEM_INTEGER          0x0007  /* Integer option */
#define MODULE_CONFIG_ITEM_BOOL             0x0008  /* Bool option */
#define MODULE_CONFIG_ITEM_ALIAS            0x0009  /* Alias option */

typedef struct module_config_s
{
    int         i_type;                                /* Configuration type */
    char *      psz_name;                                     /* Option name */
    char *      psz_text;       /* Short comment on the configuration option */
    char *      psz_longtext;    /* Long comment on the configuration option */
    char *      psz_value;                                   /* Option value */
    int         i_value;                                     /* Option value */
    void *      p_callback;      /* Function to call when commiting a change */
    boolean_t   b_dirty;           /* Dirty flag to indicate a config change */

} module_config_t;

/*****************************************************************************
 * Prototypes - these methods are used to get, set or manipulate configuration
 * data.
 *****************************************************************************/
#ifndef PLUGIN
int    config_GetIntVariable( const char *psz_name );
char * config_GetPszVariable( const char *psz_name );
void   config_PutIntVariable( const char *psz_name, int i_value );
void   config_PutPszVariable( const char *psz_name, char *psz_value );

module_config_t *config_FindConfig( const char *psz_name );
module_config_t *config_Duplicate ( module_config_t *p_config_orig,
                                    int i_config_options );
#else
#   define config_GetIntVariable p_symbols->config_GetIntVariable
#   define config_PutIntVariable p_symbols->config_PutIntVariable
#   define config_GetPszVariable p_symbols->config_GetPszVariable
#   define config_PutPszVariable p_symbols->config_PutPszVariable
#   define config_Duplicate    p_symbols->config_Duplicate
#   define config_FindConfig   p_symbols->config_FindConfig
#endif

/*****************************************************************************
 * Macros used to build the configuration structure.
 *
 * Note that internally we support only 2 types of config data: int and string.
 *   The other types declared here just map to one of these 2 basic types but
 *   have the advantage of also providing very good hints to a configuration
 *   interface so as to make it more user friendly.
 * The configuration structure also includes category hints. These hints can
 *   provide a configuration inteface with some very useful data and also allow
 *   for a more user friendly interface.
 *****************************************************************************/

#define MODULE_CONFIG_START \
    static module_config_t p_config[] = {

#define MODULE_CONFIG_STOP \
    { MODULE_CONFIG_ITEM_END, NULL, NULL, NULL, NULL, 0, NULL, 0 } };

#define ADD_CATEGORY_HINT( text, longtext ) \
    { MODULE_CONFIG_ITEM_CATEGORY, NULL, text, longtext, NULL, 0, NULL, 0 },
#define ADD_SUBCATEGORY_HINT( text, longtext ) \
    { MODULE_CONFIG_ITEM_SUBCATEGORY, NULL, text, longtext, NULL, 0, NULL, 0 },
#define END_SUBCATEGORY_HINT \
    { MODULE_CONFIG_ITEM_SUBCATEGORY_END, NULL, NULL, NULL, NULL, 0, NULL, 0 },
#define ADD_STRING( name, value, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_STRING, name, text, longtext, value, 0, \
      p_callback, 0 },
#define ADD_FILE( name, psz_value, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_FILE, name, text, longtext, psz_value, 0, \
      p_callback, 0 },
#define ADD_PLUGIN( name, i_capability, psz_value, p_callback, text, longtext)\
    { MODULE_CONFIG_ITEM_PLUGIN, name, text, longtext, psz_value, \
      i_capability, p_callback, 0 },
#define ADD_INTEGER( name, i_value, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_INTEGER, name, text, longtext, NULL, i_value, \
      p_callback, 0 },
#define ADD_BOOL( name, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_BOOL, name, text, longtext, NULL, 0, p_callback, 0 },
