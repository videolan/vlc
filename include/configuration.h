/*****************************************************************************
 * configuration.h : configuration management module
 * This file describes the programming interface for the configuration module.
 * It includes functions allowing to declare, get or set configuration options.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: configuration.h,v 1.7 2002/04/21 18:32:12 sam Exp $
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

/* Configuration hint types */
#define MODULE_CONFIG_HINT_END              0x0001  /* End of config */
#define MODULE_CONFIG_HINT_CATEGORY         0x0002  /* Start of new category */
#define MODULE_CONFIG_HINT_SUBCATEGORY      0x0003  /* Start of sub-category */
#define MODULE_CONFIG_HINT_SUBCATEGORY_END  0x0004  /* End of sub-category */

#define MODULE_CONFIG_HINT                  0x000F

/* Configuration item types */
#define MODULE_CONFIG_ITEM_STRING           0x0010  /* String option */
#define MODULE_CONFIG_ITEM_FILE             0x0020  /* File option */
#define MODULE_CONFIG_ITEM_PLUGIN           0x0030  /* Plugin option */
#define MODULE_CONFIG_ITEM_INTEGER          0x0040  /* Integer option */
#define MODULE_CONFIG_ITEM_BOOL             0x0050  /* Bool option */
#define MODULE_CONFIG_ITEM_FLOAT            0x0060  /* Float option */

#define MODULE_CONFIG_ITEM                  0x00F0

typedef struct module_config_s
{
    int          i_type;                               /* Configuration type */
    char        *psz_name;                                    /* Option name */
    char         i_short;                      /* Optional short option name */
    char        *psz_text;      /* Short comment on the configuration option */
    char        *psz_longtext;   /* Long comment on the configuration option */
    char        *psz_value;                                  /* Option value */
    int          i_value;                                    /* Option value */
    float        f_value;                                    /* Option value */
    void        *p_callback;     /* Function to call when commiting a change */
    vlc_mutex_t *p_lock;            /* lock to use when modifying the config */
    boolean_t    b_dirty;          /* Dirty flag to indicate a config change */

} module_config_t;

/*****************************************************************************
 * Prototypes - these methods are used to get, set or manipulate configuration
 * data.
 *****************************************************************************/
#ifndef PLUGIN
int    config_GetIntVariable( const char *psz_name );
float  config_GetFloatVariable( const char *psz_name );
char * config_GetPszVariable( const char *psz_name );
void   config_PutIntVariable( const char *psz_name, int i_value );
void   config_PutFloatVariable( const char *psz_name, float f_value );
void   config_PutPszVariable( const char *psz_name, char *psz_value );

int config_LoadConfigFile( const char *psz_module_name );
int config_SaveConfigFile( const char *psz_module_name );
module_config_t *config_FindConfig( const char *psz_name );
module_config_t *config_Duplicate ( module_config_t * );
char *config_GetHomeDir( void );
int config_LoadCmdLine( int *pi_argc, char *ppsz_argv[],
                        boolean_t b_ignore_errors );

#else
#   define config_GetIntVariable p_symbols->config_GetIntVariable
#   define config_PutIntVariable p_symbols->config_PutIntVariable
#   define config_GetFloatVariable p_symbols->config_GetFloatVariable
#   define config_PutFloatVariable p_symbols->config_PutFloatVariable
#   define config_GetPszVariable p_symbols->config_GetPszVariable
#   define config_PutPszVariable p_symbols->config_PutPszVariable
#   define config_Duplicate      p_symbols->config_Duplicate
#   define config_FindConfig     p_symbols->config_FindConfig
#   define config_LoadConfigFile p_symbols->config_LoadConfigFile
#   define config_SaveConfigFile p_symbols->config_SaveConfigFile
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
    { MODULE_CONFIG_HINT_END, NULL, '\0', NULL, NULL, NULL, 0, 0, NULL, 0 } };

#define ADD_CATEGORY_HINT( text, longtext ) \
    { MODULE_CONFIG_HINT_CATEGORY, NULL, '\0', text, longtext, NULL, 0, 0, \
      NULL, NULL, 0 },
#define ADD_SUBCATEGORY_HINT( text, longtext ) \
    { MODULE_CONFIG_HINT_SUBCATEGORY, NULL, '\0', text, longtext, NULL, 0, 0, \
      NULL, NULL, 0 },
#define END_SUBCATEGORY_HINT \
    { MODULE_CONFIG_HINT_SUBCATEGORY_END, NULL, '\0', NULL, NULL, NULL, 0, 0, \
      NULL, NULL, 0 },
#define ADD_STRING( name, value, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_STRING, name, '\0', text, longtext, value, 0, 0, \
      p_callback, NULL, 0 },
#define ADD_FILE( name, psz_value, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_FILE, name, '\0', text, longtext, psz_value, 0, 0, \
      p_callback, NULL, 0 },
#define ADD_PLUGIN( name, i_capability, psz_value, p_callback, text, longtext)\
    { MODULE_CONFIG_ITEM_PLUGIN, name, '\0', text, longtext, psz_value, \
      i_capability, 0, p_callback, NULL, 0 },
#define ADD_INTEGER( name, i_value, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_INTEGER, name, '\0', text, longtext, NULL, i_value, \
      0, p_callback, NULL, 0 },
#define ADD_FLOAT( name, f_value, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_FLOAT, name, '\0', text, longtext, NULL, 0, f_value, \
      p_callback, NULL, 0 },
#define ADD_BOOL( name, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_BOOL, name, '\0', text, longtext, NULL, 0, 0, \
      p_callback, NULL, 0 },
#define ADD_STRING_WITH_SHORT( name, ch, value, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_STRING, name, ch, text, longtext, value, 0, 0, \
      p_callback, NULL, 0 },
#define ADD_FILE_WITH_SHORT( name, ch, psz_value, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_FILE, name, ch, text, longtext, psz_value, 0, 0, \
      p_callback, NULL, 0 },
#define ADD_PLUGIN_WITH_SHORT( name, ch, i_capability, psz_value, p_callback, \
  text, longtext) \
    { MODULE_CONFIG_ITEM_PLUGIN, name, ch, text, longtext, psz_value, \
      i_capability, 0, p_callback, NULL, 0 },
#define ADD_INTEGER_WITH_SHORT( name, ch, i_value, p_callback, text, \
  longtext ) \
    { MODULE_CONFIG_ITEM_INTEGER, name, ch, text, longtext, NULL, i_value, 0, \
      p_callback, NULL, 0 },
#define ADD_FLOAT_WITH_SHORT( name, f_value, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_FLOAT, name, ch, text, longtext, NULL, 0, f_value, \
      p_callback, NULL, 0 },
#define ADD_BOOL_WITH_SHORT( name, ch, p_callback, text, longtext ) \
    { MODULE_CONFIG_ITEM_BOOL, name, ch, text, longtext, NULL, 0, 0, \
      p_callback, NULL, 0 },

