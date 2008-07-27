/*****************************************************************************
 * chain.c : configuration module chain parsing stuff
 *****************************************************************************
 * Copyright (C) 2002-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Eric Petit <titer@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "libvlc.h"

#include "vlc_interface.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* chain format:
    module{option=*:option=*}[:module{option=*:...}]
 */
#define SKIPSPACE( p ) { while( *p && ( *p == ' ' || *p == '\t' ) ) p++; }
#define SKIPTRAILINGSPACE( p, e ) \
    { while( e > p && ( *(e-1) == ' ' || *(e-1) == '\t' ) ) e--; }

/* go accross " " and { } */
static const char *_get_chain_end( const char *str )
{
    char c;
    const char *p = str;

    SKIPSPACE( p );

    for( ;; )
    {
        if( !*p || *p == ',' || *p == '}' ) return p;

        if( *p != '{' && *p != '"' && *p != '\'' )
        {
            p++;
            continue;
        }

        if( *p == '{' ) c = '}';
        else c = *p;
        p++;

        for( ;; )
        {
            if( !*p ) return p;

            if( *p == c ) return ++p;
            else if( *p == '{' && c == '}' ) p = _get_chain_end( p );
            else p++;
        }
    }
}

char *config_ChainCreate( char **ppsz_name, config_chain_t **pp_cfg, const char *psz_chain )
{
    config_chain_t *p_cfg = NULL;
    const char *p = psz_chain;

    *ppsz_name = NULL;
    *pp_cfg    = NULL;

    if( !p ) return NULL;

    SKIPSPACE( p );

    while( *p && *p != '{' && *p != ':' && *p != ' ' && *p != '\t' ) p++;

    if( p == psz_chain ) return NULL;

    *ppsz_name = strndup( psz_chain, p - psz_chain );

    SKIPSPACE( p );

    if( *p == '{' )
    {
        const char *psz_name;

        p++;

        for( ;; )
        {
            config_chain_t cfg;

            SKIPSPACE( p );

            psz_name = p;

            while( *p && *p != '=' && *p != ',' && *p != '{' && *p != '}' &&
                   *p != ' ' && *p != '\t' ) p++;

            /* fprintf( stderr, "name=%s - rest=%s\n", psz_name, p ); */
            if( p == psz_name )
            {
                fprintf( stderr, "config_ChainCreate: invalid options (empty) \n" );
                break;
            }

            cfg.psz_name = strndup( psz_name, p - psz_name );

            SKIPSPACE( p );

            if( *p == '=' || *p == '{' )
            {
                const char *end;
                bool b_keep_brackets = (*p == '{');

                if( *p == '=' ) p++;

                end = _get_chain_end( p );
                if( end <= p )
                {
                    cfg.psz_value = NULL;
                }
                else
                {
                    /* Skip heading and trailing spaces.
                     * This ain't necessary but will avoid simple
                     * user mistakes. */
                    SKIPSPACE( p );
                }

                if( end <= p )
                {
                    cfg.psz_value = NULL;
                }
                else
                {
                    if( *p == '\'' || *p == '"' ||
                        ( !b_keep_brackets && *p == '{' ) )
                    {
                        p++;

                        if( *(end-1) != '\'' && *(end-1) == '"' )
                            SKIPTRAILINGSPACE( p, end );

                        if( end - 1 <= p ) cfg.psz_value = NULL;
                        else cfg.psz_value = strndup( p, end -1 - p );
                    }
                    else
                    {
                        SKIPTRAILINGSPACE( p, end );
                        if( end <= p ) cfg.psz_value = NULL;
                        else cfg.psz_value = strndup( p, end - p );
                    }
                }

                p = end;
                SKIPSPACE( p );
            }
            else
            {
                cfg.psz_value = NULL;
            }

            cfg.p_next = NULL;
            if( p_cfg )
            {
                p_cfg->p_next = malloc( sizeof( config_chain_t ) );
                memcpy( p_cfg->p_next, &cfg, sizeof( config_chain_t ) );

                p_cfg = p_cfg->p_next;
            }
            else
            {
                p_cfg = malloc( sizeof( config_chain_t ) );
                memcpy( p_cfg, &cfg, sizeof( config_chain_t ) );

                *pp_cfg = p_cfg;
            }

            if( *p == ',' ) p++;

            if( *p == '}' )
            {
                p++;
                break;
            }
        }
    }

    if( *p == ':' ) return( strdup( p + 1 ) );

    return NULL;
}

void config_ChainDestroy( config_chain_t *p_cfg )
{
    while( p_cfg != NULL )
    {
        config_chain_t *p_next;

        p_next = p_cfg->p_next;

        FREENULL( p_cfg->psz_name );
        FREENULL( p_cfg->psz_value );
        free( p_cfg );

        p_cfg = p_next;
    }
}

void __config_ChainParse( vlc_object_t *p_this, const char *psz_prefix,
                          const char *const *ppsz_options, config_chain_t *cfg )
{
    if( psz_prefix == NULL ) psz_prefix = "";
    size_t plen = 1 + strlen( psz_prefix );

    /* First, var_Create all variables */
    for( size_t i = 0; ppsz_options[i] != NULL; i++ )
    {
        const char *optname = ppsz_options[i];
        if (optname[0] == '*')
            optname++;

        char name[plen + strlen( optname )];
        snprintf( name, sizeof (name), "%s%s", psz_prefix, optname );
        if( var_Create( p_this, name,
                        config_GetType( p_this, name ) | VLC_VAR_DOINHERIT ) )
            return /* VLC_xxx */;
    }

    /* Now parse options and set value */
    for(; cfg; cfg = cfg->p_next )
    {
        vlc_value_t val;
        bool b_yes = true;
        bool b_once = false;
        module_config_t *p_conf;
        int i_type;
        size_t i;

        if( cfg->psz_name == NULL || *cfg->psz_name == '\0' )
            continue;

        for( i = 0; ppsz_options[i] != NULL; i++ )
        {
            if( !strcmp( ppsz_options[i], cfg->psz_name ) )
            {
                break;
            }
            if( ( !strncmp( cfg->psz_name, "no-", 3 ) &&
                  !strcmp( ppsz_options[i], cfg->psz_name + 3 ) ) ||
                ( !strncmp( cfg->psz_name, "no", 2 ) &&
                  !strcmp( ppsz_options[i], cfg->psz_name + 2 ) ) )
            {
                b_yes = false;
                break;
            }

            if( *ppsz_options[i] == '*' &&
                !strcmp( &ppsz_options[i][1], cfg->psz_name ) )
            {
                b_once = true;
                break;
            }

        }

        if( ppsz_options[i] == NULL )
        {
            msg_Warn( p_this, "option %s is unknown", cfg->psz_name );
            continue;
        }

        /* create name */
        char name[plen + strlen( ppsz_options[i] )];
        const char *psz_name = name;
        snprintf( name, sizeof (name), "%s%s", psz_prefix,
                  b_once ? (ppsz_options[i] + 1) : ppsz_options[i] );

        /* Check if the option is deprecated */
        p_conf = config_FindConfig( p_this, name );

        /* This is basically cut and paste from src/misc/configuration.c
         * with slight changes */
        if( p_conf )
        {
            if( p_conf->b_removed )
            {
                msg_Err( p_this, "Option %s is not supported anymore.",
                         name );
                /* TODO: this should return an error and end option parsing
                 * ... but doing this would change the VLC API and all the
                 * modules so i'll do it later */
                continue;
            }
            if( p_conf->psz_oldname
             && !strcmp( p_conf->psz_oldname, name ) )
            {
                 psz_name = p_conf->psz_name;
                 msg_Warn( p_this, "Option %s is obsolete. Use %s instead.",
                           name, psz_name );
            }
        }
        /* </Check if the option is deprecated> */

        /* get the type of the variable */
        i_type = config_GetType( p_this, psz_name );
        if( !i_type )
        {
            msg_Warn( p_this, "unknown option %s (value=%s)",
                      cfg->psz_name, cfg->psz_value );
            continue;
        }

        i_type &= CONFIG_ITEM;

        if( i_type != VLC_VAR_BOOL && cfg->psz_value == NULL )
        {
            msg_Warn( p_this, "missing value for option %s", cfg->psz_name );
            continue;
        }
        if( i_type != VLC_VAR_STRING && b_once )
        {
            msg_Warn( p_this, "*option_name need to be a string option" );
            continue;
        }

        switch( i_type )
        {
            case VLC_VAR_BOOL:
                val.b_bool = b_yes;
                break;
            case VLC_VAR_INTEGER:
                val.i_int = strtol( cfg->psz_value ? cfg->psz_value : "0",
                                    NULL, 0 );
                break;
            case VLC_VAR_FLOAT:
                val.f_float = atof( cfg->psz_value ? cfg->psz_value : "0" );
                break;
            case VLC_VAR_STRING:
            case VLC_VAR_MODULE:
                val.psz_string = cfg->psz_value;
                break;
            default:
                msg_Warn( p_this, "unhandled config var type (%d)", i_type );
                memset( &val, 0, sizeof( vlc_value_t ) );
                break;
        }
        if( b_once )
        {
            vlc_value_t val2;

            var_Get( p_this, psz_name, &val2 );
            if( *val2.psz_string )
            {
                free( val2.psz_string );
                msg_Dbg( p_this, "ignoring option %s (not first occurrence)", psz_name );
                continue;
            }
            free( val2.psz_string );
        }
        var_Set( p_this, psz_name, val );
        msg_Dbg( p_this, "set config option: %s to %s", psz_name,
                 cfg->psz_value ? cfg->psz_value : "(null)" );
    }
}
