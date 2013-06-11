/*****************************************************************************
 * convert.cpp : Convertion dialogs
 ****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
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

#include "dialogs/sout.hpp"
#include "dialogs/convert.hpp"
#include "components/sout/sout_widgets.hpp"

#include "util/qt_dirs.hpp"

#include <QLabel>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QCheckBox>

ConvertDialog::ConvertDialog( QWidget *parent, intf_thread_t *_p_intf,
                              const QString& inputMRL )
              : QVLCDialog( parent, _p_intf )
{
    setWindowTitle( qtr( "Convert" ) );
    setWindowRole( "vlc-convert" );

    QGridLayout *mainLayout = new QGridLayout( this );
    SoutInputBox *inputBox = new SoutInputBox( this );
    inputBox->setMRL( inputMRL );
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
    destLabel->setBuddy( fileLine );

    QPushButton *fileSelectButton = new QPushButton( qtr( "Browse" ) );
    destLayout->addWidget( fileLine, 0, 1 );
    destLayout->addWidget( fileSelectButton, 0, 2);
    BUTTONACT( fileSelectButton, fileBrowse() );

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
    QPushButton *okButton = new QPushButton( qtr( "&Start" ) );
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
}

void ConvertDialog::fileBrowse()
{
    QString fileExtension = ( ! profile->isEnabled() ) ? ".*" : "." + profile->getMux();

    QString fileName = QFileDialog::getSaveFileName( this, qtr( "Save file..." ),
        "",
        QString( qtr( "Containers (*" ) + fileExtension + ")" ) );
    fileLine->setText( toNativeSeparators( fileName ) );
    setDestinationFileExtension();
}

void ConvertDialog::cancel()
{
    reject();
}

void ConvertDialog::close()
{
    hide();

    if( dumpRadio->isChecked() )
    {
        mrl = "demux=dump :demuxdump-file=" + fileLine->text();
    }
    else
    {
        mrl = "sout=#" + profile->getTranscode();
        if( deinterBox->isChecked() )
        {
            mrl.remove( '}' );
            mrl += ",deinterlace}";
        }
        mrl += ":";
        if( displayBox->isChecked() )
            mrl += "duplicate{dst=display,dst=";
        mrl += "std{access=file{no-overwrite},mux=" + profile->getMux()
             + ",dst='" + fileLine->text().replace( QChar('\''), "\\\'" )
             + "'}";
        if( displayBox->isChecked() )
            mrl += "}";
    }

    msg_Dbg( p_intf, "Transcode MRL: %s", qtu( mrl ) );
    accept();
}

void ConvertDialog::setDestinationFileExtension()
{
    if( !fileLine->text().isEmpty() && profile->isEnabled() )
    {
        QString newFileExtension = "." + profile->getMux();
        QString newFileName;
        int index = fileLine->text().lastIndexOf( "." );
        if( index != -1 ) {
            newFileName = fileLine->text().left( index ).append( newFileExtension );
        } else {
            newFileName = fileLine->text().append( newFileExtension );
        }
        fileLine->setText( toNativeSeparators( newFileName ) );
    }
}
