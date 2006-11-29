/*****************************************************************************
 * open.cpp : Panels for the open dialogs
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org> 
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


#include "qt4.hpp"
#include "components/open.hpp"

#include <QFileDialog>
/**************************************************************************
 * Open panel
 ***************************************************************************/

OpenPanel::~OpenPanel()
{}

/**************************************************************************
 * File open
 **************************************************************************/
FileOpenPanel::FileOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );
    ui.audioGroupBox->hide();

    BUTTONACT( ui.extraAudioButton, toggleExtraAudio() );
    BUTTONACT( ui.fileBrowseButton, browseFile() );
    BUTTONACT( ui.subBrowseButton, browseFileSub() );
    BUTTONACT( ui.audioBrowseButton, browseFileAudio() );
    CONNECT( ui.fileInput, editTextChanged(QString ), this, updateMRL());
}

FileOpenPanel::~FileOpenPanel()
{}

void FileOpenPanel::sendUpdate()
{}

QStringList FileOpenPanel::browse()
{
    return QFileDialog::getOpenFileNames( this, qtr("Open File"), "", "" );
}

void FileOpenPanel::browseFile()
{
    //FIXME ! files with spaces
    QString files = browse().join(" ");
    ui.fileInput->setEditText( files );
    ui.fileInput->addItem( files );

    if ( ui.fileInput->count() > 8 ) ui.fileInput->removeItem(0);

    updateMRL();
}

void FileOpenPanel::browseFileSub()
{
    ui.subInput->setEditText( browse().join(" ") );

    updateSubsMRL();
}

void FileOpenPanel::browseFileAudio()
{
    ui.audioFileInput->setEditText( browse().join(" ") );
}

void FileOpenPanel::updateSubsMRL()
{
    QStringList* subsMRL = new QStringList("sub-file=");
    subsMRL->append( ui.subInput->currentText() );
    //FIXME !!
    subsMRL->append( "subsdec-align=" + ui.alignSubComboBox->currentText() );
    subsMRL->append( "sub-rel-fontsize=" + ui.sizeSubComboBox->currentText() );

    subsMRL->join(" ");
}

void FileOpenPanel::updateMRL()
{
    QString MRL = ui.fileInput->currentText();

    emit(mrlUpdated(MRL));
}

QString FileOpenPanel::getUpdatedMRL()
{
    return ui.fileInput->currentText();
}

void FileOpenPanel::toggleExtraAudio()
{
   if (ui.audioGroupBox->isVisible())
   {
       ui.audioGroupBox->hide();
   }
   else
   {
      ui.audioGroupBox->show();
   }
}

void FileOpenPanel::clear()
{
    ui.fileInput->setEditText( "");
    ui.subInput->setEditText( "");
    ui.audioFileInput->setEditText( "");
}



/**************************************************************************
 * Disk open
 **************************************************************************/
DiskOpenPanel::DiskOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );
}

DiskOpenPanel::~DiskOpenPanel()
{}

void DiskOpenPanel::sendUpdate()
{}

QString DiskOpenPanel::getUpdatedMRL()
{

    //return ui.DiskInput->currentText();
    return NULL;
}



/**************************************************************************
 * Net open
 **************************************************************************/
NetOpenPanel::NetOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );
}

NetOpenPanel::~NetOpenPanel()
{}

void NetOpenPanel::sendUpdate()
{}
/*
void NetOpenPanel::sendUpdate()
{
    QString *mrl = new QString();
    QString *cache = new QString();
    getUpdatedMRL( mrl, cache );,
    emit dataUpdated( mrl, cache );
}*/

QString NetOpenPanel::getUpdatedMRL()
{
//  return ui.NetInput->currentText();
    return NULL;
}

