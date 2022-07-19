/*****************************************************************************
 * sout.cpp : Stream output dialog ( old-style )
 ****************************************************************************
 * Copyright (C) 2007-2009 the VideoLAN team
 *
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

#include "dialogs/sout/sout.hpp"
#include "util/qt_dirs.hpp"
#include "dialogs/sout/sout_widgets.hpp"
#include "widgets/native/qvlcframe.hpp"

#include <QString>

#include <assert.h>
#include <QWindow>

SoutDialog::SoutDialog( QWindow *parent, qt_intf_t *_p_intf, const QString& inputChain )
{
    p_intf = _p_intf;

    setWindowTitle( qtr( "Stream Output" ) );
    setWindowRole( "vlc-stream-output" );

    QVLCDialog::setWindowTransientParent(this, parent , p_intf);

    /* UI stuff */
    ui.setupUi( this );
    ui.inputBox->setMRL( inputChain );
    ui.helpEdit->setPlainText( qtr("This wizard will allow you to stream or "
            "convert your media for use locally, on your private network, "
            "or on the Internet.\n"
            "You should start by checking that source matches what you want "
            "your input to be and then press the \"Next\" "
            "button to continue.\n") );

    ui.mrlEdit->setToolTip ( qtr( "Stream output string.\n"
                "This is automatically generated "
                 "when you change the above settings,\n"
                 "but you can change it manually." ) ) ;

    ui.destTab->setTabsClosable( true );
    QTabBar* tb = ui.destTab->findChild<QTabBar*>();
    if( tb != NULL ) tb->tabButton(0, QTabBar::RightSide)->hide();
    connect( ui.destTab, &QTabWidget::tabCloseRequested, this, &SoutDialog::closeTab );
    ui.destTab->setTabIcon( 0, QIcon( ":/menu/add.svg" ) );

    ui.destBox->addItem( qtr( "File" ) );
    ui.destBox->addItem( "HTTP" );
    ui.destBox->addItem( "MS-WMSP (MMSH)" );
    ui.destBox->addItem( "RTSP" );
    ui.destBox->addItem( "SRT / MPEG Transport Stream" );
    ui.destBox->addItem( "RIST / MPEG Transport Stream" );
    ui.destBox->addItem( "RTP / MPEG Transport Stream" );
    ui.destBox->addItem( "RTP Audio/Video Profile" );
    ui.destBox->addItem( "UDP (legacy)" );
    ui.destBox->addItem( "Icecast" );

    BUTTONACT( ui.addButton, &SoutDialog::addDest );

    /* Connect everything to the updateChain function */

    /* Misc */
    connect( ui.soutAll, &QCheckBox::toggled, this, &SoutDialog::updateChain );
    connect( ui.localOutput, &QCheckBox::toggled, this, &SoutDialog::updateChain );
    connect( ui.transcodeBox, &QCheckBox::toggled, this, &SoutDialog::updateChain );
    connect( ui.profileSelect, &VLCProfileSelector::optionsChanged, this, &SoutDialog::updateChain );

    setButtonText( QWizard::BackButton, qtr("Back") );
    setButtonText( QWizard::CancelButton, qtr("Cancel") );
    setButtonText( QWizard::NextButton, qtr("Next") );
    setButtonText( QWizard::FinishButton, qtr("Stream") );
}

void SoutDialog::closeTab( int i )
{
    if( i == 0 ) return;

    QWidget* temp = ui.destTab->widget( i );
    ui.destTab->removeTab( i );
    delete temp;
    updateChain();
}

void SoutDialog::addDest( )
{
    VirtualDestBox *db;
    QString caption;

    switch( ui.destBox->currentIndex() )
    {
        case 0:
            db = new FileDestBox( this, p_intf );
            caption = qtr( "File" );
            break;
        case 1:
            db = new HTTPDestBox( this );
            caption = qfu( "HTTP" );
            break;
        case 2:
            db = new MMSHDestBox( this );
            caption = qfu( "WMSP" );
            break;
        case 3:
            db = new RTSPDestBox( this );
            caption = qfu( "RTSP" );
            break;
        case 4:
            db = new SRTDestBox( this, "ts" );
            caption = "SRT/TS";
            break;
        case 5:
            db = new RISTDestBox( this, "ts" );
            caption = "RIST/TS";
            break;
        case 6:
            db = new RTPDestBox( this, "ts" );
            caption = "RTP/TS";
            break;
        case 7:
            db = new RTPDestBox( this );
            caption = "RTP/AVP";
            break;
        case 8:
            db = new UDPDestBox( this );
            caption = "UDP";
            break;
        case 9:
            db = new ICEDestBox( this );
            caption = "Icecast";
            break;
        default:
            vlc_assert_unreachable();
            return;
    }

    int index = ui.destTab->addTab( db, caption );
    connect( db, &VirtualDestBox::mrlUpdated, this, &SoutDialog::updateChain );
    ui.destTab->setCurrentIndex( index );
    updateChain();
}

void SoutDialog::done( int r )
{
    chain = ui.mrlEdit->toPlainText();
    QWizard::done(r);
}

void SoutDialog::updateChain()
{
    QString qs_mux = ui.profileSelect->getMux();

    SoutChain smrl( ":sout=#" );
    if( !ui.profileSelect->getTranscode().to_string().isEmpty() && ui.transcodeBox->isChecked() )
    {
        smrl.begin( ui.profileSelect->getTranscode().to_string() );
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
        if( !vdb )
            continue;

        QString tempMRL = vdb->getMRL( qs_mux );
        if( tempMRL.isEmpty() ) continue;

        if( multi )
            smrl.option( "dst", tempMRL, true );
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

    chain = smrl.to_string();

    if( ui.soutAll->isChecked() )
        chain.append( " :sout-all" );
    else
        chain.append( " :no-sout-all" );

    chain.append( " :sout-keep" );

    ui.mrlEdit->setPlainText( chain );
}
