/*****************************************************************************
 * configuration.c management of the modules configuration
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: configuration.c,v 1.4 2002/03/16 01:40:58 gbazin Exp $
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
#include <stdio.h>                                              /* sprintf() */
#include <stdlib.h>                                      /* free(), strtol() */
#include <string.h>                                              /* strdup() */
#include <unistd.h>                                              /* getuid() */

#include <videolan/vlc.h>

#if defined(HAVE_GETPWUID) || defined(HAVE_GETPWUID_R)
#include <pwd.h>                                               /* getpwuid() */
#endif

#include <sys/stat.h>
#include <sys/types.h>

static char *config_GetHomeDir(void);

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
    vlc_mutex_lock( p_config->p_lock );
    if( p_config->psz_value ) psz_value = strdup( p_config->psz_value );
    vlc_mutex_unlock( p_config->p_lock );

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

    vlc_mutex_lock( p_config->p_lock );

    /* free old string */
    if( p_config->psz_value ) free( p_config->psz_value );

    if( psz_value ) p_config->psz_value = strdup( psz_value );
    else p_config->psz_value = NULL;

    vlc_mutex_unlock( p_config->p_lock );

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
        for( i = 0; i < p_module->i_config_lines; i++ )
        {
            if( p_module->p_config[i].i_type & MODULE_CONFIG_HINT )
                /* ignore hints */
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
module_config_t *config_Duplicate( module_t *p_module )
{
    int i;
    module_config_t *p_config;

    /* allocate memory */
    p_config = (module_config_t *)malloc( sizeof(module_config_t)
                                          * p_module->i_config_lines );
    if( p_config == NULL )
    {
        intf_ErrMsg( "config_Duplicate error: can't allocate p_config" );
        return( NULL );
    }

    for( i = 0; i < p_module->i_config_lines ; i++ )
    {
        p_config[i].i_type = p_module->p_config_orig[i].i_type;
        p_config[i].i_value = p_module->p_config_orig[i].i_value;
        p_config[i].b_dirty = p_module->p_config_orig[i].b_dirty;
        p_config[i].p_lock = &p_module->config_lock;
        if( p_module->p_config_orig[i].psz_name )
            p_config[i].psz_name =
                strdup( p_module->p_config_orig[i].psz_name );
        else p_config[i].psz_name = NULL;
        if( p_module->p_config_orig[i].psz_text )
            p_config[i].psz_text =
                strdup( p_module->p_config_orig[i].psz_text );
        else p_config[i].psz_text = NULL;
        if( p_module->p_config_orig[i].psz_longtext )
            p_config[i].psz_longtext =
                strdup( p_module->p_config_orig[i].psz_longtext );
        else p_config[i].psz_longtext = NULL;
        if( p_module->p_config_orig[i].psz_value )
            p_config[i].psz_value =
                strdup( p_module->p_config_orig[i].psz_value );
        else p_config[i].psz_value = NULL;

        /* the callback pointer is only valid when the module is loaded so this
         * value is set in ActivateModule() and reset in DeactivateModule() */
        p_config[i].p_callback = NULL;
    }

    return p_config;
}

/*****************************************************************************
 * config_LoadConfigFile: loads the configuration file.
 *****************************************************************************
 * This function is called to load the config options stored in the config
 * file.
 *****************************************************************************/
int config_LoadConfigFile( const char *psz_module_name )
{
    module_t *p_module;
    FILE *file;
    char line[1024];
    char *p_index, *psz_option_name, *psz_option_value;
    int i;
    char *psz_filename, *psz_homedir;

    /* Acquire config file lock */
    vlc_mutex_lock( &p_main->config_lock );

    psz_homedir = config_GetHomeDir();
    if( !psz_homedir )
    {
        intf_ErrMsg( "config_LoadConfigFile: GetHomeDir failed" );
        vlc_mutex_unlock( &p_main->config_lock );
        return -1;
    }
    psz_filename = (char *)malloc( strlen("/.VideoLan/vlc") +
                                   strlen(psz_homedir) + 1 );
    if( !psz_filename )
    {
        intf_ErrMsg( "config err: couldn't malloc psz_filename" );
        free( psz_homedir );
        vlc_mutex_unlock( &p_main->config_lock );
        return -1;
    }
    sprintf( psz_filename, "%s/.VideoLan/vlc", psz_homedir );
    free( psz_homedir );

    intf_WarnMsg( 5, "config_SaveConfigFile: opening config file %s",
                  psz_filename );

    file = fopen( psz_filename, "r" );
    if( !file )
    {
        intf_WarnMsg( 1, "config_LoadConfigFile: couldn't open config file %s "
                      "for reading", psz_filename );
        free( psz_filename );
        vlc_mutex_unlock( &p_main->config_lock );
        return -1;
    }

    /* Look for the selected module, if NULL then save everything */
    for( p_module = p_module_bank->first ; p_module != NULL ;
         p_module = p_module->next )
    {

        if( psz_module_name && strcmp( psz_module_name, p_module->psz_name ) )
            continue;

        /* The config file is organized in sections, one per module. Look for
         * the interesting section ( a section is of the form [foo] ) */
        rewind( file );
        while( fgets( line, 1024, file ) )
        {
            if( (line[0] == '[') && (p_index = strchr(line,']')) &&
                (p_index - &line[1] == strlen(p_module->psz_name) ) &&
                !memcmp( &line[1], p_module->psz_name,
                         strlen(p_module->psz_name) ) )
            {
                intf_WarnMsg( 5, "config_LoadConfigFile: loading config for "
                              "module <%s>", p_module->psz_name );

                break;
            }
        }
        /* either we found the section or we're at the EOF */

        /* Now try to load the options in this section */
        while( fgets( line, 1024, file ) )
        {
            if( line[0] == '[' ) break; /* end of section */

            /* ignore comments or empty lines */
            if( (line[0] == '#') || (line[0] == (char)0) ) continue;

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

            /* try to match this option with one of the module's options */
            for( i = 0; i < p_module->i_config_lines; i++ )
            {
                if( p_module->p_config[i].i_type & MODULE_CONFIG_HINT )
                    /* ignore hints */
                    continue;
                if( !strcmp( p_module->p_config[i].psz_name,
                             psz_option_name ) )
                {
                    /* We found it */
                    switch( p_module->p_config[i].i_type )
                    {
                    case MODULE_CONFIG_ITEM_BOOL:
                    case MODULE_CONFIG_ITEM_INTEGER:
                        p_module->p_config[i].i_value =
                            atoi( psz_option_value);
                        intf_WarnMsg( 7, "config_LoadConfigFile: found <%s> "
                                      "option %s=%i",
                                      p_module->psz_name,
                                      p_module->p_config[i].psz_name,
                                      p_module->p_config[i].i_value );
                        break;

                    default:
                        vlc_mutex_lock( p_module->p_config[i].p_lock );

                        /* free old string */
                        if( p_module->p_config[i].psz_value )
                            free( p_module->p_config[i].psz_value );

                        p_module->p_config[i].psz_value =
                            strdup( psz_option_value );

                        vlc_mutex_unlock( p_module->p_config[i].p_lock );

                        intf_WarnMsg( 7, "config_LoadConfigFile: found <%s> "
                                      "option %s=%s",
                                      p_module->psz_name,
                                      p_module->p_config[i].psz_name,
                                      p_module->p_config[i].psz_value );
                        break;
                    }
                }
            }
        }

    }
    
    fclose( file );
    free( psz_filename );

    vlc_mutex_unlock( &p_main->config_lock );

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
 * When we save we mustn't delete the config options of the plugins that
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
int config_SaveConfigFile( const char *psz_module_name )
{
    module_t *p_module;
    FILE *file;
    char p_line[1024], *p_index2;
    int i, i_sizebuf = 0;
    char *p_bigbuffer, *p_index;
    boolean_t b_backup;
    char *psz_filename, *psz_homedir;

    /* Acquire config file lock */
    vlc_mutex_lock( &p_main->config_lock );

    psz_homedir = config_GetHomeDir();
    if( !psz_homedir )
    {
        intf_ErrMsg( "config_SaveConfigFile: GetHomeDir failed" );
        vlc_mutex_unlock( &p_main->config_lock );
        return -1;
    }
    psz_filename = (char *)malloc( strlen("/.VideoLan/vlc") +
                                   strlen(psz_homedir) + 1 );
    if( !psz_filename )
    {
        intf_ErrMsg( "config err: couldn't malloc psz_filename" );
        free( psz_homedir );
        vlc_mutex_unlock( &p_main->config_lock );
        return -1;
    }
    sprintf( psz_filename, "%s/.VideoLan", psz_homedir );
    free( psz_homedir );
#ifndef WIN32
    mkdir( psz_filename, 0755 );
#else
    mkdir( psz_filename );
#endif
    strcat( psz_filename, "/vlc" );


    intf_WarnMsg( 5, "config_SaveConfigFile: opening config file %s",
                  psz_filename );

    file = fopen( psz_filename, "r" );
    if( !file )
    {
        intf_WarnMsg( 1, "config_SaveConfigFile: couldn't open config file %s "
                      "for reading", psz_filename );
    }
    else
    {
        /* look for file size */
        fseek( file, 0, SEEK_END );
        i_sizebuf = ftell( file );
        rewind( file );
    }

    p_bigbuffer = p_index = malloc( i_sizebuf+1 );
    if( !p_bigbuffer )
    {
        intf_ErrMsg( "config err: couldn't malloc bigbuffer" );
        if( file ) fclose( file );
        free( psz_filename );
        vlc_mutex_unlock( &p_main->config_lock );
        return -1;
    }
    p_bigbuffer[0] = 0;

    /* backup file into memory, we only need to backup the sections we won't
     * save later on */
    b_backup = 0;
    while( file && fgets( p_line, 1024, file ) )
    {
        if( (p_line[0] == '[') && (p_index2 = strchr(p_line,']')))
        {
            /* we found a section, check if we need to do a backup */
            for( p_module = p_module_bank->first; p_module != NULL;
                 p_module = p_module->next )
            {
                if( ((p_index2 - &p_line[1]) == strlen(p_module->psz_name) ) &&
                    !memcmp( &p_line[1], p_module->psz_name,
                             strlen(p_module->psz_name) ) )
                {
                    if( !psz_module_name )
                        break;
                    else if( !strcmp( psz_module_name, p_module->psz_name ) )
                        break;
                }
            }

            if( !p_module )
            {
                /* we don't have this section in our list so we need to back
                 * it up */
                *p_index2 = 0;
                intf_WarnMsg( 5, "config_SaveConfigFile: backing up config for"
                              " unknown module <%s>", &p_line[1] );
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

    file = fopen( psz_filename, "w" );
    if( !file )
    {
        intf_WarnMsg( 1, "config_SaveConfigFile: couldn't open config file %s "
                      "for writing", psz_filename );
        free( psz_filename );
        vlc_mutex_unlock( &p_main->config_lock );
        return -1;
    }

    fprintf( file, "#\n# "COPYRIGHT_MESSAGE"\n#\n\n" );

    /* Look for the selected module, if NULL then save everything */
    for( p_module = p_module_bank->first ; p_module != NULL ;
         p_module = p_module->next )
    {

        if( psz_module_name && strcmp( psz_module_name, p_module->psz_name ) )
            continue;

        if( !p_module->i_config_items )
            continue;

        intf_WarnMsg( 5, "config_SaveConfigFile: saving config for "
                      "module <%s>", p_module->psz_name );

        fprintf( file, "[%s]\n", p_module->psz_name );
        if( p_module->psz_longname )
            fprintf( file, "# %s\n#\n", p_module->psz_longname );

        for( i = 0; i < p_module->i_config_lines; i++ )
        {
            if( p_module->p_config[i].i_type & MODULE_CONFIG_HINT )
                /* ignore hints */
                continue;

            switch( p_module->p_config[i].i_type )
            {
            case MODULE_CONFIG_ITEM_BOOL:
            case MODULE_CONFIG_ITEM_INTEGER:
                if( p_module->p_config[i].psz_text )
                    fprintf( file, "# %s\n", p_module->p_config[i].psz_text );
                fprintf( file, "%s=%i\n", p_module->p_config[i].psz_name,
                         p_module->p_config[i].i_value );
                break;

            default:
                if( p_module->p_config[i].psz_value )
                {
                    if( p_module->p_config[i].psz_text )
                        fprintf( file, "# %s\n",
                                 p_module->p_config[i].psz_text );
                    fprintf( file, "%s=%s\n", p_module->p_config[i].psz_name,
                             p_module->p_config[i].psz_value );
                }
            }
        }

        fprintf( file, "\n" );
    }


    /*
     * Restore old settings from the config in file
     */
    fputs( p_bigbuffer, file );
    free( p_bigbuffer );

    fclose( file );
    free( psz_filename );
    vlc_mutex_unlock( &p_main->config_lock );

    return 0;
}

/*****************************************************************************
 * config_GetHomeDir: find the user's home directory.
 *****************************************************************************
 * This function will try by different ways to find the user's home path.
 *****************************************************************************/
static char *config_GetHomeDir(void)
{
    char *p_tmp, *p_homedir = NULL;

#if defined(HAVE_GETPWUID_R) || defined(HAVE_GETPWUID)
    struct passwd *p_pw = NULL;
#endif

#if defined(HAVE_GETPWUID_R)
    int ret;
    struct passwd pwd;
    char *p_buffer = NULL;
    int bufsize = 128;

    p_buffer = (char *)malloc( bufsize );

    if( ( ret = getpwuid_r( getuid(), &pwd, p_buffer, bufsize, &p_pw ) ) < 0 )
    {

#elif defined(HAVE_GETPWUID)
    if( ( p_pw = getpwuid( getuid() ) ) == NULL )
    {

#endif
        if( ( p_tmp = getenv( "HOME" ) ) == NULL )
        {
            intf_ErrMsg( "Unable to get home directory, set it to /tmp" );
            p_homedir = strdup( "/tmp" );
        }
        else p_homedir = strdup( p_tmp );

#if defined(HAVE_GETPWUID_R) || defined(HAVE_GETPWUID)
    }
    else
    {
        if( p_pw ) p_homedir = strdup( p_pw->pw_dir );
    }
#endif

#if defined(HAVE_GETPWUID_R)
    if( p_buffer ) free( p_buffer );
#endif

    return p_homedir;
}
