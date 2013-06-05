/*****************************************************************************
 * vlm.cpp : VLM Management
 ****************************************************************************
 * Copyright © 2008 the VideoLAN team
 * $Id$
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/vlm.hpp"

#ifdef ENABLE_VLM
#include "dialogs/open.hpp"
#include "dialogs/sout.hpp"
#include "util/qt_dirs.hpp"

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
#include <QDateTime>
#include <QSpinBox>
#include <QScrollArea>
#include <QFileDialog>


VLMDialog::VLMDialog( intf_thread_t *_p_intf ) : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
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
    ADDMEDIATYPES( N_("Broadcast"), QVLM_Broadcast );
    ADDMEDIATYPES( N_("Schedule"), QVLM_Schedule );
    ADDMEDIATYPES( N_("Video On Demand ( VOD )"), QVLM_VOD );
#undef ADDMEDIATYPES

    /* Schedule Stuffs */
    QGridLayout *schetimelayout = new QGridLayout( ui.schedBox );
    QLabel *schetimelabel = new QLabel( qtr( "Hours / Minutes / Seconds:" ) );
    schetimelayout->addWidget( schetimelabel, 0, 0 );
    QLabel *schedatelabel = new QLabel( qtr( "Day / Month / Year:" ) );
    schetimelayout->addWidget( schedatelabel, 1, 0 );
    QLabel *scherepeatLabel = new QLabel( qtr( "Repeat:" ) );
    schetimelayout->addWidget( scherepeatLabel, 2, 0 );
    QLabel *scherepeatTimeLabel = new QLabel( qtr( "Repeat delay:" ) );
    schetimelayout->addWidget( scherepeatTimeLabel, 3, 0 );

    time = new QDateTimeEdit( QTime::currentTime() );
    time->setAlignment( Qt::AlignRight );
    time->setDisplayFormat( "hh:mm:ss" );
    schetimelayout->addWidget( time, 0, 1, 1, 3 );

    date = new QDateTimeEdit( QDate::currentDate() );
    date->setAlignment( Qt::AlignRight );
    date->setCalendarPopup( true );
#ifdef _WIN32
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

    QPushButton *importButton = new QPushButton( qtr( "I&mport" ) );
    ui.buttonBox->addButton( importButton, QDialogButtonBox::ActionRole );

    QPushButton *exportButton = new QPushButton( qtr( "E&xport" ) );
    ui.buttonBox->addButton( exportButton, QDialogButtonBox::ActionRole );

    QPushButton *closeButton = new QPushButton( qtr( "&Close" ) );
    ui.buttonBox->addButton( closeButton, QDialogButtonBox::RejectRole );


    showScheduleWidget( QVLM_Broadcast );

    /* Connect the comboBox to show the right Widgets */
    CONNECT( ui.mediaType, currentIndexChanged( int ),
             this, showScheduleWidget( int ) );

    /* Connect the leftList to show the good VLMItem */
    CONNECT( ui.vlmListItem, currentRowChanged( int ),
             this, selectVLMItem( int ) );

    BUTTONACT( closeButton, close() );
    BUTTONACT( exportButton, exportVLMConf() );
    BUTTONACT( importButton, importVLMConf() );
    BUTTONACT( ui.addButton, addVLMItem() );
    BUTTONACT( ui.clearButton, clearWidgets() );
    BUTTONACT( ui.saveButton, saveModifications() );
    BUTTONACT( ui.inputButton, selectInput() );
    BUTTONACT( ui.outputButton, selectOutput() );

    if( !restoreGeometry( getSettings()->value("VLM/geometry").toByteArray() ) )
    {
        resize( QSize( 700, 500 ) );
    }
}

VLMDialog::~VLMDialog()
{
    delete vlmWrapper;

    getSettings()->setValue("VLM/geometry", saveGeometry());
   /* TODO :you have to destroy vlm here to close
    * but we shouldn't destroy vlm here in case somebody else wants it */
    if( p_vlm )
    {
        vlm_Delete( p_vlm );
    }
}

void VLMDialog::showScheduleWidget( int i )
{
    ui.schedBox->setVisible( ( i == QVLM_Schedule ) );
    ui.loopBCast->setVisible( ( i == QVLM_Broadcast ) );
    ui.vodBox->setVisible( ( i == QVLM_VOD ) );
}

void VLMDialog::selectVLMItem( int i )
{
    if( i >= 0 )
        ui.vlmItemScroll->ensureWidgetVisible( vlmItems.at( i ) );
}

bool VLMDialog::isNameGenuine( const QString& name )
{
    for( int i = 0; i < vlmItems.count(); i++ )
    {
        if( vlmItems.at( i )->name == name )
            return false;
    }
    return true;
}

void VLMDialog::addVLMItem()
{
    int vlmItemCount = vlmItems.count();

    /* Take the name and Check it */
    QString name = ui.nameLedit->text();
    if( name.isEmpty() || !isNameGenuine( name ) )
    {
        msg_Err( p_intf, "VLM Name is empty or already exists, I can't do it" );
        return;
    }

    int type = ui.mediaType->itemData( ui.mediaType->currentIndex() ).toInt();

    QString typeShortName;
    QString inputText = ui.inputLedit->text();
    QString outputText = ui.outputLedit->text();
    bool b_checked = ui.enableCheck->isChecked();
    bool b_looped = ui.loopBCast->isChecked();
    QDateTime schetime = time->dateTime();
    QDateTime schedate = date->dateTime();
    int repeatnum = scherepeatnumber->value();
    int repeatdays = repeatDays->value();
    VLMAWidget * vlmAwidget;
    outputText.remove( ":sout=" );

    switch( type )
    {
    case QVLM_Broadcast:
        typeShortName = "Bcast";
        vlmAwidget = new VLMBroadcast( name, inputText, inputOptions, outputText,
                                       b_checked, b_looped, this );
        VLMWrapper::AddBroadcast( name, inputText, inputOptions, outputText, b_checked,
                                  b_looped );
    break;
    case QVLM_VOD:
        typeShortName = "VOD";
        vlmAwidget = new VLMVod( name, inputText, inputOptions, outputText,
                                 b_checked, ui.muxLedit->text(), this );
        VLMWrapper::AddVod( name, inputText, inputOptions, outputText, b_checked );
        break;
    case QVLM_Schedule:
        typeShortName = "Sched";
        vlmAwidget = new VLMSchedule( name, inputText, inputOptions, outputText,
                                      schetime, schedate, repeatnum,
                                      repeatdays, b_checked, this );
        VLMWrapper::AddSchedule( name, inputText, inputOptions, outputText, schetime,
                                 schedate, repeatnum, repeatdays, b_checked);
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
    clearWidgets();
}

/* TODO : VOD are not exported to the file */
bool VLMDialog::exportVLMConf()
{
    QString saveVLMConfFileName = QFileDialog::getSaveFileName( this,
                                        qtr( "Save VLM configuration as..." ),
                                        QVLCUserDir( VLC_DOCUMENTS_DIR ),
                                        qtr( "VLM conf (*.vlm);;All (*)" ) );

    if( !saveVLMConfFileName.isEmpty() )
    {
        vlm_message_t *message;
        QString command = "save \"" + saveVLMConfFileName + "\"";
        vlm_ExecuteCommand( p_vlm , qtu( command ) , &message );
        vlm_MessageDelete( message );
        return true;
    }

    return false;
}

void VLMDialog::mediasPopulator()
{
    if( p_vlm )
    {
        int i_nMedias;
        QString typeShortName;
        int vlmItemCount;
        vlm_media_t ***ppp_dsc = (vlm_media_t ***)malloc( sizeof( vlm_media_t ) );

        /* Get medias information and numbers */
        vlm_Control( p_vlm, VLM_GET_MEDIAS, ppp_dsc, &i_nMedias );

        /* Loop on all of them */
        for( int i = 0; i < i_nMedias; i++ )
        {
            VLMAWidget * vlmAwidget;
            vlmItemCount = vlmItems.count();

            QString mediaName = qfu( (*ppp_dsc)[i]->psz_name );
            /* It may have several inputs, we take the first one by default
                 - an evolution will be to manage these inputs in the gui */
            QString inputText = qfu( (*ppp_dsc)[i]->ppsz_input[0] );

            QString outputText = qfu( (*ppp_dsc)[i]->psz_output );

            /* Schedule media is a quite especial, maybe there is another way to grab information */
            if( (*ppp_dsc)[i]->b_vod )
            {
                typeShortName = "VOD";
                QString mux = qfu( (*ppp_dsc)[i]->vod.psz_mux );
                vlmAwidget = new VLMVod( mediaName, inputText, inputOptions,
                                         outputText, (*ppp_dsc)[i]->b_enabled,
                                         mux, this );
            }
            else
            {
                typeShortName = "Bcast";
                vlmAwidget = new VLMBroadcast( mediaName, inputText, inputOptions,
                                               outputText, (*ppp_dsc)[i]->b_enabled,
                                               (*ppp_dsc)[i]->broadcast.b_loop, this );
            }
            /* Add an Item of the Side List */
            ui.vlmListItem->addItem( typeShortName + " : " + mediaName );
            ui.vlmListItem->setCurrentRow( vlmItemCount - 1 );

            /* Add a new VLMAWidget on the main List */
            vlmItemLayout->insertWidget( vlmItemCount, vlmAwidget );
            vlmItems.append( vlmAwidget );
            clearWidgets();
        }
        free( ppp_dsc );
    }
}

bool VLMDialog::importVLMConf()
{
    QString openVLMConfFileName = toNativeSeparators(
            QFileDialog::getOpenFileName(
            this, qtr( "Open VLM configuration..." ),
            QVLCUserDir( VLC_DOCUMENTS_DIR ),
            qtr( "VLM conf (*.vlm);;All (*)" ) ) );

    if( !openVLMConfFileName.isEmpty() )
    {
        vlm_message_t *message;
        int status;
        QString command = "load \"" + openVLMConfFileName + "\"";
        status = vlm_ExecuteCommand( p_vlm, qtu( command ) , &message );
        vlm_MessageDelete( message );
        if( status == 0 )
        {
            mediasPopulator();
        }
        else
        {
            msg_Warn( p_intf, "Failed to import vlm configuration file : %s", qtu( command ) );
            return false;
        }
        return true;
    }
    return false;
}

void VLMDialog::clearWidgets()
{
    ui.nameLedit->clear();
    ui.inputLedit->clear();
    inputOptions.clear();
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
    OpenDialog *o = OpenDialog::getInstance( this, p_intf, false, SELECT, true );
    o->exec();
    ui.inputLedit->setText( o->getMRL( false ) );
    inputOptions = o->getOptions();
}

void VLMDialog::selectOutput()
{
    SoutDialog *s = new SoutDialog( this, p_intf );
    if( s->exec() == QDialog::Accepted )
    {
        int i = s->getMrl().indexOf( " " );
        ui.outputLedit->setText( s->getMrl().left( i ) );
    }
}

/* Object Modification */
void VLMDialog::removeVLMItem( VLMAWidget *vlmObj )
{
    int index = vlmItems.indexOf( vlmObj );
    if( index < 0 ) return;
    delete ui.vlmListItem->takeItem( index );
    vlmItems.removeAt( index );
    delete vlmObj;

    /* HERE BE DRAGONS VLM REQUEST */
}

void VLMDialog::startModifyVLMItem( VLMAWidget *vlmObj )
{
    currentIndex = vlmItems.indexOf( vlmObj );
    if( currentIndex < 0 ) return;

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
        time->setDateTime( ( qobject_cast<VLMSchedule *>(vlmObj))->schetime );
        date->setDateTime( ( qobject_cast<VLMSchedule *>(vlmObj))->schedate );
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
        vlmObj->output = ui.outputLedit->text().remove( ":sout=" );
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
            (qobject_cast<VLMSchedule *>(vlmObj))->schetime = time->dateTime();
            (qobject_cast<VLMSchedule *>(vlmObj))->schedate = date->dateTime();
            (qobject_cast<VLMSchedule *>(vlmObj))->rNumber = scherepeatnumber->value();
            (qobject_cast<VLMSchedule *>(vlmObj))->rDays = repeatDays->value();
            break;
           //           vlmObj->
        }
        vlmObj->update();
    }
    clearWidgets();
}

/*********************************
 * VLMAWidget - Abstract class
 ********************************/

VLMAWidget::VLMAWidget( const QString& _name, const QString& _input,
                        const QString& _inputOptions, const QString& _output,
                        bool _enabled, VLMDialog *_parent, int _type )
                      : QGroupBox( _name, _parent )
{
    parent = _parent;
    name = _name;
    input = _input;
    inputOptions = _inputOptions;
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
    modifyButton->setIcon( QIcon( ":/menu/settings" ) );
    modifyButton->setToolTip( qtr("Change") );
    objLayout->addWidget( modifyButton, 0, 5 );

    QToolButton *deleteButton = new QToolButton;
    deleteButton->setIcon( QIcon( ":/menu/quit" ) );
    deleteButton->setToolTip("Delete");
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

void VLMAWidget::toggleEnabled( bool b_enable )
{
    VLMWrapper::EnableItem( name, b_enable );
}

/****************
 * VLMBroadcast
 ****************/
VLMBroadcast::VLMBroadcast( const QString& _name, const QString& _input,
                            const QString& _inputOptions,
                            const QString& _output, bool _enabled,
                            bool _looped, VLMDialog *_parent )
                          : VLMAWidget( _name, _input, _inputOptions, _output,
                                        _enabled, _parent, QVLM_Broadcast )
{
    nameLabel->setText( qtr("Broadcast: ") + name );
    type = QVLM_Broadcast;
    b_looped = _looped;

    playButton = new QToolButton;
    playButton->setIcon( QIcon( ":/menu/play" ) );
    playButton->setToolTip( qtr("Play") );
    objLayout->addWidget( playButton, 1, 0 );
    b_playing = true;

    QToolButton *stopButton = new QToolButton;
    stopButton->setIcon( QIcon( ":/toolbar/stop_b" ) );
    stopButton->setToolTip( qtr("Stop") );
    objLayout->addWidget( stopButton, 1, 1 );

    loopButton = new QToolButton;
    loopButton->setToolTip( qtr("Repeat") );
    objLayout->addWidget( loopButton, 1, 2 );

    BUTTONACT( playButton, togglePlayPause() );
    BUTTONACT( stopButton, stop() );
    BUTTONACT( loopButton, toggleLoop() );

    update();
}

void VLMBroadcast::update()
{
    VLMWrapper::EditBroadcast( name, input, inputOptions, output, b_enabled, b_looped );
    if( b_looped )
        loopButton->setIcon( QIcon( ":/buttons/playlist/repeat_all" ) );
    else
        loopButton->setIcon( QIcon( ":/buttons/playlist/repeat_off" ) );
}

void VLMBroadcast::togglePlayPause()
{
    if( b_playing )
    {
        VLMWrapper::ControlBroadcast( name, ControlBroadcastPause );
        playButton->setIcon( QIcon( ":/menu/pause" ) );
    }
    else
    {
        VLMWrapper::ControlBroadcast( name, ControlBroadcastPlay );
        playButton->setIcon( QIcon( ":/menu/play" ) );
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
    playButton->setIcon( QIcon( ":/menu/play" ) );
}

/****************
 * VLMSchedule
 ****************/
VLMSchedule::VLMSchedule( const QString& name_, const QString& input,
                          const QString& inputOptions,
                          const QString& output, QDateTime _schetime,
                          QDateTime _schedate, int _scherepeatnumber,
                          int _repeatDays, bool enabled, VLMDialog *parent )
            : VLMAWidget( name_, input, inputOptions, output, enabled, parent,
                          QVLM_Schedule )
{
    nameLabel->setText( qtr("Schedule: ") + name );
    schetime = _schetime;
    schedate = _schedate;
    rNumber = _scherepeatnumber;
    rDays = _repeatDays;
    type = QVLM_Schedule;
    update();
}

void VLMSchedule::update()
{
   VLMWrapper::EditSchedule( name, input, inputOptions, output, schetime, schedate,
                             rNumber, rDays, b_enabled);
}

/****************
 * VLMVOD
 ****************/
VLMVod::VLMVod( const QString& name_, const QString& input,
                const QString& inputOptions, const QString& output,
                bool enabled, const QString& _mux, VLMDialog *parent)
       : VLMAWidget( name_, input, inputOptions, output, enabled, parent,
                     QVLM_VOD )
{
    nameLabel->setText( qtr("VOD: ") + name );

    mux = _mux;
    muxLabel = new QLabel;
    objLayout->addWidget( muxLabel, 1, 0 );

    update();
}

void VLMVod::update()
{
    muxLabel->setText( mux );
    VLMWrapper::EditVod( name, input, inputOptions, output, b_enabled, mux );
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
{
    p_vlm = NULL;
}

void VLMWrapper::AddBroadcast( const QString& name, const QString& input,
                               const QString& inputOptions, const QString& output,
                               bool b_enabled, bool b_loop  )
{
    vlm_message_t *message;
    QString command = "new \"" + name + "\" broadcast";
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );
    EditBroadcast( name, input, inputOptions, output, b_enabled, b_loop );
}

void VLMWrapper::EditBroadcast( const QString& name, const QString& input,
                                const QString& inputOptions, const QString& output,
                                bool b_enabled, bool b_loop  )
{
    vlm_message_t *message;
    QString command;

    command = "setup \"" + name + "\" inputdel all";
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );

    if( !input.isEmpty() )
    {
        command = "setup \"" + name + "\" input \"" + input + "\"";
        vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
        vlm_MessageDelete( message );

        QStringList options = inputOptions.split( " :", QString::SkipEmptyParts );
        for( int i = 0; i < options.count(); i++ )
        {
            command = "setup \"" + name + "\" option \"" + options[i].trimmed() + "\"";
            vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
            vlm_MessageDelete( message );
        }
    }

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

void VLMWrapper::EnableItem( const QString& name, bool b_enable )
{
    vlm_message_t *message;
    QString command = "setup \"" + name + ( b_enable ? " enable" : " disable" );
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );
}

void VLMWrapper::ControlBroadcast( const QString& name, int BroadcastStatus,
                                   unsigned int seek )
{
    vlm_message_t *message;

    QString command = "control \"" + name + "\"";
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

void VLMWrapper::AddVod( const QString& name, const QString& input,
                         const QString& inputOptions, const QString& output,
                         bool b_enabled, const QString& mux )
{
    vlm_message_t *message;
    QString command = "new \"" + name + "\" vod";
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );
    EditVod(  name, input, inputOptions, output, b_enabled, mux );
}

void VLMWrapper::EditVod( const QString& name, const QString& input,
                          const QString& inputOptions, const QString& output,
                          bool b_enabled,
                          const QString& mux )
{
    vlm_message_t *message;
    QString command;

    if( !input.isEmpty() )
    {
        command = "setup \"" + name + "\" input \"" + input + "\"";
        vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
        vlm_MessageDelete( message );

        QStringList options = inputOptions.split( " :", QString::SkipEmptyParts );
        for( int i = 0; i < options.count(); i++ )
        {
            command = "setup \"" + name + "\" option \"" + options[i].trimmed() + "\"";
            vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
            vlm_MessageDelete( message );
        }
    }

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

void VLMWrapper::AddSchedule( const QString& name, const QString& input,
                              const QString& inputOptions, const QString& output,
                              QDateTime _schetime, QDateTime _schedate,
                              int _scherepeatnumber, int _repeatDays,
                              bool b_enabled, const QString& mux )
{
    vlm_message_t *message;
    QString command = "new \"" + name + "\" schedule";
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );
    EditSchedule(  name, input, inputOptions, output, _schetime, _schedate,
            _scherepeatnumber, _repeatDays, b_enabled, mux );
}

void VLMWrapper::EditSchedule( const QString& name, const QString& input,
                               const QString& inputOptions, const QString& output,
                               QDateTime _schetime, QDateTime _schedate,
                               int _scherepeatnumber, int _repeatDays,
                               bool b_enabled, const QString& mux )
{
    vlm_message_t *message;
    QString command;

    if( !input.isEmpty() )
    {
        command = "setup \"" + name + "\" input \"" + input + "\"";
        vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
        vlm_MessageDelete( message );

        QStringList options = inputOptions.split( " :", QString::SkipEmptyParts );
        for( int i = 0; i < options.count(); i++ )
        {
            command = "setup \"" + name + "\" option \"" + options[i].trimmed() + "\"";
            vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
            vlm_MessageDelete( message );
        }
    }

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

    command = "setup \"" + name + "\" date \"" +
        _schedate.toString( "yyyy/MM/dd" )+ "-" +
        _schetime.toString( "hh:mm:ss" ) + "\"";
    vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
    vlm_MessageDelete( message );

    if( _scherepeatnumber > 0 )
    {
       command = "setup \"" + name + "\" repeat \"" + _scherepeatnumber + "\"";
       vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
       vlm_MessageDelete( message );
    }

    if( _repeatDays > 0 )
    {
       command = "setup \"" + name + "\" period \"" + _repeatDays + "\"";
       vlm_ExecuteCommand( p_vlm, qtu( command ), &message );
       vlm_MessageDelete( message );
    }
}

void VLMDialog::toggleVisible()
{
    qDeleteAll( vlmItems );
    vlmItems.clear();

    ui.vlmListItem->clear();
    mediasPopulator();
    QVLCDialog::toggleVisible();
}


#endif
