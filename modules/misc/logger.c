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

#define MODE_TEXT 0
#define MODE_HTML 1
#define MODE_SYSLOG 2

#ifdef __APPLE__
#define LOG_DIR "Library/Logs/"
#endif

#define LOG_FILE_TEXT "vlc-log.txt"
#define LOG_FILE_HTML "vlc-log.html"

#define TEXT_HEADER "-- logger module started --\n"
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
    "      <b>-- logger module started --</b>\n"
#define HTML_FOOTER \
    "      <b>-- logger module stopped --</b>\n" \
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
    msg_subscription_t *p_sub;
    FILE *p_file;
    int   i_mode;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static void Overflow (void *, int, const msg_item_t *, const char *, va_list);
static void TextPrint (FILE *, int, const msg_item_t *, const char *);
static void HtmlPrint (FILE *, int, const msg_item_t *, const char *);
#ifdef HAVE_SYSLOG_H
static void SyslogPrint (int, const msg_item_t *, const char *);
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static const char *const mode_list[] = { "text", "html"
#ifdef HAVE_SYSLOG_H
,"syslog"
#endif
};
static const char *const mode_list_text[] = { N_("Text"), "HTML"
#ifdef HAVE_SYSLOG_H
, "syslog"
#endif
};

#define LOGMODE_TEXT N_("Log format")
#ifndef HAVE_SYSLOG_H
#define LOGMODE_LONGTEXT N_("Specify the log format. Available choices are " \
  "\"text\" (default) and \"html\".")
#else

#define LOGMODE_LONGTEXT N_("Specify the log format. Available choices are " \
  "\"text\" (default), \"html\", and \"syslog\" (special mode to send to " \
  "syslog instead of file.")

#define SYSLOG_FACILITY_TEXT N_("Syslog facility")
#define SYSLOG_FACILITY_LONGTEXT N_("Select the syslog facility where logs " \
  "will be forwarded. Available choices are \"user\" (default), \"daemon\", " \
  "and \"local0\" through \"local7\".")

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
        change_string_list( mode_list, mode_list_text, 0 )
#ifdef HAVE_SYSLOG_H
    add_string( "syslog-facility", fac_name[0], SYSLOG_FACILITY_TEXT,
                SYSLOG_FACILITY_LONGTEXT, true )
        change_string_list( fac_name, fac_name, 0 )
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
    char *psz_mode;

    CONSOLE_INTRO_MSG;
    msg_Info( p_intf, "using logger." );

    /* Allocate instance and initialize some members */
    p_sys = p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->i_mode = MODE_TEXT;
    psz_mode = var_InheritString( p_intf, "logmode" );
    if( psz_mode )
    {
        if( !strcmp( psz_mode, "text" ) )
            ;
        else if( !strcmp( psz_mode, "html" ) )
        {
            p_sys->i_mode = MODE_HTML;
        }
#ifdef HAVE_SYSLOG_H
        else if( !strcmp( psz_mode, "syslog" ) )
        {
            p_sys->i_mode = MODE_SYSLOG;
        }
#endif
        else
        {
            msg_Warn( p_intf, "invalid log mode `%s', using `text'", psz_mode );
            p_sys->i_mode = MODE_TEXT;
        }
        free( psz_mode );
    }
    else
    {
        msg_Warn( p_intf, "no log mode specified, using `text'" );
    }

    if( p_sys->i_mode != MODE_SYSLOG )
    {
        char *psz_file = var_InheritString( p_intf, "logfile" );
        if( !psz_file )
        {
#ifdef __APPLE__
            char *home = config_GetUserDir(VLC_DOCUMENTS_DIR);
            if( home == NULL
             || asprintf( &psz_file, "%s/"LOG_DIR"/%s", home,
                (p_sys->i_mode == MODE_HTML) ? LOG_FILE_HTML
                                             : LOG_FILE_TEXT ) == -1 )
                psz_file = NULL;
            free(home);
#else
            switch( p_sys->i_mode )
            {
            case MODE_HTML:
                psz_file = strdup( LOG_FILE_HTML );
                break;
            case MODE_TEXT:
            default:
                psz_file = strdup( LOG_FILE_TEXT );
                break;
            }
#endif
            msg_Warn( p_intf, "no log filename provided, using `%s'",
                               psz_file );
        }

        /* Open the log file and remove any buffering for the stream */
        msg_Dbg( p_intf, "opening logfile `%s'", psz_file );
        p_sys->p_file = vlc_fopen( psz_file, "at" );
        if( p_sys->p_file == NULL )
        {
            msg_Err( p_intf, "error opening logfile `%s'", psz_file );
            free( p_sys );
            free( psz_file );
            return -1;
        }
        setvbuf( p_sys->p_file, NULL, _IONBF, 0 );

        free( psz_file );

        switch( p_sys->i_mode )
        {
        case MODE_HTML:
            fputs( HTML_HEADER, p_sys->p_file );
            break;
        case MODE_TEXT:
        default:
            fputs( TEXT_HEADER, p_sys->p_file );
            break;
        }

    }
    else
    {
        p_sys->p_file = NULL;
#ifdef HAVE_SYSLOG_H
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

        openlog( "vlc", LOG_PID|LOG_NDELAY, i_facility );
#endif
    }

    p_sys->p_sub = vlc_Subscribe( Overflow, p_intf );

    return 0;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    /* Flush the queue and unsubscribe from the message queue */
    /* FIXME: flush */
    vlc_Unsubscribe( p_sys->p_sub );

    switch( p_sys->i_mode )
    {
    case MODE_HTML:
        fputs( HTML_FOOTER, p_sys->p_file );
        break;
#ifdef HAVE_SYSLOG_H
    case MODE_SYSLOG:
        closelog();
        break;
#endif
    case MODE_TEXT:
    default:
        fputs( TEXT_FOOTER, p_sys->p_file );
        break;
    }

    /* Close the log file */
    if( p_sys->p_file )
        fclose( p_sys->p_file );

    /* Destroy structure */
    free( p_sys );
}

/**
 * Log a message
 */
static void Overflow (void *opaque, int type, const msg_item_t *p_item,
                      const char *format, va_list ap)
{
    intf_thread_t *p_intf = opaque;
    intf_sys_t *p_sys = p_intf->p_sys;
    char *str;

    /* TODO: cache value... */
    int verbosity = var_InheritInteger( p_intf, "log-verbose" );
    if (verbosity == -1)
        verbosity = var_InheritInteger( p_intf, "verbose" );

    if( verbosity < 0 || verbosity < (type - VLC_MSG_ERR)
     || vasprintf( &str, format, ap) == -1 )
        return;

    int canc = vlc_savecancel();

    switch( p_sys->i_mode )
    {
        case MODE_HTML:
            HtmlPrint( p_sys->p_file, type, p_item, str );
            break;
#ifdef HAVE_SYSLOG_H
        case MODE_SYSLOG:
            SyslogPrint( type, p_item, str );
            break;
#endif
        case MODE_TEXT:
        default:
            TextPrint( p_sys->p_file, type, p_item, str );
            break;
    }

    vlc_restorecancel( canc );
    free( str );
}

static const char ppsz_type[4][11] = {
    ": ",
    " error: ",
    " warning: ",
    " debug: ",
};

static void TextPrint( FILE *stream, int type, const msg_item_t *item,
                       const char *str )
{
    utf8_fprintf( stream, "%s%s%s\n", item->psz_module,
                  ppsz_type[type], str );
}

#ifdef HAVE_SYSLOG_H
static void SyslogPrint( int type, const msg_item_t *item, const char *str )
{
    static const int i_prio[4] = { LOG_INFO, LOG_ERR, LOG_WARNING, LOG_DEBUG };
    int i_priority = i_prio[type];

    if( item->psz_header != NULL )
        syslog( i_priority, "[%s] %s%s%s", item->psz_header,
                item->psz_module, ppsz_type[type], str );
    else
        syslog( i_priority, "%s%s%s",
                item->psz_module, ppsz_type[type], str );
 
}
#endif

static void HtmlPrint( FILE *stream, int type, const msg_item_t *item,
                       const char *str )
{
    static const char ppsz_color[4][30] = {
        "<span style=\"color: #ffffff\">",
        "<span style=\"color: #ff6666\">",
        "<span style=\"color: #ffff66\">",
        "<span style=\"color: #aaaaaa\">",
    };

    fprintf( stream, "%s%s%s%s</span>\n", item->psz_module,
             ppsz_type[type], ppsz_color[type], str );
}
