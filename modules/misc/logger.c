/*****************************************************************************
 * logger.c : file logging plugin for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN (Centrale Réseaux) and its contributors
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>

#ifdef UNDER_CE
#   define _IONBF 0x0004
#endif

#include <vlc/vlc.h>
#include <vlc/intf.h>

#define MODE_TEXT 0
#define MODE_HTML 1

#ifdef SYS_DARWIN
#define LOG_DIR "Library/Logs/"
#endif

#define LOG_FILE_TEXT "vlc-log.txt"
#define LOG_FILE_HTML "vlc-log.html"

#define LOG_STRING( msg, file ) fwrite( msg, strlen( msg ), 1, file );

#define TEXT_HEADER "-- logger module started --\n"
#define TEXT_FOOTER "-- logger module stopped --\n"

#define HTML_HEADER \
    "<html>\n" \
    "  <head>\n" \
    "    <title>vlc log</title>\n" \
    "  </head>\n" \
    "  <body bgcolor=\"#000000\" text=\"#aaaaaa\">\n" \
    "    <pre>\n" \
    "      <b>-- logger module started --</b>\n"
#define HTML_FOOTER \
    "      <b>-- logger module stopped --</b>\n" \
    "    </pre>\n" \
    "  </body>\n" \
    "</html>\n"

/*****************************************************************************
 * intf_sys_t: description and status of log interface
 *****************************************************************************/
struct intf_sys_t
{
    int i_mode;

    FILE *    p_file; /* The log file */
    msg_subscription_t *p_sub;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );
static void Run     ( intf_thread_t * );

static void FlushQueue        ( msg_subscription_t *, FILE *, int );
static void TextPrint         ( const msg_item_t *, FILE * );
static void HtmlPrint         ( const msg_item_t *, FILE * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static char *mode_list[] = { "text", "html" };
static char *mode_list_text[] = { N_("Text"), "HTML" };

#define LOGMODE_TEXT N_("Log format")
#define LOGMODE_LONGTEXT N_("Specify the log format. Available choices are \"text\" (default) and \"html\".")

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_CONTROL );
    set_shortname( N_( "Logging" ) );
    set_description( _("File logging") );

    add_file( "logfile", NULL, NULL, N_("Log filename"), N_("Specify the log filename."), VLC_FALSE );
    add_string( "logmode", "text", NULL, LOGMODE_TEXT, LOGMODE_LONGTEXT,
                VLC_FALSE );
        change_string_list( mode_list, mode_list_text, 0 );

    set_capability( "interface", 0 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{   
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    char *psz_mode, *psz_file;

    CONSOLE_INTRO_MSG;
    msg_Info( p_intf, "Using the logger interface module..." );

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return -1;
    }

    psz_mode = config_GetPsz( p_intf, "logmode" );
    if( psz_mode )
    {
        if( !strcmp( psz_mode, "text" ) )
        {
            p_intf->p_sys->i_mode = MODE_TEXT;
        }
        else if( !strcmp( psz_mode, "html" ) )
        {
            p_intf->p_sys->i_mode = MODE_HTML;
        }
        else
        {
            msg_Err( p_intf, "invalid log mode `%s', using `text'", psz_mode );
            p_intf->p_sys->i_mode = MODE_TEXT;
        }

        free( psz_mode );
    }
    else
    {
        msg_Warn( p_intf, "no log mode specified, using `text'" );
        p_intf->p_sys->i_mode = MODE_TEXT;
    }

    psz_file = config_GetPsz( p_intf, "logfile" );
    if( !psz_file )
    {
#ifdef SYS_DARWIN
	char *psz_homedir = p_this->p_vlc->psz_homedir; 

        if( !psz_homedir )
        {
            msg_Err( p_this, "psz_homedir is null" );
            return -1;
        }
        psz_file = (char *)malloc( sizeof("/" LOG_DIR "/" LOG_FILE_HTML) +
                                       strlen(psz_homedir) );
        if( psz_file )
        {
            switch( p_intf->p_sys->i_mode )
            {
            case MODE_HTML:
                sprintf( psz_file, "%s/" LOG_DIR "/" LOG_FILE_HTML,
                     psz_homedir );
                break;
            case MODE_TEXT:
            default:
                sprintf( psz_file, "%s/" LOG_DIR "/" LOG_FILE_TEXT,
                     psz_homedir );
                break;
            }
        }
#else
	switch( p_intf->p_sys->i_mode )
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
        msg_Warn( p_intf, "no log filename provided, using `%s'", psz_file );
    }

    /* Open the log file and remove any buffering for the stream */
    msg_Dbg( p_intf, "opening logfile `%s'", psz_file );
    p_intf->p_sys->p_file = fopen( psz_file, "wt" );
    if( p_intf->p_sys->p_file == NULL )
    {
        msg_Err( p_intf, "error opening logfile `%s'", psz_file );
        free( p_intf->p_sys );
        free( psz_file );
        return -1;
    }
    setvbuf( p_intf->p_sys->p_file, NULL, _IONBF, 0 );
    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );

    free( psz_file );

    switch( p_intf->p_sys->i_mode )
    {
    case MODE_HTML:
        LOG_STRING( HTML_HEADER, p_intf->p_sys->p_file );
        break;
    case MODE_TEXT:
    default:
        LOG_STRING( TEXT_HEADER, p_intf->p_sys->p_file );
        break;
    }

    p_intf->pf_run = Run;

    return 0;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{       
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
        
    /* Flush the queue and unsubscribe from the message queue */
    FlushQueue( p_intf->p_sys->p_sub, p_intf->p_sys->p_file,
                p_intf->p_sys->i_mode );
    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

    switch( p_intf->p_sys->i_mode )
    {
    case MODE_HTML:
        LOG_STRING( HTML_FOOTER, p_intf->p_sys->p_file );
        break;
    case MODE_TEXT:
    default:
        LOG_STRING( TEXT_FOOTER, p_intf->p_sys->p_file );
        break;
    }

    /* Close the log file */
    fclose( p_intf->p_sys->p_file );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * Run: rc thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    while( !p_intf->b_die )
    {
        FlushQueue( p_intf->p_sys->p_sub, p_intf->p_sys->p_file,
                    p_intf->p_sys->i_mode );

        msleep( INTF_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * FlushQueue: flush the message queue into the log file
 *****************************************************************************/
static void FlushQueue( msg_subscription_t *p_sub, FILE *p_file, int i_mode )
{
    int i_start, i_stop;

    vlc_mutex_lock( p_sub->p_lock );
    i_stop = *p_sub->pi_stop;
    vlc_mutex_unlock( p_sub->p_lock );

    if( p_sub->i_start != i_stop )
    {
        /* Append all messages to log file */
        for( i_start = p_sub->i_start;
             i_start != i_stop;
             i_start = (i_start+1) % VLC_MSG_QSIZE )
        {
            switch( i_mode )
            {
            case MODE_HTML:
                HtmlPrint( &p_sub->p_msg[i_start], p_file );
                break;
            case MODE_TEXT:
            default:
                TextPrint( &p_sub->p_msg[i_start], p_file );
                break;
            }
        }

        vlc_mutex_lock( p_sub->p_lock );
        p_sub->i_start = i_start;
        vlc_mutex_unlock( p_sub->p_lock );
    }
}

static const char *ppsz_type[4] = { ": ", " error: ",
                                    " warning: ", " debug: " };

static void TextPrint( const msg_item_t *p_msg, FILE *p_file )
{
    LOG_STRING( p_msg->psz_module, p_file );
    LOG_STRING( ppsz_type[p_msg->i_type], p_file );
    LOG_STRING( p_msg->psz_msg, p_file );
    LOG_STRING( "\n", p_file );
}

static void HtmlPrint( const msg_item_t *p_msg, FILE *p_file )
{
    static const char *ppsz_color[4] = { "<font color=\"#ffffff\">",
                                         "<font color=\"#ff6666\">",
                                         "<font color=\"#ffff66\">",
                                         "<font color=\"#aaaaaa\">" };

    LOG_STRING( p_msg->psz_module, p_file );
    LOG_STRING( ppsz_type[p_msg->i_type], p_file );
    LOG_STRING( ppsz_color[p_msg->i_type], p_file );
    LOG_STRING( p_msg->psz_msg, p_file );
    LOG_STRING( "</font>\n", p_file );
}

