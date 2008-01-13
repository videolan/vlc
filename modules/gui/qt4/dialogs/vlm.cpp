/*****************************************************************************
 * vlm.cpp : VLM Management
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id: sout.cpp 21875 2007-09-08 16:01:33Z jb $
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *          Jean-François Massol <jf.massol -at- gmail.com>
 *          Clément Sténac <zorglub@videolan.org>
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

#ifdef ENABLE_VLM
#include "dialogs/open.hpp"
#include "dialogs/sout.hpp"

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
#include <QDateTimeEdit>
#include <QSpinBox>
#include <QHeaderView>
#include <QScrollArea>

static const char *psz_type[] = { "Broadcast", "Schedule", "VOD" };

VLMDialog *VLMDialog::instance = NULL;

VLMDialog::VLMDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    p_vlm = vlm_New( p_intf );

    if( !p_vlm )
    {
        msg_Warn( p_intf, "Couldn't build VLM object ");
        return;
    }
    vlmWrapper = new VLMWrapper( p_vlm );

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
    QLabel *scherepeatTimeLabel = new QLabel( qtr( "Repeat delay:" ) );
    schetimelayout->addWidget( scherepeatTimeLabel, 3, 0 );

    time = new QDateTimeEdit( QTime::currentTime() );
    time->setAlignment( Qt::AlignRight );
    schetimelayout->addWidget( time, 0, 1, 1, 3 );

    date = new QDateTimeEdit( QDate::currentDate() );
    date->setAlignment( Qt::AlignRight );
    date->setCalendarPopup( true );
#ifdef WIN32
    date->setDisplayFormat( "dd MM yyyy" );
#else
    date->setDisplayFormat( "dd MMMM yyyy" );
#endif
    schetimelayout->addWidget( date, 1, 1, 1, 3 );

    scherepeatnumber = new QSpinBox;
    scherepeatnumber->setAlignment( Qt::AlignRight );
    schetimelayout->addWidget( scherepeatnumber, 2, 1, 1, 3 );

    repeatDays = new QSpinBox;
    repeatDays->setAlignment( Qt::AlignRight );
    schetimelayout->addWidget( repeatDays, 3, 1, 1, 1 );
    repeatDays->setSuffix( qtr(" days") );

    repeatTime = new QDateTimeEdit;
    repeatTime->setAlignment( Qt::AlignRight );
    schetimelayout->addWidget( repeatTime, 3, 2, 1, 2 );
    repeatTime->setDisplayFormat( "hh:mm:ss" );

    /* scrollArea */
    ui.vlmItemScroll->setFrameStyle( QFrame::NoFrame );
    ui.vlmItemScroll->setWidgetResizable( true );
    vlmItemWidget = new QWidget;
    vlmItemLayout = new QVBoxLayout( vlmItemWidget );
    vlmItemWidget->setLayout( vlmItemLayout );
    ui.vlmItemScroll->setWidget( vlmItemWidget );

    QSpacerItem *spacer =
        new QSpacerItem( 10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding);
    vlmItemLayout->addItem( spacer );

    QPushButton *closeButton = new QPushButton( qtr( "Close" ) );
    ui.buttonBox->addButton( closeButton, QDialogButtonBox::AcceptRole );

    showScheduleWidget( QVLM_Broadcast );

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
    BUTTONACT( ui.inputButton, selectInput() );
    BUTTONACT( ui.outputButton, selectOutput() );
}

VLMDialog::~VLMDialog()
{
   /* FIXME :you have to destroy vlm here to close
    * but we shouldn't destroy vlm here in case somebody else wants it */
    if( p_vlm )
        vlm_Delete( p_vlm );
}

void VLMDialog::showScheduleWidget( int i )
{
    ui.schedBox->setVisible( ( i == QVLM_Schedule ) );
    ui.loopBCast->setVisible( ( i == QVLM_Broadcast ) );
    ui.vodBox->setVisible( ( i == QVLM_VOD ) );
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
    QString inputText = ui.inputLedit->text();
    QString outputText = ui.outputLedit->text();
    bool b_checked = ui.enableCheck->isChecked();
    bool b_looped = ui.loopBCast->isChecked();

    VLMAWidget * vlmAwidget;

    switch( type )
    {
    case QVLM_Broadcast:
        typeShortName = "Bcast";
        vlmAwidget = new VLMBroadcast( name, inputText, outputText,
                                  b_checked, b_looped, this );
        VLMWrapper::AddBroadcast( name, inputText, outputText, b_checked, b_looped );
    break;
    case QVLM_VOD:
        typeShortName = "VOD";
        vlmAwidget = new VLMVod( name, inputText, outputText,
                                 b_checked, ui.muxLedit->text(), this );
        VLMWrapper::AddVod( name, inputText, outputText, b_checked );
        break;
    case QVLM_Schedule:
        typeShortName = "Sched";
        vlmAwidget = new VLMSchedule( name, inputText, outputText,
                                      b_checked, this );
        break;
    default:
        msg_Warn( p_intf, "Something bad happened" );
        return;
    }

    /* Add an Item of the Side List */
    ui.vlmListItem->addItem( typeShortName + " : " + name );
    ui.vlmListItem->setCurrentRow( vlmItemCount - 1 );

    /* Add a new VLMAWidget on the main List */

    vlmItemLayout->insertWidget( vlmItemCount, vlmAwidget );
    vlmItems.append( vlmAwidget );
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
    ui.loopBCast->setChecked( false );
    ui.muxLedit->clear();
    ui.saveButton->hide();
    ui.addButton->show();
}

void VLMDialog::selectInput()
{
    OpenDialog *o = OpenDialog::getInstance( this, p_intf, 0, true );
    o->exec();
    ui.inputLedit->setText( o->getMRL() );
}

void VLMDialog::selectOutput()
{
    SoutDialog *s = SoutDialog::getInstance( this, p_intf, false );
    if( s->exec() == QDialog::Accepted )
        ui.outputLedit->setText( s->getMrl() );
}

/* Object Modification */
void VLMDialog::removeVLMItem( VLMAWidget *vlmObj )
{
    int index = vlmItems.indexOf( vlmObj );
    if( index < 0 ) return;

    //FIXME, this is going to segfault if the focus in on the ListWidget
    delete ui.vlmListItem->takeItem( index );
    vlmItems.removeAt( index );
    delete vlmObj;

    /* HERE BE DRAGONS VLM REQUEST */
}

void VLMDialog::startModifyVLMItem( VLMAWidget *vlmObj )
{
    currentIndex = vlmItems.indexOf( vlmObj );
    if( currentIndex < 0 ) return;

    msg_Dbg( p_intf, "Type: %i", vlmObj->type );
    ui.vlmListItem->setCurrentRow( currentIndex );
    ui.nameLedit->setText( vlmObj->name );
    ui.inputLedit->setText( vlmObj->input );
    ui.outputLedit->setText( vlmObj->output );
    ui.enableCheck->setChecked( vlmObj->b_enabled );

    switch( vlmObj->type )
    {
    case QVLM_Broadcast:
        ui.loopBCast->setChecked( (qobject_cast<VLMBroadcast *>(vlmObj))->b_looped );
        break;
    case QVLM_VOD:
        ui.muxLedit->setText( (qobject_cast<VLMVod *>(vlmObj))->mux );
        break;
    case QVLM_Schedule:
        //(qobject_cast<VLMSchedule *>)
        break;
    }

    ui.nameLedit->setReadOnly( true );
    ui.addButton->hide();
    ui.saveButton->show();
}

void VLMDialog::saveModifications()
{
    VLMAWidget *vlmObj = vlmItems.at( currentIndex );
    if( vlmObj )
    {
        vlmObj->input = ui.inputLedit->text();
        vlmObj->output = ui.outputLedit->text();
        vlmObj->setChecked( ui.enableCheck->isChecked() );
        vlmObj->b_enabled = ui.enableCheck->isChecked();
        switch( vlmObj->type )
        {
        case QVLM_Broadcast:
            (qobject_cast<VLMBroadcast *>(vlmObj))->b_looped = ui.loopBCast->isChecked();
            break;
        case QVLM_VOD:
            (qobject_cast<VLMVod *>(vlmObj))->mux = ui.muxLedit->text();
            break;
        case QVLM_Schedule:
            break;
           //           vlmObj->
        }
        vlmObj->update(); /* It should call the correct function is VLMAWidget
                             is abstract, but I am far from sure... FIXME ? */
    }
    clearWidgets();
}

/*********************************
 * VLMAWidget - Abstract class
 ********************************/

VLMAWidget::VLMAWidget( QString _name,
                        QString _input,
                        QString _output,
                        bool _enabled,
                        VLMDialog *_parent,
                        int _type )
                      : QGroupBox( _name, _parent )
{
    parent = _parent;
    name = _name;
    input = _input;
    output = _output;
    b_enabled = _enabled;
    type = _type;

    setCheckable( true );
    setChecked( b_enabled );

    objLayout = new QGridLayout( this );
    setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Maximum );

    nameLabel = new QLabel;
    objLayout->addWidget( nameLabel, 0, 0, 1, 4 );

    /*QLabel *time = new QLabel( "--:--/--:--" );
    objLayout->addWidget( time, 1, 3, 1, 2 );*/

    QToolButton *modifyButton = new QToolButton;
    modifyButton->setIcon( QIcon( QPixmap( ":/pixmaps/menus_settings_16px.png" ) ) );
    objLayout->addWidget( modifyButton, 0, 5 );

    QToolButton *deleteButton = new QToolButton;
    deleteButton->setIcon( QIcon( QPixmap( ":/pixmaps/menus_quit_16px.png" ) ) );
    objLayout->addWidget( deleteButton, 0, 6 );

    BUTTONACT( modifyButton, modify() );
    BUTTONACT( deleteButton, del() );
    CONNECT( this, clicked( bool ), this, toggleEnabled( bool ) );
}

void VLMAWidget::modify()
{
    parent->startModifyVLMItem( this );
}

void VLMAWidget::del()
{
    parent->removeVLMItem( this );
}

//FIXME, remove me before release
void VLMAWidget::enterEvent( QEvent *event )
{
    printf( "test" );
}

void VLMAWidget::toggleEnabled( bool b_enable )
{
    VLMWrapper::EnableItem( name, b_enable );
}

/****************
 * VLMBroadcast
 ****************/
VLMBroadcast::VLMBroadcast( QString _name, QString _input, QString _output,
                            bool _enabled, bool _looped, VLMDialog *_parent)
                          : VLMAWidget( _name, _input, _output,
                                        _enabled, _parent, QVLM_Broadcast )
{
    nameLabel->setText( "Broadcast: " + name );
    type = QVLM_Broadcast;
    b_looped = _looped;

    playButton = new QToolButton;
    playButton->setIcon( QIcon( QPixmap( ":/pixmaps/play_16px.png" ) ) );
    objLayout->addWidget( playButton, 1, 0 );
    b_playing = true;

    QToolButton *stopButton = new QToolButton;
    stopButton->setIcon( QIcon( QPixmap( ":/pixmaps/stop_16px.png" ) ) );
    objLayout->addWidget( stopButton, 1, 1 );

    loopButton = new QToolButton;
    objLayout->addWidget( loopButton, 1, 2 );

    BUTTONACT( playButton, togglePlayPause() );
    BUTTONACT( stopButton, stop() );
    BUTTONACT( loopButton, toggleLoop() );

    update();
}

void VLMBroadcast::update()
{
    VLMWrapper::EditBroadcast( name, input, output, b_enabled, b_looped );
    if( b_looped )
        loopButton->setIcon( QIcon( QPixmap( ":/pixmaps/playlist_repeat_all.png" ) ) );
    else
        loopButton->setIcon( QIcon( QPixmap( ":/pixmaps/playlist_repeat_off.png" ) ) );
}

void VLMBroadcast::togglePlayPause()
{
    if( b_playing = true )
    {
        VLMWrapper::ControlBroadcast( name, ControlBroadcastPause );
        playButton->setIcon( QIcon( QPixmap( ":/pixmaps/pause_16px.png" ) ) );
    }
    else
    {
        VLMWrapper::ControlBroadcast( name, ControlBroadcastPlay );
        playButton->setIcon( QIcon( QPixmap( ":/pixmaps/play_16px.png" ) ) );
    }
    b_playing = !b_playing;
}

void VLMBroadcast::toggleLoop()
{
    b_enabled = !b_enabled;
    update();
}

void VLMBroadcast::stop()
{
    VLMWrapper::ControlBroadcast( name, ControlBroadcastStop );
    playButton->setIcon( QIcon( QPixmap( ":/pixmaps/play_16px.png" ) ) );
}

/****************
 * VLMSchedule
 ****************/
VLMSchedule::VLMSchedule( QString name, QString input, QString output,
                            bool enabled, VLMDialog *parent)
            : VLMAWidget( name, input, output, enabled, parent, QVLM_Schedule )
{
    nameLabel->setText( "Schedule: " + name );
}

void VLMSchedule::update()
{
}

/****************
 * VLMVOD
 ****************/
VLMVod::VLMVod( QString name, QString input, QString output,
                bool enabled, QString _mux, VLMDialog *parent)
       : VLMAWidget( name, input, output, enabled, parent, QVLM_VOD )
{
    nameLabel->setText( "VOD:" + name );

    mux = _mux;
    muxLabel = new QLabel;
    objLayout->addWidget( muxLabel, 1, 0 );

    update();
}

void VLMVod::update()
{
    muxLabel->setText( mux );
    VLMWrapper::EditVod( name, input, output, b_enabled, mux );
}


/*******************
 * VLMWrapper
 *******************/
vlm_t * VLMWrapper::p_vlm = NULL;

VLMWrapper::VLMWrapper( vlm_t *_p_vlm )
{
    p_vlm = _p_vlm;
}

VLMWrapper::~VLMWrapper()
{}

void VLMWrapper::AddBroadcast( const QString name, QString input,
                               QString output,
                               bool b_enabled, bool b_loop  )
{
    vlm_message_t *message;
    QString command = "new \"" + name + "\" broadcast";
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );
    EditBroadcast( name, input, output, b_enabled, b_loop );
}

void VLMWrapper::EditBroadcast( const QString name, const QString input,
                                const QString output,
                                bool b_enabled, bool b_loop  )
{
    vlm_message_t *message;
    QString command;

    command = "setup \"" + name + "\" inputdel all";
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );
    command = "setup \"" + name + "\" input \"" + input + "\"";
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );
    if( !output.isEmpty() )
    {
        command = "setup \"" + name + "\" output \"" + output + "\"";
        vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
        vlm_MessageDelete( message );
    }
    if( b_enabled )
    {
        command = "setup \"" + name + "\" enabled";
        vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
        vlm_MessageDelete( message );
    }
    if( b_loop )
    {
        command = "setup \"" + name + "\" loop";
        vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
        vlm_MessageDelete( message );
    }
}

void VLMWrapper::EnableItem( const QString name, bool b_enable )
{
    vlm_message_t *message;
    QString command = "setup \"" + name + ( b_enable ? " enable" : " disable" );
}

void VLMWrapper::ControlBroadcast( const QString name, int BroadcastStatus,
                                   unsigned int seek )
{
    vlm_message_t *message;

    QString command = "control \"" + name;
    switch( BroadcastStatus )
    {
    case ControlBroadcastPlay:
        command += " play";
        break;
    case ControlBroadcastPause:
        command += " pause";
        break;
    case ControlBroadcastStop:
        command += " stop";
        break;
    case ControlBroadcastSeek:
        command += " seek" + seek;
        break;
    }
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );
}

void VLMWrapper::AddVod( const QString name, const QString input,
                         const QString output,
                         bool b_enabled, const QString mux )
{
    vlm_message_t *message;
    QString command = "new \"" + name + "\" vod";
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );
    EditVod(  name, input, output, b_enabled, mux );
}

void VLMWrapper::EditVod( const QString name, const QString input,
                          const QString output,
                          bool b_enabled,
                          const QString mux )
{
    vlm_message_t *message;
    QString command = "setup \"" + name + "\" input \"" + input + "\"";
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );
    if( !output.isEmpty() )
    {
        command = "setup \"" + name + "\" output \"" + output + "\"";
        vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
        vlm_MessageDelete( message );
    }
    if( b_enabled )
    {
        command = "setup \"" + name + "\" enabled";
        vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
        vlm_MessageDelete( message );
    }
    if( !mux.isEmpty() )
    {
        command = "setup \"" + name + "\" mux \"" + mux + "\"";
        vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
        vlm_MessageDelete( message );
    }
}
#endif
