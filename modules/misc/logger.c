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

#include <stdarg.h>

#ifdef __ANDROID__
# include <android/log.h>
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

#ifdef __ANDROID__
static void AndroidPrint(void *, int, const vlc_log_t *, const char *, va_list);
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin ()
    set_shortname( N_( "Logging" ) )
    set_description( N_("File logging") )

    set_category( CAT_ADVANCED )
    set_subcategory( SUBCAT_ADVANCED_MISC )

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

#ifdef __ANDROID__
    msg_Info( p_this, "using logger." );

    vlc_LogSet( p_intf->p_libvlc, AndroidPrint, p_intf );
    return VLC_SUCCESS;
#else
    msg_Err( p_intf, "The logger interface no longer exists." );
    msg_Info( p_intf, "As of VLC version 0.9.0, use --file-logging to write "
              "logs to a file." );
# ifndef _WIN32
    msg_Info( p_intf, "Use --syslog to send logs to the system logger." );
# endif
    return VLC_EGENERIC;
#endif
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    /* Flush the queue and unsubscribe from the message queue */
    vlc_LogSet( p_this->p_libvlc, NULL, NULL );
}

#ifdef __ANDROID__
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
