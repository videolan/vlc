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

#include "dialogs/sout.hpp"
#include "util/qt_dirs.hpp"
#include "components/sout/sout_widgets.hpp"

#include <QString>
#include <QFileDialog>
#include <QToolButton>
#include <QSpinBox>
#include <assert.h>

SoutDialog::SoutDialog( QWidget *parent, intf_thread_t *_p_intf, const QString& inputMRL )
           : QWizard( parent )
{
    p_intf = _p_intf;

    setWindowTitle( qtr( "Stream Output" ) );
    setWindowRole( "vlc-stream-output" );

    /* UI stuff */
    ui.setupUi( this );
    ui.inputBox->setMRL( inputMRL );
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
    CONNECT( ui.destTab, tabCloseRequested( int ), this, closeTab( int ) );
    ui.destTab->setTabIcon( 0, QIcon( ":/buttons/playlist/playlist_add.svg" ) );

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

    BUTTONACT( ui.addButton, addDest() );

//     /* Connect everything to the updateMRL function */
#define CB( x ) CONNECT( ui.x, toggled( bool ), this, updateMRL() );
#define CT( x ) CONNECT( ui.x, textChanged( const QString& ), this, updateMRL() );
#define CS( x ) CONNECT( ui.x, valueChanged( int ), this, updateMRL() );
#define CC( x ) CONNECT( ui.x, currentIndexChanged( int ), this, updateMRL() );

    /* Misc */
    CB( soutAll );
    CB( localOutput ); CB( transcodeBox );
    CONNECT( ui.profileSelect, optionsChanged(), this, updateMRL() );

    setButtonText( QWizard::BackButton, qtr("Back") );
    setButtonText( QWizard::CancelButton, qtr("Cancel") );
    setButtonText( QWizard::NextButton, qtr("Next") );
    setButtonText( QWizard::FinishButton, qtr("Stream") );

#undef CC
#undef CS
#undef CT
#undef CB
}

void SoutDialog::closeTab( int i )
{
    if( i == 0 ) return;

    QWidget* temp = ui.destTab->widget( i );
    ui.destTab->removeTab( i );
    delete temp;
    updateMRL();
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
    CONNECT( db, mrlUpdated(), this, updateMRL() );
    ui.destTab->setCurrentIndex( index );
    updateMRL();
}

void SoutDialog::done( int r )
{
    mrl = ui.mrlEdit->toPlainText();
    QWizard::done(r);
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
        if( !vdb )
            continue;

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

    if( ui.soutAll->isChecked() )
        mrl.append( " :sout-all" );
    else
        mrl.append( " :no-sout-all" );

    mrl.append( " :sout-keep" );

    ui.mrlEdit->setPlainText( mrl );
}

