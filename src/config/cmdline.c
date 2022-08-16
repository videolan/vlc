/*****************************************************************************
 * cmdline.c: command line parsing
 *****************************************************************************
 * Copyright (C) 2001-2007 VLC authors and VideoLAN
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
#include <vlc_actions.h>
#include <vlc_charset.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>

#include "vlc_getopt.h"
#include "vlc_jaro_winkler.h"

#include "ansi_term.h"
#include "configuration.h"
#include "modules/modules.h"

#include <assert.h>

/**
 * Perform early scan of arguments for a small subset of simple options.
 *
 * Before proper full processing can be done, which requires full knowledge of
 * all available options, we have a need to perform a preliminary check for a
 * few options that affect plugin loading. This is because the full option set
 * consists of a core subset plus various plugin subsets and we thus need to
 * load plugin data to know the plugin subsets, but there are a few core options
 * that actually affect loading of plugin data.
 *
 * Note that we want to be cautious about doing too much here. Firstly simply
 * to avoid duplicating effort done later in the proper full processing step.
 * Additionally though because fundamentally the very notion of a preliminary
 * scan is problematic in terms of taking "correct" (or at least consistent)
 * action in the face of certain use or misuse of options. Consider the fact
 * that it is standard practice with option arguments that take a value, should
 * a value not be provided within the same argument, to just consume the next
 * argument as the option's value, no matter whether or not the next argument
 * might "look" like an option itself (or an "early terminator"). Such is the
 * behaviour of getopt. (It may perhaps seem wrong to do things that way, but
 * that's how it works; it would alternatively be wrong for getopt in general to
 * prevent options from taking values that just happen to resemble these things;
 * and besides, bad consumption will only occur through bad option use which is
 * fair to expect to cause incorrect results and in many cases ideally would be
 * caught early by validation checks on option values anyway). So, a preliminary
 * scan, by not knowing the full option set and not following the standard
 * parsing rules (since it can't), naturally introduces risks of "incorrect" or
 * "inconsistent" interpretation of the set of arguments, at least wrt. the
 * later full/proper processing, and the more that we do, the more such
 * incorrect/inconsistent processing scenarios we introduce. Furthermore
 * consider the additional complication of short option "sets" (which is a short
 * option argument involving multiple characters like `-abc`). In a short option
 * set only the last option in the set (which is not necessarily its last
 * character) can be one that takes a value, with any characters coming after it
 * (if any) taken as its option value (if none then the next argument is
 * consumed instead). In a preliminary scan where we do not know the full set of
 * available short options, we can risk misinterpreting characters in short
 * option sets as options when they are meant to be consumed as an option value.
 * Thus it is very problematic to properly try to deal with an option like
 * `--help`, which comes with the short option `-h` as well as `--no-help`, here
 * in a preliminary scan, which is unfortunate because it might make a nice
 * optimisation if we could.
 *
 * Considering this discussion, it is simply best that we keep option checks
 * here to an absolute bare minimum of just the three plugin data related ones
 * that we have no choice but to scan for early, and we must put up with a
 * minimal amount of possible incorrect/inconsistent interpretation as
 * unavoidable. (Though we could add some code after the full scan to determine
 * whether or not the wrong action took place and issue a warning if so).
 *
 * @param p_this object to write command line options as variables to
 * @param argc number of command line arguments
 * @param argv command line arguments
 */
void config_CmdLineEarlyScan( libvlc_int_t *p_this, int argc, const char *argv[] )
{
#if !defined(HAVE_DYNAMIC_PLUGINS) && !defined(_WIN32)
    VLC_UNUSED(p_this); VLC_UNUSED(argc); VLC_UNUSED(argv);
#else
    for( int i = 0; i < argc; i++ )
    {
        const char *arg = argv[i];

        /* Early terminator */
        if( strcmp( arg, "--" ) == 0 )
            break;

#define check_option_variant(option, option_name, value) \
    if( strcmp( arg, option ) == 0 ) \
    { \
        var_Create (p_this, option_name, VLC_VAR_BOOL); \
        var_SetBool (p_this, option_name, value); \
        continue; \
    }
#define check_option(option_name) \
    check_option_variant("--"   option_name, option_name, true)  \
    check_option_variant("--no-"option_name, option_name, false) \
    check_option_variant("--no" option_name, option_name, false)
#define check_string(option_name) \
    if( strncmp( arg, "--" option_name "=", strlen("--" option_name "=") ) == 0 ) \
    { \
        const char *value = arg + strlen("--" option_name "="); \
        var_Create (p_this, option_name, VLC_VAR_STRING); \
        var_SetString (p_this, option_name, value); \
        continue; \
    }

#ifdef HAVE_DYNAMIC_PLUGINS
        check_option("plugins-cache")
        check_option("plugins-scan")
        check_option("reset-plugins-cache")
#endif

#if defined(_WIN32) || defined(__OS2__)
        check_option("high-priority")
#endif
#ifdef _WIN32
        check_string("clock-source")
#endif

#undef check_option
#undef check_option_variant
#undef check_string
    }
#endif
}

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
 * @param ppsz_args command line arguments [IN/OUT]
 * @param pindex index of the first non-option argument [OUT]
 * @return 0 on success, -1 on error.
 */
int config_LoadCmdLine( libvlc_int_t *p_this, int i_argc,
                        const char *ppsz_argv[], int *pindex )
{
    int i_cmd, i_index, i_opts, i_shortopts, flag, i_verbose = 0;
    struct vlc_option *p_longopts;

    /* Short options */
    i_shortopts = 0;
    const module_config_t *pp_shortopts[256] = { NULL };
    char *psz_shortopts;

    /*
     * Generate the longopts and shortopts structures used by getopt_long
     */

    i_opts = 0;
    for (const vlc_plugin_t *p = vlc_plugins; p != NULL; p = p->next)
        /* count the number of exported configuration options (to allocate
         * longopts). We also need to allocate space for two options when
         * dealing with boolean to allow for --foo and --no-foo */
        i_opts += p->conf.count + 2 * p->conf.booleans;

    p_longopts = vlc_alloc( i_opts + 1, sizeof(*p_longopts)  );
    if( p_longopts == NULL )
        return -1;

    psz_shortopts = malloc( 2 * i_opts + 1 );
    if( psz_shortopts == NULL )
    {
        free( p_longopts );
        return -1;
    }

    /* Indicate that we want to know the difference between unknown option and
       missing option value issues */
    psz_shortopts[0] = ':';
    i_shortopts = 1;

    /* Fill the p_longopts and psz_shortopts structures */
    i_index = 0;
    for (const vlc_plugin_t *p = vlc_plugins; p != NULL; p = p->next)
    {
        for (size_t i = 0; i < p->conf.size; i++)
        {
            const struct vlc_param *param = p->conf.params + i;
            const module_config_t *p_item = &param->item;

            /* Ignore hints */
            if( !CONFIG_ITEM(p_item->i_type) )
                continue;

            /* Add item to long options */
            p_longopts[i_index].name = strdup( p_item->psz_name );
            if( p_longopts[i_index].name == NULL ) continue;
            p_longopts[i_index].flag = &flag;
            p_longopts[i_index].val = 0;
            p_longopts[i_index].is_obsolete = param->obsolete;

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
                p_longopts[i_index].is_obsolete = param->obsolete;
                p_longopts[i_index].flag = &flag;
                p_longopts[i_index].val = 1;
                i_index++;

                if( asprintf( &psz_name, "no-%s", p_item->psz_name ) == -1 )
                    continue;
                p_longopts[i_index].name = psz_name;
                p_longopts[i_index].has_arg = false;
                p_longopts[i_index].is_obsolete = param->obsolete;
                p_longopts[i_index].flag = &flag;
                p_longopts[i_index].val = 1;
            }
            i_index++;

            /* If item also has a short option, add it */
            if (param->shortname)
            {
                pp_shortopts[param->shortname] = p_item;
                psz_shortopts[i_shortopts++] = param->shortname;

                if( p_item->i_type != CONFIG_ITEM_BOOL
                 && param->shortname != 'v' )
                {
                    psz_shortopts[i_shortopts] = ':';
                    i_shortopts++;
                }
            }
        }
    }

    /* Close the longopts and shortopts structures */
    memset( &p_longopts[i_index], 0, sizeof(*p_longopts) );
    psz_shortopts[i_shortopts] = '\0';

    int ret = -1;
    bool color = false;
#ifndef _WIN32
    color = (isatty(STDERR_FILENO));
#endif

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
            const char *psz_full_name = p_longopts[i_index].name;
            const char *psz_name = psz_full_name;

            /* Check if we deal with a --nofoo or --no-foo long option */
            if( flag ) psz_name += psz_full_name[2] == '-' ? 3 : 2;

            /* Store the configuration option */
            p_conf = config_FindConfig( psz_name );
            if( p_conf )
            {
                struct vlc_param *param = container_of(p_conf,
                                                       struct vlc_param, item);
                /* Check if the option is deprecated */
                if (param->obsolete)
                {
                    fprintf(stderr,
                            _( "%sWarning:%s Option --%s no longer exists.\n" ),
                            color ? TS_YELLOW_BOLD : "",
                            color ? TS_RESET : "",
                            psz_full_name);
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
                        var_Change( p_this, psz_name, VLC_VAR_SETMINMAX,
                            (vlc_value_t){ .i_int = p_conf->min.i },
                            (vlc_value_t){ .i_int = p_conf->max.i } );
                        var_SetInteger( p_this, psz_name,
                                        strtoll(state.arg, NULL, 0));
                        break;
                    case CONFIG_ITEM_FLOAT:
                        var_Create( p_this, psz_name, VLC_VAR_FLOAT );
                        var_Change( p_this, psz_name, VLC_VAR_SETMINMAX,
                            (vlc_value_t){ .f_float = p_conf->min.f },
                            (vlc_value_t){ .f_float = p_conf->max.f } );
                        var_SetFloat( p_this, psz_name, vlc_atof_c(state.arg) );
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
        if( i_cmd != '?' && i_cmd != ':' && pp_shortopts[i_cmd] != NULL )
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
                case CONFIG_ITEM_FLOAT:
                    var_Create( p_this, name, VLC_VAR_FLOAT );
                    var_SetFloat( p_this, name, vlc_atof_c(state.arg) );
                    break;
                case CONFIG_ITEM_BOOL:
                    var_Create( p_this, name, VLC_VAR_BOOL );
                    var_SetBool( p_this, name, true );
                    break;
            }

            continue;
        }

        /* Internal error: unknown option or missing option value */
        char *optlabel;
        if ( (state.opt && asprintf(&optlabel, "%s-%c%s",
                                    color ? TS_YELLOW : "", state.opt,
                                    color ? TS_RESET : "") < 0)
          || (!state.opt && asprintf(&optlabel, "%s%s%s",
                                     color ? TS_YELLOW : "", ppsz_argv[state.ind-1],
                                     color ? TS_RESET : "") < 0) )
        {
            /* just ignore failure - unlikely and not worth trying to handle in some way */
            optlabel = NULL;
        }

        fprintf( stderr, _( "%sError:%s " ), color ? TS_RED_BOLD : "", color ? TS_RESET : "");
        if (i_cmd == ':')
        {
            fprintf( stderr, _( "Missing mandatory value for option %s\n" ), optlabel );
        }
        else
        {
            fprintf( stderr, _( "Unknown option `%s'\n" ), optlabel );

            /* suggestion matching */
            if( !state.opt )
            {
                float jw_filter = 0.8, best_metric = jw_filter, metric;
                const char *best = NULL;
                const char *jw_a = ppsz_argv[state.ind-1] + 2;
                for (size_t i = 0; i < (size_t)i_opts; i++) {
                    if (p_longopts[i].is_obsolete)
                        continue;
                    const char *jw_b = p_longopts[i].name;
                    if (vlc_jaro_winkler(jw_a, jw_b, &metric) == 0) { //ignore failed malloc calls
                        if (metric > best_metric || (!best && metric >= jw_filter)) {
                            best = jw_b;
                            best_metric = metric;
                        }
                    }
                }
                if (best)
                    fprintf( stderr, _( "       Did you mean %s--%s%s?\n" ),
                             color ? TS_GREEN : "", best, color ? TS_RESET : "" );
            }
        }
        fprintf( stderr, _( "For more information try %s--help%s\n" ),
                 color ? TS_GREEN : "", color ? TS_RESET : "" );
        free( optlabel );
        goto out;
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
    return ret;
}
