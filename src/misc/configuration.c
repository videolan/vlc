/*****************************************************************************
 * configuration.c management of the modules configuration
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: configuration.c,v 1.27 2002/06/01 12:32:01 sam Exp $
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

#include <vlc/vlc.h>

#include <stdio.h>                                              /* sprintf() */
#include <stdlib.h>                                      /* free(), strtol() */
#include <string.h>                                              /* strdup() */
#include <errno.h>                                                  /* errno */

#ifdef HAVE_UNISTD_H
#    include <unistd.h>                                          /* getuid() */
#endif

#ifdef HAVE_GETOPT_LONG
#   ifdef HAVE_GETOPT_H
#       include <getopt.h>                                       /* getopt() */
#   endif
#else
#   include "GNUgetopt/getopt.h"
#endif

#if defined(HAVE_GETPWUID)
#include <pwd.h>                                               /* getpwuid() */
#endif

#include <sys/stat.h>
#include <sys/types.h>

/*****************************************************************************
 * config_GetInt: get the value of an int variable
 *****************************************************************************
 * This function is used to get the value of variables which are internally
 * represented by an integer (MODULE_CONFIG_ITEM_INTEGER and
 * MODULE_CONFIG_ITEM_BOOL).
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
    if( (p_config->i_type!=MODULE_CONFIG_ITEM_INTEGER) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_BOOL) )
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
 * represented by a float (MODULE_CONFIG_ITEM_FLOAT).
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
    if( p_config->i_type != MODULE_CONFIG_ITEM_FLOAT )
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
 * represented by a string (MODULE_CONFIG_ITEM_STRING, MODULE_CONFIG_ITEM_FILE,
 * and MODULE_CONFIG_ITEM_MODULE).
 *
 * Important note: remember to free() the returned char* because it a duplicate
 *   of the actual value. It isn't safe to return a pointer to the actual value
 *   as it can be modified at any time.
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
    if( (p_config->i_type!=MODULE_CONFIG_ITEM_STRING) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_FILE) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_MODULE) )
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
 * represented by a string (MODULE_CONFIG_ITEM_STRING, MODULE_CONFIG_ITEM_FILE,
 * and MODULE_CONFIG_ITEM_MODULE).
 *****************************************************************************/
void __config_PutPsz( vlc_object_t *p_this, 
                      const char *psz_name, char *psz_value )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return;
    }
    if( (p_config->i_type!=MODULE_CONFIG_ITEM_STRING) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_FILE) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_MODULE) )
    {
        msg_Err( p_this, "option %s does not refer to a string", psz_name );
        return;
    }

    vlc_mutex_lock( p_config->p_lock );

    /* free old string */
    if( p_config->psz_value ) free( p_config->psz_value );

    if( psz_value ) p_config->psz_value = strdup( psz_value );
    else p_config->psz_value = NULL;

    vlc_mutex_unlock( p_config->p_lock );

    if( p_config->pf_callback )
    {
        p_config->pf_callback( p_this );
    }
}

/*****************************************************************************
 * config_PutInt: set the integer value of an int variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by an integer (MODULE_CONFIG_ITEM_INTEGER and
 * MODULE_CONFIG_ITEM_BOOL).
 *****************************************************************************/
void __config_PutInt( vlc_object_t *p_this, const char *psz_name, int i_value )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return;
    }
    if( (p_config->i_type!=MODULE_CONFIG_ITEM_INTEGER) &&
        (p_config->i_type!=MODULE_CONFIG_ITEM_BOOL) )
    {
        msg_Err( p_this, "option %s does not refer to an int", psz_name );
        return;
    }

    p_config->i_value = i_value;

    if( p_config->pf_callback )
    {
        p_config->pf_callback( p_this );
    }
}

/*****************************************************************************
 * config_PutFloat: set the value of a float variable
 *****************************************************************************
 * This function is used to set the value of variables which are internally
 * represented by a float (MODULE_CONFIG_ITEM_FLOAT).
 *****************************************************************************/
void __config_PutFloat( vlc_object_t *p_this,
                        const char *psz_name, float f_value )
{
    module_config_t *p_config;

    p_config = config_FindConfig( p_this, psz_name );

    /* sanity checks */
    if( !p_config )
    {
        msg_Err( p_this, "option %s does not exist", psz_name );
        return;
    }
    if( p_config->i_type != MODULE_CONFIG_ITEM_FLOAT )
    {
        msg_Err( p_this, "option %s does not refer to a float", psz_name );
        return;
    }

    p_config->f_value = f_value;

    if( p_config->pf_callback )
    {
        p_config->pf_callback( p_this );
    }
}

/*****************************************************************************
 * config_FindConfig: find the config structure associated with an option.
 *****************************************************************************
 * FIXME: This function really needs to be optimized.
 *****************************************************************************/
module_config_t *config_FindConfig( vlc_object_t *p_this, const char *psz_name )
{
    module_t *p_module;
    module_config_t *p_item;

    if( !psz_name ) return NULL;

    for( p_module = p_this->p_vlc->module_bank.first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        for( p_item = p_module->p_config;
             p_item->i_type != MODULE_CONFIG_HINT_END;
             p_item++ )
        {
            if( p_item->i_type & MODULE_CONFIG_HINT )
                /* ignore hints */
                continue;
            if( !strcmp( psz_name, p_item->psz_name ) )
                return p_item;
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
void config_Duplicate( module_t *p_module, module_config_t *p_orig )
{
    int i, i_lines = 1;
    module_config_t *p_item;

    /* Calculate the structure length */
    p_module->i_config_items = 0;
    p_module->i_bool_items = 0;

    for( p_item = p_orig; p_item->i_type != MODULE_CONFIG_HINT_END; p_item++ )
    {
        i_lines++;

        if( p_item->i_type & MODULE_CONFIG_ITEM )
        {
            p_module->i_config_items++;
        }

        if( p_item->i_type == MODULE_CONFIG_ITEM_BOOL )
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

    /* Initialize the global lock */
    vlc_mutex_init( p_module->p_this, &p_module->config_lock );

    /* Do the duplication job */
    for( i = 0; i < i_lines ; i++ )
    {
        p_module->p_config[i].i_type = p_orig[i].i_type;
        p_module->p_config[i].i_short = p_orig[i].i_short;
        p_module->p_config[i].i_value = p_orig[i].i_value;
        p_module->p_config[i].f_value = p_orig[i].f_value;
        p_module->p_config[i].b_dirty = p_orig[i].b_dirty;

        p_module->p_config[i].psz_name = p_orig[i].psz_name ?
                                   strdup( _(p_orig[i].psz_name) ) : NULL;
        p_module->p_config[i].psz_text = p_orig[i].psz_text ?
                                   strdup( _(p_orig[i].psz_text) ) : NULL;
        p_module->p_config[i].psz_longtext = p_orig[i].psz_longtext ?
                                   strdup( _(p_orig[i].psz_longtext) ) : NULL;
        p_module->p_config[i].psz_value = p_orig[i].psz_value ?
                                   strdup( _(p_orig[i].psz_value) ) : NULL;

        p_module->p_config[i].p_lock = &p_module->config_lock;

        /* the callback pointer is only valid when the module is loaded so this
         * value is set in ActivateModule() and reset in DeactivateModule() */
        p_module->p_config[i].pf_callback = NULL;
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

    for( ; p_item->i_type != MODULE_CONFIG_HINT_END ; p_item++ )
    {
        if( p_item->psz_name )
        {
            free( p_item->psz_name );
        }

        if( p_item->psz_text )
        {
            free( p_item->psz_text );
        }

        if( p_item->psz_longtext )
        {
            free( p_item->psz_longtext );
        }

        if( p_item->psz_value )
        {
            free( p_item->psz_value );
        }
    }

    free( p_module->p_config );
    p_module->p_config = NULL;

    /* Remove the global lock */
    vlc_mutex_destroy( &p_module->config_lock );
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
    while( p_new->i_type != MODULE_CONFIG_HINT_END )
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
    while( p_new->i_type != MODULE_CONFIG_HINT_END )
    {
        p_new->pf_callback = NULL;
        p_new++;
    }
}

/*****************************************************************************
 * config_LoadConfigFile: loads the configuration file.
 *****************************************************************************
 * This function is called to load the config options stored in the config
 * file.
 *****************************************************************************/
int config_LoadConfigFile( vlc_object_t *p_this, const char *psz_module_name )
{
    module_t *p_module;
    module_config_t *p_item;
    FILE *file;
    char line[1024];
    char *p_index, *psz_option_name, *psz_option_value;
    char *psz_filename, *psz_homedir;

    /* Acquire config file lock */
    vlc_mutex_lock( &p_this->p_vlc->config_lock );

    psz_homedir = p_this->p_vlc->psz_homedir;
    if( !psz_homedir )
    {
        msg_Err( p_this, "psz_homedir is null" );
        vlc_mutex_unlock( &p_this->p_vlc->config_lock );
        return -1;
    }
    psz_filename = (char *)malloc( strlen("/" CONFIG_DIR "/" CONFIG_FILE) +
                                   strlen(psz_homedir) + 1 );
    if( !psz_filename )
    {
        msg_Err( p_this, "out of memory" );
        vlc_mutex_unlock( &p_this->p_vlc->config_lock );
        return -1;
    }
    sprintf( psz_filename, "%s/" CONFIG_DIR "/" CONFIG_FILE, psz_homedir );

    msg_Dbg( p_this, "opening config file %s", psz_filename );

    file = fopen( psz_filename, "rt" );
    if( !file )
    {
        msg_Warn( p_this, "config file %s does not exist yet", psz_filename );
        free( psz_filename );
        vlc_mutex_unlock( &p_this->p_vlc->config_lock );
        return -1;
    }

    /* Look for the selected module, if NULL then save everything */
    for( p_module = p_this->p_vlc->module_bank.first ; p_module != NULL ;
         p_module = p_module->next )
    {

        if( psz_module_name
             && strcmp( psz_module_name, p_module->psz_object_name ) )
        {
            continue;
        }

        /* The config file is organized in sections, one per module. Look for
         * the interesting section ( a section is of the form [foo] ) */
        rewind( file );
        while( fgets( line, 1024, file ) )
        {
            if( (line[0] == '[') && (p_index = strchr(line,']')) &&
                (p_index - &line[1] == strlen(p_module->psz_object_name) ) &&
                !memcmp( &line[1], p_module->psz_object_name,
                         strlen(p_module->psz_object_name) ) )
            {
                msg_Dbg( p_this, "loading config for module <%s>",
                                 p_module->psz_object_name );

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

            /* try to match this option with one of the module's options */
            for( p_item = p_module->p_config;
                 p_item->i_type != MODULE_CONFIG_HINT_END;
                 p_item++ )
            {
                if( p_item->i_type & MODULE_CONFIG_HINT )
                    /* ignore hints */
                    continue;

                if( !strcmp( p_item->psz_name, psz_option_name ) )
                {
                    /* We found it */
                    switch( p_item->i_type )
                    {
                    case MODULE_CONFIG_ITEM_BOOL:
                    case MODULE_CONFIG_ITEM_INTEGER:
                        if( !*psz_option_value )
                            break;                    /* ignore empty option */
                        p_item->i_value = atoi( psz_option_value);
                        msg_Dbg( p_this, "found <%s> option %s=%i",
                                 p_module->psz_object_name, p_item->psz_name,
                                 p_item->i_value );
                        break;

                    case MODULE_CONFIG_ITEM_FLOAT:
                        if( !*psz_option_value )
                            break;                    /* ignore empty option */
                        p_item->f_value = (float)atof( psz_option_value);
                        msg_Dbg( p_this, "found <%s> option %s=%f",
                                 p_module->psz_object_name, p_item->psz_name,
                                 (double)p_item->f_value );
                        break;

                    default:
                        vlc_mutex_lock( p_item->p_lock );

                        /* free old string */
                        if( p_item->psz_value )
                            free( p_item->psz_value );

                        p_item->psz_value = *psz_option_value ?
                            strdup( psz_option_value ) : NULL;

                        vlc_mutex_unlock( p_item->p_lock );

                        msg_Dbg( p_this, "found <%s> option %s=%s",
                                 p_module->psz_object_name, p_item->psz_name,
                                 p_item->psz_value != NULL ?
                                      p_item->psz_value : "(NULL)" );
                        break;
                    }
                }
            }
        }

    }
    
    fclose( file );
    free( psz_filename );

    vlc_mutex_unlock( &p_this->p_vlc->config_lock );

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
int config_SaveConfigFile( vlc_object_t *p_this, const char *psz_module_name )
{
    module_t *p_module;
    module_config_t *p_item;
    FILE *file;
    char p_line[1024], *p_index2;
    int i_sizebuf = 0;
    char *p_bigbuffer, *p_index;
    vlc_bool_t b_backup;
    char *psz_filename, *psz_homedir;

    /* Acquire config file lock */
    vlc_mutex_lock( &p_this->p_vlc->config_lock );

    psz_homedir = p_this->p_vlc->psz_homedir;
    if( !psz_homedir )
    {
        msg_Err( p_this, "psz_homedir is null" );
        vlc_mutex_unlock( &p_this->p_vlc->config_lock );
        return -1;
    }
    psz_filename = (char *)malloc( strlen("/" CONFIG_DIR "/" CONFIG_FILE) +
                                   strlen(psz_homedir) + 1 );
    if( !psz_filename )
    {
        msg_Err( p_this, "out of memory" );
        vlc_mutex_unlock( &p_this->p_vlc->config_lock );
        return -1;
    }
    sprintf( psz_filename, "%s/" CONFIG_DIR, psz_homedir );

#ifndef WIN32
    if( mkdir( psz_filename, 0755 ) && errno != EEXIST )
#else
    if( mkdir( psz_filename ) && errno != EEXIST )
#endif
    {
        msg_Err( p_this, "could not create %s (%s)",
                         psz_filename, strerror(errno) );
    }

    strcat( psz_filename, "/" CONFIG_FILE );


    msg_Dbg( p_this, "opening config file %s", psz_filename );

    file = fopen( psz_filename, "rt" );
    if( !file )
    {
        msg_Warn( p_this, "config file %s does not exist yet", psz_filename );
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
        msg_Err( p_this, "out of memory" );
        if( file ) fclose( file );
        free( psz_filename );
        vlc_mutex_unlock( &p_this->p_vlc->config_lock );
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
            for( p_module = p_this->p_vlc->module_bank.first; p_module != NULL;
                 p_module = p_module->next )
            {
                if( ((p_index2 - &p_line[1])
                       == strlen(p_module->psz_object_name) ) &&
                    !memcmp( &p_line[1], p_module->psz_object_name,
                             strlen(p_module->psz_object_name) ) )
                {
                    if( !psz_module_name )
                        break;
                    else if( !strcmp( psz_module_name,
                                      p_module->psz_object_name ) )
                        break;
                }
            }

            if( !p_module )
            {
                /* we don't have this section in our list so we need to back
                 * it up */
                *p_index2 = 0;
                msg_Dbg( p_this, "backing up config for unknown module <%s>",
                                 &p_line[1] );
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

    file = fopen( psz_filename, "wt" );
    if( !file )
    {
        msg_Warn( p_this, "could not open config file %s for writing",
                          psz_filename );
        free( psz_filename );
        vlc_mutex_unlock( &p_this->p_vlc->config_lock );
        return -1;
    }

    fprintf( file, "###\n###  " COPYRIGHT_MESSAGE "\n###\n\n" );

    /* Look for the selected module, if NULL then save everything */
    for( p_module = p_this->p_vlc->module_bank.first ; p_module != NULL ;
         p_module = p_module->next )
    {

        if( psz_module_name && strcmp( psz_module_name,
                                       p_module->psz_object_name ) )
            continue;

        if( !p_module->i_config_items )
            continue;

        msg_Dbg( p_this, "saving config for module <%s>",
                         p_module->psz_object_name );

        fprintf( file, "[%s]", p_module->psz_object_name );
        if( p_module->psz_longname )
            fprintf( file, " # %s\n\n", p_module->psz_longname );
        else
            fprintf( file, "\n\n" );

        for( p_item = p_module->p_config;
             p_item->i_type != MODULE_CONFIG_HINT_END;
             p_item++ )
        {
            if( p_item->i_type & MODULE_CONFIG_HINT )
                /* ignore hints */
                continue;

            switch( p_item->i_type )
            {
            case MODULE_CONFIG_ITEM_BOOL:
            case MODULE_CONFIG_ITEM_INTEGER:
                if( p_item->psz_text )
                    fprintf( file, "# %s (%s)\n", p_item->psz_text,
                             (p_item->i_type == MODULE_CONFIG_ITEM_BOOL) ?
                             _("boolean") : _("integer") );
                fprintf( file, "%s=%i\n", p_item->psz_name, p_item->i_value );
                break;

            case MODULE_CONFIG_ITEM_FLOAT:
                if( p_item->psz_text )
                    fprintf( file, "# %s (%s)\n", p_item->psz_text,
                             _("float") );
                fprintf( file, "%s=%f\n", p_item->psz_name,
                         (double)p_item->f_value );
                break;

            default:
                if( p_item->psz_text )
                    fprintf( file, "# %s (%s)\n", p_item->psz_text,
                             _("string") );
                fprintf( file, "%s=%s\n", p_item->psz_name,
                         p_item->psz_value ? p_item->psz_value : "" );
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
    vlc_mutex_unlock( &p_this->p_vlc->config_lock );

    return 0;
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
int config_LoadCmdLine( vlc_object_t *p_this, int *pi_argc, char *ppsz_argv[],
                        vlc_bool_t b_ignore_errors )
{
    int i_cmd, i_index, i_opts, i_shortopts, flag;
    module_t *p_module;
    module_config_t *p_item;
    struct option *p_longopts;

    /* Short options */
    module_config_t *pp_shortopts[256];
    char *psz_shortopts;

    /* Set default configuration and copy arguments */
    p_this->p_vlc->i_argc    = *pi_argc;
    p_this->p_vlc->ppsz_argv = ppsz_argv;

    p_this->p_vlc->p_channel = NULL;

#ifdef SYS_DARWIN
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

    /*
     * Generate the longopts and shortopts structures used by getopt_long
     */

    i_opts = 0;
    for( p_module = p_this->p_vlc->module_bank.first;
         p_module != NULL ;
         p_module = p_module->next )
    {
        /* count the number of exported configuration options (to allocate
         * longopts). We also need to allocate space for too options when
         * dealing with boolean to allow for --foo and --no-foo */
        i_opts += (p_module->i_config_items + p_module->i_bool_items);
    }

    p_longopts = malloc( sizeof(struct option) * (i_opts + 1) );
    if( p_longopts == NULL )
    {
        msg_Err( p_this, "out of memory" );
        return -1;
    }

    psz_shortopts = malloc( sizeof( char ) * (2 * i_opts + 1) );
    if( psz_shortopts == NULL )
    {
        msg_Err( p_this, "out of memory" );
        free( p_longopts );
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
    for( p_module = p_this->p_vlc->module_bank.first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        for( p_item = p_module->p_config;
             p_item->i_type != MODULE_CONFIG_HINT_END;
             p_item++ )
        {
            /* Ignore hints */
            if( p_item->i_type & MODULE_CONFIG_HINT )
                continue;

            /* Add item to long options */
            p_longopts[i_index].name = strdup( p_item->psz_name );
            if( p_longopts[i_index].name == NULL ) continue;
            p_longopts[i_index].has_arg =
                (p_item->i_type == MODULE_CONFIG_ITEM_BOOL)?
                                               no_argument : required_argument;
            p_longopts[i_index].flag = &flag;
            p_longopts[i_index].val = 0;
            i_index++;

            /* When dealing with bools we also need to add the --no-foo
             * option */
            if( p_item->i_type == MODULE_CONFIG_ITEM_BOOL )
            {
                char *psz_name = malloc( strlen(p_item->psz_name) + 4 );
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
                if( p_item->i_type != MODULE_CONFIG_ITEM_BOOL )
                {
                    psz_shortopts[i_shortopts] = ':';
                    i_shortopts++;
                }
            }
        }
    }

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

            /* Check if we deal with a --no-foo long option */
            if( flag ) psz_name += 3;

            /* Store the configuration option */
            p_conf = config_FindConfig( p_this, psz_name );

            if( p_conf ) switch( p_conf->i_type )
            {
            case MODULE_CONFIG_ITEM_STRING:
            case MODULE_CONFIG_ITEM_FILE:
            case MODULE_CONFIG_ITEM_MODULE:
                config_PutPsz( p_this, psz_name, optarg );
                break;
            case MODULE_CONFIG_ITEM_INTEGER:
                config_PutInt( p_this, psz_name, atoi(optarg));
                break;
            case MODULE_CONFIG_ITEM_FLOAT:
                config_PutFloat( p_this, psz_name, (float)atof(optarg) );
                break;
            case MODULE_CONFIG_ITEM_BOOL:
                config_PutInt( p_this, psz_name, !flag );
                break;
            }

            continue;
        }

        /* A short option has been recognized */
        if( pp_shortopts[i_cmd] != NULL )
        {
            switch( pp_shortopts[i_cmd]->i_type )
            {
            case MODULE_CONFIG_ITEM_STRING:
            case MODULE_CONFIG_ITEM_FILE:
            case MODULE_CONFIG_ITEM_MODULE:
                config_PutPsz( p_this, pp_shortopts[i_cmd]->psz_name, optarg );
                break;
            case MODULE_CONFIG_ITEM_INTEGER:
                config_PutInt( p_this, pp_shortopts[i_cmd]->psz_name,
                                       atoi(optarg));
                break;
            case MODULE_CONFIG_ITEM_BOOL:
                config_PutInt( p_this, pp_shortopts[i_cmd]->psz_name, 1 );
                break;
            }

            continue;
        }

        /* Internal error: unknown option */
        if( !b_ignore_errors )
        {
            fprintf( stderr, "unknown option `%s'\n", ppsz_argv[optind-1] );
            fprintf( stderr, "Try `%s --help' for more information.\n",
                             p_this->p_vlc->psz_object_name );

            free( p_longopts );
            free( psz_shortopts );
            if( b_ignore_errors ) free( ppsz_argv );
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

/*****************************************************************************
 * config_GetHomeDir: find the user's home directory.
 *****************************************************************************
 * This function will try by different ways to find the user's home path.
 * Note that this function is not reentrant, it should be called only once
 * at the beginning of main where the result will be stored for later use.
 *****************************************************************************/
char *config_GetHomeDir( void )
{
    char *p_tmp, *p_homedir = NULL;

#if defined(HAVE_GETPWUID)
    struct passwd *p_pw = NULL;
#endif

#ifdef WIN32
    typedef HRESULT (WINAPI *SHGETFOLDERPATH)( HWND, int, HANDLE, DWORD,
                                               LPTSTR );
#   define CSIDL_FLAG_CREATE 0x8000
#   define CSIDL_APPDATA 0x1A
#   define SHGFP_TYPE_CURRENT 0

    HINSTANCE shfolder_dll;
    SHGETFOLDERPATH SHGetFolderPath ;

    /* load the shell32 dll to retreive SHGetFolderPath */
    if( ( shfolder_dll = LoadLibrary("shfolder.dll") ) != NULL )
    {
        SHGetFolderPath = (void *)GetProcAddress( shfolder_dll,
                                                  "SHGetFolderPathA" );
        if ( SHGetFolderPath != NULL )
        {
            p_homedir = (char *)malloc( MAX_PATH );
            if( !p_homedir )
            {
//X                intf_ErrMsg( "config error: couldn't malloc p_homedir" );
                return NULL;
            }

            /* get the "Application Data" folder for the current user */
            if( S_OK == SHGetFolderPath( NULL,
                                         CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                                         NULL, SHGFP_TYPE_CURRENT,
                                         p_homedir ) )
            {
                FreeLibrary( shfolder_dll );
                return p_homedir;
            }
            free( p_homedir );
        }
        FreeLibrary( shfolder_dll );
    }
#endif

#if defined(HAVE_GETPWUID)
    if( ( p_pw = getpwuid( getuid() ) ) == NULL )
#endif
    {
        if( ( p_tmp = getenv( "HOME" ) ) == NULL )
        {
            if( ( p_tmp = getenv( "TMP" ) ) == NULL )
            {
                p_homedir = strdup( "/tmp" );
            }
            else p_homedir = strdup( p_tmp );

//X            intf_ErrMsg( "config error: unable to get home directory, "
//X                         "using %s instead", p_homedir );

        }
        else p_homedir = strdup( p_tmp );
    }
#if defined(HAVE_GETPWUID)
    else
    {
        p_homedir = strdup( p_pw->pw_dir );
    }
#endif

    return p_homedir;
}
