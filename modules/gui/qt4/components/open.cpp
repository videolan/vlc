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
#include "util/customwidgets.hpp"

#include <QFileDialog>
#include <QDialogButtonBox>
#include <QLineEdit>

/**************************************************************************
 * File open
 **************************************************************************/
FileOpenPanel::FileOpenPanel( QWidget *_parent, intf_thread_t *_p_intf ) :
                                OpenPanel( _parent, _p_intf )
{
    /* Classic UI Setup */
    ui.setupUi( this );

    /* Use a QFileDialog and customize it because we don't want to
       rewrite it all. Be careful to your eyes cause there are a few hacks.
       Be very careful and test correctly when you modify this. */

    /* Set Filters for file selection */
    QString fileTypes = "";
    ADD_FILTER_MEDIA( fileTypes );
    ADD_FILTER_VIDEO( fileTypes );
    ADD_FILTER_AUDIO( fileTypes );
    ADD_FILTER_PLAYLIST( fileTypes );
    ADD_FILTER_ALL( fileTypes );
    fileTypes.replace(QString(";*"), QString(" *"));

    // Make this QFileDialog a child of tempWidget from the ui.
    dialogBox = new QFileDialog( ui.tempWidget, NULL,
            qfu( p_intf->p_libvlc->psz_homedir ), fileTypes );
    dialogBox->setFileMode( QFileDialog::ExistingFiles );
    /* We don't want to see a grip in the middle of the window, do we? */
    dialogBox->setSizeGripEnabled( false );
    dialogBox->setToolTip( qtr( "Select one or multiple files, or a folder" ));

    // Add it to the layout
    ui.gridLayout->addWidget( dialogBox, 0, 0, 1, 3 );

    // But hide the two OK/Cancel buttons. Enable them for debug.
#ifndef WIN32
    findChild<QDialogButtonBox*>()->hide();
#endif

    /* Ugly hacks to get the good Widget */
    //This lineEdit is the normal line in the fileDialog.
    lineFileEdit = findChildren<QLineEdit*>()[3];
    lineFileEdit->hide();

    /* Make a list of QLabel inside the QFileDialog to access the good ones */
    QList<QLabel *> listLabel = findChildren<QLabel*>();

    /* Hide the FileNames one. Enable it for debug */
    listLabel[4]->hide();
    /* Change the text that was uncool in the usual box */
    listLabel[5]->setText( qtr( "Filter:" ) );

    /* Hacks Continued Catch the close event */
    dialogBox->installEventFilter( this );

    // Hide the subtitles control by default.
    ui.subFrame->hide();


    BUTTONACT( ui.subBrowseButton, browseFileSub() );
    BUTTONACT( ui.subCheckBox, toggleSubtitleFrame());

    CONNECT( ui.fileInput, editTextChanged(QString ), this, updateMRL());
    CONNECT( ui.subInput, editTextChanged(QString ), this, updateMRL());
    CONNECT( ui.alignSubComboBox, currentIndexChanged(int), this, updateMRL());
    CONNECT( ui.sizeSubComboBox, currentIndexChanged(int), this, updateMRL());
    CONNECT( lineFileEdit, textChanged( QString ), this, browseFile());
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
    foreach( QString file, dialogBox->selectedFiles() ) {
         fileString += "\"" + file + "\" ";
    }
    ui.fileInput->setEditText( fileString );
    updateMRL();
}

void FileOpenPanel::browseFileSub()
{
    // FIXME Handle selection of more than one subtitles file
    QStringList files = THEDP->showSimpleOpen( qtr("Open subtitles file"),
                            EXT_FILTER_SUBTITLE,
                            dialogBox->directory().absolutePath() );
    ui.subInput->setEditText( files.join(" ") );
    updateMRL();
}

void FileOpenPanel::updateMRL()
{
    QString mrl = ui.fileInput->currentText();

    if( ui.subCheckBox->isChecked() ) {
        mrl.append( " :sub-file=" + ui.subInput->currentText() );
        mrl.append( " :subsdec-align=" + ui.alignSubComboBox->currentText() );
        mrl.append( " :sub-rel-fontsize=" + ui.sizeSubComboBox->currentText() );
    }
    emit mrlUpdated( mrl );
    emit methodChanged( "file-caching" );
}


/* Function called by Open Dialog when clicke on Play/Enqueue */
void FileOpenPanel::accept()
{
    ui.fileInput->addItem(ui.fileInput->currentText());
    if ( ui.fileInput->count() > 8 ) ui.fileInput->removeItem(0);
}


/* Function called by Open Dialog when clicked on cancel */
void FileOpenPanel::clear()
{
    ui.fileInput->setEditText( "" );
    ui.subInput->setEditText( "" );
}

bool FileOpenPanel::eventFilter(QObject *object, QEvent *event)
{
    if ( ( object == dialogBox ) && ( event->type() == QEvent::Hide ) )
    {
         event->ignore();
         return true;
    }
    // standard event processing
    else
        return QObject::eventFilter(object, event);
}

void FileOpenPanel::toggleSubtitleFrame()
{
    if (ui.subFrame->isVisible())
    {
        ui.subFrame->hide();
//        setMinimumHeight(1);
        resize( sizeHint());
    }
    else
    {
        ui.subFrame->show();
    }

    /* Update the MRL */
    updateMRL();
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
