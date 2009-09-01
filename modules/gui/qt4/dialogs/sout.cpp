/*****************************************************************************
 * sout.cpp : Stream output dialog ( old-style )
 ****************************************************************************
 * Copyright (C) 2007-2009 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/sout.hpp"
#include "util/qt_dirs.hpp"
#include "components/sout/sout_widgets.hpp"

#include <QString>
#include <QFileDialog>
#include <QToolButton>
#include <assert.h>

SoutDialog::SoutDialog( QWidget *parent, intf_thread_t *_p_intf, const QString& inputMRL )
           : QVLCDialog( parent,  _p_intf )
{
    setWindowTitle( qtr( "Stream Output" ) );
    setWindowRole( "vlc-stream-output" );

    /* UI stuff */
    ui.setupUi( this );
    ui.inputBox->setMRL( inputMRL );
    ui.helpEdit->setPlainText( qtr("This dialog will allow you to stream or "
            "convert your media for use locally, on your private network, "
            "or on the Internet.\n"
            "You should start by checking that source matches what you want "
            "your input to be and then press the \"Next\" "
            "button to continue.\n") );

    ui.mrlEdit->setToolTip ( qtr( "Stream output string.\n"
                "This is automatically generated "
                 "when you change the above settings,\n"
                 "but you can change it manually." ) ) ;

#if 0
    /* This needs Qt4.5 to be cool */
    ui.destTab->setTabsClosable( true );
#else
    closeTabButton = new QToolButton( this );
    ui.destTab->setCornerWidget( closeTabButton );
    closeTabButton->hide();
    closeTabButton->setAutoRaise( true );
    closeTabButton->setIcon( QIcon( ":/toolbar/clear" ) );
    BUTTONACT( closeTabButton, closeTab() );
#endif
    CONNECT( ui.destTab, currentChanged( int ), this, tabChanged( int ) );
    ui.destTab->setTabIcon( 0, QIcon( ":/buttons/playlist/playlist_add" ) );

    ui.destBox->addItem( qtr( "File" ) );
    ui.destBox->addItem( "HTTP" );
    ui.destBox->addItem( "MMS" );
    ui.destBox->addItem( "UDP" );
    ui.destBox->addItem( "RTP" );
    ui.destBox->addItem( "IceCast" );

    BUTTONACT( ui.addButton, addDest() );

//     /* Connect everything to the updateMRL function */
#define CB( x ) CONNECT( ui.x, toggled( bool ), this, updateMRL() );
#define CT( x ) CONNECT( ui.x, textChanged( const QString& ), this, updateMRL() );
#define CS( x ) CONNECT( ui.x, valueChanged( int ), this, updateMRL() );
#define CC( x ) CONNECT( ui.x, currentIndexChanged( int ), this, updateMRL() );

    /* Misc */
    CB( soutAll ); CB( soutKeep );  CS( ttl ); CT( sapName ); CT( sapGroup );
    CB( localOutput );
    CONNECT( ui.profileSelect, optionsChanged(), this, updateMRL() );

    okButton = new QPushButton( qtr( "&Stream" ) );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ) );

    okButton->setDefault( true );
    ui.acceptButtonBox->addButton( okButton, QDialogButtonBox::AcceptRole );
    ui.acceptButtonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    BUTTONACT( okButton, ok() );
    BUTTONACT( cancelButton, cancel() );

    BUTTONACT( ui.nextButton, next() );
    BUTTONACT( ui.nextButton2, next() );
    BUTTONACT( ui.prevButton, prev() );
    BUTTONACT( ui.prevButton2, prev() );

#undef CC
#undef CS
#undef CT
#undef CB
}

void SoutDialog::next()
{
    ui.toolBox->setCurrentIndex( ui.toolBox->currentIndex() + 1 );
}

void SoutDialog::prev()
{
    ui.toolBox->setCurrentIndex( ui.toolBox->currentIndex() - 1 );
}

void SoutDialog::tabChanged( int i )
{
    closeTabButton->setVisible( (i != 0) );
}

void SoutDialog::closeTab()
{
    int i = ui.destTab->currentIndex();
    if( i == 0 ) return;

    QWidget *temp = ui.destTab->currentWidget();
    ui.destTab->removeTab( i );
    delete temp;
    updateMRL();
}

void SoutDialog::addDest( )
{
    int index;
    switch( ui.destBox->currentIndex() )
    {
        case 0:
            {
                FileDestBox *fdb = new FileDestBox( this );
                index = ui.destTab->addTab( fdb, "File" );
                CONNECT( fdb, mrlUpdated(), this, updateMRL() );
            }
            break;
        case 1:
            {
                HTTPDestBox *hdb = new HTTPDestBox( this );
                index = ui.destTab->addTab( hdb, "HTTP" );
                CONNECT( hdb, mrlUpdated(), this, updateMRL() );
            }
            break;
        case 2:
            {
                MMSHDestBox *mdb = new MMSHDestBox( this );
                index = ui.destTab->addTab( mdb, "MMSH" );
                CONNECT( mdb, mrlUpdated(), this, updateMRL() );
            }
            break;
        case 3:
            {
                UDPDestBox *udb = new UDPDestBox( this );
                index = ui.destTab->addTab( udb, "UDP" );
                CONNECT( udb, mrlUpdated(), this, updateMRL() );
            }
            break;
        case 4:
            {
                RTPDestBox *rdb = new RTPDestBox( this );
                index = ui.destTab->addTab( rdb, "RTP" );
                CONNECT( rdb, mrlUpdated(), this, updateMRL() );
            }
            break;
        case 5:
            {
                ICEDestBox *idb = new ICEDestBox( this );
                index = ui.destTab->addTab( idb, "Icecast" );
                CONNECT( idb, mrlUpdated(), this, updateMRL() );
            }
            break;
        default:
            assert(0);
    }

    ui.destTab->setCurrentIndex( index );
    updateMRL();
}

void SoutDialog::ok()
{
    mrl = ui.mrlEdit->toPlainText();
    accept();
}

void SoutDialog::cancel()
{
    mrl.clear();
    reject();
}

void SoutDialog::updateMRL()
{
    QString qs_mux = ui.profileSelect->getMux();

    SoutMrl smrl( ":sout=#" );
    if( !ui.profileSelect->getTranscode().isEmpty() && ui.transcodeBox->isChecked() )
    {
        smrl.begin( ui.profileSelect->getTranscode() );
        smrl.end();
    }

    bool multi = false;

    if( ui.destTab->count() >= 3 ||
        ( ui.destTab->count() == 2 && ui.localOutput->isChecked() ) )
        multi = true;

    if( multi )
        smrl.begin( "duplicate" );

    for( int i = 1; i < ui.destTab->count(); i++ )
    {
        VirtualDestBox *vdb = qobject_cast<VirtualDestBox *>(ui.destTab->widget( i ));
        QString tempMRL = vdb->getMRL( qs_mux );

        if( tempMRL.isEmpty() ) continue;
        if( multi )
            smrl.option( "dst", tempMRL );
        else
        {
            smrl.begin( tempMRL);
            smrl.end();
        }
    }
    if( ui.localOutput->isChecked() )
    {
        if( multi )
            smrl.option( "dst", "display" );
        else
        {
            smrl.begin( "display" );
            smrl.end();
        }
    }

    if ( multi ) smrl.end();

    mrl = smrl.getMrl();

    /* FIXME, deal with SAP
    sout.b_sap = ui.sap->isChecked();
    sout.psz_group = strdup( qtu( ui.sapGroup->text() ) );
    sout.psz_name = strdup( qtu( ui.sapName->text() ) ); */

    if( ui.soutAll->isChecked() )  mrl.append( " :sout-all" );

    if( ui.soutKeep->isChecked() ) mrl.append( " :sout-keep" );

    ui.mrlEdit->setPlainText( mrl );
}

