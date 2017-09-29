/*****************************************************************************
 * open.cpp : Advanced open dialog
 *****************************************************************************
 * Copyright © 2006-2011 the VideoLAN team
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include "dialogs/open.hpp"
#include "dialogs_provider.hpp"
#include "recents.hpp"
#include "util/qt_dirs.hpp"

#include <QTabWidget>
#include <QRegExp>
#include <QMenu>

#ifndef NDEBUG
# define DEBUG_QT 1
#endif

OpenDialog *OpenDialog::instance = NULL;

OpenDialog* OpenDialog::getInstance( QWidget *parent, intf_thread_t *p_intf,
        bool b_rawInstance, int _action_flag, bool b_selectMode, bool _b_pl )
{
    /* Creation */
    if( !instance )
        instance = new OpenDialog( parent, p_intf, b_selectMode,
                                   _action_flag, _b_pl );
    else if( !b_rawInstance )
    {
        /* Request the instance but change small details:
           - Button menu */
        if( b_selectMode )
            _action_flag = SELECT; /* This should be useless, but we never know
                                      if the call is correct */
        instance->setWindowModality( Qt::WindowModal );
        instance->i_action_flag = _action_flag;
        instance->b_pl = _b_pl;
        instance->setMenuAction();
    }
    return instance;
}

OpenDialog::OpenDialog( QWidget *parent,
                        intf_thread_t *_p_intf,
                        bool b_selectMode,
                        int _action_flag,
                        bool _b_pl)  :  QVLCDialog( parent, _p_intf )
{
    i_action_flag = _action_flag;
    b_pl =_b_pl;

    if( b_selectMode ) /* Select mode */
        i_action_flag = SELECT;

    /* Basic Creation of the Window */
    ui.setupUi( this );
    setWindowTitle( qtr( "Open Media" ) );
    setWindowRole( "vlc-open-media" );
    setWindowModality( Qt::WindowModal );

    /* Tab definition and creation */
    fileOpenPanel    = new FileOpenPanel( this, p_intf );
    discOpenPanel    = new DiscOpenPanel( this, p_intf );
    netOpenPanel     = new NetOpenPanel( this, p_intf );
    captureOpenPanel = new CaptureOpenPanel( this, p_intf );

    /* Insert the tabs */
    ui.Tab->insertTab( OPEN_FILE_TAB, fileOpenPanel, QIcon( ":/type/file-asym.svg" ),
                       qtr( "&File" ) );
    ui.Tab->insertTab( OPEN_DISC_TAB, discOpenPanel, QIcon( ":/type/disc.svg" ),
                       qtr( "&Disc" ) );
    ui.Tab->insertTab( OPEN_NETWORK_TAB, netOpenPanel, QIcon( ":/type/network.svg" ),
                       qtr( "&Network" ) );
    ui.Tab->insertTab( OPEN_CAPTURE_TAB, captureOpenPanel,
                       QIcon( ":/type/capture-card.svg" ), qtr( "Capture &Device" ) );

    /* Hide the Slave input widgets */
    ui.slaveLabel->hide();
    ui.slaveText->hide();
    ui.slaveBrowseButton->hide();

    /* Buttons Creation */
    /* Play Button */
    playButton = ui.playButton;

    /* Cancel Button */
    cancelButton = new QPushButton( qtr( "&Cancel" ) );

    /* Select Button */
    selectButton = new QPushButton( qtr( "&Select" ) );

    /* Menu for the Play button */
    QMenu * openButtonMenu = new QMenu( "Open", playButton );
    openButtonMenu->addAction( qtr( "&Enqueue" ), this, SLOT( enqueue() ),
                                    QKeySequence( "Alt+E" ) );
    openButtonMenu->addAction( qtr( "&Play" ), this, SLOT( play() ),
                                    QKeySequence( "Alt+P" ) );
    openButtonMenu->addAction( qtr( "&Stream" ), this, SLOT( stream() ) ,
                                    QKeySequence( "Alt+S" ) );
    openButtonMenu->addAction( qtr( "C&onvert" ), this, SLOT( transcode() ) ,
                                    QKeySequence( "Alt+O" ) );

    playButton->setMenu( openButtonMenu );

    /* Add the three Buttons */
    ui.buttonsBox->addButton( selectButton, QDialogButtonBox::AcceptRole );
    ui.buttonsBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    /* At creation time, modify the default buttons */
    setMenuAction();

    /* Force MRL update on tab change */
    CONNECT( ui.Tab, currentChanged( int ), this, signalCurrent( int ) );

    CONNECT( fileOpenPanel, mrlUpdated( const QStringList&, const QString& ),
             this, updateMRL( const QStringList&, const QString& ) );
    CONNECT( netOpenPanel, mrlUpdated( const QStringList&, const QString& ),
             this, updateMRL( const QStringList&, const QString& ) );
    CONNECT( discOpenPanel, mrlUpdated( const QStringList&, const QString& ),
             this, updateMRL( const QStringList&, const QString& ) );
    CONNECT( captureOpenPanel, mrlUpdated( const QStringList&, const QString& ),
             this, updateMRL( const QStringList&, const QString& ) );

    CONNECT( fileOpenPanel, methodChanged( const QString& ),
             this, newCachingMethod( const QString& ) );
    CONNECT( netOpenPanel, methodChanged( const QString& ),
             this, newCachingMethod( const QString& ) );
    CONNECT( discOpenPanel, methodChanged( const QString& ),
             this, newCachingMethod( const QString& ) );
    CONNECT( captureOpenPanel, methodChanged( const QString& ),
             this, newCachingMethod( const QString& ) );

    /* Advanced frame Connects */
    CONNECT( ui.slaveCheckbox, toggled( bool ), this, updateMRL() );
    CONNECT( ui.slaveText, textChanged( const QString& ), this, updateMRL() );
    CONNECT( ui.cacheSpinBox, valueChanged( int ), this, updateMRL() );
    CONNECT( ui.startTimeTimeEdit, timeChanged ( const QTime& ), this, updateMRL() );
    CONNECT( ui.stopTimeTimeEdit, timeChanged ( const QTime& ), this, updateMRL() );
    BUTTONACT( ui.advancedCheckBox, toggleAdvancedPanel() );
    BUTTONACT( ui.slaveBrowseButton, browseInputSlave() );

    /* Buttons action */
    BUTTONACT( playButton, selectSlots() );
    BUTTONACT( selectButton, close() );
    BUTTONACT( cancelButton, cancel() );

    /* Hide the advancedPanel */
    if( !getSettings()->value( "OpenDialog/advanced", false ).toBool())
    {
        ui.advancedFrame->hide();
        ui.advancedFrame->setEnabled( false );
    }
    else
        ui.advancedCheckBox->setChecked( true );

    /* Initialize caching */
    storedMethod = "";
    newCachingMethod( "file-caching" );

    /* enforce section due to .ui bug */
    ui.startTimeTimeEdit->setCurrentSection( QDateTimeEdit::SecondSection );
    ui.stopTimeTimeEdit->setCurrentSection( QDateTimeEdit::SecondSection );

    setMinimumSize( sizeHint() );
    setMaximumWidth( 900 );
    resize( getSettings()->value( "OpenDialog/size", QSize( 500, 400 ) ).toSize() );
}

/* Finish the dialog and decide if you open another one after */
void OpenDialog::setMenuAction()
{
    if( i_action_flag == SELECT )
    {
        playButton->hide();
        selectButton->show();
        selectButton->setDefault( true );
    }
    else
    {
        switch ( i_action_flag )
        {
        case OPEN_AND_STREAM:
            playButton->setText( qtr( "&Stream" ) );
            break;
        case OPEN_AND_SAVE:
            playButton->setText( qtr( "C&onvert / Save" ) );
            break;
        case OPEN_AND_ENQUEUE:
            playButton->setText( qtr( "&Enqueue" ) );
            break;
        case OPEN_AND_PLAY:
        default:
            playButton->setText( qtr( "&Play" ) );
        }
        playButton->show();
        selectButton->hide();
    }
}

OpenDialog::~OpenDialog()
{
    getSettings()->setValue( "OpenDialog/size", size() -
                 ( ui.advancedFrame->isEnabled() ?
                   QSize(0, ui.advancedFrame->height()) : QSize(0, 0) ) );
    getSettings()->setValue( "OpenDialog/advanced", ui.advancedFrame->isVisible() );
}

/* Used by VLM dialog and inputSlave selection */
QString OpenDialog::getMRL( bool b_all )
{
    if( itemsMRL.count() == 0 ) return "";
    return b_all ? itemsMRL[0] + getOptions()
                 : itemsMRL[0];
}

QStringList OpenDialog::getMRLs( bool b_with_options )
{
    if ( !b_with_options ) return itemsMRL;
    QStringList postfixedMRLs;
    foreach( const QString &mrl, itemsMRL )
        postfixedMRLs << QString( mrl ).append( getOptions() );
    return postfixedMRLs;
}

QString OpenDialog::getOptions()
{
    return ui.advancedLineInput->text();
}

void OpenDialog::showTab( int i_tab )
{
    if( i_tab == OPEN_CAPTURE_TAB ) captureOpenPanel->initialize();
    ui.Tab->setCurrentIndex( i_tab );
    show();
    if( ui.Tab->currentWidget() != NULL )
    {
        OpenPanel *panel = qobject_cast<OpenPanel *>( ui.Tab->currentWidget() );
        assert( panel );
        panel->onFocus();
    }
}

void OpenDialog::toggleAdvancedPanel()
{
    if( ui.advancedFrame->isVisible() )
    {
        ui.advancedFrame->hide();
        ui.advancedFrame->setEnabled( false );
        if( size().isValid() )
            resize( size().width(), size().height()
                    - ui.advancedFrame->height() );
    }
    else
    {
        ui.advancedFrame->show();
        ui.advancedFrame->setEnabled( true );
        if( size().isValid() )
            resize( size().width(), size().height()
                    + ui.advancedFrame->height() );
    }
}

void OpenDialog::browseInputSlave()
{
    OpenDialog *od = new OpenDialog( this, p_intf, true, SELECT );
    od->exec();
    ui.slaveText->setText( od->getMRL( false ) );
    delete od;
}

/* Function called on signal currentChanged triggered */
void OpenDialog::signalCurrent( int i_tab )
{
    if( i_tab == OPEN_CAPTURE_TAB ) captureOpenPanel->initialize();
    if( ui.Tab->currentWidget() != NULL )
    {
        OpenPanel *panel = qobject_cast<OpenPanel *>( ui.Tab->currentWidget() );
        assert( panel );
        panel->onFocus();
        panel->updateMRL();
        panel->updateContext( i_action_flag == OPEN_AND_PLAY ?
                              OpenPanel::CONTEXT_INTERACTIVE :
                              OpenPanel::CONTEXT_BATCH );
    }
}

/***********
 * Actions *
 ***********/
/* If Cancel is pressed or escaped */
void OpenDialog::cancel()
{
    /* Clear the panels */
    for( int i = 0; i < OPEN_TAB_MAX; i++ )
        qobject_cast<OpenPanel*>( ui.Tab->widget( i ) )->clear();

    /* Clear the variables */
    itemsMRL.clear();
    optionsMRL.clear();

    /* If in Select Mode, reject instead of hiding */
    if( i_action_flag == SELECT ) reject();
    else hide();
}

/* If EnterKey is pressed */
void OpenDialog::close()
{
    /* If in Select Mode, accept instead of selecting a Slot */
    if( i_action_flag == SELECT )
        accept();
    else
        selectSlots();
}

/* Play button */
void OpenDialog::selectSlots()
{
    switch ( i_action_flag )
    {
    case OPEN_AND_STREAM:
        stream();
        break;
    case OPEN_AND_SAVE:
        transcode();
        break;
    case OPEN_AND_ENQUEUE:
        enqueue();
        break;
    case OPEN_AND_PLAY:
    default:
        play();
    }
}

/* Play Action, called from selectSlots or play Menu */
void OpenDialog::play()
{
    enqueue( false );
}

/* Enqueue Action, called from selectSlots or enqueue Menu */
void OpenDialog::enqueue( bool b_enqueue )
{
    toggleVisible();

    if( i_action_flag == SELECT )
    {
        accept();
        return;
    }

    for( int i = 0; i < OPEN_TAB_MAX; i++ )
        qobject_cast<OpenPanel*>( ui.Tab->widget( i ) )->onAccept();

    /* Sort alphabetically */
    itemsMRL.sort();

    /* Go through the item list */
    for( int i = 0; i < itemsMRL.count(); i++ )
    {
        bool b_start = !i && !b_enqueue;

        /* Take options from the UI, not from what we stored */
        QStringList optionsList = getOptions().split( " :" );

        /* Switch between enqueuing and starting the item */
        Open::openMRLwithOptions( p_intf, itemsMRL[i], &optionsList, b_start, b_pl );
    }
}

void OpenDialog::transcode()
{
    stream( true );
}

void OpenDialog::stream( bool b_transcode_only )
{
//    QString soutMRL = getMRL( false );
//    if( soutMRL.isEmpty() ) return;

    for( int i = 0; i < OPEN_TAB_MAX; i++ )
        qobject_cast<OpenPanel*>( ui.Tab->widget( i ) )->onAccept();

    QStringList soutMRLS = getMRLs(false);
    if(soutMRLS.empty())
    {
        return;
    }

    toggleVisible();

    /* Dbg and send :D */
    msg_Dbg( p_intf, "MRL(s) passed to the Sout: %i", soutMRLS.length() );
    for(int i = 0; i < soutMRLS.length(); i++)
    {
        msg_Dbg( p_intf, "MRL(s) passed to the Sout: %s", qtu( soutMRLS[i] ) );
    }
    THEDP->streamingDialog( this, soutMRLS, b_transcode_only,
                            getOptions().split( " :" ) );
}

/* Update the MRL items from the panels */
void OpenDialog::updateMRL( const QStringList& item, const QString& tempMRL )
{
    optionsMRL = tempMRL;
    itemsMRL = item;
    updateMRL();
}

/* Format the time to milliseconds */
QString TimeToMilliseconds( const QTimeEdit *t ) {
    return QString::number( ( t->minimumTime().msecsTo( t->time() ) ) / 1000.0, 'f', 3);
}

/* Update the complete MRL */
void OpenDialog::updateMRL() {
    QString mrl = optionsMRL;
    if( ui.slaveCheckbox->isChecked() ) {
        mrl += " :input-slave=" + ui.slaveText->text();
    }
    mrl += QString( " :%1=%2" ).arg( storedMethod ).
                                arg( ui.cacheSpinBox->value() );
    if( ui.startTimeTimeEdit->time() != ui.startTimeTimeEdit->minimumTime() ) {
        mrl += " :start-time=" + TimeToMilliseconds( ui.startTimeTimeEdit );
    }
    if( ui.stopTimeTimeEdit->time() > ui.startTimeTimeEdit->time() ) {
        mrl += " :stop-time=" + TimeToMilliseconds( ui.stopTimeTimeEdit );
    }
    ui.advancedLineInput->setText( mrl );
    ui.mrlLine->setText( itemsMRL.join( " " ) );
    /* Only allow action without valid items */
    playButton->setEnabled( !itemsMRL.isEmpty() );
    selectButton->setEnabled( !itemsMRL.isEmpty() );
}

/* Change the caching combobox */
void OpenDialog::newCachingMethod( const QString& method )
{
    if( method != storedMethod ) {
        storedMethod = method;
        int i_value = var_InheritInteger( p_intf, qtu( storedMethod ) );
        ui.cacheSpinBox->setValue( i_value );
    }
}

/* Split the entries
 * FIXME! */
QStringList OpenDialog::SeparateEntries( const QString& entries )
{
    bool b_quotes_mode = false;

    QStringList entries_array;
    QString entry;

    int index = 0;
    while( index < entries.count() )
    {
        int delim_pos = entries.indexOf( QRegExp( "\\s+|\"" ), index );
        if( delim_pos < 0 ) delim_pos = entries.count() - 1;
        entry += entries.mid( index, delim_pos - index + 1 );
        index = delim_pos + 1;

        if( entry.isEmpty() ) continue;

        if( !b_quotes_mode && entry.endsWith( "\"" ) )
        {
            /* Enters quotes mode */
            entry.truncate( entry.count() - 1 );
            b_quotes_mode = true;
        }
        else if( b_quotes_mode && entry.endsWith( "\"" ) )
        {
            /* Finished the quotes mode */
            entry.truncate( entry.count() - 1 );
            b_quotes_mode = false;
        }
        else if( !b_quotes_mode && !entry.endsWith( "\"" ) )
        {
            /* we found a non-quoted standalone string */
            if( index < entries.count() ||
                entry.endsWith( " " ) || entry.endsWith( "\t" ) ||
                entry.endsWith( "\r" ) || entry.endsWith( "\n" ) )
                entry.truncate( entry.count() - 1 );
            if( !entry.isEmpty() ) entries_array.append( entry );
            entry.clear();
        }
        else
        {;}
    }

    if( !entry.isEmpty() ) entries_array.append( entry );

    return entries_array;
}

