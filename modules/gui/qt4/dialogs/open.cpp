/*****************************************************************************
 * open.cpp : Advanced open dialog
 ****************************************************************************
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

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
    printf ( "%i" , i_tab);
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
    QStringList tempMRL = mrl.split( QRegExp("\"\\s+\""),
                                     QString::SkipEmptyParts );
    if( !isModal() )
    {
        for( size_t i = 0 ; i< tempMRL.size(); i++ )
        {
             QString mrli = tempMRL[i].remove( QRegExp( "^\"" ) ).
                                       remove( QRegExp( "\"\\s+$" ) );
             const char * psz_utf8 = qtu( tempMRL[i] );
             if ( b_enqueue )
             {
                 /* Enqueue and Preparse all items*/
                 playlist_Add( THEPL, psz_utf8, NULL,
                                PLAYLIST_APPEND | PLAYLIST_PREPARSE,
                                PLAYLIST_END, VLC_TRUE, VLC_FALSE );

             }
             else
             {
                 /* Play the first one, parse and enqueue the other ones */
                 playlist_Add( THEPL, psz_utf8, NULL,
                                PLAYLIST_APPEND | (i ? 0 : PLAYLIST_GO) |
                                ( i ? PLAYLIST_PREPARSE : 0 ),
                                PLAYLIST_END, VLC_TRUE, VLC_FALSE );
             }
        }

    }
    else
        accept();
}

void OpenDialog::toggleAdvancedPanel()
{
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
