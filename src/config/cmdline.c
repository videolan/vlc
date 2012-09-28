/*****************************************************************************
 * cmdline.c: command line parsing
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "../libvlc.h"
#include <vlc_keys.h>
#include <vlc_charset.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>

#include "vlc_getopt.h"

#include "configuration.h"
#include "modules/modules.h"

#include <assert.h>

#undef config_LoadCmdLine
/**
 * Parse command line for configuration options.
 *
 * Now that the module_bank has been initialized, we can dynamically
 * generate the longopts structure used by getops. We have to do it this way
 * because we don't know (and don't want to know) in advance the configuration
 * options used (ie. exported) by each module.
 *
 * @param p_this object to write command line options as variables to
 * @param i_argc number of command line arguments
 * @param ppsz_args commandl ine arguments [IN/OUT]
 * @param pindex NULL to ignore unknown options,
 *               otherwise index of the first non-option argument [OUT]
 * @return 0 on success, -1 on error.
 */
int config_LoadCmdLine( vlc_object_t *p_this, int i_argc,
                        const char *ppsz_argv[], int *pindex )
{
    int i_cmd, i_index, i_opts, i_shortopts, flag, i_verbose = 0;
    struct vlc_option *p_longopts;
    const char **argv_copy = NULL;
#define b_ignore_errors (pindex == NULL)

    /* Short options */
    module_config_t *pp_shortopts[256];
    char *psz_shortopts;

    /* List all modules */
    size_t count;
    module_t **list = module_list_get (&count);

    /*
     * Generate the longopts and shortopts structures used by getopt_long
     */

    i_opts = 0;
    for (size_t i = 0; i < count; i++)
    {
        /* count the number of exported configuration options (to allocate
         * longopts). We also need to allocate space for two options when
         * dealing with boolean to allow for --foo and --no-foo */
        module_t *p_parser = list[i];

        i_opts += p_parser->i_config_items + 2 * p_parser->i_bool_items;
    }

    p_longopts = malloc( sizeof(*p_longopts) * (i_opts + 1) );
    if( p_longopts == NULL )
    {
        module_list_free (list);
        return -1;
    }

    psz_shortopts = malloc( 2 * i_opts + 1 );
    if( psz_shortopts == NULL )
    {
        free( p_longopts );
        module_list_free (list);
        return -1;
    }

    /* If we are requested to ignore errors, then we must work on a copy
     * of the ppsz_argv array, otherwise getopt_long will reorder it for
     * us, ignoring the arity of the options */
    if( b_ignore_errors )
    {
        argv_copy = (const char**)malloc( i_argc * sizeof(char *) );
        if( argv_copy == NULL )
        {
            free( psz_shortopts );
            free( p_longopts );
            module_list_free (list);
            return -1;
        }
        memcpy( argv_copy, ppsz_argv, i_argc * sizeof(char *) );
        ppsz_argv = argv_copy;
    }

    i_shortopts = 0;
    for( i_index = 0; i_index < 256; i_index++ )
    {
        pp_shortopts[i_index] = NULL;
    }

    /* Fill the p_longopts and psz_shortopts structures */
    i_index = 0;
    for (size_t i = 0; i < count; i++)
    {
        module_t *p_parser = list[i];
        module_config_t *p_item, *p_end;

        if( !p_parser->i_config_items )
            continue;

        for( p_item = p_parser->p_config, p_end = p_item + p_parser->confsize;
             p_item < p_end;
             p_item++ )
        {
            /* Ignore hints */
            if( !CONFIG_ITEM(p_item->i_type) )
                continue;

            /* Add item to long options */
            p_longopts[i_index].name = strdup( p_item->psz_name );
            if( p_longopts[i_index].name == NULL ) continue;
            p_longopts[i_index].flag = &flag;
            p_longopts[i_index].val = 0;

            if( CONFIG_CLASS(p_item->i_type) != CONFIG_ITEM_BOOL )
                p_longopts[i_index].has_arg = true;
            else
            /* Booleans also need --no-foo and --nofoo options */
            {
                char *psz_name;

                p_longopts[i_index].has_arg = false;
                i_index++;

                if( asprintf( &psz_name, "no%s", p_item->psz_name ) == -1 )
                    continue;
                p_longopts[i_index].name = psz_name;
                p_longopts[i_index].has_arg = false;
                p_longopts[i_index].flag = &flag;
                p_longopts[i_index].val = 1;
                i_index++;

                if( asprintf( &psz_name, "no-%s", p_item->psz_name ) == -1 )
                    continue;
                p_longopts[i_index].name = psz_name;
                p_longopts[i_index].has_arg = false;
                p_longopts[i_index].flag = &flag;
                p_longopts[i_index].val = 1;
            }
            i_index++;

            /* If item also has a short option, add it */
            if( p_item->i_short )
            {
                pp_shortopts[(int)p_item->i_short] = p_item;
                psz_shortopts[i_shortopts] = p_item->i_short;
                i_shortopts++;
                if( p_item->i_type != CONFIG_ITEM_BOOL
                 && p_item->i_short != 'v' )
                {
                    psz_shortopts[i_shortopts] = ':';
                    i_shortopts++;
                }
            }
        }
    }

    /* We don't need the module list anymore */
    module_list_free( list );

    /* Close the longopts and shortopts structures */
    memset( &p_longopts[i_index], 0, sizeof(*p_longopts) );
    psz_shortopts[i_shortopts] = '\0';

    int ret = -1;

    /*
     * Parse the command line options
     */
    vlc_getopt_t state;
    state.ind = 0 ; /* set to 0 to tell GNU getopt to reinitialize */
    while( ( i_cmd = vlc_getopt_long( i_argc, (char **)ppsz_argv,
                                      psz_shortopts,
                                      p_longopts, &i_index, &state ) ) != -1 )
    {
        /* A long option has been recognized */
        if( i_cmd == 0 )
        {
            module_config_t *p_conf;
            const char *psz_name = p_longopts[i_index].name;

            /* Check if we deal with a --nofoo or --no-foo long option */
            if( flag ) psz_name += psz_name[2] == '-' ? 3 : 2;

            /* Store the configuration option */
            p_conf = config_FindConfig( p_this, psz_name );
            if( p_conf )
            {
                /* Check if the option is deprecated */
                if( p_conf->b_removed )
                {
                    fprintf(stderr,
                            "Warning: option --%s no longer exists.\n",
                            psz_name);
                    continue;
                }

                switch( CONFIG_CLASS(p_conf->i_type) )
                {
                    case CONFIG_ITEM_STRING:
                        var_Create( p_this, psz_name, VLC_VAR_STRING );
                        var_SetString( p_this, psz_name, state.arg );
                        break;
                    case CONFIG_ITEM_INTEGER:
                        var_Create( p_this, psz_name, VLC_VAR_INTEGER );
                        var_SetInteger( p_this, psz_name,
                                        strtoll(state.arg, NULL, 0));
                        break;
                    case CONFIG_ITEM_FLOAT:
                        var_Create( p_this, psz_name, VLC_VAR_FLOAT );
                        var_SetFloat( p_this, psz_name, us_atof(state.arg) );
                        break;
                    case CONFIG_ITEM_BOOL:
                        var_Create( p_this, psz_name, VLC_VAR_BOOL );
                        var_SetBool( p_this, psz_name, !flag );
                        break;
                }
                continue;
            }
        }

        /* A short option has been recognized */
        if( pp_shortopts[i_cmd] != NULL )
        {
            const char *name = pp_shortopts[i_cmd]->psz_name;
            switch( CONFIG_CLASS(pp_shortopts[i_cmd]->i_type) )
            {
                case CONFIG_ITEM_STRING:
                    var_Create( p_this, name, VLC_VAR_STRING );
                    var_SetString( p_this, name, state.arg );
                    break;
                case CONFIG_ITEM_INTEGER:
                    var_Create( p_this, name, VLC_VAR_INTEGER );
                    if( i_cmd == 'v' )
                    {
                        i_verbose++; /* -v */
                        var_SetInteger( p_this, name, i_verbose );
                    }
                    else
                    {
                        var_SetInteger( p_this, name,
                                        strtoll(state.arg, NULL, 0) );
                    }
                    break;
                case CONFIG_ITEM_BOOL:
                    var_Create( p_this, name, VLC_VAR_BOOL );
                    var_SetBool( p_this, name, true );
                    break;
            }

            continue;
        }

        /* Internal error: unknown option */
        if( !b_ignore_errors )
        {
            fputs( "vlc: unknown option"
                     " or missing mandatory argument ", stderr );
            if( state.opt )
            {
                fprintf( stderr, "`-%c'\n", state.opt );
            }
            else
            {
                fprintf( stderr, "`%s'\n", ppsz_argv[state.ind-1] );
            }
            fputs( "Try `vlc --help' for more information.\n", stderr );
            goto out;
        }
    }

    ret = 0;
    if( pindex != NULL )
        *pindex = state.ind;
out:
    /* Free allocated resources */
    for( i_index = 0; p_longopts[i_index].name; i_index++ )
        free( (char *)p_longopts[i_index].name );
    free( p_longopts );
    free( psz_shortopts );
    free( argv_copy );
    return ret;
}

