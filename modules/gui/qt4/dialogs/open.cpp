/*****************************************************************************
 * open.cpp : Advanced open dialog
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
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

#include "input_manager.hpp"

#include "dialogs/open.hpp"
#include "components/open.hpp"

#include <QTabWidget>
#include <QGridLayout>
#include <QFileDialog>
#include <QRegExp>
#include <QMenu>

OpenDialog *OpenDialog::instance = NULL;

OpenDialog::OpenDialog( QWidget *parent, intf_thread_t *_p_intf, bool modal,
                        int _action_flag )  :  QVLCDialog( parent, _p_intf )
{
    setModal( modal );
    i_action_flag = _action_flag;

    /* Basic Creation of the Window */
    ui.setupUi( this );
    setWindowTitle( qtr("Open" ) );
    resize( 410, 300);

    /* Tab definition and creation */
    fileOpenPanel = new FileOpenPanel( ui.Tab, p_intf );
    discOpenPanel = new DiscOpenPanel( ui.Tab, p_intf );
    netOpenPanel = new NetOpenPanel( ui.Tab, p_intf );
    captureOpenPanel = new CaptureOpenPanel( ui.Tab, p_intf );

    /* Insert the tabs */
    ui.Tab->insertTab( OPEN_FILE_TAB, fileOpenPanel, qtr( "&File" ) );
    ui.Tab->insertTab( OPEN_DISC_TAB, discOpenPanel, qtr( "&Disc" ) );
    ui.Tab->insertTab( OPEN_NETWORK_TAB, netOpenPanel, qtr( "&Network" ) );
    ui.Tab->insertTab( OPEN_CAPTURE_TAB, captureOpenPanel,
                                qtr( "Capture &Device" ) );

    /* Hide the Slave input widgets */
    ui.slaveLabel->hide();
    ui.slaveText->hide();
    ui.slaveBrowseButton->hide();

    /* Hide the advancedPanel */
    if(! config_GetInt( p_intf, "qt-adv-options") )
        ui.advancedFrame->hide();
    else
        ui.advancedCheckBox->setCheckState( Qt::Checked );

    /* Buttons Creation */
    QSizePolicy buttonSizePolicy( QSizePolicy::Expanding, QSizePolicy::Minimum );
    buttonSizePolicy.setHorizontalStretch(0);
    buttonSizePolicy.setVerticalStretch(0);

    /* Play Button */
    playButton = new QToolButton( this );
    playButton->setText( qtr( "&Play" ) );
    playButton->setSizePolicy( buttonSizePolicy );
    playButton->setMinimumSize( QSize(90, 0) );
    playButton->setPopupMode( QToolButton::MenuButtonPopup );
    playButton->setToolButtonStyle( Qt::ToolButtonTextOnly );

    /* Cancel Button */
    cancelButton = new QPushButton();
    cancelButton->setText( qtr( "&Cancel" ) );
    cancelButton->setSizePolicy( buttonSizePolicy );

    /* Menu for the Play button */
    QMenu * openButtonMenu = new QMenu( "Open" );
    openButtonMenu->addAction( qtr("&Enqueue"), this, SLOT( enqueue() ),
                                    QKeySequence( "Alt+E") );
    openButtonMenu->addAction( qtr("&Play"), this, SLOT( play() ),
                                    QKeySequence( "Alt+P" ) );
    openButtonMenu->addAction( qtr("&Stream"), this, SLOT( stream() ) ,
                                    QKeySequence( "Alt+S" ) );
    openButtonMenu->addAction( qtr("&Convert"), this, SLOT( transcode() ) ,
                                    QKeySequence( "Alt+C" ) );

    playButton->setMenu( openButtonMenu );

    ui.buttonsBox->addButton( playButton, QDialogButtonBox::AcceptRole );
    ui.buttonsBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    /* Force MRL update on tab change */
    CONNECT( ui.Tab, currentChanged(int), this, signalCurrent());

    CONNECT( fileOpenPanel, mrlUpdated( QString ), this, updateMRL(QString) );
    CONNECT( netOpenPanel, mrlUpdated( QString ), this, updateMRL(QString) );
    CONNECT( discOpenPanel, mrlUpdated( QString ), this, updateMRL(QString) );
    CONNECT( captureOpenPanel, mrlUpdated( QString ), this, updateMRL(QString) );

    CONNECT( fileOpenPanel, methodChanged( QString ),
                                                 this, newCachingMethod(QString) );
    CONNECT( netOpenPanel, methodChanged( QString ),
                                                 this, newCachingMethod(QString) );
    CONNECT( discOpenPanel, methodChanged( QString ),
                                                 this, newCachingMethod(QString) );
    CONNECT( captureOpenPanel, methodChanged( QString ),
                                                 this, newCachingMethod(QString) );

    /* Advanced frame Connects */
    CONNECT( ui.slaveText, textChanged(QString), this, updateMRL());
    CONNECT( ui.cacheSpinBox, valueChanged(int), this, updateMRL());
    CONNECT( ui.startTimeSpinBox, valueChanged(int), this, updateMRL());
    BUTTONACT( ui.advancedCheckBox , toggleAdvancedPanel() );

    /* Buttons action */
    BUTTONACT( playButton, play());
    BUTTONACT( cancelButton, cancel());

    /* At creation time, modify the default buttons */
    if ( i_action_flag ) setMenuAction();

    /* Initialize caching */
    storedMethod = "";
    newCachingMethod("file-caching");

    mainHeight = advHeight = 0;
}

OpenDialog::~OpenDialog()
{
}

/* Finish the dialog and decide if you open another one after */
void OpenDialog::setMenuAction()
{
    switch ( i_action_flag )
    {
        case OPEN_AND_STREAM:
            playButton->setText( qtr("&Stream") );
            BUTTONACT( playButton, stream() );
            break;
        case OPEN_AND_SAVE:
            playButton->setText( qtr("&Convert / Save") );
            BUTTONACT( playButton, transcode() );
            break;
        case OPEN_AND_ENQUEUE:
            playButton->setText( qtr("&Enqueue") );
            BUTTONACT( playButton, enqueue() );
            break;
        case OPEN_AND_PLAY:
        default:
            playButton->setText( qtr("&Play") );
            BUTTONACT( playButton, play() );
   }
}

void OpenDialog::showTab( int i_tab=0 )
{
    this->show();
    ui.Tab->setCurrentIndex( i_tab );
}

void OpenDialog::signalCurrent() {
    if (ui.Tab->currentWidget() != NULL)
        (dynamic_cast<OpenPanel *>( ui.Tab->currentWidget() ))->updateMRL();
}

void OpenDialog::toggleAdvancedPanel()
{
    //FIXME does not work under Windows
    if( ui.advancedFrame->isVisible() ) {
        ui.advancedFrame->hide();
#ifndef WIN32
        setMinimumHeight(1);
        resize( width(), mainHeight );
#endif
    } else {
#ifndef WIN32
        if( mainHeight == 0 )
            mainHeight = height();
#endif

        ui.advancedFrame->show();
#ifndef WIN32
        if( advHeight == 0 ) {
            advHeight = height() - mainHeight;
        }
        resize( width(), mainHeight + advHeight );
#endif
    }
}

/***********
 * Actions *
 ***********/

/* If Cancel is pressed or escaped */
void OpenDialog::cancel()
{
    for( int i = 0; i <= OPEN_TAB_MAX; i++ ) 
        dynamic_cast<OpenPanel*>(ui.Tab->widget( i ))->clear();
    toggleVisible();
    if( isModal() ) reject();
}

/* If EnterKey is pressed */
void OpenDialog::close()
{
    /* FIXME */
    if ( !i_action_flag )
    {
        play();
    }
    else
    {
        stream();
    }
}

/* Play button */
void OpenDialog::play()
{
    finish( false );
}

void OpenDialog::enqueue()
{
    finish( true );
}

void OpenDialog::transcode()
{
    stream( true );
}

void OpenDialog::stream( bool b_transcode_only )
{
    /* not finished FIXME */
    /* Should go through the finish function */
    THEDP->streamingDialog( mrl, b_transcode_only );
}

void OpenDialog::finish( bool b_enqueue = false )
{
    toggleVisible();
    mrl = ui.advancedLineInput->text();

    if( !isModal() )
    {
        QStringList tempMRL = SeparateEntries( mrl );
        for( size_t i = 0; i < tempMRL.size(); i++ )
        {
            bool b_start = !i && !b_enqueue;
            input_item_t *p_input;

            p_input = input_ItemNew( p_intf, qtu( tempMRL[i] ), NULL );

            /* Insert options */
            while( i + 1 < tempMRL.size() && tempMRL[i + 1].startsWith( ":" ) )
            {
                i++;
                input_ItemAddOption( p_input, qtu( tempMRL[i] ) );
            }

            /* Switch between enqueuing and starting the item */
            if( b_start )
            {
                playlist_AddInput( THEPL, p_input,
                                   PLAYLIST_APPEND | PLAYLIST_GO,
                                   PLAYLIST_END, VLC_TRUE, VLC_FALSE );
            }
            else
            {
                playlist_AddInput( THEPL, p_input,
                                   PLAYLIST_APPEND | PLAYLIST_PREPARSE,
                                   PLAYLIST_END, VLC_TRUE, VLC_FALSE );
            }
        }
    }
    else
        accept();
}


/* Update the MRL */
void OpenDialog::updateMRL( QString tempMRL )
{
    mainMRL = tempMRL;
    updateMRL();
}

void OpenDialog::updateMRL() {
    mrl = mainMRL;
    if( ui.slaveCheckbox->isChecked() ) {
        mrl += " :input-slave=" + ui.slaveText->text();
    }
    int i_cache = config_GetInt( p_intf, qta(storedMethod) );
    if( i_cache != ui.cacheSpinBox->value() ) {
        mrl += QString(" :%1=%2").arg(storedMethod).
                                  arg(ui.cacheSpinBox->value());
    }
    if( ui.startTimeSpinBox->value()) {
        mrl += " :start-time=" + QString("%1").
            arg(ui.startTimeSpinBox->value());
    }
    ui.advancedLineInput->setText(mrl);
}

void OpenDialog::newCachingMethod( QString method )
{
    if( method != storedMethod ) {
        storedMethod = method;
        int i_value = config_GetInt( p_intf, qta( storedMethod ) );
        ui.cacheSpinBox->setValue( i_value );
    }
}

QStringList OpenDialog::SeparateEntries( QString entries )
{
    bool b_quotes_mode = false;

    QStringList entries_array;
    QString entry;

    int index = 0;
    while( index < entries.size() )
    {
        int delim_pos = entries.indexOf( QRegExp( "\\s+|\"" ), index );
        if( delim_pos < 0 ) delim_pos = entries.size() - 1;
        entry += entries.mid( index, delim_pos - index + 1 );
        index = delim_pos + 1;

        if( entry.isEmpty() ) continue;

        if( !b_quotes_mode && entry.endsWith( "\"" ) )
        {
            /* Enters quotes mode */
            entry.truncate( entry.size() - 1 );
            b_quotes_mode = true;
        }
        else if( b_quotes_mode && entry.endsWith( "\"" ) )
        {
            /* Finished the quotes mode */
            entry.truncate( entry.size() - 1 );
            b_quotes_mode = false;
        }
        else if( !b_quotes_mode && !entry.endsWith( "\"" ) )
        {
            /* we found a non-quoted standalone string */
            if( index < entries.size() ||
                entry.endsWith( " " ) || entry.endsWith( "\t" ) ||
                entry.endsWith( "\r" ) || entry.endsWith( "\n" ) )
                entry.truncate( entry.size() - 1 );
            if( !entry.isEmpty() ) entries_array.append( entry );
            entry.clear();
        }
        else
        {;}
    }

    if( !entry.isEmpty() ) entries_array.append( entry );

    return entries_array;
}
