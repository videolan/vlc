/*****************************************************************************
 * vlm.cpp : VLM Management
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id: sout.cpp 21875 2007-09-08 16:01:33Z jb $
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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
#include <QScrollArea>

static const char *psz_type[] = { "Broadcast", "Schedule", "VOD" };

VLMDialog *VLMDialog::instance = NULL;

VLMDialog::VLMDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    // UI stuff
    ui.setupUi( this );
    ui.saveButton->hide();

#define ADDMEDIATYPES( str, type ) ui.mediaType->addItem( qtr( str ), QVariant( type ) );
    ADDMEDIATYPES( "Broadcast", QVLM_Broadcast );
    ADDMEDIATYPES( "Schedule", QVLM_Schedule );
    ADDMEDIATYPES( "Video On Demand ( VOD )", QVLM_VOD );
#undef ADDMEDIATYPES

    /* Schedule Stuffs */
    QGridLayout *schetimelayout = new QGridLayout( ui.schedBox );
    QLabel *schetimelabel = new QLabel( qtr( "Hours/Minutes/Seconds:" ) );
    schetimelayout->addWidget( schetimelabel, 0, 0 );
    QLabel *schedatelabel = new QLabel( qtr( "Day Month Year:" ) );
    schetimelayout->addWidget( schedatelabel, 1, 0 );
    QLabel *scherepeatLabel = new QLabel( qtr( "Repeat:" ) );
    schetimelayout->addWidget( scherepeatLabel, 2, 0 );
    
    time = new QTimeEdit( QTime::currentTime() );
    time->setAlignment( Qt::AlignRight );
    schetimelayout->addWidget( time, 0, 1 );
    
    date = new QDateEdit( QDate::currentDate() );
    date->setAlignment( Qt::AlignRight );
#ifdef WIN32
    date->setDisplayFormat( "dd MM yyyy" );
#else
    date->setDisplayFormat( "dd MMMM yyyy" );
#endif
    schetimelayout->addWidget( date, 1, 1 );

    scherepeatnumber = new QSpinBox;
    scherepeatnumber->setAlignment( Qt::AlignRight );
    schetimelayout->addWidget( scherepeatnumber, 2, 1 );
    
    /* scrollArea */
    ui.vlmItemScroll->setFrameStyle( QFrame::NoFrame );
    ui.vlmItemScroll->setWidgetResizable( true );
    vlmItemWidget = new QWidget;
    vlmItemLayout = new QVBoxLayout( vlmItemWidget );
    vlmItemWidget->setLayout( vlmItemLayout );
    ui.vlmItemScroll->setWidget( vlmItemWidget );

    QSpacerItem *spacer = new QSpacerItem( 10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding);
    vlmItemLayout->addItem( spacer );

    QPushButton *closeButton = new QPushButton( qtr( "Close" ) );
    ui.buttonBox->addButton( closeButton, QDialogButtonBox::AcceptRole );

    ui.schedBox->hide();

    /* Connect the comboBox to show the right Widgets */
    CONNECT( ui.mediaType, currentIndexChanged( int ),
             this, showScheduleWidget( int ) );

    /* Connect the leftList to show the good VLMItem */
    CONNECT( ui.vlmListItem, currentRowChanged( int ),
             this, selectVLMItem( int ) );
    
    BUTTONACT( closeButton, close() );
    BUTTONACT( ui.addButton, addVLMItem() );
    BUTTONACT( ui.clearButton, clearWidgets() );
    BUTTONACT( ui.saveButton, saveModifications() );
}

VLMDialog::~VLMDialog(){}

void VLMDialog::showScheduleWidget( int i )
{
    ui.schedBox->setVisible( ( i == QVLM_Schedule ) );
}

void VLMDialog::selectVLMItem( int i )
{
    ui.vlmItemScroll->ensureWidgetVisible( vlmItems.at( i ) );
}

bool VLMDialog::isNameGenuine( QString name )
{
    for( int i = 0; i < vlmItems.size(); i++ )
    {
        if( vlmItems.at( i )->name == name )
            return false;
    }
    return true;
}

void VLMDialog::addVLMItem()
{
    int vlmItemCount = vlmItems.size();

    /* Take the name and Check it */
    QString name = ui.nameLedit->text();
    if( name.isEmpty() || !isNameGenuine( name ) )
    {
        msg_Dbg( p_intf, "VLM Name is empty or already exists, I can't do it" );
        return;
    }

    int type = ui.mediaType->itemData( ui.mediaType->currentIndex() ).toInt();
    QString typeShortName;
    
    switch( type )
    {
    case QVLM_Broadcast:
        typeShortName = "Bcast";
    break;
    case QVLM_VOD:
        typeShortName = "VOD";
        break;
    case QVLM_Schedule:
        typeShortName = "Sched";
        break;
    default:
        msg_Warn( p_intf, "Something bad happened" );
        return;
    }

    /* Add an Item of the Side List */
    ui.vlmListItem->addItem( typeShortName + " : " + name );
    ui.vlmListItem->setCurrentRow( vlmItemCount - 1 );

    /* Add a new VLMObject on the main List */
    VLMObject *vlmObject = new VLMObject( type, name, 
                                          ui.inputLedit->text(),
                                          ui.outputLedit->text(),
                                          ui.enableCheck->isChecked(),
                                          this );

    vlmItemLayout->insertWidget( vlmItemCount, vlmObject );
    vlmItems.append( vlmObject );

    /* HERE BE DRAGONS VLM REQUEST */
}

void VLMDialog::clearWidgets()
{
    ui.nameLedit->clear();
    ui.inputLedit->clear();
    ui.outputLedit->clear();
    time->setTime( QTime::currentTime() );
    date->setDate( QDate::currentDate() );
    ui.enableCheck->setChecked( true );
    ui.nameLedit->setReadOnly( false );
}

void VLMDialog::saveModifications()
{
    VLMObject *vlmObj = vlmItems.at( currentIndex );
    if( vlmObj )
    {
        vlmObj->input = ui.inputLedit->text();
        vlmObj->output = ui.outputLedit->text();
        vlmObj->setChecked( ui.enableCheck->isChecked() );
        vlmObj->b_enabled = ui.enableCheck->isChecked();
    }
    ui.saveButton->hide();
    ui.addButton->show();
    clearWidgets();
}    

/* Object Modification */
void VLMDialog::removeVLMItem( VLMObject *vlmObj )
{
    int index = vlmItems.indexOf( vlmObj );
    if( index < 0 ) return;
    
    delete ui.vlmListItem->takeItem( index );
    vlmItems.removeAt( index );    
    delete vlmObj;

    /* HERE BE DRAGONS VLM REQUEST */
}

void VLMDialog::startModifyVLMItem( VLMObject *vlmObj )
{
    currentIndex = vlmItems.indexOf( vlmObj );
    if( currentIndex < 0 ) return;

    ui.vlmListItem->setCurrentRow( currentIndex );
    ui.nameLedit->setText( vlmObj->name );
    ui.inputLedit->setText( vlmObj->input );
    ui.outputLedit->setText( vlmObj->output );
    ui.enableCheck->setChecked( vlmObj->b_enabled );

    ui.nameLedit->setReadOnly( true );
    ui.addButton->hide();
    ui.saveButton->show();
}


/*********************************
 * VLMObject
 ********************************/
VLMObject::VLMObject( int type, 
                      QString _name,
                      QString _input,
                      QString _output,
                      bool _enabled,
                      VLMDialog *_parent ) 
         : QGroupBox( _name, _parent )
{
    parent = _parent;
    name = _name;
    input = _input;
    output = _output;
    b_enabled = _enabled;
    setChecked( b_enabled );

    QGridLayout *objLayout = new QGridLayout( this );
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Maximum );
    
    QLabel *label = new QLabel( psz_type[type] + ( ": " + name ) );
    objLayout->addWidget( label, 0, 0, 1, 4 );

    QToolButton *playButton = new QToolButton;
    playButton->setIcon( QIcon( QPixmap( ":/pixmaps/play_16px.png" ) ) );
    objLayout->addWidget( playButton, 1, 0 );
    
    QToolButton *stopButton = new QToolButton;
    stopButton->setIcon( QIcon( QPixmap( ":/pixmaps/stop_16px.png" ) ) );
    objLayout->addWidget( stopButton, 1, 1 );
    
    QToolButton *loopButton = new QToolButton;
    loopButton->setIcon( QIcon( QPixmap( ":/pixmaps/playlist_repeat_off.png" ) ) );
    objLayout->addWidget( loopButton, 1, 2 );
    
    QLabel *time = new QLabel( "--:--/--:--" );
    objLayout->addWidget( time, 1, 3, 1, 2 );

    QToolButton *modifyButton = new QToolButton;
    modifyButton->setIcon( QIcon( QPixmap( ":/pixmaps/menus_settings_16px.png" ) ) );
    objLayout->addWidget( modifyButton, 0, 5 );
    
    QToolButton *deleteButton = new QToolButton;
    deleteButton->setIcon( QIcon( QPixmap( ":/pixmaps/menus_quit_16px.png" ) ) );
    objLayout->addWidget( deleteButton, 0, 6 );
    
    BUTTONACT( playButton, togglePlayPause() );
    BUTTONACT( stopButton, stop() );
    BUTTONACT( modifyButton, modify() );
    BUTTONACT( deleteButton, del() );
    BUTTONACT( loopButton, toggleLoop() );
}

void VLMObject::modify()
{
    parent->startModifyVLMItem( this );
}

void VLMObject::del()
{
    parent->removeVLMItem( this );
}

void VLMObject::togglePlayPause()
{
    
}

void VLMObject::toggleLoop()
{
    
}

void VLMObject::stop()
{
    
}

void VLMObject::enterEvent( QEvent *event )
{
    printf( "test" );
}
