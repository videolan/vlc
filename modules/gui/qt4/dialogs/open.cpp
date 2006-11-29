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

#include "dialogs/open.hpp"
#include "components/open.hpp"

#include "qt4.hpp"
#include "util/qvlcframe.hpp"

#include "input_manager.hpp"
#include "dialogs_provider.hpp"

OpenDialog *OpenDialog::instance = NULL;

OpenDialog::OpenDialog( intf_thread_t *_p_intf ) : QVLCFrame( _p_intf )
{
    setWindowTitle( qtr("Open" ) );
    ui.setupUi( this );
    fileOpenPanel = new FileOpenPanel(this , _p_intf );
    diskOpenPanel = new DiskOpenPanel(this , _p_intf );
    netOpenPanel = new NetOpenPanel(this , _p_intf );
    ui.Tab->addTab(fileOpenPanel, "File");
    ui.Tab->addTab(diskOpenPanel, "Disk");
    ui.Tab->addTab(netOpenPanel, "Network");
    ui.advancedFrame->hide();

    connect( fileOpenPanel, SIGNAL(mrlUpdated( QString )),
            this, SLOT( updateMRL(QString)));
    BUTTONACT( ui.closeButton, ok());
    BUTTONACT( ui.cancelButton, cancel());
    BUTTONACT( ui.advancedButton , toggleAdvancedPanel() );
}

OpenDialog::~OpenDialog()
{
}

void OpenDialog::showTab(int i_tab=0)
{
    this->show();
    ui.Tab->setCurrentIndex(i_tab);
}

void OpenDialog::cancel()
{
    fileOpenPanel->clear();
    this->toggleVisible();
}

void OpenDialog::ok()
{
    QStringList tempMRL = MRL.split(" ");
    for( size_t i = 0 ; i< tempMRL.size(); i++ )
    {
         const char * psz_utf8 = qtu( tempMRL[i] );
         /* Play the first one, parse and enqueue the other ones */
         playlist_Add( THEPL, psz_utf8, NULL,
                       PLAYLIST_APPEND | (i ? 0 : PLAYLIST_GO) |
                       ( i ? PLAYLIST_PREPARSE : 0 ),
                       PLAYLIST_END, VLC_TRUE );
      }

    this->toggleVisible();
}

void OpenDialog::changedTab()
{
}

void OpenDialog::toggleAdvancedPanel()
{
    if (ui.advancedFrame->isVisible())
    {
        ui.advancedFrame->hide();
    }
    else
    {
        ui.advancedFrame->show();
    }
}

void OpenDialog::updateMRL(QString tempMRL)
{
    MRL = tempMRL;
    ui.advancedLineInput->setText(MRL);
}

