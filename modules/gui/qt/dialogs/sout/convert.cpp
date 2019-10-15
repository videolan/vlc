/*****************************************************************************
 * convert.cpp : Convertion dialogs
 ****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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

#include "dialogs/sout/sout.hpp"
#include "dialogs/sout/convert.hpp"
#include "dialogs/sout/sout_widgets.hpp"

#include "util/qt_dirs.hpp"

#include <QLabel>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QCheckBox>

#define urlToDisplayString(filestr) toNativeSeparators(QUrl(filestr).toDisplayString(\
    QUrl::RemovePassword | QUrl::PreferLocalFile | QUrl::NormalizePathSegments ))

ConvertDialog::ConvertDialog( QWidget *parent, intf_thread_t *_p_intf,
                              const QStringList& inputMRLs )
              : QVLCDialog( parent, _p_intf ),
                singleFileSelected( inputMRLs.length() == 1 )
{
    setWindowTitle( qtr( "Convert" ) );
    setWindowRole( "vlc-convert" );

    QGridLayout *mainLayout = new QGridLayout( this );
    SoutInputBox *inputBox = new SoutInputBox( this );
    incomingMRLs = &inputMRLs;

    if(singleFileSelected)
    {
        inputBox->setMRL( inputMRLs[0] );
    }
    else
    {
        inputBox->setMRL( qtr( "Multiple files selected." ) );
    }
    mainLayout->addWidget( inputBox, 0, 0, 1, -1  );

    /**
     * Destination
     **/
    QGroupBox *destBox = new QGroupBox( qtr( "Destination" ) );
    QGridLayout *destLayout = new QGridLayout( destBox );

    QLabel *destLabel = new QLabel( qtr( "Destination file:" ) );
    destLayout->addWidget( destLabel, 0, 0);

    fileLine = new QLineEdit;
    fileLine->setMinimumWidth( 300 );
    fileLine->setFocus( Qt::ActiveWindowFocusReason );
    fileLine->setReadOnly(true);
    destLabel->setBuddy( fileLine );
    // You can set a specific name for only one file.
    if(singleFileSelected)
    {
        QPushButton *fileSelectButton = new QPushButton( qtr( "Browse" ) );
        destLayout->addWidget( fileSelectButton, 0, 2);
        BUTTONACT( fileSelectButton, fileBrowse() );
    }

    // but multiple files follow a naming convention
    else
    {
        fileLine->setText( qtr( "Multiple Files Selected." ) );
        fileLine->setReadOnly(true);
        fileLine->setToolTip( qtr( "Files will be placed in the same directory "
                "with the same name." ) );

        appendBox = new QCheckBox( qtr( "Append '-converted' to filename" ) );
        destLayout->addWidget( appendBox, 1, 0 );
    }
    destLayout->addWidget( fileLine, 0, 1 );
    mainLayout->addWidget( destBox, 3, 0, 1, -1  );


    /* Profile Editor */
    QGroupBox *settingBox = new QGroupBox( qtr( "Settings" ) );
    QGridLayout *settingLayout = new QGridLayout( settingBox );

    QRadioButton *convertRadio = new QRadioButton( qtr( "Convert" ) );
    dumpRadio = new QRadioButton( qtr( "Dump raw input" ) );
    QButtonGroup *buttonGroup = new QButtonGroup(this);
    buttonGroup->addButton( convertRadio );
    buttonGroup->addButton( dumpRadio );
    convertRadio->setChecked( true );

    settingLayout->addWidget( convertRadio, 1, 0 );

    QWidget *convertPanel = new QWidget( this );
    QVBoxLayout *convertLayout = new QVBoxLayout( convertPanel );

    displayBox = new QCheckBox( qtr( "Display the output" ) );
    displayBox->setToolTip( qtr( "This display the resulting media, but can "
                               "slow things down." ) );
    convertLayout->addWidget( displayBox );

    deinterBox = new QCheckBox( qtr( "Deinterlace" ) );
    convertLayout->addWidget( deinterBox );

    profile = new VLCProfileSelector( this );
    convertLayout->addWidget( profile );

    settingLayout->addWidget( convertPanel, 2, 0 );

    settingLayout->addWidget( dumpRadio, 5, 0 );

    mainLayout->addWidget( settingBox, 1, 0, 1, -1  );

    /* Buttons */
    okButton = new QPushButton( qtr( "&Start" ) );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ) );
    QDialogButtonBox *buttonBox = new QDialogButtonBox;

    okButton->setDefault( true );
    buttonBox->addButton( okButton, QDialogButtonBox::AcceptRole );
    buttonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    mainLayout->addWidget( buttonBox, 5, 3 );

    BUTTONACT(okButton,close());
    BUTTONACT(cancelButton,cancel());

    CONNECT( convertRadio, toggled(bool), convertPanel, setEnabled(bool) );
    CONNECT(profile, optionsChanged(), this, setDestinationFileExtension());
    CONNECT(fileLine, editingFinished(), this, setDestinationFileExtension());
    CONNECT(fileLine, textChanged(const QString&), this, validate());

    validate();
}

void ConvertDialog::fileBrowse()
{
    QString fileExtension = ( ! profile->isEnabled() ) ? ".*" : "." + profile->getMux();

    outgoingMRL = QFileDialog::getSaveFileUrl( this, qtr( "Save file..." ),
        p_intf->p_sys->filepath,
        QString( "%1 (*%2);;%3 (*.*)" ).arg( qtr( "Containers" ) )
            .arg( fileExtension ).arg( qtr("All") ), 0, QFileDialog::DontConfirmOverwrite );
    fileLine->setText( urlToDisplayString( outgoingMRL ) );
    setDestinationFileExtension();
}

void ConvertDialog::cancel()
{
    reject();
}

void ConvertDialog::close()
{
    hide();

    for(int i = 0; i < incomingMRLs->length(); i++)
    {
        SoutChain mrl;

        if( dumpRadio->isChecked() )
        {
            mrl.header("demux=dump :demuxdump-file=" + fileLine->text());
        }
        else
        {
            mrl = profile->getTranscode();
            mrl.header( "sout=#" + mrl.getHeader() );
            if( deinterBox->isChecked() )
            {
                mrl.option("deinterlace");
            }

            QString newFileName;

            // Only one file, use the destination provided
            if(singleFileSelected)
            {
                newFileName = outgoingMRL.toLocalFile();
            }

            // Multiple, use the convention.
            else
            {
                QString fileExtension = ( ! profile->isEnabled() ) ? ".*" : "." + profile->getMux();

                newFileName = incomingMRLs->at(i);

                // Remove the file:// from the front of our MRL
                newFileName = QUrl(newFileName).toLocalFile();

                // Remote the existing extention (if any)
                int extentionPos = newFileName.lastIndexOf('.');
                if(extentionPos >= 0)
                {
                    newFileName = newFileName.remove(extentionPos, newFileName.length() - extentionPos);
                }

                // If we have multiple files (i.e. we have an appenBox) and it's checked
                if( appendBox->isChecked() )
                {
                    newFileName = newFileName.append("-converted");
                }

                // Stick our new extention on
                newFileName = newFileName.append(fileExtension);
            }

            newFileName.replace( QChar('\''), "\\\'" );


            mrl.end();
            SoutModule dstModule("std");
            SoutModule file("file");
            file.option("no-overwrite");
            dstModule.option("access", file);
            dstModule.option("mux", profile->getMux());
            dstModule.option("dst", "'" + newFileName + "'");

            if( displayBox->isChecked() )
            {
                SoutModule duplicate("duplicate");
                duplicate.option("dst", "display");
                duplicate.option("dst", dstModule);
                mrl.module(duplicate);
            }
            else
            {
                mrl.module(dstModule);
            }
        }
        msg_Dbg( p_intf, "Transcode chain: %s", qtu( mrl.to_string() ) );
        mrls.append(mrl.to_string());
    }
    accept();
}

void ConvertDialog::setDestinationFileExtension()
{
    if( !outgoingMRL.isEmpty() && profile->isEnabled() )
    {
        QString filepath = outgoingMRL.path(QUrl::FullyEncoded);
        if( filepath.lastIndexOf( "." ) == -1 )
        {
            QString newFileExtension = "." + profile->getMux();
            outgoingMRL.setPath(filepath + newFileExtension);
            fileLine->setText( urlToDisplayString( outgoingMRL ) );
        }
    }
}

void ConvertDialog::validate()
{
    okButton->setEnabled( !fileLine->text().isEmpty() );
}
