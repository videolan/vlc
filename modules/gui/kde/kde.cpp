/*****************************************************************************
 * kde.cpp : KDE plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Andres Krapf <dae@chez.com> Sun Mar 25 2001
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

#include "common.h"

#include "interface.h"

#include <iostream>

#include <kaction.h>
#include <kapp.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#include <kmainwindow.h>
#include <kstdaction.h>
#include <qwidget.h>

/*****************************************************************************
 * The local class and prototypes
 *****************************************************************************/
class KInterface;
class KAboutData;

static int open( vlc_object_t * p_this );
static void close( vlc_object_t * p_this );
static void run(intf_thread_t *p_intf);

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    /* int i = getenv( "DISPLAY" ) == NULL ? 8 : 85; */
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_GENERAL );
    set_description( _("KDE interface") );
    add_file( "kde-uirc", DATA_PATH "/ui.rc", NULL, N_( "path to ui.rc file" ), NULL, VLC_TRUE );
    set_capability( "interface", 0 ); /* 0 used to be i, disabled because kvlc not maintained */
    set_program( "kvlc" );
    set_callbacks( open, close );
vlc_module_end();

/*****************************************************************************
 * KThread::open: initialize and create window
 *****************************************************************************/
static int open(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return( 1 );
    }

    p_intf->pf_run = run;
    return ( 0 );
}

/*****************************************************************************
 * KThread::close: destroy interface window
 *****************************************************************************/
static void close(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    if( p_intf->p_sys->p_input )
    {
        vlc_object_release( p_intf->p_sys->p_input );
    }

    delete p_intf->p_sys->p_app;
    delete p_intf->p_sys->p_about;
    msg_Unsubscribe(p_intf, p_intf->p_sys->p_msg);
    free( p_intf->p_sys );
}

/*****************************************************************************
 * KThread::run: KDE thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
void run(intf_thread_t *p_intf)
{
    p_intf->p_sys->p_about =
      new KAboutData( "kvlc", I18N_NOOP("Kvlc"), VERSION,
         _("This is the VLC media player, a DVD, MPEG and DivX player. It can "
           "play MPEG and MPEG2 files from a file or from a network source."),
         KAboutData::License_GPL,
         _("(c) 1996-2004 the VideoLAN team"),
         0, 0, "");

    p_intf->p_sys->p_about->addAuthor( "the VideoLAN team", 0,
                                       "<videolan@videolan.org>" );

    int argc = 5;
    char *argv[] = { "vlc", "--icon", DATA_PATH "/kvlc32x32.png", "--miniicon", DATA_PATH "/kvlc16x16.png" };
    KCmdLineArgs::init( argc, argv, p_intf->p_sys->p_about );

    /* Subscribe to message queue */
    p_intf->p_sys->p_msg = msg_Subscribe( p_intf );

    p_intf->p_sys->p_app = new KApplication();
    p_intf->p_sys->p_window = new KInterface(p_intf);
    p_intf->p_sys->p_window->setCaption( VOUT_TITLE " (KDE interface)" );

    p_intf->p_sys->p_input = NULL;

    p_intf->p_sys->p_window->show();
    p_intf->p_sys->p_app->exec();
}

