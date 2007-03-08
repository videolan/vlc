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
#include "dialogs_provider.hpp"

/**************************************************************************
 * File open
 **************************************************************************/
FileOpenPanel::FileOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );


    BUTTONACT( ui.fileBrowseButton, browseFile() );
    BUTTONACT( ui.subBrowseButton, browseFileSub() );

    BUTTONACT( ui.subGroupBox, updateMRL());
    CONNECT( ui.fileInput, editTextChanged(QString ), this, updateMRL());
    CONNECT( ui.subInput, editTextChanged(QString ), this, updateMRL());
    CONNECT( ui.alignSubComboBox, currentIndexChanged(int), this, updateMRL());
    CONNECT( ui.sizeSubComboBox, currentIndexChanged(int), this, updateMRL());
}

FileOpenPanel::~FileOpenPanel()
{}

QStringList FileOpenPanel::browse(QString help)
{
    return THEDP->showSimpleOpen( help );
}

void FileOpenPanel::browseFile()
{
    QString fileString = "";
    QStringList files = browse( qtr("Open File") );
    foreach( QString file, files) {
        fileString += "\"" + file + "\" ";
    }
    ui.fileInput->setEditText( fileString );
    ui.fileInput->addItem( fileString );
    if ( ui.fileInput->count() > 8 ) ui.fileInput->removeItem(0);

    updateMRL();
}

void FileOpenPanel::browseFileSub()
{
    ui.subInput->setEditText( browse( qtr("Open subtitles file") ).join(" ") );
    updateMRL();
}

void FileOpenPanel::updateMRL()
{
    QString mrl = ui.fileInput->currentText();

    if( ui.subGroupBox->isChecked() ) {
        mrl.append( " :sub-file=" + ui.subInput->currentText() );
        mrl.append( " :subsdec-align=" + ui.alignSubComboBox->currentText() );
        mrl.append( " :sub-rel-fontsize=" + ui.sizeSubComboBox->currentText() );
    }
    emit mrlUpdated(mrl);
    emit methodChanged( "file-caching" );
}

void FileOpenPanel::clear()
{
    ui.fileInput->setEditText( "");
    ui.subInput->setEditText( "");
}


/**************************************************************************
 * Disk open
 **************************************************************************/
DiskOpenPanel::DiskOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );

    CONNECT( ui.deviceCombo, editTextChanged(QString ), this, updateMRL());
    BUTTONACT( ui.dvdRadioButton, updateMRL());
    BUTTONACT( ui.vcdRadioButton, updateMRL());
    BUTTONACT( ui.audioCDRadioButton, updateMRL());

    CONNECT( ui.titleSpin, valueChanged(int), this, updateMRL());
    CONNECT( ui.chapterSpin, valueChanged(int), this, updateMRL());
}

DiskOpenPanel::~DiskOpenPanel()
{}

void DiskOpenPanel::clear()
{
    ui.titleSpin->setValue(0);
    ui.chapterSpin->setValue(0);
}

void DiskOpenPanel::updateMRL()
{
    QString mrl = "";
    /* DVD */
    if( ui.dvdRadioButton->isChecked() ) {
        mrl = "dvd://" + ui.deviceCombo->currentText();
        emit methodChanged( "dvdnav-caching" );

        if ( ui.titleSpin->value() > 0 ) {
            mrl += QString("@%1").arg(ui.titleSpin->value());
            if ( ui.chapterSpin->value() > 0 ) {
                mrl+= QString(":%1").arg(ui.chapterSpin->value());
            }
        }

    /* VCD */
    } else if (ui.vcdRadioButton->isChecked() ) {
        mrl = "vcd://" + ui.deviceCombo->currentText();
        emit methodChanged( "vcd-caching" );

        if( ui.titleSpin->value() > 0 ) {
            mrl += QString("@%1").arg(ui.titleSpin->value());
        }

    /* CDDA */
    } else {
        mrl = "cdda://" + ui.deviceCombo->currentText();
    }

    emit mrlUpdated(mrl);
}



/**************************************************************************
 * Net open
 **************************************************************************/
NetOpenPanel::NetOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );

    CONNECT( ui.protocolCombo, currentIndexChanged(int),
             this, updateProtocol(int) );
    CONNECT( ui.portSpin, valueChanged(int), this, updateMRL());
    CONNECT( ui.addressText, textChanged(QString), this, updateAddress());
    CONNECT( ui.timeShift, clicked(), this, updateMRL());
    CONNECT( ui.ipv6, clicked(), this, updateMRL());

    ui.protocolCombo->addItem("HTTP", QVariant("http"));
    ui.protocolCombo->addItem("FTP", QVariant("ftp"));
    ui.protocolCombo->addItem("MMS", QVariant("mms"));
    ui.protocolCombo->addItem("RTSP", QVariant("rtsp"));
    ui.protocolCombo->addItem("UDP/RTP (unicast)", QVariant("udp"));
    ui.protocolCombo->addItem("UDP/RTP (multicast)", QVariant("udp"));
}

NetOpenPanel::~NetOpenPanel()
{}

void NetOpenPanel::clear()
{}

void NetOpenPanel::updateProtocol(int idx) {
    QString addr = ui.addressText->text();
    QString proto = ui.protocolCombo->itemData(idx).toString();

    ui.timeShift->setEnabled( idx >= 4);
    ui.ipv6->setEnabled( idx == 4 );
    ui.addressText->setEnabled( idx != 4 );
    ui.portSpin->setEnabled( idx >= 4 );

    /* If we already have a protocol in the address, replace it */
    if( addr.contains( "://")) {
        msg_Err( p_intf, "replace");
        addr.replace(QRegExp("^.*://"), proto + "://");
        ui.addressText->setText(addr);
    }

    updateMRL();
}

void NetOpenPanel::updateAddress() {
    updateMRL();
}

void NetOpenPanel::updateMRL() {
    QString mrl = "";
    QString addr = ui.addressText->text();
    int proto = ui.protocolCombo->currentIndex();

    if( addr.contains( "://") && proto != 4 ) {
        mrl = addr;
    } else {
        switch(proto) {
        case 0:
            mrl = "http://" + addr;
            emit methodChanged("http-caching");
            break;
        case 2:
            mrl = "mms://" + addr;
            emit methodChanged("mms-caching");
            break;
        case 1:
            mrl = "ftp://" + addr;
            emit methodChanged("ftp-caching");
            break;
        case 3: /* RTSP */
            mrl = "rtsp://" + addr;
            emit methodChanged("rtsp-caching");
            break;
        case 4:
            mrl = "udp://@";
            if( ui.ipv6->isEnabled() && ui.ipv6->isChecked() ) {
                mrl += "[::]";
            }
            mrl += QString(":%1").arg(ui.portSpin->value());
            emit methodChanged("udp-caching");
            break;
        case 5: /* UDP multicast */
            mrl = "udp://@";
            /* Add [] to IPv6 */
            if ( addr.contains(':') && !addr.contains('[') ) {
                mrl += "[" + addr + "]";
            } else mrl += addr;
            mrl += QString(":%1").arg(ui.portSpin->value());
            emit methodChanged("udp-caching");
        }
    }
    if( ui.timeShift->isEnabled() && ui.timeShift->isChecked() ) {
        mrl += " :access-filter=timeshift";
    }
    emit mrlUpdated(mrl);
}

/**************************************************************************
 * Capture open
 **************************************************************************/
CaptureOpenPanel::CaptureOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    ui.setupUi( this );
}

CaptureOpenPanel::~CaptureOpenPanel()
{}

void CaptureOpenPanel::clear()
{}

void CaptureOpenPanel::updateMRL()
{
    QString mrl = "";
    emit mrlUpdated(mrl);
}
