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

#include <iostream>
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

VLMDialog *VLMDialog::instance = NULL;


VLMDialog::VLMDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    // UI stuff
    ui.setupUi( this );

    /* Layout in the main groupBox */
    layout = new QVBoxLayout( ui.groupBox );

    mediatype = new QComboBox( ui.groupBox );
    layout->addWidget( mediatype );

#define ADDMEDIATYPES( type ) mediatype->addItem( qtr( type ) );

    ADDMEDIATYPES( "Broadcast" );
    ADDMEDIATYPES( "Video On Demand ( VOD )" );
    ADDMEDIATYPES( "Schedule" );

    makeBcastPage();
    makeVODPage();
    makeSchedulePage();

    /* Create a Stacked Widget to old the different phases */
    slayout = new QStackedWidget( ui.groupBox );
    slayout->addWidget( pBcast );
    slayout->addWidget( pVod );
    slayout->addWidget( pSchedule );

    layout->addWidget( slayout );

    QPushButton *closeButton = new QPushButton( qtr("Close") );
    QPushButton *cancelButton = new QPushButton( qtr("Cancel") );
    ui.buttonBox->addButton( closeButton, QDialogButtonBox::AcceptRole );
    ui.buttonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    CONNECT( mediatype, currentIndexChanged( int ), slayout,
            setCurrentIndex( int ) );
    CONNECT( closeButton, clicked(), this, hide() );

}

VLMDialog::~VLMDialog(){}

void VLMDialog::makeBcastPage()
{
    pBcast = new QWidget( ui.groupBox );
    bcastlayout = new QGridLayout( pBcast );
    bcastname = new QLabel( qtr( "Name :" ),pBcast );
    bcastnameledit = new QLineEdit( pBcast );
    bcastenable = new QCheckBox( qtr( "Enable" ),pBcast );
    bcastinput = new QLabel( qtr( "Input :" ),pBcast );
    bcastinputledit = new QLineEdit( pBcast );
    bcastinputtbutton = new QToolButton( pBcast );
    bcastoutput = new QLabel( qtr( "Output :" ),pBcast );
    bcastoutputledit = new QLineEdit( pBcast );
    bcastoutputtbutton = new QToolButton( pBcast );
    bcastcontrol = new QGroupBox( qtr( "Controls" ),pBcast );
    bcastgbox = new QHBoxLayout( bcastcontrol );
    bcastplay = new QPushButton( qtr( "Play" ),bcastcontrol );
    bcastpause = new QPushButton( qtr( "Pause" ),bcastcontrol );
    bcaststop = new QPushButton( qtr( "Stop" ),bcastcontrol );
    bcastadd = new QPushButton( qtr( "Add" ),pBcast );
    bcastremove = new QPushButton( qtr( "Remove" ),pBcast );

// Adding all widgets in the QGridLayout
    bcastgbox->addWidget( bcastplay );
    bcastgbox->addWidget( bcastpause );
    bcastgbox->addWidget( bcaststop );
    bcastlayout->addWidget( bcastname,0,0 );
    bcastlayout->addWidget( bcastnameledit,0,1 );
    bcastlayout->addWidget( bcastenable,0,2 );
    bcastlayout->addWidget( bcastinput,1,0 );
    bcastlayout->addWidget( bcastinputledit,1,1 );
    bcastlayout->addWidget( bcastinputtbutton,1,2 );
    bcastlayout->addWidget( bcastoutput,2,0 );
    bcastlayout->addWidget( bcastoutputledit,2,1 );
    bcastlayout->addWidget( bcastoutputtbutton,2,2 );
    bcastlayout->addWidget( bcastcontrol,3,0,1,3 );
    bcastlayout->addWidget( bcastadd,4,1 );
    bcastlayout->addWidget( bcastremove,4,2 );
}

void VLMDialog::makeVODPage()
{
    pVod = new QWidget( ui.groupBox );
    vodlayout = new QGridLayout( pVod );
    vodname = new QLabel( qtr( "Name :" ),pVod );
    vodnameledit = new QLineEdit( pVod );
    vodenable = new QCheckBox( qtr( "Enable" ),pVod );
    vodinput = new QLabel( qtr( "Input :" ),pVod );
    vodinputledit = new QLineEdit( pVod );
    vodinputtbutton = new QToolButton( pVod );
    vodoutput = new QLabel( qtr( "Output :" ),pVod );
    vodoutputledit = new QLineEdit( pVod );
    vodoutputtbutton = new QToolButton( pVod );
    vodadd = new QPushButton( qtr( "Add" ),pVod );
    vodremove = new QPushButton( qtr( "Remove" ),pVod );

// Adding all widgets in the QGridLayout
    vodlayout->addWidget( vodname,0,0 );
    vodlayout->addWidget( vodnameledit,0,1 );
    vodlayout->addWidget( vodenable,0,2 );
    vodlayout->addWidget( vodinput,1,0 );
    vodlayout->addWidget( vodinputledit,1,1 );
    vodlayout->addWidget( vodinputtbutton,1,2 );
    vodlayout->addWidget( vodoutput,2,0 );
    vodlayout->addWidget( vodoutputledit,2,1 );
    vodlayout->addWidget( vodoutputtbutton,2,2 );
    vodlayout->addWidget( vodadd,3,1 );
    vodlayout->addWidget( vodremove,3,2 );
}

void VLMDialog::makeSchedulePage()
{
    pSchedule = new QWidget( ui.groupBox );
    schelayout = new QGridLayout( pSchedule );
    schename = new QLabel( qtr( "Name :" ),pSchedule );
    schenameledit = new QLineEdit( pSchedule );
    scheenable = new QCheckBox( qtr( "Enable" ),pSchedule );
    scheinput = new QLabel( qtr( "Input :" ),pSchedule );
    scheinputledit = new QLineEdit( pSchedule );
    scheinputtbutton = new QToolButton( pSchedule );
    scheoutput = new QLabel( qtr( "Output :" ),pSchedule );
    scheoutputledit = new QLineEdit( pSchedule );
    scheoutputtbutton = new QToolButton( pSchedule );
    schecontrol = new QGroupBox( qtr( "Time Control" ),pSchedule );
    scheadd = new QPushButton( qtr( "Add" ),pSchedule );
    scheremove = new QPushButton( qtr( "Remove" ),pSchedule );
    schetimelayout = new QGridLayout( schecontrol );
    schetimelabel = new QLabel( qtr( "Hours/Minutes/Seconds :" ), schecontrol );
    schedatelabel = new QLabel( qtr( "Day/Month/Year :" ), schecontrol );
    schetimerepeat = new QLabel( qtr( "Repeat :" ), schecontrol );
    time = new QTimeEdit( schecontrol );
    date = new QDateEdit( schecontrol );
    scherepeatnumber = new QSpinBox( schecontrol );

    //scheadd->setMaximumWidth( 30 );

// Adding all widgets in the QGridLayout
    schetimelayout->addWidget( schetimelabel,0,0 );
    schetimelayout->addWidget( time,0,1 );
    schetimelayout->addWidget( schedatelabel,1,0 );
    schetimelayout->addWidget( date,1,1 );
    schetimelayout->addWidget( schetimerepeat,2,0 );
    schetimelayout->addWidget( scherepeatnumber,2,1 );
    schelayout->addWidget( schename,0,0 );
    schelayout->addWidget( schenameledit,0,1 );
    schelayout->addWidget( scheenable,0,2 );
    schelayout->addWidget( scheinput,1,0 );
    schelayout->addWidget( scheinputledit,1,1 );
    schelayout->addWidget( scheinputtbutton,1,2 );
    schelayout->addWidget( scheoutput,2,0 );
    schelayout->addWidget( scheoutputledit,2,1 );
    schelayout->addWidget( scheoutputtbutton,2,2 );
    schelayout->addWidget( schecontrol,3,0,1,3 );
    schelayout->addWidget( scheadd,4,1 );
    schelayout->addWidget( scheremove,4,2 );
}

