/*****************************************************************************
 * help.c: command line help
 *****************************************************************************
 * Copyright (C) 1998-2011 VLC authors and VideoLAN
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_charset.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include "modules/modules.h"
#include "config/configuration.h"
#include "libvlc.h"

#if defined( _WIN32 ) && !VLC_WINSTORE_APP
static void ShowConsole (void);
static void PauseConsole (void);
#else
# define ShowConsole() (void)0
# define PauseConsole() (void)0
# include <unistd.h>
#endif

static void Help (vlc_object_t *, const char *);
static void Usage (vlc_object_t *, const char *);
static void Version (void);
static void ListModules (vlc_object_t *, bool);
static int ConsoleWidth (void);

/**
 * Checks for help command line options such as --help or --version.
 * If one is found, print the corresponding text.
 * \return true if a command line options caused some help message to be
 * printed, false otherwise. 
 */
bool config_PrintHelp (vlc_object_t *obj)
{
    char *str;

    /* Check for short help option */
    if (var_InheritBool (obj, "help"))
    {
        Help (obj, "help");
        return true;
    }

    /* Check for version option */
    if (var_InheritBool (obj, "version"))
    {
        Version();
        return true;
    }

    /* Check for help on modules */
    str = var_InheritString (obj, "module");
    if (str != NULL)
    {
        Help (obj, str);
        free (str);
        return true;
    }

    /* Check for full help option */
    if (var_InheritBool (obj, "full-help"))
    {
        var_Create (obj, "advanced", VLC_VAR_BOOL);
        var_SetBool (obj, "advanced", true);
        var_Create (obj, "help-verbose", VLC_VAR_BOOL);
        var_SetBool (obj, "help-verbose", true);
        Help (obj, "full-help");
        return true;
    }

    /* Check for long help option */
    if (var_InheritBool (obj, "longhelp"))
    {
        Help (obj, "longhelp");
        return true;
    }

    /* Check for module list option */
    if (var_InheritBool (obj, "list"))
    {
        ListModules (obj, false );
        return true;
    }

    if (var_InheritBool (obj, "list-verbose"))
    {
        ListModules (obj, true);
        return true;
    }

    return false;
}

/*****************************************************************************
 * Help: print program help
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static inline void print_help_on_full_help( void )
{
    utf8_fprintf( stdout, "\n" );
    utf8_fprintf( stdout, "%s\n", _("To get exhaustive help, use '-H'.") );
}

static const char vlc_usage[] = N_(
  "Usage: %s [options] [stream] ...\n"
  "You can specify multiple streams on the commandline.\n"
  "They will be enqueued in the playlist.\n"
  "The first item specified will be played first.\n"
  "\n"
  "Options-styles:\n"
  "  --option  A global option that is set for the duration of the program.\n"
  "   -option  A single letter version of a global --option.\n"
  "   :option  An option that only applies to the stream directly before it\n"
  "            and that overrides previous settings.\n"
  "\n"
  "Stream MRL syntax:\n"
  "  [[access][/demux]://]URL[#[title][:chapter][-[title][:chapter]]]\n"
  "  [:option=value ...]\n"
  "\n"
  "  Many of the global --options can also be used as MRL specific :options.\n"
  "  Multiple :option=value pairs can be specified.\n"
  "\n"
  "URL syntax:\n"
  "  file:///path/file              Plain media file\n"
  "  http://host[:port]/file        HTTP URL\n"
  "  ftp://host[:port]/file         FTP URL\n"
  "  mms://host[:port]/file         MMS URL\n"
  "  screen://                      Screen capture\n"
  "  dvd://[device]                 DVD device\n"
  "  vcd://[device]                 VCD device\n"
  "  cdda://[device]                Audio CD device\n"
  "  udp://[[<source address>]@[<bind address>][:<bind port>]]\n"
  "                                 UDP stream sent by a streaming server\n"
  "  vlc://pause:<seconds>          Pause the playlist for a certain time\n"
  "  vlc://quit                     Special item to quit VLC\n"
  "\n");

static void Help (vlc_object_t *p_this, char const *psz_help_name)
{
    ShowConsole();

    if( psz_help_name && !strcmp( psz_help_name, "help" ) )
    {
        utf8_fprintf( stdout, _(vlc_usage), "vlc" );
        Usage( p_this, "=help" );
        Usage( p_this, "=main" );
        print_help_on_full_help();
    }
    else if( psz_help_name && !strcmp( psz_help_name, "longhelp" ) )
    {
        utf8_fprintf( stdout, _(vlc_usage), "vlc" );
        Usage( p_this, NULL );
        print_help_on_full_help();
    }
    else if( psz_help_name && !strcmp( psz_help_name, "full-help" ) )
    {
        utf8_fprintf( stdout, _(vlc_usage), "vlc" );
        Usage( p_this, NULL );
    }
    else if( psz_help_name )
    {
        Usage( p_this, psz_help_name );
    }

    PauseConsole();
}

/*****************************************************************************
 * Usage: print module usage
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
#   define COL(x)  "\033[" #x ";1m"
#   define RED     COL(31)
#   define GREEN   COL(32)
#   define YELLOW  COL(33)
#   define BLUE    COL(34)
#   define MAGENTA COL(35)
#   define CYAN    COL(36)
#   define WHITE   COL(0)
#   define GRAY    "\033[0m"
static void
print_help_section( const module_t *m, const module_config_t *p_item,
                    bool b_color, bool b_description )
{
    if( !p_item ) return;
    if( b_color )
    {
        utf8_fprintf( stdout, RED"   %s:\n"GRAY,
                      module_gettext( m, p_item->psz_text ) );
        if( b_description && p_item->psz_longtext )
            utf8_fprintf( stdout, MAGENTA"   %s\n"GRAY,
                          module_gettext( m, p_item->psz_longtext ) );
    }
    else
    {
        utf8_fprintf( stdout, "   %s:\n",
                      module_gettext( m, p_item->psz_text ) );
        if( b_description && p_item->psz_longtext )
            utf8_fprintf( stdout, "   %s\n",
                          module_gettext(m, p_item->psz_longtext ) );
    }
}

static void Usage (vlc_object_t *p_this, char const *psz_search)
{
#define FORMAT_STRING "  %s --%s%s%s%s%s%s%s "
    /* short option ------'    | | | | | | |
     * option name ------------' | | | | | |
     * <bra ---------------------' | | | | |
     * option type or "" ----------' | | | |
     * ket> -------------------------' | | |
     * padding spaces -----------------' | |
     * comment --------------------------' |
     * comment suffix ---------------------'
     *
     * The purpose of having bra and ket is that we might i18n them as well.
     */

#define COLOR_FORMAT_STRING (WHITE"  %s --%s"YELLOW"%s%s%s%s%s%s "GRAY)
#define COLOR_FORMAT_STRING_BOOL (WHITE"  %s --%s%s%s%s%s%s%s "GRAY)

#define LINE_START 8
#define PADDING_SPACES 25
#ifdef _WIN32
#   define OPTION_VALUE_SEP "="
#else
#   define OPTION_VALUE_SEP " "
#endif
    char psz_spaces_text[PADDING_SPACES+LINE_START+1];
    char psz_spaces_longtext[LINE_START+3];
    char psz_format[sizeof(COLOR_FORMAT_STRING)];
    char psz_format_bool[sizeof(COLOR_FORMAT_STRING_BOOL)];
    char psz_buffer[10000];
    char psz_short[4];
    int i_width = ConsoleWidth() - (PADDING_SPACES+LINE_START+1);
    int i_width_description = i_width + PADDING_SPACES - 1;
    bool b_advanced    = var_InheritBool( p_this, "advanced" );
    bool b_description = var_InheritBool( p_this, "help-verbose" );
    bool b_description_hack;
    bool b_color       = var_InheritBool( p_this, "color" );
    bool b_has_advanced = false;
    bool b_found       = false;
    unsigned i_only_advanced = 0; /* Number of modules ignored because they
                               * only have advanced options */
    bool b_strict = psz_search && *psz_search == '=';
    if( b_strict ) psz_search++;

    memset( psz_spaces_text, ' ', PADDING_SPACES+LINE_START );
    psz_spaces_text[PADDING_SPACES+LINE_START] = '\0';
    memset( psz_spaces_longtext, ' ', LINE_START+2 );
    psz_spaces_longtext[LINE_START+2] = '\0';
#ifndef _WIN32
    if( !isatty( 1 ) )
#endif
        b_color = false; // don't put color control codes in a .txt file

    if( b_color )
    {
        strcpy( psz_format, COLOR_FORMAT_STRING );
        strcpy( psz_format_bool, COLOR_FORMAT_STRING_BOOL );
    }
    else
    {
        strcpy( psz_format, FORMAT_STRING );
        strcpy( psz_format_bool, FORMAT_STRING );
    }

    /* List all modules */
    size_t count;
    module_t **list = module_list_get (&count);

    /* Ugly hack to make sure that the help options always come first
     * (part 1) */
    if( !psz_search )
        Usage( p_this, "help" );

    /* Enumerate the config for each module */
    for (size_t i = 0; i < count; i++)
    {
        module_t *p_parser = list[i];
        module_config_t *p_item = NULL;
        module_config_t *p_section = NULL;
        module_config_t *p_end = p_parser->p_config + p_parser->confsize;
        const char *objname = module_get_object (p_parser);
        bool b_help_module;

        if( psz_search &&
            ( b_strict ? strcmp( objname, psz_search )
                       : !strstr( objname, psz_search ) ) )
        {
            char *const *pp_shortcuts = p_parser->pp_shortcuts;
            unsigned i;
            for( i = 0; i < p_parser->i_shortcuts; i++ )
            {
                if( b_strict ? !strcmp( psz_search, pp_shortcuts[i] )
                             : !!strstr( pp_shortcuts[i], psz_search ) )
                    break;
            }
            if( i == p_parser->i_shortcuts )
                continue;
        }

        /* Ignore modules without config options */
        if( !p_parser->i_config_items )
        {
            continue;
        }

        b_help_module = !strcmp( "help", objname );
        /* Ugly hack to make sure that the help options always come first
         * (part 2) */
        if( !psz_search && b_help_module )
            continue;

        /* Ignore modules with only advanced config options if requested */
        if( !b_advanced )
        {
            for( p_item = p_parser->p_config;
                 p_item < p_end;
                 p_item++ )
            {
                if( CONFIG_ITEM(p_item->i_type) &&
                    !p_item->b_advanced && !p_item->b_removed ) break;
            }

            if( p_item == p_end )
            {
                i_only_advanced++;
                continue;
            }
        }

        b_found = true;

        /* Print name of module */
        if( strcmp( "main", objname ) )
        {
            if( b_color )
                utf8_fprintf( stdout, "\n " GREEN "%s" GRAY " (%s)\n",
                              module_gettext( p_parser, p_parser->psz_longname ),
                              objname );
            else
                utf8_fprintf( stdout, "\n %s\n",
                              module_gettext(p_parser, p_parser->psz_longname ) );
        }
        if( p_parser->psz_help )
        {
            if( b_color )
                utf8_fprintf( stdout, CYAN" %s\n"GRAY,
                              module_gettext( p_parser, p_parser->psz_help ) );
            else
                utf8_fprintf( stdout, " %s\n",
                              module_gettext( p_parser, p_parser->psz_help ) );
        }

        /* Print module options */
        for( p_item = p_parser->p_config;
             p_item < p_end;
             p_item++ )
        {
            char *psz_text, *psz_spaces = psz_spaces_text;
            const char *psz_bra = NULL, *psz_type = NULL, *psz_ket = NULL;
            const char *psz_suf = "", *psz_prefix = NULL;
            signed int i;
            size_t i_cur_width;

            /* Skip removed options */
            if( p_item->b_removed )
            {
                continue;
            }
            /* Skip advanced options if requested */
            if( p_item->b_advanced && !b_advanced )
            {
                b_has_advanced = true;
                continue;
            }

            switch( CONFIG_CLASS(p_item->i_type) )
            {
            case 0: // hint class
                switch( p_item->i_type )
                {
                case CONFIG_HINT_CATEGORY:
                case CONFIG_HINT_USAGE:
                    if( !strcmp( "main", objname ) )
                    {
                        if( b_color )
                            utf8_fprintf( stdout, GREEN "\n %s\n" GRAY,
                                          module_gettext( p_parser, p_item->psz_text ) );
                        else
                            utf8_fprintf( stdout, "\n %s\n",
                                          module_gettext( p_parser, p_item->psz_text ) );
                    }
                    if( b_description && p_item->psz_longtext )
                    {
                        if( b_color )
                            utf8_fprintf( stdout, CYAN " %s\n" GRAY,
                                          module_gettext( p_parser, p_item->psz_longtext ) );
                        else
                            utf8_fprintf( stdout, " %s\n",
                                          module_gettext( p_parser, p_item->psz_longtext ) );
                }
                break;

                case CONFIG_HINT_SUBCATEGORY:
                    if( strcmp( "main", objname ) )
                        break;
                case CONFIG_SECTION:
                    p_section = p_item;
                    break;
                }
                break;

            case CONFIG_ITEM_STRING:
                print_help_section( p_parser, p_section, b_color,
                                    b_description );
                p_section = NULL;
                psz_bra = OPTION_VALUE_SEP "<";
                psz_type = _("string");
                psz_ket = ">";

                if( p_item->list_count )
                {
                    psz_bra = OPTION_VALUE_SEP "{";
                    psz_type = psz_buffer;
                    psz_buffer[0] = '\0';
                    for( i = 0; i < p_item->list_count; i++ )
                    {
                        if( i ) strcat( psz_buffer, "," );
                        strcat( psz_buffer, p_item->list.psz[i] );
                    }
                    psz_ket = "}";
                }
                break;
            case CONFIG_ITEM_INTEGER:
                print_help_section( p_parser, p_section, b_color,
                                    b_description );
                p_section = NULL;
                psz_bra = OPTION_VALUE_SEP "<";
                psz_type = _("integer");
                psz_ket = ">";

                if( p_item->min.i || p_item->max.i )
                {
                    sprintf( psz_buffer, "%s [%"PRId64" .. %"PRId64"]",
                             psz_type, p_item->min.i, p_item->max.i );
                    psz_type = psz_buffer;
                }

                if( p_item->list_count )
                {
                    psz_bra = OPTION_VALUE_SEP "{";
                    psz_type = psz_buffer;
                    psz_buffer[0] = '\0';
                    for( i = 0; i < p_item->list_count; i++ )
                    {
                        if( i ) strcat( psz_buffer, ", " );
                        sprintf( psz_buffer + strlen(psz_buffer), "%i (%s)",
                                 p_item->list.i[i],
                                 module_gettext( p_parser, p_item->list_text[i] ) );
                    }
                    psz_ket = "}";
                }
                break;
            case CONFIG_ITEM_FLOAT:
                print_help_section( p_parser, p_section, b_color,
                                    b_description );
                p_section = NULL;
                psz_bra = OPTION_VALUE_SEP "<";
                psz_type = _("float");
                psz_ket = ">";
                if( p_item->min.f || p_item->max.f )
                {
                    sprintf( psz_buffer, "%s [%f .. %f]", psz_type,
                             p_item->min.f, p_item->max.f );
                    psz_type = psz_buffer;
                }
                break;
            case CONFIG_ITEM_BOOL:
                print_help_section( p_parser, p_section, b_color,
                                    b_description );
                p_section = NULL;
                psz_bra = ""; psz_type = ""; psz_ket = "";
                if( !b_help_module )
                {
                    psz_suf = p_item->value.i ? _(" (default enabled)") :
                                                _(" (default disabled)");
                }
                break;
            }

            if( !psz_type )
            {
                continue;
            }

            /* Add short option if any */
            if( p_item->i_short )
            {
                sprintf( psz_short, "-%c,", p_item->i_short );
            }
            else
            {
                strcpy( psz_short, "   " );
            }

            i = PADDING_SPACES - strlen( p_item->psz_name )
                 - strlen( psz_bra ) - strlen( psz_type )
                 - strlen( psz_ket ) - 1;

            if( CONFIG_CLASS(p_item->i_type) == CONFIG_ITEM_BOOL
             && !b_help_module )
            {
                psz_prefix =  ", --no-";
                i -= strlen( p_item->psz_name ) + strlen( psz_prefix );
            }

            if( i < 0 )
            {
                psz_spaces[0] = '\n';
                i = 0;
            }
            else
            {
                psz_spaces[i] = '\0';
            }

            if( CONFIG_CLASS(p_item->i_type) == CONFIG_ITEM_BOOL
             && !b_help_module )
            {
                utf8_fprintf( stdout, psz_format_bool, psz_short,
                              p_item->psz_name, psz_prefix, p_item->psz_name,
                              psz_bra, psz_type, psz_ket, psz_spaces );
            }
            else
            {
                utf8_fprintf( stdout, psz_format, psz_short, p_item->psz_name,
                         "", "", psz_bra, psz_type, psz_ket, psz_spaces );
            }

            psz_spaces[i] = ' ';

            /* We wrap the rest of the output */
            sprintf( psz_buffer, "%s%s", module_gettext( p_parser, p_item->psz_text ),
                     psz_suf );
            b_description_hack = b_description;

 description:
            psz_text = psz_buffer;
            i_cur_width = b_description && !b_description_hack
                          ? i_width_description
                          : i_width;
            if( !*psz_text ) strcpy(psz_text, " ");
            while( *psz_text )
            {
                char *psz_parser, *psz_word;
                size_t i_end = strlen( psz_text );

                /* If the remaining text fits in a line, print it. */
                if( i_end <= i_cur_width )
                {
                    if( b_color )
                    {
                        if( !b_description || b_description_hack )
                            utf8_fprintf( stdout, BLUE"%s\n"GRAY, psz_text );
                        else
                            utf8_fprintf( stdout, "%s\n", psz_text );
                    }
                    else
                    {
                        utf8_fprintf( stdout, "%s\n", psz_text );
                    }
                    break;
                }

                /* Otherwise, eat as many words as possible */
                psz_parser = psz_text;
                do
                {
                    psz_word = psz_parser;
                    psz_parser = strchr( psz_word, ' ' );
                    /* If no space was found, we reached the end of the text
                     * block; otherwise, we skip the space we just found. */
                    psz_parser = psz_parser ? psz_parser + 1
                                            : psz_text + i_end;

                } while( (size_t)(psz_parser - psz_text) <= i_cur_width );

                /* We cut a word in one of these cases:
                 *  - it's the only word in the line and it's too long.
                 *  - we used less than 80% of the width and the word we are
                 *    going to wrap is longer than 40% of the width, and even
                 *    if the word would have fit in the next line. */
                if( psz_word == psz_text
             || ( (size_t)(psz_word - psz_text) < 80 * i_cur_width / 100
             && (size_t)(psz_parser - psz_word) > 40 * i_cur_width / 100 ) )
                {
                    char c = psz_text[i_cur_width];
                    psz_text[i_cur_width] = '\0';
                    if( b_color )
                    {
                        if( !b_description || b_description_hack )
                            utf8_fprintf( stdout, BLUE"%s\n%s"GRAY,
                                          psz_text, psz_spaces );
                        else
                            utf8_fprintf( stdout, "%s\n%s",
                                          psz_text, psz_spaces );
                    }
                    else
                    {
                        utf8_fprintf( stdout, "%s\n%s", psz_text, psz_spaces );
                    }
                    psz_text += i_cur_width;
                    psz_text[0] = c;
                }
                else
                {
                    psz_word[-1] = '\0';
                    if( b_color )
                    {
                        if( !b_description || b_description_hack )
                            utf8_fprintf( stdout, BLUE"%s\n%s"GRAY,
                                          psz_text, psz_spaces );
                        else
                            utf8_fprintf( stdout, "%s\n%s",
                                          psz_text, psz_spaces );
                    }
                    else
                    {
                        utf8_fprintf( stdout, "%s\n%s", psz_text, psz_spaces );
                    }
                    psz_text = psz_word;
                }
            }

            if( b_description_hack && p_item->psz_longtext )
            {
                sprintf( psz_buffer, "%s%s",
                         module_gettext( p_parser, p_item->psz_longtext ),
                         psz_suf );
                b_description_hack = false;
                psz_spaces = psz_spaces_longtext;
                utf8_fprintf( stdout, "%s", psz_spaces );
                goto description;
            }
        }
    }

    if( b_has_advanced )
    {
        if( b_color )
            utf8_fprintf( stdout, "\n" WHITE "%s" GRAY " %s\n", _( "Note:" ),
           _( "add --advanced to your command line to see advanced options."));
        else
            utf8_fprintf( stdout, "\n%s %s\n", _( "Note:" ),
           _( "add --advanced to your command line to see advanced options."));
    }

    if( i_only_advanced > 0 )
    {
        if( b_color )
            utf8_fprintf( stdout, "\n" WHITE "%s" GRAY " ", _( "Note:" ) );
        else
            utf8_fprintf( stdout, "\n%s ", _( "Note:" ) );

        utf8_fprintf( stdout, vlc_ngettext("%u module was not displayed "
                                     "because it only has advanced options.\n",
                                           "%u modules were not displayed "
                                  "because they only have advanced options.\n",
                      i_only_advanced ), i_only_advanced );
    }
    else if( !b_found )
    {
        if( b_color )
            utf8_fprintf( stdout, "\n" WHITE "%s" GRAY "\n",
                       _( "No matching module found. Use --list or " \
                          "--list-verbose to list available modules." ) );
        else
            utf8_fprintf( stdout, "\n%s\n",
                       _( "No matching module found. Use --list or " \
                          "--list-verbose to list available modules." ) );
    }

    /* Release the module list */
    module_list_free (list);
}

/*****************************************************************************
 * ListModules: list the available modules with their description
 *****************************************************************************
 * Print a list of all available modules (builtins and plugins) and a short
 * description for each one.
 *****************************************************************************/
static void ListModules (vlc_object_t *p_this, bool b_verbose)
{
    bool b_color = var_InheritBool( p_this, "color" );

    ShowConsole();
#ifdef _WIN32
    b_color = false; // don't put color control codes in a .txt file
#else
    if( !isatty( 1 ) )
        b_color = false;
#endif

    /* List all modules */
    size_t count;
    module_t **list = module_list_get (&count);

    /* Enumerate each module */
    for (size_t j = 0; j < count; j++)
    {
        module_t *p_parser = list[j];
        const char *objname = module_get_object (p_parser);
        if( b_color )
            utf8_fprintf( stdout, GREEN"  %-22s "WHITE"%s\n"GRAY, objname,
                          module_gettext( p_parser, p_parser->psz_longname ) );
        else
            utf8_fprintf( stdout, "  %-22s %s\n", objname,
                          module_gettext( p_parser, p_parser->psz_longname ) );

        if( b_verbose )
        {
            char *const *pp_shortcuts = p_parser->pp_shortcuts;
            for( unsigned i = 0; i < p_parser->i_shortcuts; i++ )
            {
                if( strcmp( pp_shortcuts[i], objname ) )
                {
                    if( b_color )
                        utf8_fprintf( stdout, CYAN"   s %s\n"GRAY,
                                      pp_shortcuts[i] );
                    else
                        utf8_fprintf( stdout, "   s %s\n",
                                      pp_shortcuts[i] );
                }
            }
            if( p_parser->psz_capability )
            {
                if( b_color )
                    utf8_fprintf( stdout, MAGENTA"   c %s (%d)\n"GRAY,
                                  p_parser->psz_capability,
                                  p_parser->i_score );
                else
                    utf8_fprintf( stdout, "   c %s (%d)\n",
                                  p_parser->psz_capability,
                                  p_parser->i_score );
            }
        }
    }
    module_list_free (list);
    PauseConsole();
}

/*****************************************************************************
 * Version: print complete program version
 *****************************************************************************
 * Print complete program version and build number.
 *****************************************************************************/
static void Version( void )
{
    ShowConsole();
    utf8_fprintf( stdout, _("VLC version %s (%s)\n"), VERSION_MESSAGE,
                  psz_vlc_changeset );
    utf8_fprintf( stdout, _("Compiled by %s on %s (%s)\n"),
             VLC_CompileBy(), VLC_CompileHost(), __DATE__" "__TIME__ );
    utf8_fprintf( stdout, _("Compiler: %s\n"), VLC_Compiler() );
    utf8_fprintf( stdout, "%s", LICENSE_MSG );
    PauseConsole();
}

#if defined( _WIN32 ) && !VLC_WINSTORE_APP
/*****************************************************************************
 * ShowConsole: On Win32, create an output console for debug messages
 *****************************************************************************
 * This function is useful only on Win32.
 *****************************************************************************/
static void ShowConsole( void )
{
    FILE *f_help = NULL;

    if( getenv( "PWD" ) ) return; /* Cygwin shell or Wine */

    if( !AllocConsole() ) return;

    /* Use the ANSI code page (e.g. Windows-1252) as expected by the LibVLC
     * Unicode/locale subsystem. By default, we have the obsolecent OEM code
     * page (e.g. CP437 or CP850). */
    SetConsoleOutputCP (GetACP ());
    SetConsoleTitle (TEXT("VLC media player version "PACKAGE_VERSION));

    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );

    f_help = fopen( "vlc-help.txt", "wt" );
    if( f_help != NULL )
    {
        fclose( f_help );
        freopen( "vlc-help.txt", "wt", stdout );
        utf8_fprintf( stderr, _("\nDumped content to vlc-help.txt file.\n") );
    }
    else freopen( "CONOUT$", "w", stdout );
}

/*****************************************************************************
 * PauseConsole: On Win32, wait for a key press before closing the console
 *****************************************************************************
 * This function is useful only on Win32.
 *****************************************************************************/
static void PauseConsole( void )
{
    if( getenv( "PWD" ) ) return; /* Cygwin shell or Wine */

    utf8_fprintf( stderr, _("\nPress the RETURN key to continue...\n") );
    getchar();
    fclose( stdout );
}
#endif

/*****************************************************************************
 * ConsoleWidth: Return the console width in characters
 *****************************************************************************
 * We use the stty shell command to get the console width; if this fails or
 * if the width is less than 80, we default to 80.
 *****************************************************************************/
static int ConsoleWidth( void )
{
    unsigned i_width = 80;

#ifndef _WIN32
    FILE *file = popen( "stty size 2>/dev/null", "r" );
    if (file != NULL)
    {
        if (fscanf (file, "%*u %u", &i_width) <= 0)
            i_width = 80;
        pclose( file );
    }
#elif !VLC_WINSTORE_APP
    CONSOLE_SCREEN_BUFFER_INFO buf;

    if (GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &buf))
        i_width = buf.dwSize.X;
#endif

    return i_width;
}
