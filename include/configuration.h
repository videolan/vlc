/*****************************************************************************
 * configuration.h : configuration management module
 * This file describes the programming interface for the configuration module.
 * It includes functions allowing to declare, get or set configuration options.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: configuration.h,v 1.15 2002/07/03 19:40:49 sam Exp $
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
#define CONFIG_HINT_END                     0x0001  /* End of config */
#define CONFIG_HINT_CATEGORY                0x0002  /* Start of new category */
#define CONFIG_HINT_SUBCATEGORY             0x0003  /* Start of sub-category */
#define CONFIG_HINT_SUBCATEGORY_END         0x0004  /* End of sub-category */
#define CONFIG_HINT_USAGE                   0x0005  /* Usage information */

#define CONFIG_HINT                         0x000F

/* Configuration item types */
#define CONFIG_ITEM_STRING                  0x0010  /* String option */
#define CONFIG_ITEM_FILE                    0x0020  /* File option */
#define CONFIG_ITEM_MODULE                  0x0030  /* Module option */
#define CONFIG_ITEM_INTEGER                 0x0040  /* Integer option */
#define CONFIG_ITEM_BOOL                    0x0050  /* Bool option */
#define CONFIG_ITEM_FLOAT                   0x0060  /* Float option */

#define CONFIG_ITEM                         0x00F0

struct module_config_s
{
    int          i_type;                               /* Configuration type */
    char        *psz_name;                                    /* Option name */
    char         i_short;                      /* Optional short option name */
    char        *psz_text;      /* Short comment on the configuration option */
    char        *psz_longtext;   /* Long comment on the configuration option */
    char        *psz_value;                                  /* Option value */
    int          i_value;                                    /* Option value */
    float        f_value;                                    /* Option value */

    /* Function to call when commiting a change */
    void ( * pf_callback ) ( vlc_object_t * );

    char       **ppsz_list;        /* List of possible values for the option */

    vlc_mutex_t *p_lock;            /* Lock to use when modifying the config */
    vlc_bool_t   b_dirty;          /* Dirty flag to indicate a config change */
};

/*****************************************************************************
 * Prototypes - these methods are used to get, set or manipulate configuration
 * data.
 *****************************************************************************/
VLC_EXPORT( int,    __config_GetInt,   (vlc_object_t *, const char *) );
VLC_EXPORT( void,   __config_PutInt,   (vlc_object_t *, const char *, int) );
VLC_EXPORT( float,  __config_GetFloat, (vlc_object_t *, const char *) );
VLC_EXPORT( void,   __config_PutFloat, (vlc_object_t *, const char *, float) );
VLC_EXPORT( char *, __config_GetPsz,   (vlc_object_t *, const char *) );
VLC_EXPORT( void,   __config_PutPsz,   (vlc_object_t *, const char *, const char *) );

VLC_EXPORT( int,    __config_LoadCmdLine,  ( vlc_object_t *, int *, char *[], vlc_bool_t ) );
VLC_EXPORT( char *,   config_GetHomeDir,     ( void ) );
VLC_EXPORT( int,    __config_LoadConfigFile, ( vlc_object_t *, const char * ) );
VLC_EXPORT( int,    __config_SaveConfigFile, ( vlc_object_t *, const char * ) );
VLC_EXPORT( module_config_t *, config_FindConfig,( vlc_object_t *, const char *psz_name ) );

VLC_EXPORT( void, config_Duplicate, ( module_t *, module_config_t * ) );
            void  config_Free       ( module_t * );

VLC_EXPORT( void, config_SetCallbacks, ( module_config_t *, module_config_t * ) );
VLC_EXPORT( void, config_UnsetCallbacks, ( module_config_t * ) );

#define config_GetInt(a,b) __config_GetInt(CAST_TO_VLC_OBJECT(a),b)
#define config_PutInt(a,b,c) __config_PutInt(CAST_TO_VLC_OBJECT(a),b,c)
#define config_GetFloat(a,b) __config_GetFloat(CAST_TO_VLC_OBJECT(a),b)
#define config_PutFloat(a,b,c) __config_PutFloat(CAST_TO_VLC_OBJECT(a),b,c)
#define config_GetPsz(a,b) __config_GetPsz(CAST_TO_VLC_OBJECT(a),b)
#define config_PutPsz(a,b,c) __config_PutPsz(CAST_TO_VLC_OBJECT(a),b,c)

#define config_LoadCmdLine(a,b,c,d) __config_LoadCmdLine(CAST_TO_VLC_OBJECT(a),b,c,d)
#define config_LoadConfigFile(a,b) __config_LoadConfigFile(CAST_TO_VLC_OBJECT(a),b)
#define config_SaveConfigFile(a,b) __config_SaveConfigFile(CAST_TO_VLC_OBJECT(a),b)

/*****************************************************************************
 * Macros used to build the configuration structure.
 *
 * Note that internally we support only 3 types of config data: int , float
 *   and string.
 *   The other types declared here just map to one of these 3 basic types but
 *   have the advantage of also providing very good hints to a configuration
 *   interface so as to make it more user friendly.
 * The configuration structure also includes category hints. These hints can
 *   provide a configuration interface with some very useful data and again
 *   allow for a more user friendly interface.
 *****************************************************************************/

#define MODULE_CONFIG_START \
    static module_config_t p_config[] = {
#define MODULE_CONFIG_STOP \
    { CONFIG_HINT_END, NULL, '\0' } };

#define ADD_CATEGORY_HINT( text, longtext ) \
    { CONFIG_HINT_CATEGORY, NULL, '\0', text, longtext },
#define ADD_SUBCATEGORY_HINT( text, longtext ) \
    { CONFIG_HINT_SUBCATEGORY, NULL, '\0', text, longtext },
#define END_SUBCATEGORY_HINT \
    { CONFIG_HINT_SUBCATEGORY_END, NULL, '\0' },
#define ADD_USAGE_HINT( text ) \
    { CONFIG_HINT_USAGE, NULL, '\0', text },

#define ADD_STRING( name, psz_value, p_callback, text, longtext ) \
    { CONFIG_ITEM_STRING, name, '\0', text, longtext, psz_value, 0, 0, \
      p_callback },
#define ADD_STRING_FROM_LIST( name, psz_value, ppsz_list, p_callback, text, \
      longtext ) \
    { CONFIG_ITEM_STRING, name, '\0', text, longtext, psz_value, 0, 0, \
      p_callback, ppsz_list },
#define ADD_FILE( name, psz_value, p_callback, text, longtext ) \
    { CONFIG_ITEM_FILE, name, '\0', text, longtext, psz_value, 0, 0, \
      p_callback },
#define ADD_MODULE( name, i_caps, psz_value, p_callback, text, longtext ) \
    { CONFIG_ITEM_MODULE, name, '\0', text, longtext, psz_value, i_caps, 0, \
      p_callback },
#define ADD_INTEGER( name, i_value, p_callback, text, longtext ) \
    { CONFIG_ITEM_INTEGER, name, '\0', text, longtext, NULL, i_value, 0, \
      p_callback },
#define ADD_FLOAT( name, f_value, p_callback, text, longtext ) \
    { CONFIG_ITEM_FLOAT, name, '\0', text, longtext, NULL, 0, f_value, \
      p_callback },
#define ADD_BOOL( name, b_value, p_callback, text, longtext ) \
    { CONFIG_ITEM_BOOL, name, '\0', text, longtext, NULL, b_value, 0, \
      p_callback },

/* These should be seldom used. They were added just to provide easy shortcuts
 * for the command line interface */
#define ADD_STRING_WITH_SHORT( name, ch, psz_value, p_callback, text, ltext ) \
    { CONFIG_ITEM_STRING, name, ch, text, ltext, psz_value, 0, 0, \
      p_callback },
#define ADD_FILE_WITH_SHORT( name, ch, psz_value, p_callback, text, ltext ) \
    { CONFIG_ITEM_FILE, name, ch, text, ltext, psz_value, 0, 0, \
      p_callback },
#define ADD_MODULE_WITH_SHORT( name, ch, i_capability, psz_value, p_callback, \
    text, ltext) \
    { CONFIG_ITEM_MODULE, name, ch, text, ltext, psz_value, i_capability, 0, \
      p_callback },
#define ADD_INTEGER_WITH_SHORT( name, ch, i_value, p_callback, text, ltext ) \
    { CONFIG_ITEM_INTEGER, name, ch, text, ltext, NULL, i_value, 0, \
      p_callback },
#define ADD_FLOAT_WITH_SHORT( name, ch, f_value, p_callback, text, ltext ) \
    { CONFIG_ITEM_FLOAT, name, ch, text, ltext, NULL, 0, f_value, \
      p_callback },
#define ADD_BOOL_WITH_SHORT( name, ch, b_value, p_callback, text, ltext ) \
    { CONFIG_ITEM_BOOL, name, ch, text, ltext, NULL, b_value, 0, \
      p_callback },
