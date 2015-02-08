/*****************************************************************************
 * logger.c : file logging plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_fs.h>
#include <vlc_charset.h>

#include <stdarg.h>
#include <assert.h>
#include <errno.h>

#ifdef __ANDROID__
# include <android/log.h>
#endif

#define LOG_FILE_TEXT "vlc-log.txt"
#define LOG_FILE_HTML "vlc-log.html"

#define TEXT_HEADER "\xEF\xBB\xBF-- logger module started --\n"
#define TEXT_FOOTER "-- logger module stopped --\n"

#define HTML_HEADER \
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n" \
    "  \"http://www.w3.org/TR/html4/strict.dtd\">\n" \
    "<html>\n" \
    "  <head>\n" \
    "    <title>vlc log</title>\n" \
    "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n" \
    "  </head>\n" \
    "  <body style=\"background-color: #000000; color: #aaaaaa;\">\n" \
    "    <pre>\n" \
    "      <strong>-- logger module started --</strong>\n"
#define HTML_FOOTER \
    "      <strong>-- logger module stopped --</strong>\n" \
    "    </pre>\n" \
    "  </body>\n" \
    "</html>\n"

/*****************************************************************************
 * intf_sys_t: description and status of log interface
 *****************************************************************************/
struct intf_sys_t
{
    FILE *p_file;
    const char *footer;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static void TextPrint(void *, int, const vlc_log_t *, const char *, va_list);
static void HtmlPrint(void *, int, const vlc_log_t *, const char *, va_list);
#ifdef __ANDROID__
static void AndroidPrint(void *, int, const vlc_log_t *, const char *, va_list);
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static const char *const mode_list[] = { "text", "html"
#ifdef __ANDROID__
,"android"
#endif
};
static const char *const mode_list_text[] = { N_("Text"), "HTML"
#ifdef __ANDROID__
,"android"
#endif
};

#define LOGMODE_TEXT N_("Log format")
#define LOGMODE_LONGTEXT N_("Specify the logging format.")

#define LOGVERBOSE_TEXT N_("Verbosity")
#define LOGVERBOSE_LONGTEXT N_("Select the verbosity to use for log or -1 to " \
"use the same verbosity given by --verbose.")

vlc_module_begin ()
    set_shortname( N_( "Logging" ) )
    set_description( N_("File logging") )

    set_category( CAT_ADVANCED )
    set_subcategory( SUBCAT_ADVANCED_MISC )

    add_savefile( "logfile", NULL,
             N_("Log filename"), N_("Specify the log filename."), false )
    add_string( "logmode", "text", LOGMODE_TEXT, LOGMODE_LONGTEXT,
                false )
        change_string_list( mode_list, mode_list_text )
    add_integer( "log-verbose", -1, LOGVERBOSE_TEXT, LOGVERBOSE_LONGTEXT,
           false )
    
    add_obsolete_string( "rrd-file" )

    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys;

    CONSOLE_INTRO_MSG;
    msg_Info( p_intf, "using logger." );

    /* Allocate instance and initialize some members */
    p_sys = p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->p_file = NULL;
    vlc_log_cb cb = TextPrint;
    const char *filename = LOG_FILE_TEXT, *header = TEXT_HEADER;
    p_sys->footer = TEXT_FOOTER;

    char *mode = var_InheritString( p_intf, "logmode" );
    if( mode != NULL )
    {
        if( !strcmp( mode, "html" ) )
        {
            p_sys->footer = HTML_FOOTER;
            filename = LOG_FILE_HTML;
            header = HTML_HEADER;
            cb = HtmlPrint;
        }
#ifdef __ANDROID__
        else if( !strcmp( mode, "android" ) )
            cb = AndroidPrint;
#endif
        else if( strcmp( mode, "text" ) )
            msg_Warn( p_intf, "invalid log mode `%s', using `text'", mode );
        free( mode );
    }

#ifdef __ANDROID__
    if( cb == AndroidPrint )
    {
        /* nothing to do */
    }
    else
#endif
    {
        char *psz_file = var_InheritString( p_intf, "logfile" );
        if( !psz_file )
        {
#ifdef __APPLE__
# define LOG_DIR "Library/Logs"
            char *home = config_GetUserDir(VLC_HOME_DIR);
            if( home == NULL
             || asprintf( &psz_file, "%s/"LOG_DIR"/%s", home,
                          filename ) == -1 )
                psz_file = NULL;
            free(home);
            filename = psz_file;
#endif
            msg_Warn( p_intf, "no log filename provided, using `%s'",
                      filename );
        }
        else
            filename = psz_file;

        /* Open the log file and remove any buffering for the stream */
        msg_Dbg( p_intf, "opening logfile `%s'", filename );
        p_sys->p_file = vlc_fopen( filename, "at" );
        if( p_sys->p_file == NULL )
        {
            msg_Err( p_intf, "error opening logfile `%s': %s", filename,
                     vlc_strerror_c(errno) );
            free( psz_file );
            free( p_sys );
            return VLC_EGENERIC;
        }
        free( psz_file );
        setvbuf( p_sys->p_file, NULL, _IONBF, 0 );
        fputs( header, p_sys->p_file );
    }

    vlc_LogSet( p_intf->p_libvlc, cb, p_intf );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    /* Flush the queue and unsubscribe from the message queue */
    vlc_LogSet( p_intf->p_libvlc, NULL, NULL );

    /* Close the log file */
    if( p_sys->p_file )
    {
        fputs( p_sys->footer, p_sys->p_file );
        fclose( p_sys->p_file );
    }

    /* Destroy structure */
    free( p_sys );
}

static bool IgnoreMessage( intf_thread_t *p_intf, int type )
{
    /* TODO: cache value... */
    int verbosity = var_InheritInteger( p_intf, "log-verbose" );
    if (verbosity == -1)
        verbosity = var_InheritInteger( p_intf, "verbose" );

    return verbosity < 0 || verbosity < (type - VLC_MSG_ERR);
}

/*
 * Logging callbacks
 */

static const char ppsz_type[4][9] = {
    "",
    " error",
    " warning",
    " debug",
};

#ifdef __ANDROID__
static const android_LogPriority prioritytype[4] = {
    ANDROID_LOG_INFO,
    ANDROID_LOG_ERROR,
    ANDROID_LOG_WARN,
    ANDROID_LOG_DEBUG
};

static void AndroidPrint( void *opaque, int type, const vlc_log_t *item,
                       const char *fmt, va_list ap )
{
    (void)item;
    intf_thread_t *p_intf = opaque;

    if( IgnoreMessage( p_intf, type ) )
        return;

    int canc = vlc_savecancel();
    __android_log_vprint(prioritytype[type], "VLC", fmt, ap);
    vlc_restorecancel( canc );
}
#endif

static void TextPrint( void *opaque, int type, const vlc_log_t *item,
                       const char *fmt, va_list ap )
{
    intf_thread_t *p_intf = opaque;
    FILE *stream = p_intf->p_sys->p_file;

    if( IgnoreMessage( p_intf, type ) )
        return;

    int canc = vlc_savecancel();
    flockfile( stream );
    fprintf( stream, "%s%s: ", item->psz_module, ppsz_type[type] );
    vfprintf( stream, fmt, ap );
    putc_unlocked( '\n', stream );
    funlockfile( stream );
    vlc_restorecancel( canc );
}

static void HtmlPrint( void *opaque, int type, const vlc_log_t *item,
                       const char *fmt, va_list ap )
{
    static const unsigned color[4] = {
        0xffffff, 0xff6666, 0xffff66, 0xaaaaaa,
    };

    intf_thread_t *p_intf = opaque;
    FILE *stream = p_intf->p_sys->p_file;

    if( IgnoreMessage( p_intf, type ) )
        return;

    int canc = vlc_savecancel();
    flockfile( stream );
    fprintf( stream, "%s%s: <span style=\"color: #%06x\">",
             item->psz_module, ppsz_type[type], color[type] );
    /* FIXME: encode special ASCII characters */
    vfprintf( stream, fmt, ap );
    fputs( "</span>\n", stream );
    funlockfile( stream );
    vlc_restorecancel( canc );
}
