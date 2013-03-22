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

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

/*****************************************************************************
 * intf_sys_t: description and status of log interface
 *****************************************************************************/
struct intf_sys_t
{
    FILE *p_file;
    const char *footer;
    char *ident;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static void TextPrint(void *, int, const vlc_log_t *, const char *, va_list);
static void HtmlPrint(void *, int, const vlc_log_t *, const char *, va_list);
#ifdef HAVE_SYSLOG_H
static void SyslogPrint(void *, int, const vlc_log_t *, const char *, va_list);
#endif
#ifdef __ANDROID__
static void AndroidPrint(void *, int, const vlc_log_t *, const char *, va_list);
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static const char *const mode_list[] = { "text", "html"
#ifdef HAVE_SYSLOG_H
,"syslog"
#endif
#ifdef __ANDROID__
,"android"
#endif
};
static const char *const mode_list_text[] = { N_("Text"), "HTML"
#ifdef HAVE_SYSLOG_H
, "syslog"
#endif
#ifdef __ANDROID__
,"android"
#endif
};

#define LOGMODE_TEXT N_("Log format")
#define LOGMODE_LONGTEXT N_("Specify the logging format.")

#ifdef HAVE_SYSLOG_H
#define SYSLOG_IDENT_TEXT N_("Syslog ident")
#define SYSLOG_IDENT_LONGTEXT N_("Set the ident that VLC would use when " \
  "logging to syslog.")

#define SYSLOG_FACILITY_TEXT N_("Syslog facility")
#define SYSLOG_FACILITY_LONGTEXT N_("Select the syslog facility where logs " \
  "will be forwarded.")

/* First in list is the default facility used. */
#define DEFINE_SYSLOG_FACILITY \
  DEF( "user",   LOG_USER ), \
  DEF( "daemon", LOG_DAEMON ), \
  DEF( "local0", LOG_LOCAL0 ), \
  DEF( "local1", LOG_LOCAL1 ), \
  DEF( "local2", LOG_LOCAL2 ), \
  DEF( "local3", LOG_LOCAL3 ), \
  DEF( "local4", LOG_LOCAL4 ), \
  DEF( "local5", LOG_LOCAL5 ), \
  DEF( "local6", LOG_LOCAL6 ), \
  DEF( "local7", LOG_LOCAL7 )

#define DEF( a, b ) a
static const char *const fac_name[]   = { DEFINE_SYSLOG_FACILITY };
#undef  DEF
#define DEF( a, b ) b
static const int         fac_number[] = { DEFINE_SYSLOG_FACILITY };
#undef  DEF
enum                   { fac_entries = sizeof(fac_name)/sizeof(fac_name[0]) };
#undef  DEFINE_SYSLOG_FACILITY

#endif

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
#ifdef HAVE_SYSLOG_H
    add_string( "syslog-ident", "vlc", SYSLOG_IDENT_TEXT,
                SYSLOG_IDENT_LONGTEXT, true )
    add_string( "syslog-facility", fac_name[0], SYSLOG_FACILITY_TEXT,
                SYSLOG_FACILITY_LONGTEXT, true )
        change_string_list( fac_name, fac_name )
#endif
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
            header = HTML_HEADER;
            cb = HtmlPrint;
        }
#ifdef HAVE_SYSLOG_H
        else if( !strcmp( mode, "syslog" ) )
            cb = SyslogPrint;
#endif
#ifdef __ANDROID__
        else if( !strcmp( mode, "android" ) )
            cb = AndroidPrint;
#endif
        else if( strcmp( mode, "text" ) )
            msg_Warn( p_intf, "invalid log mode `%s', using `text'", mode );
        free( mode );
    }

#ifdef HAVE_SYSLOG_H
    if( cb == SyslogPrint )
    {
        int i_facility;
        char *psz_facility = var_InheritString( p_intf, "syslog-facility" );
        if( psz_facility )
        {
            bool b_valid = 0;
            for( size_t i = 0; i < fac_entries; ++i )
            {
                if( !strcmp( psz_facility, fac_name[i] ) )
                {
                    i_facility = fac_number[i];
                    b_valid = 1;
                    break;
                }
            }
            if( !b_valid )
            {
                msg_Warn( p_intf, "invalid syslog facility `%s', using `%s'",
                          psz_facility, fac_name[0] );
                i_facility = fac_number[0];
            }
            free( psz_facility );
        }
        else
        {
            msg_Warn( p_intf, "no syslog facility specified, using `%s'",
                      fac_name[0] );
            i_facility = fac_number[0];
        }

        char *psz_syslog_ident = var_InheritString( p_intf, "syslog-ident" );
        if (unlikely(psz_syslog_ident == NULL))
        {
            free( p_sys );
            return VLC_ENOMEM;
        }

        p_sys->ident = psz_syslog_ident;
        openlog( p_sys->ident, LOG_PID|LOG_NDELAY, i_facility );

        p_sys->p_file = NULL;
    }
    else
#endif
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
        free( psz_file );
        if( p_sys->p_file == NULL )
        {
            msg_Err( p_intf, "error opening logfile `%s': %m", filename );
            free( p_sys );
            return VLC_EGENERIC;
        }
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
#ifdef HAVE_SYSLOG_H
    if( p_sys->p_file == NULL )
    {
        closelog();
        free( p_sys->ident );
    }
    else
#endif
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

#ifdef HAVE_SYSLOG_H
static void SyslogPrint( void *opaque, int type, const vlc_log_t *item,
                         const char *fmt, va_list ap )
{
    static const int i_prio[4] = { LOG_INFO, LOG_ERR, LOG_WARNING, LOG_DEBUG };

    intf_thread_t *p_intf = opaque;
    char *str;
    int i_priority = i_prio[type];

    if( IgnoreMessage( p_intf, type )
     || unlikely(vasprintf( &str, fmt, ap ) == -1) )
        return;

    int canc = vlc_savecancel();
    if( item->psz_header != NULL )
        syslog( i_priority, "[%s] %s%s: %s", item->psz_header,
                item->psz_module, ppsz_type[type], str );
    else
        syslog( i_priority, "%s%s: %s",
                item->psz_module, ppsz_type[type], str );
    vlc_restorecancel( canc );
    free( str );
}
#endif

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
