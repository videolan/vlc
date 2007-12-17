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

#include <vlc/vlc.h>
#include "../libvlc.h"
#include "vlc_keys.h"
#include "vlc_charset.h"

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

#include "config.h"
#include "modules/modules.h"

static inline char *strdupnull (const char *src)
{
    return src ? strdup (src) : NULL;
}

static inline char *_strdupnull (const char *src)
{
    return src ? strdup (_(src)) : NULL;
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

    p_config->b_dirty = VLC_TRUE;

    val.psz_string = (char *)p_config->value.psz;

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

    p_config->b_dirty = VLC_TRUE;

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

    p_config->b_dirty = VLC_TRUE;

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
 *****************************************************************************/
module_config_t *config_FindConfig( vlc_object_t *p_this, const char *psz_name )
{
    vlc_list_t *p_list;
    int i_index;

    if( !psz_name ) return NULL;

    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        module_config_t *p_item, *p_end;
        module_t *p_parser = (module_t *)p_list->p_values[i_index].p_object;

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
                vlc_list_release( p_list );
                return p_item;
            }
        }
    }

    vlc_list_release( p_list );

    return NULL;
}

/*****************************************************************************
 * config_Duplicate: creates a duplicate of a module's configuration data.
 *****************************************************************************
 * Unfortunatly we cannot work directly with the module's config data as
 * this module might be unloaded from memory at any time (remember HideModule).
 * This is why we need to create an exact copy of the config data.
 *****************************************************************************/
int config_Duplicate( module_t *p_module, const module_config_t *p_orig,
                      size_t n )
{
    const module_config_t *p_item, *p_end = p_orig + n;

    /* Calculate the structure length */
    p_module->i_config_items = 0;
    p_module->i_bool_items = 0;

    for( p_item = p_orig; p_item < p_end; p_item++ )
    {
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
    p_module->p_config = (module_config_t *)calloc( n, sizeof(*p_orig) );
    if( p_module->p_config == NULL )
    {
        msg_Err( p_module, "config error: can't duplicate p_config" );
        return VLC_ENOMEM;
    }
    p_module->confsize = n;

    /* Do the duplication job */
    for( size_t i = 0; i < n ; i++ )
    {
        p_module->p_config[i] = p_orig[i];
        p_module->p_config[i].p_lock = &p_module->object_lock;
    }
    return VLC_SUCCESS;
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
    int i_index;
    vlc_list_t *p_list;
    module_t *p_module;

    /* Acquire config file lock */
    vlc_mutex_lock( &p_this->p_libvlc->config_lock );

    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        p_module = (module_t *)p_list->p_values[i_index].p_object ;
        if( p_module->b_submodule ) continue;

        for (size_t i = 0; i < p_module->confsize; i++ )
        {
            if (IsConfigIntegerType (p_module->p_config[i].i_type))
                p_module->p_config[i].value.i = p_module->p_config[i].orig.i;
            else
            if (IsConfigFloatType (p_module->p_config[i].i_type))
                p_module->p_config[i].value.f = p_module->p_config[i].orig.f;
            else
            if (IsConfigStringType (p_module->p_config[i].i_type))
            {
                free ((char *)p_module->p_config[i].value.psz);
                p_module->p_config[i].value.psz =
                        strdupnull (p_module->p_config[i].orig.psz);
            }
        }
    }

    vlc_list_release( p_list );
    vlc_mutex_unlock( &p_this->p_libvlc->config_lock );
}

/**
 * config_GetDataDir: find directory where shared data is installed
 *
 * @return a string (always succeeds).
 */
const char *config_GetDataDir( void )
{
#if defined (WIN32) || defined (UNDER_CE)
    return vlc_global()->psz_vlcpath;
#elif defined(__APPLE__) || defined (SYS_BEOS)
    static char path[PATH_MAX] = "";

    if( *path == '\0' )
    {
        snprintf( path, sizeof( path ), "%s/share",
                  vlc_global()->psz_vlcpath );
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
    const char *psz_localhome = NULL;

#if defined(WIN32) && !defined(UNDER_CE)
    typedef HRESULT (WINAPI *SHGETFOLDERPATH)( HWND, int, HANDLE, DWORD,
                                               LPWSTR );
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
                                                  _T("SHGetFolderPathW") );
        if ( SHGetFolderPath != NULL )
        {
            wchar_t whomedir[MAX_PATH];

            /* get the "Application Data" folder for the current user */
            if( S_OK == SHGetFolderPath( NULL,
                                         (b_appdata ? CSIDL_APPDATA :
                                           CSIDL_PROFILE) | CSIDL_FLAG_CREATE,
                                         NULL, SHGFP_TYPE_CURRENT,
                                         whomedir ) )
            {
                FreeLibrary( shfolder_dll );
                return FromWide( whomedir );
            }
        }
        FreeLibrary( shfolder_dll );
    }

#elif defined(UNDER_CE)

#ifndef CSIDL_APPDATA
#   define CSIDL_APPDATA 0x1A
#endif

    wchar_t whomedir[MAX_PATH];

    /* get the "Application Data" folder for the current user */
    if( SHGetSpecialFolderPath( NULL, whomedir, CSIDL_APPDATA, 1 ) )
        return FromWide( whomedir );
#endif

    psz_localhome = getenv( "HOME" );
    if( psz_localhome == NULL )
    {
#if defined(HAVE_GETPWUID)
        struct passwd *p_pw;
        (void)b_appdata;

        if( ( p_pw = getpwuid( getuid() ) ) != NULL )
            psz_localhome = p_pw->pw_dir;
        else
#endif
        {
            psz_localhome = getenv( "TMP" );
            if( psz_localhome == NULL )
                psz_localhome = "/tmp";
        }
    }

    return FromLocaleDup( psz_localhome );
}

/**
 * Get the user's home directory
 */
char *config_GetHomeDir( void )
{
    return GetDir( VLC_FALSE );
}

/**
 * Get the user's main data and config directory:
 *   - on windows that's the App Data directory;
 *   - on other OSes it's the same as the home directory.
 */
char *config_GetUserDir( void );
char *config_GetUserDir( void )
{
    return GetDir( VLC_TRUE );
}

/**
 * Get the user's VLC configuration directory
 */
char *config_GetConfigDir( libvlc_int_t *p_libvlc )
{
    char *psz_dir;
#if defined(WIN32) || defined(__APPLE__) || defined(SYS_BEOS)
    char *psz_parent = config_GetUserDir();
    if( !psz_parent ) psz_parent = p_libvlc->psz_homedir;
    if( asprintf( &psz_dir, "%s" DIR_SEP CONFIG_DIR, psz_parent ) == -1 )
        return NULL;
    return psz_dir;
#else
    /* XDG Base Directory Specification - Version 0.6 */
    char *psz_env = getenv( "XDG_CONFIG_HOME" );
    if( psz_env )
    {
        if( asprintf( &psz_dir, "%s/vlc", psz_env ) == -1 )
            return NULL;
        return psz_dir;
    }
    psz_env = getenv( "HOME" );
    if( !psz_env ) psz_env = p_libvlc->psz_homedir; /* not part of XDG spec but we want a sensible fallback */
    if( asprintf( &psz_dir, "%s/.config/vlc", psz_env ) == -1 )
        return NULL;
    return psz_dir;
#endif
}

/**
 * Get the user's VLC data directory
 * (used for stuff like the skins, custom lua modules, ...)
 */
char *config_GetUserDataDir( libvlc_int_t *p_libvlc )
{
    char *psz_dir;
#if defined(WIN32) || defined(__APPLE__) || defined(SYS_BEOS)
    char *psz_parent = config_GetUserDir();
    if( !psz_parent ) psz_parent = p_libvlc->psz_homedir;
    if( asprintf( &psz_dir, "%s" DIR_SEP CONFIG_DIR, psz_parent ) == -1 )
        return NULL;
    return psz_dir;
#else
    /* XDG Base Directory Specification - Version 0.6 */
    char *psz_env = getenv( "XDG_DATA_HOME" );
    if( psz_env )
    {
        if( asprintf( &psz_dir, "%s/vlc", psz_env ) == -1 )
            return NULL;
        return psz_dir;
    }
    psz_env = getenv( "HOME" );
    if( !psz_env ) psz_env = p_libvlc->psz_homedir; /* not part of XDG spec but we want a sensible fallback */
    if( asprintf( &psz_dir, "%s/.local/share/vlc", psz_env ) == -1 )
        return NULL;
    return psz_dir;
#endif
}

/**
 * Get the user's VLC cache directory
 * (used for stuff like the modules cache, the album art cache, ...)
 */
char *config_GetCacheDir( libvlc_int_t *p_libvlc )
{
    char *psz_dir;
#if defined(WIN32) || defined(__APPLE__) || defined(SYS_BEOS)
    char *psz_parent = config_GetUserDir();
    if( !psz_parent ) psz_parent = p_libvlc->psz_homedir;
    if( asprintf( &psz_dir, "%s" DIR_SEP CONFIG_DIR, psz_parent ) == -1 )
        return NULL;
    return psz_dir;
#else
    /* XDG Base Directory Specification - Version 0.6 */
    char *psz_env = getenv( "XDG_CACHE_HOME" );
    if( psz_env )
    {
        if( asprintf( &psz_dir, "%s/vlc", psz_env ) == -1 )
            return NULL;
        return psz_dir;
    }
    psz_env = getenv( "HOME" );
    if( !psz_env ) psz_env = p_libvlc->psz_homedir; /* not part of XDG spec but we want a sensible fallback */
    if( asprintf( &psz_dir, "%s/.cache/vlc", psz_env ) == -1 )
        return NULL;
    return psz_dir;
#endif
}

/* Adds an extra interface to the configuration */
void __config_AddIntf( vlc_object_t *p_this, const char *psz_intf )
{
    assert( psz_intf );

    char *psz_config, *psz_parser;
    size_t i_len = strlen( psz_intf );

    psz_config = psz_parser = config_GetPsz( p_this->p_libvlc, "control" );
    while( psz_parser )
    {
        if( !strncmp( psz_intf, psz_parser, i_len ) )
        {
            free( psz_config );
            return;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }
    free( psz_config );

    psz_config = psz_parser = config_GetPsz( p_this->p_libvlc, "extraintf" );
    while( psz_parser )
    {
        if( !strncmp( psz_intf, psz_parser, i_len ) )
        {
            free( psz_config );
            return;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }

    /* interface not found in the config, let's add it */
    if( psz_config && strlen( psz_config ) > 0 )
    {
        char *psz_newconfig;
        if( asprintf( &psz_newconfig, "%s:%s", psz_config, psz_intf ) != -1 )
        {
            config_PutPsz( p_this->p_libvlc, "extraintf", psz_newconfig );
            free( psz_newconfig );
        }
    }
    else
        config_PutPsz( p_this->p_libvlc, "extraintf", psz_intf );

    free( psz_config );
}

/* Removes an extra interface from the configuration */
void __config_RemoveIntf( vlc_object_t *p_this, const char *psz_intf )
{
    assert( psz_intf );

    char *psz_config, *psz_parser;
    size_t i_len = strlen( psz_intf );

    psz_config = psz_parser = config_GetPsz( p_this->p_libvlc, "extraintf" );
    while( psz_parser )
    {
        if( !strncmp( psz_intf, psz_parser, i_len ) )
        {
            char *psz_newconfig;
            char *psz_end = psz_parser + i_len;
            if( *psz_end == ':' ) psz_end++;
            *psz_parser = '\0';
            if( asprintf( &psz_newconfig, "%s%s", psz_config, psz_end ) != -1 )
            {
                config_PutPsz( p_this->p_libvlc, "extraintf", psz_newconfig );
                free( psz_newconfig );
            }
            break;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }
    free( psz_config );

    psz_config = psz_parser = config_GetPsz( p_this->p_libvlc, "control" );
    while( psz_parser )
    {
        if( !strncmp( psz_intf, psz_parser, i_len ) )
        {
            char *psz_newconfig;
            char *psz_end = psz_parser + i_len;
            if( *psz_end == ':' ) psz_end++;
            *psz_parser = '\0';
            if( asprintf( &psz_newconfig, "%s%s", psz_config, psz_end ) != -1 )
            {
                config_PutPsz( p_this->p_libvlc, "control", psz_newconfig );
                free( psz_newconfig );
            }
            break;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }
    free( psz_config );
}

/*
 * Returns VLC_TRUE if the specified extra interface is present in the
 * configuration, VLC_FALSE if not
 */
vlc_bool_t __config_ExistIntf( vlc_object_t *p_this, const char *psz_intf )
{
    assert( psz_intf );

    char *psz_config, *psz_parser;
    size_t i_len = strlen( psz_intf );

    psz_config = psz_parser = config_GetPsz( p_this->p_libvlc, "extraintf" );
    while( psz_parser )
    {
        if( !strncmp( psz_parser, psz_intf, i_len ) )
        {
            free( psz_config );
            return VLC_TRUE;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }
    free( psz_config );

    psz_config = psz_parser = config_GetPsz( p_this->p_libvlc, "control" );
    while( psz_parser )
    {
        if( !strncmp( psz_parser, psz_intf, i_len ) )
        {
            free( psz_config );
            return VLC_TRUE;
        }
        psz_parser = strchr( psz_parser, ':' );
        if( psz_parser ) psz_parser++; /* skip the ':' */
    }
    free( psz_config );

    return VLC_FALSE;
}

