/*****************************************************************************
 * kde.cpp : KDE plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: kde.cpp,v 1.8 2002/03/01 16:07:00 sam Exp $
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

#include "kde_common.h"

#include "kde_interface.h"

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
 * Capabilities defined in the other files.
 *****************************************************************************/
static void intf_getfunctions( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
extern "C"
{

MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "KDE interface module" )
    ADD_CAPABILITY( INTF, 80 )
    ADD_SHORTCUT( "kde" )
    ADD_PROGRAM( "kvlc" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    intf_getfunctions( &p_module->p_functions->intf );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

} // extern "C"

/*****************************************************************************
 * The local class.
 *****************************************************************************/
class KInterface;
class KAboutData;

class KThread
{
    private:
        KThread ( KThread &thread ) { };
        KThread &operator= ( KThread &thread ) { return ( *this ); };

        intf_thread_t *p_intf;
        
    public:
        KThread(intf_thread_t *p_intf);
        ~KThread();

        // These methods get exported to the core
        static int     probe   ( probedata_t *p_data );
        static int     open    ( intf_thread_t *p_intf );
        static void    close   ( intf_thread_t *p_intf );
        static void    run     ( intf_thread_t *p_intf );
};

/*****************************************************************************
 * Functions exported as capabilities.
 *****************************************************************************/
static void intf_getfunctions( function_list_t * p_function_list )
{
    p_function_list->pf_probe = KThread::probe;
    p_function_list->functions.intf.pf_open  = KThread::open;
    p_function_list->functions.intf.pf_close = KThread::close;
    p_function_list->functions.intf.pf_run   = KThread::run;
}

/*****************************************************************************
 * KThread::KThread: KDE interface constructor
 *****************************************************************************/
KThread::KThread(intf_thread_t *p_intf)
{
    this->p_intf = p_intf;

    p_intf->p_sys->p_about =
        new KAboutData( "VideoLAN Client", I18N_NOOP("Kvlc"), VERSION,
            "This is the VideoLAN client, a DVD and MPEG player. It can play MPEG and MPEG 2 files from a file or from a network source.", KAboutData::License_GPL,
            "(C) 1996, 1997, 1998, 1999, 2000, 2001 - the VideoLAN Team", 0, 0, "dae@chez.com");

    char *authors[][2] = {
        { "Régis Duchesne", "<regis@via.ecp.fr>" },
        { "Michel Lespinasse", "<walken@zoy.org>" },
        { "Olivier Pomel", "<pomel@via.ecp.fr>" },
        { "Pierre Baillet", "<oct@zoy.org>" },
        { "Jean-Philippe Grimaldi", "<jeanphi@via.ecp.fr>" },
        { "Andres Krapf", "<dae@via.ecp.fr>" },
        { "Christophe Massiot", "<massiot@via.ecp.fr>" },
        { "Vincent Seguin", "<seguin@via.ecp.fr>" },
        { "Benoit Steiner", "<benny@via.ecp.fr>" },
        { "Arnaud de Bossoreille de Ribou", "<bozo@via.ecp.fr>" },
        { "Jean-Marc Dressler", "<polux@via.ecp.fr>" },
        { "Gaël Hendryckx", "<jimmy@via.ecp.fr>" },
        { "Samuel Hocevar","<sam@zoy.org>" },
        { "Brieuc Jeunhomme", "<bbp@via.ecp.fr>" },
        { "Michel Kaempf", "<maxx@via.ecp.fr>" },
        { "Stéphane Borel", "<stef@via.ecp.fr>" },
        { "Renaud Dartus", "<reno@via.ecp.fr>" },
        { "Henri Fallon", "<henri@via.ecp.fr>" },
        { NULL, NULL },
    };

    for ( int i = 0; NULL != authors[i][0]; i++ ) {
        p_intf->p_sys->p_about->addAuthor( authors[i][0], 0, authors[i][1] );
    }

    int argc = 1;
    char *argv[] = { p_main->psz_arg0, NULL };
    KCmdLineArgs::init( argc, argv, p_intf->p_sys->p_about );

    p_intf->p_sys->p_app = new KApplication();
    p_intf->p_sys->p_window = new KInterface(p_intf);
    p_intf->p_sys->p_window->setCaption( VOUT_TITLE " (KDE interface)" );
}

/*****************************************************************************
 * KThread::~KThread: KDE interface destructor
 *****************************************************************************/
KThread::~KThread()
{
    /* XXX: can be deleted if the user closed the window ! */
    //delete p_intf->p_sys->p_window;

    delete p_intf->p_sys->p_app;
    delete p_intf->p_sys->p_about;
}

/*****************************************************************************
 * KThread::probe: probe the interface and return a score
 *****************************************************************************
 * This function tries to initialize KDE and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
int KThread::probe(probedata_t *p_data )
{
    return ( 80 );
}

/*****************************************************************************
 * KThread::open: initialize and create window
 *****************************************************************************/
int KThread::open(intf_thread_t *p_intf)
{
    /* Allocate instance and initialize some members */
    p_intf->p_sys = (intf_sys_t *)malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg( "intf error: %s", strerror(ENOMEM) );
        return( 1 );
    }

    p_intf->p_sys->p_thread = new KThread(p_intf);
    return ( 0 );
}

/*****************************************************************************
 * KThread::close: destroy interface window
 *****************************************************************************/
void KThread::close(intf_thread_t *p_intf)
{
    delete p_intf->p_sys->p_thread;
    free( p_intf->p_sys );
}

/*****************************************************************************
 * KThread::run: KDE thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
void KThread::run(intf_thread_t *p_intf)
{
    p_intf->p_sys->p_window->show();
    p_intf->p_sys->p_app->exec();
}

