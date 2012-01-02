/*****************************************************************************
 * profile_selector.cpp : A small profile selector and editor
 ****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
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


#include "components/sout/profile_selector.hpp"
#include "components/sout/profiles.hpp"
#include "dialogs/sout.hpp"

#include <QHBoxLayout>
#include <QToolButton>
#include <QComboBox>
#include <QLabel>
#include <QMessageBox>

#include <assert.h>

VLCProfileSelector::VLCProfileSelector( QWidget *_parent ): QWidget( _parent )
{
    QHBoxLayout *layout = new QHBoxLayout( this );

    QLabel *prLabel = new QLabel( qtr( "Profile"), this );
    layout->addWidget( prLabel );

    profileBox = new QComboBox( this );
    layout->addWidget( profileBox );

    QToolButton *editButton = new QToolButton( this );
    editButton->setIcon( QIcon( ":/menu/preferences" ) );
    editButton->setToolTip( qtr( "Edit selected profile" ) );
    layout->addWidget( editButton );

    QToolButton *deleteButton = new QToolButton( this );
    deleteButton->setIcon( QIcon( ":/toolbar/clear" ) );
    deleteButton->setToolTip( qtr( "Delete selected profile" ) );
    layout->addWidget( deleteButton );

    QToolButton *newButton = new QToolButton( this );
    newButton->setIcon( QIcon( ":/new" ) );
    newButton->setToolTip( qtr( "Create a new profile" ) );
    layout->addWidget(newButton);

    BUTTONACT( newButton, newProfile() );
    BUTTONACT( editButton, editProfile() );
    BUTTONACT( deleteButton, deleteProfile() );
    fillProfilesCombo();

    CONNECT( profileBox, activated( int ),
             this, updateOptions( int ) );

    updateOptions( 0 );
}

inline void VLCProfileSelector::fillProfilesCombo()
{
    QSettings settings(
#ifdef WIN32
            QSettings::IniFormat,
#else
            QSettings::NativeFormat,
#endif
            QSettings::UserScope, "vlc", "vlc-qt-interface" );

    int i_size = settings.beginReadArray( "codecs-profiles" );

    for( int i = 0; i < i_size; i++ )
    {
        settings.setArrayIndex( i );
        if( settings.value( "Profile-Name" ).toString().isEmpty() ) continue;
        profileBox->addItem( settings.value( "Profile-Name" ).toString(),
                settings.value( "Profile-Value" ) );
    }
    if( i_size == 0 )
    {
        for( size_t i = 0; i < NB_PROFILE; i++ )
        {
            profileBox->addItem( video_profile_name_list[i],
                                 video_profile_value_list[i] );
        }
    }
    settings.endArray();
}

void VLCProfileSelector::newProfile()
{
    editProfile( "", "" );
}

void VLCProfileSelector::editProfile()
{
    editProfile( profileBox->currentText(),
                 profileBox->itemData( profileBox->currentIndex() ).toString() );
}

void VLCProfileSelector::editProfile( const QString& qs, const QString& value )
{
    /* Create the Profile Editor */
    VLCProfileEditor *editor = new VLCProfileEditor( qs, value, this );

    /* Show it */
    if( QDialog::Accepted == editor->exec() )
    {
        /* New Profile */
        if( qs.isEmpty() )
            profileBox->addItem( editor->name, QVariant( editor->transcodeValue() ) );
        /* Update old profile */
        else
        {
            /* Look for the profile */
            int i_profile = profileBox->findText( qs );
            assert( i_profile != -1 );
            profileBox->setItemText( i_profile, editor->name );
            profileBox->setItemData( i_profile, QVariant( editor->transcodeValue() ) );
            /* Force mrl recreation */
            updateOptions( i_profile );
        }
    }
    delete editor;

    saveProfiles();
    emit optionsChanged();
}

void VLCProfileSelector::deleteProfile()
{
    profileBox->removeItem( profileBox->currentIndex() );
    saveProfiles();
}

void VLCProfileSelector::saveProfiles()
{
    QSettings settings(
#ifdef WIN32
            QSettings::IniFormat,
#else
            QSettings::NativeFormat,
#endif
            QSettings::UserScope, "vlc", "vlc-qt-interface" );

    settings.remove( "codecs-profiles" ); /* Erase old profiles to be rewritten */
    settings.beginWriteArray( "codecs-profiles" );
    for( int i = 0; i < profileBox->count(); i++ )
    {
        settings.setArrayIndex( i );
        settings.setValue( "Profile-Name", profileBox->itemText( i ) );
        settings.setValue( "Profile-Value", profileBox->itemData( i ).toString() );
    }
    settings.endArray();
}

void VLCProfileSelector::updateOptions( int i )
{
    QStringList options = profileBox->itemData( i ).toString().split( ";" );
    if( options.count() < 16 )
        return;

    mux = options[0];

    SoutMrl smrl;
    if( options[1].toInt() || options[2].toInt() || options[3].toInt() )
    {
        smrl.begin( "transcode" );

        if( options[1].toInt() )
        {
            smrl.option( "vcodec", options[4] );
            if( options[4] != "none" )
            {
                smrl.option( "vb", options[5].toInt() );
                if( !options[7].isEmpty() && options[7].toInt() > 0 )
                    smrl.option( "fps", options[7] );
                if( !options[6].isEmpty() )
                    smrl.option( "scale", options[6] );
                if( !options[8].isEmpty() && options[8].toInt() > 0 )
                    smrl.option( "width", options[8].toInt() );
                if( !options[9].isEmpty() && options[9].toInt() > 0 )
                    smrl.option( "height", options[9].toInt() );
            }
        }

        if( options[2].toInt() )
        {
            smrl.option( "acodec", options[10] );
            if( options[10] != "none" )
            {
                smrl.option( "ab", options[11].toInt() );
                smrl.option( "channels", options[12].toInt() );
                smrl.option( "samplerate", options[13].toInt() );
            }
        }

        if( options[3].toInt() )
        {
            smrl.option( "scodec", options[14] );
            if( options[15].toInt() )
                smrl.option( "soverlay" );
        }

        smrl.end();

        transcode = smrl.getMrl();
    }
    else
        transcode = "";
    emit optionsChanged();
}


/**
 * VLCProfileEditor
 **/
VLCProfileEditor::VLCProfileEditor( const QString& qs_name, const QString& value,
        QWidget *_parent )
                 : QVLCDialog( _parent, NULL )
{
    ui.setupUi( this );
    if( !qs_name.isEmpty() )
    {
        ui.profileLine->setText( qs_name );
        ui.profileLine->setReadOnly( true );
    }
    registerCodecs();
    CONNECT( ui.transcodeVideo, toggled( bool ),
            this, setVTranscodeOptions( bool ) );
    CONNECT( ui.transcodeAudio, toggled( bool ),
            this, setATranscodeOptions( bool ) );
    CONNECT( ui.transcodeSubs, toggled( bool ),
            this, setSTranscodeOptions( bool ) );
    setVTranscodeOptions( false );
    setATranscodeOptions( false );
    setSTranscodeOptions( false );

    QPushButton *saveButton = new QPushButton( qtr( "Save" ) );
    ui.buttonBox->addButton( saveButton, QDialogButtonBox::AcceptRole );
    BUTTONACT( saveButton, close() );
    QPushButton *cancelButton = new QPushButton( qtr( "Cancel" ) );
    ui.buttonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );
    BUTTONACT( cancelButton, reject() );

    fillProfile( value );
}

inline void VLCProfileEditor::registerCodecs()
{

#define ADD_VCODEC( name, fourcc ) ui.vCodecBox->addItem( name, QVariant( fourcc ) );
    ADD_VCODEC( "MPEG-1", "mp1v" )
    ADD_VCODEC( "MPEG-2", "mp2v" )
    ADD_VCODEC( "MPEG-4", "mp4v" )
    ADD_VCODEC( "DIVX 1" , "DIV1" )
    ADD_VCODEC( "DIVX 2" , "DIV2" )
    ADD_VCODEC( "DIVX 3" , "DIV3" )
    ADD_VCODEC( "H-263", "H263" )
    ADD_VCODEC( "H-264", "h264" )
    ADD_VCODEC( "VP8", "VP80" )
    ADD_VCODEC( "WMV1", "WMV1" )
    ADD_VCODEC( "WMV2" , "WMV2" )
    ADD_VCODEC( "M-JPEG", "MJPG" )
    ADD_VCODEC( "Theora", "theo" )
    ADD_VCODEC( "Dirac", "drac" )
#undef ADD_VCODEC

#define ADD_ACODEC( name, fourcc ) ui.aCodecBox->addItem( name, QVariant( fourcc ) );
    ADD_ACODEC( "MPEG Audio", "mpga" )
    ADD_ACODEC( "MP3", "mp3" )
    ADD_ACODEC( "MPEG 4 Audio ( AAC )", "mp4a" )
    ADD_ACODEC( "A52/AC-3", "a52" )
    ADD_ACODEC( "Vorbis", "vorb" )
    ADD_ACODEC( "Flac", "flac" )
    ADD_ACODEC( "Speex", "spx" )
    ADD_ACODEC( "WAV", "s16l" )
    ADD_ACODEC( "WMA2", "wma2" )
#undef ADD_ACODEC

#define ADD_SCALING( factor ) ui.vScaleBox->addItem( factor );
    ADD_SCALING( "1" )
    ADD_SCALING( "0.25" )
    ADD_SCALING( "0.5" )
    ADD_SCALING( "0.75" )
    ADD_SCALING( "1.25" )
    ADD_SCALING( "1.5" )
    ADD_SCALING( "1.75" )
    ADD_SCALING( "2" )
#undef ADD_SCALING

#define ADD_SAMPLERATE( sample ) ui.aSampleBox->addItem( sample );
    ADD_SAMPLERATE( "8000" )
    ADD_SAMPLERATE( "11025" )
    ADD_SAMPLERATE( "22050" )
    ADD_SAMPLERATE( "44100" )
    ADD_SAMPLERATE( "48000" )
#undef ADD_SAMPLERATE

#define ADD_SCODEC( name, fourcc ) ui.subsCodecBox->addItem( name, QVariant( fourcc ) );
    ADD_SCODEC( "DVB subtitle", "dvbs" )
    ADD_SCODEC( "T.140", "t140" )
#undef ADD_SCODEC
}

void VLCProfileEditor::fillProfile( const QString& qs )
{
    QStringList options = qs.split( ";" );
    if( options.count() < 16 )
        return;

    const QString mux = options[0];
#define CHECKMUX( button, text) if( text == mux ) ui.button->setChecked( true ); else
    CHECKMUX( PSMux, "ps" )
    CHECKMUX( TSMux, "ts" )
    CHECKMUX( WEBMux, "webm" )
    CHECKMUX( MPEG1Mux, "mpeg1" )
    CHECKMUX( OggMux, "ogg" )
    CHECKMUX( ASFMux, "asf" )
    CHECKMUX( MOVMux, "mp4" )
    CHECKMUX( WAVMux, "wav" )
    CHECKMUX( RAWMux, "raw" )
    CHECKMUX( FLVMux, "flv" )
    CHECKMUX( MKVMux, "mkv" )
    CHECKMUX( AVIMux, "avi" )
    CHECKMUX( MJPEGMux, "mpjpeg" ){}
#undef CHECKMUX

    ui.keepVideo->setChecked( !options[1].toInt() );
    ui.transcodeVideo->setChecked( ( options[4] != "none" ) );
    ui.keepAudio->setChecked( !options[2].toInt() );
    ui.transcodeAudio->setChecked( ( options[10] != "none" ) );
    ui.transcodeSubs->setChecked( options[3].toInt() );

    ui.vCodecBox->setCurrentIndex( ui.vCodecBox->findData( options[4] ) );
    ui.vBitrateSpin->setValue( options[5].toInt() );
    ui.vScaleBox->setEditText( options[6] );
    ui.vFrameBox->setValue( options[7].toDouble() );
    ui.widthBox->setText( options[8] );
    ui.heightBox->setText( options[9] );

    ui.aCodecBox->setCurrentIndex( ui.aCodecBox->findData( options[10] ) );
    ui.aBitrateSpin->setValue( options[11].toInt() );
    ui.aChannelsSpin->setValue( options[12].toInt() );
    ui.aSampleBox->setCurrentIndex( ui.aSampleBox->findText( options[13] ) );

    ui.subsCodecBox->setCurrentIndex( ui.subsCodecBox->findData( options[14] ) );
    ui.subsOverlay->setChecked( options[15].toInt() );
}

void VLCProfileEditor::setVTranscodeOptions( bool b_trans )
{
    ui.vCodecLabel->setEnabled( b_trans );
    ui.vCodecBox->setEnabled( b_trans );
    ui.vBitrateLabel->setEnabled( b_trans );
    ui.vBitrateSpin->setEnabled( b_trans );
    ui.vScaleLabel->setEnabled( b_trans );
    ui.vScaleBox->setEnabled( b_trans );
    ui.heightBox->setEnabled( b_trans );
    ui.heightLabel->setEnabled( b_trans );
    ui.widthBox->setEnabled( b_trans );
    ui.widthLabel->setEnabled( b_trans );
    ui.vFrameBox->setEnabled( b_trans );
    ui.vFrameLabel->setEnabled( b_trans );
    ui.keepVideo->setEnabled( b_trans );
}

void VLCProfileEditor::setATranscodeOptions( bool b_trans )
{
    ui.aCodecLabel->setEnabled( b_trans );
    ui.aCodecBox->setEnabled( b_trans );
    ui.aBitrateLabel->setEnabled( b_trans );
    ui.aBitrateSpin->setEnabled( b_trans );
    ui.aChannelsLabel->setEnabled( b_trans );
    ui.aChannelsSpin->setEnabled( b_trans );
    ui.aSampleLabel->setEnabled( b_trans );
    ui.aSampleBox->setEnabled( b_trans );
    ui.keepAudio->setEnabled( b_trans );
}

void VLCProfileEditor::setSTranscodeOptions( bool b_trans )
{
    ui.subsCodecBox->setEnabled( b_trans );
    ui.subsOverlay->setEnabled( b_trans );
}

void VLCProfileEditor::close()
{
    if( ui.profileLine->text().isEmpty() )
    {
        QMessageBox::warning( this, qtr(" Profile Name Missing" ),
                qtr( "You must set a name for the profile." ) );
        ui.profileLine->setFocus();
        return;
    }
    name = ui.profileLine->text();

    accept();
}

QString VLCProfileEditor::transcodeValue()
{
#define SMUX( x, txt ) if( ui.x->isChecked() ) muxValue =  txt; else
    SMUX( PSMux, "ps" )
    SMUX( TSMux, "ts" )
    SMUX( WEBMux, "webm" )
    SMUX( MPEG1Mux, "mpeg1" )
    SMUX( OggMux, "ogg" )
    SMUX( ASFMux, "asf" )
    SMUX( MOVMux, "mp4" )
    SMUX( WAVMux, "wav" )
    SMUX( RAWMux, "raw" )
    SMUX( FLVMux, "flv" )
    SMUX( MKVMux, "mkv" )
    SMUX( AVIMux, "avi" )
    SMUX( MJPEGMux, "mpjpeg" ){}
#undef SMUX

#define currentData( box ) box->itemData( box->currentIndex() )
    QString qs_acodec, qs_vcodec;

    qs_vcodec = ( ui.transcodeVideo->isChecked() ) ? currentData( ui.vCodecBox ).toString()
                                                   : "none";
    qs_acodec = ( ui.transcodeAudio->isChecked() ) ? currentData( ui.aCodecBox ).toString()
                                                   : "none";
    QStringList transcodeMRL;
    transcodeMRL
            << muxValue

            << QString::number( !ui.keepVideo->isChecked() )
            << QString::number( !ui.keepAudio->isChecked() )
            << QString::number( ui.transcodeSubs->isChecked() )

            << qs_vcodec
            << QString::number( ui.vBitrateSpin->value() )
            << ui.vScaleBox->currentText()
            << QString::number( ui.vFrameBox->value() )
            << ui.widthBox->text()
            << ui.heightBox->text()

            << qs_acodec
            << QString::number( ui.aBitrateSpin->value() )
            << QString::number( ui.aChannelsSpin->value() )
            << ui.aSampleBox->currentText()

            << currentData( ui.subsCodecBox ).toString()
            << QString::number( ui.subsOverlay->isChecked() );
#undef currentData

    return transcodeMRL.join( ";" );
}

