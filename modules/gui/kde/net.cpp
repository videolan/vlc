/***************************************************************************
                          net.cpp  -  description
                             -------------------
    begin                : Mon Apr 9 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#include "net.h"

#include <kdialogbase.h>
#include <klineedit.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qradiobutton.h>
#include <qspinbox.h>
#include <qstring.h>
#include <qvbox.h>
#include <qvbuttongroup.h>
#include <qvgroupbox.h>
#include <qwidget.h>

KNetDialog::KNetDialog( QWidget *parent, const char *name )
           :KDialogBase( parent, name, true,
                         QString::null, Ok|Cancel, Ok, true )
{
    QVBox *pageVBox = makeVBoxMainWidget();

    QHBox *layout = new QHBox( pageVBox );
    layout->setSpacing( 5 );
    fButtonGroup = new QVButtonGroup( _("Protocol"), layout );
    fTSButton = new QRadioButton( "TS", fButtonGroup);
    fTSButton->setChecked( true );
    fRTPButton = new QRadioButton( "RTP", fButtonGroup);
    fRTPButton->setEnabled( false );
    fHTTPButton = new QRadioButton( "HTTP", fButtonGroup);
    fHTTPButton->setEnabled( false );

    QVGroupBox *serverVBox = new QVGroupBox( _("Starting position"), layout );

    QHBox *titleHBox = new QHBox( serverVBox );
    new QLabel( _("Address "), titleHBox );
    fAddress = new KLineEdit( "vls", titleHBox );
    QHBox *portHBox = new QHBox( serverVBox );
    new QLabel( _("Port "), portHBox );
    fPort = new QSpinBox( 0, 65535, 1, portHBox );
}

KNetDialog::~KNetDialog()
{
}

QString KNetDialog::protocol() const
{
    if ( fTSButton->isChecked() )
    {
        return ( QString( "ts" ) );
    }
    else if ( fRTPButton->isChecked() )
    {
        return ( QString( "rtp" ) );
    }
    else
    {
        return ( QString( "http" ) );
    }
}

QString KNetDialog::server() const
{
    return ( fAddress->text() );
}

int KNetDialog::port() const
{
    return ( fPort->value() );
}
