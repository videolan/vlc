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
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_charset.h>

#include <assert.h>

#ifdef UNDER_CE
#   define _IONBF 0x0004
#endif

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

#if HAVE_SYSLOG_H
#include <syslog.h>
#endif

struct msg_cb_data_t
{
    intf_thread_t *p_intf;
    FILE *p_file;
    int   i_mode;
};

/*****************************************************************************
 * intf_sys_t: description and status of log interface
 *****************************************************************************/
struct intf_sys_t
{
    struct
    {
        FILE *stream;
        vlc_thread_t thread;
    } rrd;

    msg_subscription_t *p_sub;
    msg_cb_data_t msg;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

static void Overflow (msg_cb_data_t *p_sys, msg_item_t *p_item, unsigned overruns);
static void TextPrint         ( const msg_item_t *, FILE * );
static void HtmlPrint         ( const msg_item_t *, FILE * );
#ifdef HAVE_SYSLOG_H
static void SyslogPrint       ( const msg_item_t *);
#endif

static void *DoRRD( void * );

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
#ifdef HAVE_SYSLOG_H
#define LOGMODE_LONGTEXT N_("Specify the log format. Available choices are " \
  "\"text\" (default), \"html\", and \"syslog\" (special mode to send to " \
  "syslog instead of file.")
#else
#define LOGMODE_LONGTEXT N_("Specify the log format. Available choices are " \
  "\"text\" (default) and \"html\".")
#endif

vlc_module_begin ()
    set_shortname( N_( "Logging" ) )
    set_description( N_("File logging") )

    set_category( CAT_ADVANCED )
    set_subcategory( SUBCAT_ADVANCED_MISC )

    add_file( "logfile", NULL, NULL,
             N_("Log filename"), N_("Specify the log filename."), false )
    add_string( "logmode", "text", NULL, LOGMODE_TEXT, LOGMODE_LONGTEXT,
                false )
        change_string_list( mode_list, mode_list_text, 0 )

    add_file( "rrd-file", NULL, NULL, N_("RRD output file") ,
                    N_("Output data for RRDTool in this file." ), true )

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
    char *psz_mode, *psz_rrd_file;

    CONSOLE_INTRO_MSG;
    msg_Info( p_intf, "using logger..." );

    /* Allocate instance and initialize some members */
    p_sys = p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    p_sys->msg.p_intf = p_intf;
    p_sys->msg.i_mode = MODE_TEXT;
    psz_mode = var_CreateGetString( p_intf, "logmode" );
    if( psz_mode )
    {
        if( !strcmp( psz_mode, "text" ) )
            ;
        else if( !strcmp( psz_mode, "html" ) )
        {
            p_sys->msg.i_mode = MODE_HTML;
        }
#ifdef HAVE_SYSLOG_H
        else if( !strcmp( psz_mode, "syslog" ) )
        {
            p_sys->msg.i_mode = MODE_SYSLOG;
        }
#endif
        else
        {
            msg_Warn( p_intf, "invalid log mode `%s', using `text'", psz_mode );
            p_sys->msg.i_mode = MODE_TEXT;
        }
        free( psz_mode );
    }
    else
    {
        msg_Warn( p_intf, "no log mode specified, using `text'" );
    }

    if( p_sys->msg.i_mode != MODE_SYSLOG )
    {
        char *psz_file = config_GetPsz( p_intf, "logfile" );
        if( !psz_file )
        {
#ifdef __APPLE__
            if( asprintf( &psz_file, "%s/"LOG_DIR"/%s", config_GetHomeDir(),
                (p_sys->msg.i_mode == MODE_HTML) ? LOG_FILE_HTML
                                             : LOG_FILE_TEXT ) == -1 )
                psz_file = NULL;
#else
            switch( p_sys->msg.i_mode )
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
        p_sys->msg.p_file = utf8_fopen( psz_file, "at" );
        if( p_sys->msg.p_file == NULL )
        {
            msg_Err( p_intf, "error opening logfile `%s'", psz_file );
            free( p_sys );
            free( psz_file );
            return -1;
        }
        setvbuf( p_sys->msg.p_file, NULL, _IONBF, 0 );

        free( psz_file );

        switch( p_sys->msg.i_mode )
        {
        case MODE_HTML:
            fputs( HTML_HEADER, p_sys->msg.p_file );
            break;
        case MODE_TEXT:
        default:
            fputs( TEXT_HEADER, p_sys->msg.p_file );
            break;
        }

    }
    else
    {
        p_sys->msg.p_file = NULL;
#ifdef HAVE_SYSLOG_H
        openlog( "vlc", LOG_PID|LOG_NDELAY, LOG_DAEMON );
#endif
    }

    psz_rrd_file = config_GetPsz( p_intf, "rrd-file" );
    if( psz_rrd_file && *psz_rrd_file )
    {
        FILE *rrd = utf8_fopen( psz_rrd_file, "w" );
        if (rrd != NULL)
        {
            setvbuf (rrd, NULL, _IOLBF, BUFSIZ);
            if (!vlc_clone (&p_sys->rrd.thread, DoRRD, p_intf,
                            VLC_THREAD_PRIORITY_LOW))
                p_sys->rrd.stream = rrd;
            else
            {
                fclose (rrd);
                p_sys->rrd.stream = NULL;
            }
        }
    }
    else
        p_sys->rrd.stream = NULL;
    free( psz_rrd_file );

    p_sys->p_sub = msg_Subscribe( p_intf->p_libvlc, Overflow, &p_sys->msg );

    return 0;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    if (p_sys->rrd.stream)
    {
        vlc_cancel (p_sys->rrd.thread);
        vlc_join (p_sys->rrd.thread, NULL);
    }

    /* Flush the queue and unsubscribe from the message queue */
    /* FIXME: flush */
    msg_Unsubscribe( p_sys->p_sub );

    switch( p_sys->msg.i_mode )
    {
    case MODE_HTML:
        fputs( HTML_FOOTER, p_sys->msg.p_file );
        break;
#ifdef HAVE_SYSLOG_H
    case MODE_SYSLOG:
        closelog();
        break;
#endif
    case MODE_TEXT:
    default:
        fputs( TEXT_FOOTER, p_sys->msg.p_file );
        break;
    }

    /* Close the log file */
    if( p_sys->msg.p_file )
        fclose( p_sys->msg.p_file );

    /* Destroy structure */
    free( p_sys );
}

/**
 * Log a message
 */
static void Overflow (msg_cb_data_t *p_sys, msg_item_t *p_item, unsigned overruns)
{
    int verbosity = var_CreateGetInteger( p_sys->p_intf, "verbose" );
    int priority = 0;

    switch( p_item->i_type )
    {
        case VLC_MSG_WARN: priority = 1; break;
        case VLC_MSG_DBG:  priority = 2; break;
    }
    if (verbosity < priority)
        return;

    switch( p_sys->i_mode )
    {
        case MODE_HTML:
            HtmlPrint( p_item, p_sys->p_file );
            break;
#ifdef HAVE_SYSLOG_H
        case MODE_SYSLOG:
            SyslogPrint( p_item );
            break;
#endif
        case MODE_TEXT:
        default:
            TextPrint( p_item, p_sys->p_file );
            break;
    }
}

static const char ppsz_type[4][11] = {
    ": ",
    " error: ",
    " warning: ",
    " debug: ",
};

static void TextPrint( const msg_item_t *p_msg, FILE *p_file )
{
    fprintf( p_file, "%s%s%s\n", p_msg->psz_module, ppsz_type[p_msg->i_type],
             p_msg->psz_msg );
}

#ifdef HAVE_SYSLOG_H
static void SyslogPrint( const msg_item_t *p_msg )
{
    static const int i_prio[4] = { LOG_INFO, LOG_ERR, LOG_WARNING, LOG_DEBUG };
    int i_priority = i_prio[p_msg->i_type];

    if( p_msg->psz_header )
        syslog( i_priority, "%s%s %s: %s", p_msg->psz_header,
                ppsz_type[p_msg->i_type],
                p_msg->psz_module, p_msg->psz_msg );
    else
        syslog( i_priority, "%s%s: %s", p_msg->psz_module, 
                ppsz_type[p_msg->i_type], p_msg->psz_msg );
 
}
#endif

static void HtmlPrint( const msg_item_t *p_msg, FILE *p_file )
{
    static const char ppsz_color[4][30] = {
        "<span style=\"color: #ffffff\">",
        "<span style=\"color: #ff6666\">",
        "<span style=\"color: #ffff66\">",
        "<span style=\"color: #aaaaaa\">",
    };

    fprintf( p_file, "%s%s%s%s</span>\n", p_msg->psz_module,
             ppsz_type[p_msg->i_type], ppsz_color[p_msg->i_type],
             p_msg->psz_msg );
}

static void *DoRRD (void *data)
{
    intf_thread_t *p_intf = data;
    FILE *file = p_intf->p_sys->rrd.stream;

    for (;;)
    {
        /* FIXME: I wonder how memory synchronization occurs here...
         * -- Courmisch */
        if( p_intf->p_libvlc->p_stats )
        {
            lldiv_t in = lldiv( p_intf->p_libvlc->p_stats->f_input_bitrate * 1000000,
                                1000 );
            lldiv_t dm = lldiv( p_intf->p_libvlc->p_stats->f_demux_bitrate * 1000000,
                                1000 );
            lldiv_t out = lldiv( p_intf->p_libvlc->p_stats->f_output_bitrate * 1000000,
                                1000 );
            fprintf( file,
                    "%"PRIi64":%lld.%03llu:%lld.%03llu:%lld.%03llu\n",
                    (int64_t)time(NULL), in.quot, in.rem, dm.quot, dm.rem, out.quot, out.rem );
        }
#undef msleep /* yeah, we really want to wake up every second here */
        msleep (CLOCK_FREQ);
    }
    assert (0);
}
