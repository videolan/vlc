/*****************************************************************************
 * console.c: console logger
 *****************************************************************************
 * Copyright © 1998-2005 VLC authors and VideoLAN
 * Copyright © 2006-2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>         /* isatty(), STDERR_FILNO */

#include <vlc_common.h>
#include <vlc_plugin.h>

static const int ptr_width = 2 * /* hex digits */ sizeof (uintptr_t);
static const char msg_type[4][9] = { "", " error", " warning", " debug" };
static char verbosities[VLC_MSG_DBG];

#ifdef __OS2__
#include <vlc_charset.h>

static int OS2ConsoleOutput(FILE *stream, const char *format, va_list ap)
{
    char *msg;
    char *os2msg;

    if (vasprintf(&msg, format, ap) == -1 )
        return -1;

    if ((os2msg = ToLocale(msg)) == NULL)
    {
        free(msg);

        return -1;
    }

    fputs(os2msg, stream);

    LocaleFree(os2msg);
    free(msg);

    return 0;
}
#endif

#ifndef _WIN32
# define COL(x,y) "\033[" #x ";" #y "m"
# define RED      COL(31,1)
# define GREEN    COL(32,1)
# define YELLOW   COL(0,33)
# define WHITE    COL(0,1)
# define GRAY     "\033[0m"
static const char msg_color[4][8] = { WHITE, RED, YELLOW, GRAY };

static void LogConsoleColor(void *opaque, int type, const vlc_log_t *meta,
                            const char *format, va_list ap)
{
    FILE *stream = stderr;
    int verbose = (char *)opaque - verbosities;

    if (verbose < type)
        return;

    flockfile(stream);
    fprintf(stream, "["GREEN"%0*"PRIxPTR GRAY"] ", ptr_width,
            meta->i_object_id);
    if (meta->psz_header != NULL)
        fprintf(stream, "[%s] ", meta->psz_header);
    fprintf(stream, "%s %s%s: %s", meta->psz_module, meta->psz_object_type,
            msg_type[type], msg_color[type]);
#ifdef __OS2__
    if (OS2ConsoleOutput(stream, format, ap) == -1)
#endif
    vfprintf(stream, format, ap);
    fputs(GRAY"\n", stream);
    funlockfile(stream);
}

static const struct vlc_logger_operations color_ops =
{
    LogConsoleColor,
    NULL
};
#endif /* !_WIN32 */

static void LogConsoleGray(void *opaque, int type, const vlc_log_t *meta,
                           const char *format, va_list ap)
{
    FILE *stream = stderr;
    int verbose = (char *)opaque - verbosities;

    if (verbose < type)
        return;

    flockfile(stream);
    fprintf(stream, "[%0*"PRIxPTR"] ", ptr_width, meta->i_object_id);
    if (meta->psz_header != NULL)
        fprintf(stream, "[%s] ", meta->psz_header);
    fprintf(stream, "%s %s%s: ", meta->psz_module, meta->psz_object_type,
            msg_type[type]);
#ifdef __OS2__
    if (OS2ConsoleOutput(stream, format, ap) == -1)
#endif
    vfprintf(stream, format, ap);
    putc_unlocked('\n', stream);
    funlockfile(stream);
}

static const struct vlc_logger_operations gray_ops =
{
    LogConsoleGray,
    NULL
};

static const struct vlc_logger_operations *Open(vlc_object_t *obj,
                                                void **restrict sysp)
{
    int verbosity = -1;

    if (!var_InheritBool(obj, "quiet"))
    {
        const char *str = getenv("VLC_VERBOSE");
        if (str != NULL)
           verbosity = atoi(str);
        else
           verbosity = var_InheritInteger(obj, "verbose");
    }

    if (verbosity < 0)
        return NULL;

    verbosity += VLC_MSG_ERR;
    if (verbosity > VLC_MSG_DBG)
        verbosity = VLC_MSG_DBG;

    *sysp = verbosities + verbosity;

#if defined (HAVE_ISATTY) && !defined (_WIN32)
    if (isatty(STDERR_FILENO) && var_InheritBool(obj, "color"))
        return &color_ops;
#endif
    return &gray_ops;
}

#define QUIET_TEXT N_("Be quiet")
#define QUIET_LONGTEXT N_("Turn off all messages on the console.")

vlc_module_begin()
    set_shortname(N_("Console log"))
    set_description(N_("Console logger"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_MISC)
    set_capability("logger", 10)
    set_callback(Open)

    add_bool("quiet", false, QUIET_TEXT, QUIET_LONGTEXT)
        change_short('q')
        change_volatile()
vlc_module_end ()
