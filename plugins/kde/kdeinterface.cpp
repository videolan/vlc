/***************************************************************************
                          kdeinterface.cpp  -  description
                             -------------------
    begin                : Sun Mar 25 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#define MODULE_NAME kde
#include "intf_plugin.h"

#include "kdeinterface.h"
#include "kinterfacemain.h"

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
 * Functions exported as capabilities.
 *****************************************************************************/
extern "C"
{

void _M( intf_getfunctions )( function_list_t * p_function_list )
{
    p_function_list->pf_probe = KDEInterface::probe;
    p_function_list->functions.intf.pf_open  = KDEInterface::open;
    p_function_list->functions.intf.pf_close = KDEInterface::close;
    p_function_list->functions.intf.pf_run   = KDEInterface::run;
}

}

/*****************************************************************************
 * KDEInterface::KDEInterface: KDE interface constructor
 *****************************************************************************/
KDEInterface::KDEInterface(intf_thread_t *p_intf)
{
	fAboutData = new KAboutData("VideoLAN Client", I18N_NOOP("Kvlc"),
			VERSION,
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
		fAboutData->addAuthor( authors[i][0], 0, authors[i][1] );
	}

	int argc = 1;
	char *argv[] = { "" };
	KCmdLineArgs::init( argc, argv, fAboutData );

	fApplication = new KApplication();
	fWindow = new KInterfaceMain(p_intf);
   fWindow->setCaption( VOUT_TITLE " (KDE interface)" );

}

/*****************************************************************************
 * KDEInterface::~KDEInterface: KDE interface destructor
 *****************************************************************************/
KDEInterface::~KDEInterface()
{
cerr << "entering ~KDEInterface()\n";
//	delete ( fApplication );
cerr << "leaving ~KDEInterface()\n";
}

/*****************************************************************************
 * KDEInterface::probe: probe the interface and return a score
 *****************************************************************************
 * This function tries to initialize KDE and returns a score to the
 * plugin manager so that it can select the best plugin.
 *****************************************************************************/
int KDEInterface::probe(probedata_t *p_data )
{
	if ( TestMethod( INTF_METHOD_VAR, "kde" ) )
	{
		return ( 999 );
	}
	
	if ( TestProgram( "kvlc" ) )
	{
		return ( 180 );
	}
	
	return ( 80 );
}

/*****************************************************************************
 * KDEInterface::open: initialize and create window
 *****************************************************************************/
int KDEInterface::open(intf_thread_t *p_intf)
{
	p_intf->p_sys = (intf_sys_s*) new KDEInterface(p_intf); // XXX static_cast ?
    return ( 0 );
}

/*****************************************************************************
 * KDEInterface::close: destroy interface window
 *****************************************************************************/
void KDEInterface::close(intf_thread_t *p_intf)
{
//	delete ( ( KDEInterface* ) p_intf->p_sys );
}

/*****************************************************************************
 * KDEInterface::run: KDE thread
 *****************************************************************************
 * This part of the interface is in a separate thread so that we can call
 * exec() from within it without annoying the rest of the program.
 *****************************************************************************/
void KDEInterface::run(intf_thread_t *p_intf)
{
	// XXX static_cast ?
	KDEInterface *kdeInterface = (KDEInterface*) p_intf->p_sys;
	kdeInterface->fWindow->show();
	kdeInterface->fApplication->exec();
}
