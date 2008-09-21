/*****************************************************************************
 * file.c: configuration file handling
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
#include "../libvlc.h"
#include "vlc_charset.h"
#include "vlc_keys.h"

#include <errno.h>                                                  /* errno */
#include <assert.h>
#include <limits.h>
#ifdef __APPLE__
#   include <xlocale.h>
#else
#include <locale.h>
#endif

#include "configuration.h"
#include "modules/modules.h"

static char *ConfigKeyToString( int );

static inline char *strdupnull (const char *src)
{
    return src ? strdup (src) : NULL;
}

/**
 * Get the user's configuration file
 */
static char *config_GetConfigFile( void )
{
    char *psz_dir = config_GetUserConfDir();
    char *psz_configfile;

    if( asprintf( &psz_configfile, "%s" DIR_SEP CONFIG_FILE, psz_dir ) == -1 )
        psz_configfile = NULL;
    free( psz_dir );
    return psz_configfile;
}

static FILE *config_OpenConfigFile( vlc_object_t *p_obj, const char *mode )
{
    char *psz_filename = libvlc_priv (p_obj->p_libvlc)->psz_configfile;
    FILE *p_stream;

    if( !psz_filename )
    {
        psz_filename = config_GetConfigFile();
    }

    msg_Dbg( p_obj, "opening config file (%s)", psz_filename );

    p_stream = utf8_fopen( psz_filename, mode );
    if( p_stream == NULL && errno != ENOENT )
    {
        msg_Err( p_obj, "cannot open config file (%s): %m",
                 psz_filename );

    }
#if !( defined(WIN32) || defined(__APPLE__) || defined(SYS_BEOS) )
    else if( p_stream == NULL && errno == ENOENT && mode[0] == 'r' )
    {
        /* This is the fallback for pre XDG Base Directory
         * Specification configs */
        char *psz_old;
        if( asprintf( &psz_old, "%s" DIR_SEP CONFIG_DIR DIR_SEP CONFIG_FILE,
                      config_GetHomeDir() ) != -1 )
        {
            p_stream = utf8_fopen( psz_old, mode );
            if( p_stream )
            {
                /* Old config file found. We want to write it at the
                 * new location now. */
                msg_Info( p_obj->p_libvlc, "Found old config file at %s. "
                          "VLC will now use %s.", psz_old, psz_filename );
                char *psz_readme;
                if( asprintf(&psz_readme,"%s"DIR_SEP CONFIG_DIR DIR_SEP"README",
                              config_GetHomeDir() ) != -1 )
                {
                    FILE *p_readme = utf8_fopen( psz_readme, "wt" );
                    if( p_readme )
                    {
                        fprintf( p_readme, "The VLC media player "
                                 "configuration folder has moved to comply\n"
                                 "with the XDG Base Directory Specification "
                                 "version 0.6. Your\nconfiguration has been "
                                 "copied to the new location:\n%s\nYou can "
                                 "delete this directory and all its contents.",
                                  psz_filename);
                        fclose( p_readme );
                    }
                    free( psz_readme );
                }
            }
            free( psz_old );
        }
    }
#endif
    else if( p_stream != NULL )
    {
        libvlc_priv (p_obj->p_libvlc)->psz_configfile = psz_filename;
    }

    return p_stream;
}


static int strtoi (const char *str)
{
    char *end;
    long l;

    errno = 0;
    l = strtol (str, &end, 0);

    if (!errno)
    {
        if ((l > INT_MAX) || (l < INT_MIN))
            errno = ERANGE;
        if (*end)
            errno = EINVAL;
    }
    return (int)l;
}


/*****************************************************************************
 * config_LoadConfigFile: loads the configuration file.
 *****************************************************************************
 * This function is called to load the config options stored in the config
 * file.
 *****************************************************************************/
int __config_LoadConfigFile( vlc_object_t *p_this, const char *psz_module_name )
{
    libvlc_priv_t *priv = libvlc_priv (p_this->p_libvlc);
    FILE *file;

    file = config_OpenConfigFile (p_this, "rt");
    if (file == NULL)
        return VLC_EGENERIC;

    /* Acquire config file lock */
    vlc_mutex_lock( &priv->config_lock );

    /* Look for the selected module, if NULL then save everything */
    module_t **list = module_list_get (NULL);

    /* Look for UTF-8 Byte Order Mark */
    char * (*convert) (const char *) = strdupnull;
    char bom[3];

    if ((fread (bom, 1, 3, file) != 3)
     || memcmp (bom, "\xEF\xBB\xBF", 3))
    {
        convert = FromLocaleDup;
        rewind (file); /* no BOM, rewind */
    }

    module_t *module = NULL;
    char line[1024], section[1022];
    section[0] = '\0';

    /* Ensure consistent number formatting... */
    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t baseloc = uselocale (loc);

    while (fgets (line, 1024, file) != NULL)
    {
        /* Ignore comments and empty lines */
        switch (line[0])
        {
            case '#':
            case '\n':
            case '\0':
                continue;
        }

        if (line[0] == '[')
        {
            char *ptr = strchr (line, ']');
            if (ptr == NULL)
                continue; /* syntax error; */
            *ptr = '\0';

            /* New section ( = a given module) */
            strcpy (section, line + 1);
            module = NULL;

            if ((psz_module_name == NULL)
             || (strcmp (psz_module_name, section) == 0))
            {
                for (int i = 0; list[i]; i++)
                {
                    module_t *m = list[i];

                    if ((strcmp (section, m->psz_object_name) == 0)
                     && (m->i_config_items > 0)) /* ignore config-less modules */
                    {
                        module = m;
                        if (psz_module_name != NULL)
                            msg_Dbg (p_this,
                                     "loading config for module \"%s\"",
                                     section);
                        break;
                    }
                }
            }

            continue;
        }

        if (module == NULL)
            continue; /* no need to parse if there is no matching module */

        char *ptr = strchr (line, '\n');
        if (ptr != NULL)
            *ptr = '\0';

        /* look for option name */
        const char *psz_option_name = line;

        ptr = strchr (line, '=');
        if (ptr == NULL)
            continue; /* syntax error */

        *ptr = '\0';
        const char *psz_option_value = ptr + 1;

        /* try to match this option with one of the module's options */
        for (size_t i = 0; i < module->confsize; i++)
        {
            module_config_t *p_item = module->p_config + i;

            if ((p_item->i_type & CONFIG_HINT)
             || strcmp (p_item->psz_name, psz_option_name))
                continue;

            /* We found it */
            errno = 0;

            switch( p_item->i_type )
            {
                case CONFIG_ITEM_BOOL:
                case CONFIG_ITEM_INTEGER:
                {
                    long l = strtoi (psz_option_value);
                    if (errno)
                        msg_Warn (p_this, "Integer value (%s) for %s: %m",
                                  psz_option_value, psz_option_name);
                    else
                        p_item->saved.i = p_item->value.i = (int)l;
                    break;
                }

                case CONFIG_ITEM_FLOAT:
                    if( !*psz_option_value )
                        break;                    /* ignore empty option */
                    p_item->value.f = (float)atof (psz_option_value);
                    p_item->saved.f = p_item->value.f;
                    break;

                case CONFIG_ITEM_KEY:
                    if( !*psz_option_value )
                        break;                    /* ignore empty option */
                    p_item->value.i = ConfigStringToKey(psz_option_value);
                    p_item->saved.i = p_item->value.i;
                    break;

                default:
                    vlc_mutex_lock( p_item->p_lock );

                    /* free old string */
                    free( (char*) p_item->value.psz );
                    free( (char*) p_item->saved.psz );

                    p_item->value.psz = convert (psz_option_value);
                    p_item->saved.psz = strdupnull (p_item->value.psz);

                    vlc_mutex_unlock( p_item->p_lock );
                    break;
            }

            break;
        }
    }

    if (ferror (file))
    {
        msg_Err (p_this, "error reading configuration: %m");
        clearerr (file);
    }
    fclose (file);

    module_list_free (list);
    if (loc != (locale_t)0)
    {
        uselocale (baseloc);
        freelocale (loc);
    }

    vlc_mutex_unlock( &priv->config_lock );
    return 0;
}

/*****************************************************************************
 * config_CreateDir: Create configuration directory if it doesn't exist.
 *****************************************************************************/
int config_CreateDir( vlc_object_t *p_this, const char *psz_dirname )
{
    if( !psz_dirname || !*psz_dirname ) return -1;

    if( utf8_mkdir( psz_dirname, 0700 ) == 0 )
        return 0;

    switch( errno )
    {
        case EEXIST:
            return 0;

        case ENOENT:
        {
            /* Let's try to create the parent directory */
            char psz_parent[strlen( psz_dirname ) + 1], *psz_end;
            strcpy( psz_parent, psz_dirname );

            psz_end = strrchr( psz_parent, DIR_SEP_CHAR );
            if( psz_end && psz_end != psz_parent )
            {
                *psz_end = '\0';
                if( config_CreateDir( p_this, psz_parent ) == 0 )
                {
                    if( !utf8_mkdir( psz_dirname, 0700 ) )
                        return 0;
                }
            }
        }
    }

    msg_Err( p_this, "could not create %s: %m", psz_dirname );
    return -1;
}

static int
config_Write (FILE *file, const char *type, const char *desc,
              bool comment, const char *name, const char *fmt, ...)
{
    va_list ap;
    int ret;

    if (desc == NULL)
        desc = "?";

    if (fprintf (file, "# %s (%s)\n%s%s=", desc, _(type),
                 comment ? "#" : "", name) < 0)
        return -1;

    va_start (ap, fmt);
    ret = vfprintf (file, fmt, ap);
    va_end (ap);
    if (ret < 0)
        return -1;

    if (fputs ("\n\n", file) == EOF)
        return -1;
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
                           bool b_autosave )
{
    libvlc_priv_t *priv = libvlc_priv (p_this->p_libvlc);
    module_t *p_parser;
    FILE *file;
    char p_line[1024], *p_index2;
    int i_sizebuf = 0;
    char *p_bigbuffer, *p_index;
    bool b_backup;
    int i_index;

    /* Acquire config file lock */
    vlc_mutex_lock( &priv->config_lock );

    if( libvlc_priv (p_this->p_libvlc)->psz_configfile == NULL )
    {
        char *psz_configdir = config_GetUserConfDir();
        if( !psz_configdir ) /* XXX: This should never happen */
        {
            msg_Err( p_this, "no configuration directory defined" );
            vlc_mutex_unlock( &priv->config_lock );
            return -1;
        }

        config_CreateDir( p_this, psz_configdir );
        free( psz_configdir );
    }

    file = config_OpenConfigFile( p_this, "rt" );
    if( file != NULL )
    {
        /* look for file size */
        fseek( file, 0L, SEEK_END );
        i_sizebuf = ftell( file );
        fseek( file, 0L, SEEK_SET );
    }

    p_bigbuffer = p_index = malloc( i_sizebuf+1 );
    if( !p_bigbuffer )
    {
        if( file ) fclose( file );
        vlc_mutex_unlock( &priv->config_lock );
        return -1;
    }
    p_bigbuffer[0] = 0;

    /* List all available modules */
    module_t **list = module_list_get (NULL);

    /* backup file into memory, we only need to backup the sections we won't
     * save later on */
    b_backup = 0;
    while( file && fgets( p_line, 1024, file ) )
    {
        if( (p_line[0] == '[') && (p_index2 = strchr(p_line,']')))
        {

            /* we found a section, check if we need to do a backup */
            for( i_index = 0; (p_parser = list[i_index]) != NULL; i_index++ )
            {
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

            if( list[i_index] == NULL )
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

    file = config_OpenConfigFile (p_this, "wt");
    if( !file )
    {
        module_list_free (list);
        free( p_bigbuffer );
        vlc_mutex_unlock( &priv->config_lock );
        return -1;
    }

    fprintf( file, "\xEF\xBB\xBF###\n###  " COPYRIGHT_MESSAGE "\n###\n\n"
       "###\n### lines beginning with a '#' character are comments\n###\n\n" );

    /* Ensure consistent number formatting... */
    locale_t loc = newlocale (LC_NUMERIC_MASK, "C", NULL);
    locale_t baseloc = uselocale (loc);

    /* Look for the selected module, if NULL then save everything */
    for( i_index = 0; (p_parser = list[i_index]) != NULL; i_index++ )
    {
        module_config_t *p_item, *p_end;

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

        for( p_item = p_parser->p_config, p_end = p_item + p_parser->confsize;
             p_item < p_end;
             p_item++ )
        {
            /* Do not save the new value in the configuration file
             * if doing an autosave, and the item is not an "autosaved" one. */
            bool b_retain = b_autosave && !p_item->b_autosave;

            if ((p_item->i_type & CONFIG_HINT) /* ignore hint */
             || p_item->b_removed              /* ignore deprecated option */
             || p_item->b_unsaveable)          /* ignore volatile option */
                continue;

            if (IsConfigIntegerType (p_item->i_type))
            {
                int val = b_retain ? p_item->saved.i : p_item->value.i;
                if (p_item->i_type == CONFIG_ITEM_KEY)
                {
                    char *psz_key = ConfigKeyToString (val);
                    config_Write (file, p_item->psz_text, N_("key"),
                                  val == p_item->orig.i,
                                  p_item->psz_name, "%s",
                                  psz_key ? psz_key : "");
                    free (psz_key);
                }
                else
                    config_Write (file, p_item->psz_text,
                                  (p_item->i_type == CONFIG_ITEM_BOOL)
                                      ? N_("boolean") : N_("integer"),
                                  val == p_item->orig.i,
                                  p_item->psz_name, "%d", val);
                p_item->saved.i = val;
            }
            else
            if (IsConfigFloatType (p_item->i_type))
            {
                float val = b_retain ? p_item->saved.f : p_item->value.f;
                config_Write (file, p_item->psz_text, N_("float"),
                              val == p_item->orig.f,
                              p_item->psz_name, "%f", val);
                p_item->saved.f = val;
            }
            else
            {
                const char *psz_value = b_retain ? p_item->saved.psz
                                                 : p_item->value.psz;
                bool modified;

                assert (IsConfigStringType (p_item->i_type));

                if (b_retain && (psz_value == NULL)) /* FIXME: hack */
                    psz_value = p_item->orig.psz;

                modified =
                    (psz_value != NULL)
                        ? ((p_item->orig.psz != NULL)
                            ? (strcmp (psz_value, p_item->orig.psz) != 0)
                            : true)
                        : (p_item->orig.psz != NULL);

                config_Write (file, p_item->psz_text, N_("string"),
                              !modified, p_item->psz_name, "%s",
                              psz_value ? psz_value : "");

                if ( !b_retain )
                {

                    free ((char *)p_item->saved.psz);
                    if( (psz_value && p_item->orig.psz &&
                         strcmp( psz_value, p_item->orig.psz )) ||
                        !psz_value || !p_item->orig.psz)
                        p_item->saved.psz = strdupnull (psz_value);
                    else
                        p_item->saved.psz = NULL;
                }
            }

            if (!b_retain)
                p_item->b_dirty = false;
        }
    }

    module_list_free (list);
    if (loc != (locale_t)0)
    {
        uselocale (baseloc);
        freelocale (loc);
    }

    /*
     * Restore old settings from the config in file
     */
    fputs( p_bigbuffer, file );
    free( p_bigbuffer );

    fclose( file );
    vlc_mutex_unlock( &priv->config_lock );

    return 0;
}

int config_AutoSaveConfigFile( vlc_object_t *p_this )
{
    libvlc_priv_t *priv = libvlc_priv (p_this->p_libvlc);
    size_t i_index;
    bool done;

    assert( p_this );

    /* Check if there's anything to save */
    vlc_mutex_lock( &priv->config_lock );
    module_t **list = module_list_get (NULL);
    for( i_index = 0; list[i_index]; i_index++ )
    {
        module_t *p_parser = list[i_index];
        module_config_t *p_item, *p_end;

        if( !p_parser->i_config_items ) continue;

        for( p_item = p_parser->p_config, p_end = p_item + p_parser->confsize;
             p_item < p_end;
             p_item++ )
        {
            if( p_item->b_autosave && p_item->b_dirty ) break;
        }
        if( p_item < p_end ) break;
    }
    done = list[i_index] == NULL;
    module_list_free (list);
    vlc_mutex_unlock( &priv->config_lock );

    return done ? VLC_SUCCESS : SaveConfigFile( p_this, NULL, true );
}

int __config_SaveConfigFile( vlc_object_t *p_this, const char *psz_module_name )
{
    return SaveConfigFile( p_this, psz_module_name, false );
}

/**
 * Get the user's configuration file when given with the --config option
 */
char *config_GetCustomConfigFile( libvlc_int_t *p_libvlc )
{
    char *psz_configfile = config_GetPsz( p_libvlc, "config" );
    if( psz_configfile != NULL )
    {
        if( psz_configfile[0] == '~' && psz_configfile[1] == '/' )
        {
            /* This is incomplete: we should also support the ~cmassiot/ syntax */
            char *psz_buf;
            if( asprintf( &psz_buf, "%s/%s", config_GetHomeDir(),
                          psz_configfile + 2 ) == -1 )
            {
                free( psz_configfile );
                return NULL;
            }
            free( psz_configfile );
            psz_configfile = psz_buf;
        }
    }
    return psz_configfile;
}

int ConfigStringToKey( const char *psz_key )
{
    int i_key = 0;
    unsigned int i;
    const char *psz_parser = strchr( psz_key, '-' );
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

char *ConfigKeyToString( int i_key )
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

