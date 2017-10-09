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
#include <wchar.h>
#include <wctype.h>
#include <limits.h>
#include <float.h>

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_charset.h>
#include "modules/modules.h"
#include "config/configuration.h"
#include "libvlc.h"

#if defined( _WIN32 )
# include <vlc_charset.h>
# define wcwidth(cp) (cp, 1) /* LOL */
#else
# include <unistd.h>
# include <termios.h>
# include <sys/ioctl.h>
#endif

#if defined( _WIN32 ) && !VLC_WINSTORE_APP
static void ShowConsole (void);
static void PauseConsole (void);
#else
# define ShowConsole() (void)0
# define PauseConsole() (void)0
#endif

static void Help (vlc_object_t *, const char *);
static void Usage (vlc_object_t *, const char *);
static void Version (void);
static void ListModules (vlc_object_t *, bool);

/**
 * Returns the console width or a best guess.
 */
static unsigned ConsoleWidth(void)
{
#ifdef TIOCGWINSZ
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
        return ws.ws_col;
#endif
#ifdef WIOCGETD
    struct uwdata uw;

    if (ioctl(STDOUT_FILENO, WIOCGETD, &uw) == 0)
        return uw.uw_height / uw.uw_vs;
#endif
#if defined (_WIN32) && !VLC_WINSTORE_APP
    CONSOLE_SCREEN_BUFFER_INFO buf;

    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &buf))
        return buf.dwSize.X;
#endif
    return 80;
}

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
    putchar('\n');
    puts(_("To get exhaustive help, use '-H'."));
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
        printf(_(vlc_usage), "vlc");
        Usage( p_this, "=core" );
        print_help_on_full_help();
    }
    else if( psz_help_name && !strcmp( psz_help_name, "longhelp" ) )
    {
        printf(_(vlc_usage), "vlc");
        Usage( p_this, NULL );
        print_help_on_full_help();
    }
    else if( psz_help_name && !strcmp( psz_help_name, "full-help" ) )
    {
        printf(_(vlc_usage), "vlc");
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
#   define LINE_START      8
#   define PADDING_SPACES 25

static void print_section(const module_t *m, const module_config_t **sect,
                          bool color, bool desc)
{
    const module_config_t *item = *sect;

    if (item == NULL)
        return;
    *sect = NULL;

    printf(color ? RED"   %s:\n"GRAY : "   %s:\n",
           module_gettext(m, item->psz_text));
    if (desc && item->psz_longtext != NULL)
        printf(color ? MAGENTA"   %s\n"GRAY : "   %s\n",
               module_gettext(m, item->psz_longtext));
}

static void print_desc(const char *str, unsigned margin, bool color)
{
    unsigned width = ConsoleWidth() - margin;

    if (color)
        fputs(BLUE, stdout);

    const char *word = str;
    int wordlen = 0, wordwidth = 0;
    unsigned offset = 0;
    bool newline = true;

    while (str[0])
    {
        uint32_t cp;
        size_t charlen = vlc_towc(str, &cp);
        if (unlikely(charlen == (size_t)-1))
            break;

        int charwidth = wcwidth(cp);
        if (charwidth < 0)
            charwidth = 0;

        str += charlen;

        if (iswspace(cp))
        {
            if (!newline)
            {
                putchar(' '); /* insert space */
                charwidth = 1;
            }
            fwrite(word, 1, wordlen, stdout); /* write complete word */
            word = str;
            wordlen = 0;
            wordwidth = 0;
            newline = false;
        }
        else
        {
            wordlen += charlen;
            wordwidth += charwidth;
        }

        offset += charwidth;
        if (offset >= width)
        {
            if (newline)
            {   /* overflow (word wider than line) */
                fwrite(word, 1, wordlen - charlen, stdout);
                word = str - charlen;
                wordlen = charlen;
                wordwidth = charwidth;
            }
            printf("\n%*s", margin, ""); /* new line */
            offset = wordwidth;
            newline = true;
        }
    }

    if (!newline)
        putchar(' ');
    printf(color ? "%s\n"GRAY : "%s\n", word);
}

static int vlc_swidth(const char *str)
{
    for (int total = 0;;)
    {
        uint32_t cp;
        size_t charlen = vlc_towc(str, &cp);

        if (charlen == 0)
            return total;
        if (charlen == (size_t)-1)
            return -1;
        str += charlen;

        int w = wcwidth(cp);
        if (w == -1)
            return -1;
        total += w;
    }
}

static void print_item(const vlc_object_t *p_this, const module_t *m, const module_config_t *item,
                       const module_config_t **section, bool color, bool desc)
{
#ifndef _WIN32
# define OPTION_VALUE_SEP " "
#else
# define OPTION_VALUE_SEP "="
#endif
    const char *bra = OPTION_VALUE_SEP "<", *type, *ket = ">";
    const char *prefix = NULL, *suffix = NULL;
    char *typebuf = NULL;

    switch (CONFIG_CLASS(item->i_type))
    {
        case 0: // hint class
            switch (item->i_type)
            {
                case CONFIG_HINT_CATEGORY:
                case CONFIG_HINT_USAGE:
                    printf(color ? GREEN "\n %s\n" GRAY : "\n %s\n",
                           module_gettext(m, item->psz_text));

                    if (desc && item->psz_longtext != NULL)
                        printf(color ? CYAN " %s\n" GRAY : " %s\n",
                               module_gettext(m, item->psz_longtext));
                    break;

                case CONFIG_SECTION:
                    *section = item;
                    break;
            }
            return;

        case CONFIG_ITEM_STRING:
        {
            type = _("string");

            char **ppsz_values, **ppsz_texts;

            ssize_t i_count = config_GetPszChoices(VLC_OBJECT(p_this), item->psz_name, &ppsz_values, &ppsz_texts);

            if (i_count > 0)
            {
                size_t len = 0;

                for (unsigned i = 0; i < i_count; i++)
                    len += strlen(ppsz_values[i]) + 1;

                typebuf = malloc(len);
                if (typebuf == NULL)
                    goto end_string;

                bra = OPTION_VALUE_SEP "{";
                type = typebuf;
                ket = "}";

                *typebuf = 0;
                for (unsigned i = 0; i < i_count; i++)
                {
                    if (i > 0)
                        strcat(typebuf, ",");
                    strcat(typebuf, ppsz_values[i]);
                }

            end_string:
                for (unsigned i = 0; i < i_count; i++)
                {
                    free(ppsz_values[i]);
                    free(ppsz_texts[i]);
                }
                free(ppsz_values);
                free(ppsz_texts);
            }

            break;
        }
        case CONFIG_ITEM_INTEGER:
        {
            type = _("integer");

            int64_t *pi_values;
            char **ppsz_texts;

            ssize_t i_count = config_GetIntChoices(VLC_OBJECT(p_this), item->psz_name, &pi_values, &ppsz_texts);

            if (i_count > 0)
            {
                size_t len = 0;

                for (unsigned i = 0; i < i_count; i++)
                    len += strlen(ppsz_texts[i])
                           + 4 * sizeof (int64_t) + 5;

                typebuf = malloc(len);
                if (typebuf == NULL)
                    goto end_integer;

                bra = OPTION_VALUE_SEP "{";
                type = typebuf;
                ket = "}";

                *typebuf = 0;
                for (unsigned i = 0; i < item->list_count; i++)
                {
                    if (i != 0)
                        strcat(typebuf, ", ");
                    sprintf(typebuf + strlen(typebuf), "%"PRIi64" (%s)",
                            pi_values[i],
                            ppsz_texts[i]);
                }

            end_integer:
                for (unsigned i = 0; i < i_count; i++)
                    free(ppsz_texts[i]);
                free(pi_values);
                free(ppsz_texts);
            }
            else if (item->min.i != INT64_MIN || item->max.i != INT64_MAX )
            {
                if (asprintf(&typebuf, "%s [%"PRId64" .. %"PRId64"]",
                             type, item->min.i, item->max.i) >= 0)
                    type = typebuf;
                else
                    typebuf = NULL;
            }
            break;
        }
        case CONFIG_ITEM_FLOAT:
            type = _("float");
            if (item->min.f != FLT_MIN || item->max.f != FLT_MAX)
            {
                if (asprintf(&typebuf, "%s [%f .. %f]", type,
                             item->min.f, item->max.f) >= 0)
                    type = typebuf;
                else
                    typebuf = NULL;
            }
            break;

        case CONFIG_ITEM_BOOL:
            bra = type = ket = "";
            prefix = ", --no-";
            suffix = item->value.i ? _("(default enabled)")
                                   : _("(default disabled)");
            break;
       default:
            return;
    }

    print_section(m, section, color, desc);

    /* Add short option if any */
    char shortopt[4];
    if (item->i_short != '\0')
        sprintf(shortopt, "-%c,", item->i_short);
    else
        strcpy(shortopt, "   ");

    if (CONFIG_CLASS(item->i_type) == CONFIG_ITEM_BOOL)
        printf(color ? WHITE"  %s --%s"      "%s%s%s%s%s "GRAY
                     : "  %s --%s%s%s%s%s%s ", shortopt, item->psz_name,
               prefix, item->psz_name, bra, type, ket);
    else
        printf(color ? WHITE"  %s --%s"YELLOW"%s%s%s%s%s "GRAY
                     : "  %s --%s%s%s%s%s%s ", shortopt, item->psz_name,
               "", "",  /* XXX */      bra, type, ket);

    /* Wrap description */
    int offset = PADDING_SPACES - strlen(item->psz_name)
               - strlen(bra) - vlc_swidth(type) - strlen(ket) - 1;
    if (CONFIG_CLASS(item->i_type) == CONFIG_ITEM_BOOL)
        offset -= strlen(item->psz_name) + vlc_swidth(prefix);
    if (offset < 0)
    {
        putchar('\n');
        offset = PADDING_SPACES + LINE_START;
    }

    printf("%*s", offset, "");
    print_desc(module_gettext(m, item->psz_text),
               PADDING_SPACES + LINE_START, color);

    if (suffix != NULL)
    {
        printf("%*s", PADDING_SPACES + LINE_START, "");
        print_desc(suffix, PADDING_SPACES + LINE_START, color);
    }

    if (desc && (item->psz_longtext != NULL && item->psz_longtext[0]))
    {   /* Wrap long description */
        printf("%*s", LINE_START + 2, "");
        print_desc(module_gettext(m, item->psz_longtext),
                   LINE_START + 2, false);
    }

    free(typebuf);
}

static bool module_match(const module_t *m, const char *pattern, bool strict)
{
    if (pattern == NULL)
        return true;

    const char *objname = module_get_object(m);

    if (strict ? (strcmp(objname, pattern) == 0)
               : (strstr(objname, pattern) != NULL))
        return true;

    for (unsigned i = 0; i < m->i_shortcuts; i++)
    {
        const char *shortcut = m->pp_shortcuts[i];

        if (strict ? (strcmp(shortcut, pattern) == 0)
                   : (strstr(shortcut, pattern) != NULL))
            return true;
    }
    return false;
}

static bool plugin_show(const vlc_plugin_t *plugin, bool advanced)
{
    for (size_t i = 0; i < plugin->conf.size; i++)
    {
        const module_config_t *item = plugin->conf.items + i;

        if (!CONFIG_ITEM(item->i_type))
            continue;
        if (item->b_removed)
            continue;
        if ((!advanced) && item->b_advanced)
            continue;
        return true;
    }
    return false;
}

static void Usage (vlc_object_t *p_this, char const *psz_search)
{
    bool b_has_advanced = false;
    bool found = false;
    unsigned i_only_advanced = 0; /* Number of modules ignored because they
                               * only have advanced options */
    bool strict = false;
    if (psz_search != NULL && psz_search[0] == '=')
    {
        strict = true;
        psz_search++;
    }

    bool color = false;
#ifndef _WIN32
    if (isatty(STDOUT_FILENO))
        color = var_InheritBool(p_this, "color");
#endif

    const bool desc = var_InheritBool(p_this, "help-verbose");
    const bool advanced = var_InheritBool(p_this, "advanced");

    /* Enumerate the config for each module */
    for (const vlc_plugin_t *p = vlc_plugins; p != NULL; p = p->next)
    {
        const module_t *m = p->module;
        const module_config_t *section = NULL;
        const char *objname = module_get_object(m);

        if (p->conf.count == 0)
            continue; /* Ignore modules without config options */
        if (!module_match(m, psz_search, strict))
            continue;
        found = true;

        if (!plugin_show(p, advanced))
        {   /* Ignore plugins with only advanced config options if requested */
            i_only_advanced++;
            continue;
        }

        /* Print name of module */
        printf(color ? "\n " GREEN "%s" GRAY " (%s)\n" : "\n %s (%s)\n",
               module_gettext(m, m->psz_longname), objname);
        if (m->psz_help != NULL)
            printf(color ? CYAN" %s\n"GRAY : " %s\n",
                   module_gettext(m, m->psz_help));

        /* Print module options */
        for (size_t j = 0; j < p->conf.size; j++)
        {
            const module_config_t *item = p->conf.items + j;

            if (item->b_removed)
                continue; /* Skip removed options */
            if (item->b_advanced && !advanced)
            {   /* Skip advanced options unless requested */
                b_has_advanced = true;
                continue;
            }
            print_item(p_this, m, item, &section, color, desc);
        }
    }

    if( b_has_advanced )
        printf(color ? "\n" WHITE "%s" GRAY " %s\n"
                     : "\n%s %s\n", _( "Note:" ), _( "add --advanced to your "
                                     "command line to see advanced options."));
    if( i_only_advanced > 0 )
    {
        printf(color ? "\n" WHITE "%s" GRAY " " : "\n%s ", _( "Note:" ) );
        printf(vlc_ngettext("%u module was not displayed because it only has "
               "advanced options.\n", "%u modules were not displayed because "
               "they only have advanced options.\n", i_only_advanced),
               i_only_advanced);
    }
    else if (!found)
        printf(color ? "\n" WHITE "%s" GRAY "\n" : "\n%s\n",
               _("No matching module found. Use --list or "
                 "--list-verbose to list available modules."));
}

/*****************************************************************************
 * ListModules: list the available modules with their description
 *****************************************************************************
 * Print a list of all available modules (builtins and plugins) and a short
 * description for each one.
 *****************************************************************************/
static void ListModules (vlc_object_t *p_this, bool b_verbose)
{
    bool color = false;

    ShowConsole();
#ifndef _WIN32
    if (isatty(STDOUT_FILENO))
        color = var_InheritBool(p_this, "color");
#else
    (void) p_this;
#endif

    /* List all modules */
    size_t count;
    module_t **list = module_list_get (&count);

    /* Enumerate each module */
    for (size_t j = 0; j < count; j++)
    {
        module_t *p_parser = list[j];
        const char *objname = module_get_object (p_parser);
        printf(color ? GREEN"  %-22s "WHITE"%s\n"GRAY : "  %-22s %s\n",
               objname, module_gettext(p_parser, p_parser->psz_longname));

        if( b_verbose )
        {
            const char *const *pp_shortcuts = p_parser->pp_shortcuts;
            for( unsigned i = 0; i < p_parser->i_shortcuts; i++ )
                if( strcmp( pp_shortcuts[i], objname ) )
                    printf(color ? CYAN"   s %s\n"GRAY : "   s %s\n",
                           pp_shortcuts[i]);
            if (p_parser->psz_capability != NULL)
                printf(color ? MAGENTA"   c %s (%d)\n"GRAY : "   c %s (%d)\n",
                       p_parser->psz_capability, p_parser->i_score);
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
    printf(_("VLC version %s (%s)\n"), VERSION_MESSAGE, psz_vlc_changeset);
    printf(_("Compiled by %s on %s (%s)\n"), VLC_CompileBy(),
           VLC_CompileHost(), __DATE__" "__TIME__ );
    printf(_("Compiler: %s\n"), VLC_Compiler());
    fputs(LICENSE_MSG, stdout);
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
    if( getenv( "PWD" ) ) return; /* Cygwin shell or Wine */

    if( !AllocConsole() ) return;

    /* Use the ANSI code page (e.g. Windows-1252) as expected by the LibVLC
     * Unicode/locale subsystem. By default, we have the obsolecent OEM code
     * page (e.g. CP437 or CP850). */
    SetConsoleOutputCP (GetACP ());
    SetConsoleTitle (TEXT("VLC media player version ") TEXT(PACKAGE_VERSION));

    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );

    if( freopen( "vlc-help.txt", "wt", stdout ) != NULL )
    {
        fputs( "\xEF\xBB\xBF", stdout );
        fprintf( stderr, _("\nDumped content to vlc-help.txt file.\n") );
    }
    else
        freopen( "CONOUT$", "w", stdout );
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
