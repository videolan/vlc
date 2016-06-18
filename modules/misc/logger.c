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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>

#include <stdarg.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open    ( vlc_object_t * );
static void Close   ( vlc_object_t * );

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

    msg_Err( p_intf, "The logger interface no longer exists." );
    msg_Info( p_intf, "As of VLC version 0.9.0, use --file-logging to write "
              "logs to a file." );
# ifndef _WIN32
    msg_Info( p_intf, "Use --syslog to send logs to the system logger." );
# endif
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: destroy interface stuff
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    /* Flush the queue and unsubscribe from the message queue */
    vlc_LogSet( p_this->obj.libvlc, NULL, NULL );
}
