/*****************************************************************************
 * open.cpp : Advanced open dialog
 *****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: streaminfo.cpp 16816 2006-09-23 20:56:52Z jb $
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

#include <QTabWidget>
#include <QGridLayout>
#include <QFileDialog>
#include <QRegExp>

#include "dialogs/open.hpp"
#include "components/open.hpp"

#include "qt4.hpp"
#include "util/qvlcframe.hpp"

#include "input_manager.hpp"
#include "dialogs_provider.hpp"

OpenDialog *OpenDialog::instance = NULL;

OpenDialog::OpenDialog( QWidget *parent, intf_thread_t *_p_intf, bool modal ) :
                                                QVLCDialog( parent, _p_intf )
{
    setModal( modal );
    ui.setupUi( this );
    setWindowTitle( qtr("Open" ) );
    resize( 500, 300);

    fileOpenPanel = new FileOpenPanel( this , p_intf );
    diskOpenPanel = new DiskOpenPanel( this , p_intf );
    netOpenPanel = new NetOpenPanel( this , p_intf );
    captureOpenPanel = new CaptureOpenPanel( this, p_intf );

    ui.Tab->addTab( fileOpenPanel, qtr( "&File" ) );
    ui.Tab->addTab( diskOpenPanel, qtr( "&Disc" ) );
    ui.Tab->addTab( netOpenPanel, qtr( "&Network" ) );
    ui.Tab->addTab( captureOpenPanel, qtr( "Capture &Device" ) );

    ui.advancedFrame->hide();

    /* Force MRL update on tab change */
    CONNECT( ui.Tab, currentChanged(int), this, signalCurrent());

    CONNECT( fileOpenPanel, mrlUpdated( QString ), this, updateMRL(QString) );
    CONNECT( netOpenPanel, mrlUpdated( QString ), this, updateMRL(QString) );
    CONNECT( diskOpenPanel, mrlUpdated( QString ), this, updateMRL(QString) );
    CONNECT( captureOpenPanel, mrlUpdated( QString ), this,
            updateMRL(QString) );


    CONNECT( fileOpenPanel, methodChanged( QString ),
             this, newMethod(QString) );
    CONNECT( netOpenPanel, methodChanged( QString ),
             this, newMethod(QString) );
    CONNECT( diskOpenPanel, methodChanged( QString ),
             this, newMethod(QString) );

    CONNECT( ui.slaveText, textChanged(QString), this, updateMRL());
    CONNECT( ui.cacheSpinBox, valueChanged(int), this, updateMRL());

    BUTTONACT( ui.closeButton, play());
    BUTTONACT( ui.cancelButton, cancel());
    BUTTONACT( ui.enqueueButton, enqueue());
    BUTTONACT( ui.advancedCheckBox , toggleAdvancedPanel() );

    /* Initialize caching */
    storedMethod = "";
    newMethod("file-caching");

    mainHeight = advHeight = 0;
}

OpenDialog::~OpenDialog()
{
}

void OpenDialog::showTab(int i_tab=0)
{
    this->show();
    ui.Tab->setCurrentIndex(i_tab);
}

void OpenDialog::signalCurrent() {
    if (ui.Tab->currentWidget() != NULL) {
        (dynamic_cast<OpenPanel*>(ui.Tab->currentWidget()))->updateMRL();
    }
}

void OpenDialog::cancel()
{
    fileOpenPanel->clear();
    this->toggleVisible();
    if( isModal() )
        reject();
}

void OpenDialog::play()
{
    playOrEnqueue( false );
}

void OpenDialog::enqueue()
{
    playOrEnqueue( true );
}

void OpenDialog::playOrEnqueue( bool b_enqueue = false )
{
    this->toggleVisible();
    mrl = ui.advancedLineInput->text();

    if( !isModal() )
    {
        QStringList tempMRL = SeparateEntries( mrl );
        for( size_t i = 0; i < tempMRL.size(); i++ )
        {
            bool b_start = !i && !b_enqueue;
            input_item_t *p_input;
            const char *psz_utf8 = qtu( tempMRL[i] );

            p_input = input_ItemNew( p_intf, psz_utf8, NULL );

            /* Insert options */
            while( i + 1 < tempMRL.size() && tempMRL[i + 1].startsWith( ":" ) )
            {
                i++;
                psz_utf8 = qtu( tempMRL[i] );
                input_ItemAddOption( p_input, psz_utf8 );
            }

            if( b_start )
            {
                playlist_AddInput( THEPL, p_input,
                                   PLAYLIST_APPEND | PLAYLIST_GO,
                                   PLAYLIST_END, VLC_TRUE, VLC_FALSE );
            }
            else
            {
                playlist_AddInput( THEPL, p_input,
                                   PLAYLIST_APPEND|PLAYLIST_PREPARSE,
                                   PLAYLIST_END, VLC_TRUE, VLC_FALSE );
            }
        }
    }
    else
        accept();
}

void OpenDialog::toggleAdvancedPanel()
{
    //FIXME does not work under Windows
    if (ui.advancedFrame->isVisible()) {
        ui.advancedFrame->hide();
        setMinimumHeight(1);
        resize( width(), mainHeight );

    } else {
        if( mainHeight == 0 )
            mainHeight = height();

        ui.advancedFrame->show();
        if( advHeight == 0 ) {
            advHeight = height() - mainHeight;
        }
        resize( width(), mainHeight + advHeight );
    }
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
    ui.advancedLineInput->setText(mrl);
}

void OpenDialog::updateMRL(QString tempMRL)
{
    mainMRL = tempMRL;
    updateMRL();
}

void OpenDialog::newMethod(QString method)
{
    if( method != storedMethod ) {
        storedMethod = method;
        int i_value = config_GetInt( p_intf, qta(storedMethod) );
        ui.cacheSpinBox->setValue(i_value);
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
