/*****************************************************************************
 * sout.cpp : stream output dialog ( old-style )
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id: sout.cpp 21875 2007-09-08 16:01:33Z jb $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Jean-François Massol <jf.massol -at- gmail.com>
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
 * Foundation, Inc., 51 Franklin street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "dialogs/vlm.hpp"
#include <vlc_streaming.h>

#include <QString>
#include <QFileDialog>
#include <QComboBox>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QWidget>
#include <QGridLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QToolButton>
#include <QGroupBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTimeEdit>
#include <QDateEdit>
#include <QSpinBox>
#include <QHeaderView>

VLMDialog *VLMDialog::instance = NULL;


VLMDialog::VLMDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    // UI stuff
    ui.setupUi( this );

#define ADDMEDIATYPES( str, type ) ui.mediaType->addItem( qtr( str ), QVariant( type ) );
    ADDMEDIATYPES( "Broadcast", QVLM_Broadcast );
    ADDMEDIATYPES( "Video On Demand ( VOD )", QVLM_VOD );
    ADDMEDIATYPES( "Schedule", QVLM_Schedule );
#undef ADDMEDIATYPES
    
  /*  ui.mediasDB->horizontalHeader()->setResizeMode( 0, QHeaderView::ResizeToContents );
    ui.mediasDB->horizontalHeader()->resizeSection( 1, 160 );
    ui.mediasDB->horizontalHeader()->resizeSection( 2, 120 );
    ui.mediasDB->horizontalHeader()->resizeSection( 3, 120 );
    ui.mediasDB->horizontalHeader()->setStretchLastSection ( true );*/

    QGridLayout *bcastlayout = new QGridLayout( ui.pBcast );
    QLabel *bcastname = new QLabel( qtr( "Name:" ) );
    bcastnameledit = new QLineEdit;
    bcastenable = new QCheckBox( qtr( "Enable" ) );
    bcastenable->setCheckState( Qt::Checked );
    QLabel *bcastinput = new QLabel( qtr( "Input:" ) );
    bcastinputledit = new QLineEdit;
    bcastinputtbutton = new QToolButton;
    QLabel *bcastoutput = new QLabel( qtr( "Output:" ) );
    bcastoutputledit = new QLineEdit;
    bcastoutputtbutton = new QToolButton;
    QGroupBox *bcastcontrol = new QGroupBox( qtr( "Controls" ) );
    QHBoxLayout *bcastgbox = new QHBoxLayout( bcastcontrol );
    bcastplay = new QPushButton( qtr( "Play" ) );
    bcastpause = new QPushButton( qtr( "Pause" ) );
    bcaststop = new QPushButton( qtr( "Stop" ) );

    bcastgbox->addWidget( bcastplay );
    bcastgbox->addWidget( bcastpause );
    bcastgbox->addWidget( bcaststop );

    bcastlayout->addWidget( bcastname, 0, 0 );
    bcastlayout->addWidget( bcastnameledit, 0, 1 );
    bcastlayout->addWidget( bcastenable, 0, 2 );
    bcastlayout->addWidget( bcastinput, 1, 0 );
    bcastlayout->addWidget( bcastinputledit, 1, 1 );
    bcastlayout->addWidget( bcastinputtbutton, 1, 2 );
    bcastlayout->addWidget( bcastoutput, 2, 0 );
    bcastlayout->addWidget( bcastoutputledit, 2, 1 );
    bcastlayout->addWidget( bcastoutputtbutton, 2, 2 );
    bcastlayout->addWidget( bcastcontrol, 3, 0, 1, 3 );
    QSpacerItem *spacerItem = new QSpacerItem(10, 5,
                        QSizePolicy::Expanding, QSizePolicy::MinimumExpanding );
    bcastlayout->addItem(spacerItem, 4, 0, 1, 1);


    QGridLayout *vodlayout = new QGridLayout( ui.pVod );
    QLabel *vodname = new QLabel( qtr( "Name :" ) );
    vodnameledit = new QLineEdit;
    vodenable = new QCheckBox( qtr( "Enable" ) );
    QLabel *vodinput = new QLabel( qtr( "Input :" ) );
    vodinputledit = new QLineEdit;
    vodinputtbutton = new QToolButton;
    QLabel *vodoutput = new QLabel( qtr( "Output :" ) );
    vodoutputledit = new QLineEdit;
    vodoutputtbutton = new QToolButton;

    vodlayout->addWidget( vodname, 0, 0 );
    vodlayout->addWidget( vodnameledit, 0, 1 );
    vodlayout->addWidget( vodenable, 0, 2 );
    vodlayout->addWidget( vodinput, 1, 0 );
    vodlayout->addWidget( vodinputledit, 1, 1 );
    vodlayout->addWidget( vodinputtbutton, 1, 2 );
    vodlayout->addWidget( vodoutput, 2, 0 );
    vodlayout->addWidget( vodoutputledit, 2, 1 );
    vodlayout->addWidget( vodoutputtbutton, 2, 2 );
    QSpacerItem *spacerVod = new QSpacerItem(10, 5, 
                        QSizePolicy::Expanding, QSizePolicy::MinimumExpanding );
    vodlayout->addItem( spacerVod, 4, 0, 1, 1);


    QGridLayout *schelayout = new QGridLayout( ui.pSched );
    QLabel *schename = new QLabel( qtr( "Name:" ) );
    schenameledit = new QLineEdit;
    scheenable = new QCheckBox( qtr( "Enable" ) );
    QLabel *scheinput = new QLabel( qtr( "Input:" ) );
    scheinputledit = new QLineEdit;
    scheinputtbutton = new QToolButton;
    QLabel *scheoutput = new QLabel( qtr( "Output:" ) );
    scheoutputledit = new QLineEdit;
    scheoutputtbutton = new QToolButton;

    QGroupBox *schecontrol = new QGroupBox( qtr( "Time Control" ), ui.pSched );
    QGridLayout *schetimelayout = new QGridLayout( schecontrol );
    QLabel *schetimelabel = new QLabel( qtr( "Hours/Minutes/Seconds:" ) );
    QLabel *schedatelabel = new QLabel( qtr( "Day Month Year:" ) );
    QLabel *schetimerepeat = new QLabel( qtr( "Repeat:" ) );
    time = new QTimeEdit( QTime::currentTime() );
    time->setAlignment( Qt::AlignRight );
    date = new QDateEdit( QDate::currentDate() );
    date->setAlignment( Qt::AlignRight );
#ifdef WIN32
    date->setDisplayFormat( "dd MM yyyy" );
#else
    date->setDisplayFormat( "dd MMMM yyyy" );
#endif
    scherepeatnumber = new QSpinBox;
    scherepeatnumber->setAlignment( Qt::AlignRight );
    schecontrol->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Minimum );

    schetimelayout->addWidget( schetimelabel, 0, 0 );
    schetimelayout->addWidget( time, 0, 1 );
    schetimelayout->addWidget( schedatelabel, 1, 0 );
    schetimelayout->addWidget( date, 1, 1 );
    schetimelayout->addWidget( schetimerepeat, 2, 0 );
    schetimelayout->addWidget( scherepeatnumber, 2, 1 );
    schelayout->addWidget( schename, 0, 0 );
    schelayout->addWidget( schenameledit, 0, 1 );
    schelayout->addWidget( scheenable, 0, 2 );
    schelayout->addWidget( scheinput, 1, 0 );
    schelayout->addWidget( scheinputledit, 1, 1 );
    schelayout->addWidget( scheinputtbutton, 1, 2 );
    schelayout->addWidget( scheoutput, 2, 0 );
    schelayout->addWidget( scheoutputledit, 2, 1 );
    schelayout->addWidget( scheoutputtbutton, 2, 2 );
    schelayout->addWidget( schecontrol, 3, 0, 1, 3 );

    QPushButton *closeButton = new QPushButton( qtr( "Close" ) );
    QPushButton *cancelButton = new QPushButton( qtr( "Cancel" ) );
    ui.buttonBox->addButton( closeButton, QDialogButtonBox::AcceptRole );
    ui.buttonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    ui.mediaStacked->setCurrentIndex( QVLM_Broadcast );
    CONNECT( ui.mediaType, currentIndexChanged( int ),
             ui.mediaStacked, setCurrentIndex( int ) );

    BUTTONACT( closeButton, finish() );
    BUTTONACT( cancelButton, cancel() );

    BUTTONACT( ui.addButton, addVLMItem() );
    BUTTONACT( ui.clearButton, clearVLMItem() );
}

VLMDialog::~VLMDialog(){}

void VLMDialog::cancel()
{
    hide();

}

void VLMDialog::finish()
{
   // for( int i = 0; i < ui.mediasDB->topLevelItemCount(); i++ );
    hide();
}

void VLMDialog::addVLMItem()
{
   // int row =  ui.mediasDB->rowCount() -1 ;
    int type = ui.mediaType->itemData( ui.mediaType->currentIndex() ).toInt();
    QString str;
    QString name;

    switch( type )
    {
    case QVLM_Broadcast:
        str = "broadcast";
        name = bcastnameledit->text();        
    break;
    case QVLM_VOD:
        str = "vod";
        name = vodnameledit->text();
        break;
    case QVLM_Schedule:
        str = "schedule";
        name = schenameledit->text();
        break;
    default:
        break;
    }

    QGroupBox *groupItem = new QGroupBox( name );
    
    /*QTableWidgetItem *newItem = new QTableWidgetItem( str );
    ui.mediasDB->setItem( row, 0,  newItem );
    QTableWidgetItem *newItem2 = new QTableWidgetItem( name );
    ui.mediasDB->setItem( row, 1,  newItem2 );*/

}

void VLMDialog::removeVLMItem()
{
    
}


void VLMDialog::clearVLMItem()
{
    
}
