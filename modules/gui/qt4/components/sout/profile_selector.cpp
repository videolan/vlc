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
#include <QRadioButton>
#include <QRegExp>
#include <QButtonGroup>
#include <QSpinBox>
#include <QUrl>
#include <QListWidgetItem>

#include <assert.h>
#include <vlc_modules.h>

#define CATPROP2NAME( val ) QString("valueholder_%1").arg( val )
#define CATANDPROP( cat, prop ) QString("%1_%2").arg( cat ).arg( prop )
#define OLDFORMAT "^\\w+;\\d+;\\d+;\\d+;"

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
#ifdef _WIN32
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
            profileBox->setItemData( i_profile, editor->transcodeValue() );
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
#ifdef _WIN32
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
    QString options = profileBox->itemData( i ).toString();
    QRegExp rx(OLDFORMAT);
    if ( !options.contains( ";" ) ) return;
    if ( rx.indexIn( options ) != -1 )
        return updateOptionsOldFormat( i );

    transcode = "";

    QStringList tuples = options.split( ";" );
    typedef QHash<QString, QString> proptovalueHashType;
    QHash<QString, proptovalueHashType *> categtopropHash;
    proptovalueHashType *proptovalueHash;
    QString value;

    /* Build a double hash structure because we need to make ordered lookups */
    foreach ( const QString &tuple, tuples )
    {
        QStringList keyvalue = tuple.split( "=" );
        if ( keyvalue.count() != 2 ) continue;
        QString key = keyvalue[0];
        value = keyvalue[1];
        keyvalue = key.split( "_" );
        if ( keyvalue.count() != 2 ) continue;
        QString categ = keyvalue[0];
        QString prop = keyvalue[1];

        if ( ! categtopropHash.contains( categ ) )
        {
            proptovalueHash = new proptovalueHashType();
            categtopropHash.insert( categ, proptovalueHash );
        } else {
            proptovalueHash = categtopropHash.value( categ );
        }
        proptovalueHash->insert( prop, value );
    }

    /* Now we can build the/translate into MRL */
#define HASHPICK( categ, prop ) \
    if ( categtopropHash.contains( categ ) ) \
    {\
        proptovalueHash = categtopropHash.value( categ );\
        value = proptovalueHash->take( prop );\
    }\
    else value = QString()

    SoutMrl smrl;
    smrl.begin( "transcode" );

    /* First muxer options */
    HASHPICK( "muxer", "mux" );
    if ( value.isEmpty() ) goto cleanup;
    mux = value;

    HASHPICK( "video", "enable" );
    if ( !value.isEmpty() )
    {
        HASHPICK( "video", "codec" );

        if ( !value.isEmpty() )
        {
            smrl.option( "vcodec", value );

            HASHPICK( "vcodec", "bitrate" );
            if ( value.toInt() > 0 )
            {
                smrl.option( "vb", value.toInt() );
            }

            HASHPICK( "video", "filters" );
            if ( !value.isEmpty() )
            {
                QStringList valuesList = QUrl::fromPercentEncoding( value.toLatin1() ).split( ";" );
                smrl.option( "vfilter", valuesList.join( ":" ) );
            }

            /*if ( codec is h264 )*/
            {
                /* special handling */
                QStringList codecoptions;

                HASHPICK( "vcodec", "qp" );
                if( value.toInt() > 0 )
                    codecoptions << QString( "qp=%1" ).arg( value );

                HASHPICK( "vcodec", "custom" );
                if( !value.isEmpty() )
                    codecoptions << QUrl::fromPercentEncoding( value.toLatin1() );

                if ( codecoptions.count() )
                    smrl.option( "venc",
                        QString("x264{%1}").arg( codecoptions.join(",") ) );
            }

            HASHPICK( "vcodec", "framerate" );
            if ( !value.isEmpty() && value.toInt() > 0 )
                smrl.option( "fps", value );

            HASHPICK( "vcodec", "scale" );
            if ( !value.isEmpty() )
                smrl.option( "scale", value );

            HASHPICK( "vcodec", "width" );
            if ( !value.isEmpty() && value.toInt() > 0 )
                smrl.option( "width", value );

            HASHPICK( "vcodec", "height" );
            if ( !value.isEmpty() && value.toInt() > 0 )
                smrl.option( "height", value );
        }
    } else {
        smrl.option( "vcodec", "none" );
    }

    HASHPICK( "audio", "enable" );
    if ( !value.isEmpty() )
    {
        HASHPICK( "audio", "codec" );
        if ( !value.isEmpty() )
        {
            smrl.option( "acodec", value );

            HASHPICK( "acodec", "bitrate" );
            smrl.option( "ab", value.toInt() );

            HASHPICK( "acodec", "channels" );
            smrl.option( "channels", value.toInt() );

            HASHPICK( "acodec", "samplerate" );
            smrl.option( "samplerate", value.toInt() );

            HASHPICK( "audio", "filters" );
            if ( !value.isEmpty() )
            {
                QStringList valuesList = QUrl::fromPercentEncoding( value.toLatin1() ).split( ";" );
                smrl.option( "afilter", valuesList.join( ":" ) );
            }

        }
    } else {
        smrl.option( "acodec", "none" );
    }

    HASHPICK( "subtitles", "enable" );
    if( !value.isEmpty() )
    {
        HASHPICK( "subtitles", "codec" );
        smrl.option( "scodec", value );

        HASHPICK( "subtitles", "overlay" );
        if ( !value.isEmpty() )
            smrl.option( "soverlay" );
    }
    smrl.end();
#undef HASHPICK

    transcode = smrl.getMrl();

    cleanup:
    /* Temp hash tables cleanup */
    foreach( proptovalueHashType *hash, categtopropHash )
        delete hash;

    emit optionsChanged();
}

void VLCProfileSelector::updateOptionsOldFormat( int i )
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
    ui.buttonGroup->setObjectName( CATPROP2NAME( CATANDPROP( "muxer", "mux" ) ) );
    if( !qs_name.isEmpty() )
    {
        ui.profileLine->setText( qs_name );
        ui.profileLine->setReadOnly( true );
    }
    loadCapabilities();
    registerCodecs();
    registerFilters();

    QPushButton *saveButton = new QPushButton(
                ( qs_name.isEmpty() ) ? qtr( "Create" ) : qtr( "Save" ) );
    ui.buttonBox->addButton( saveButton, QDialogButtonBox::AcceptRole );
    BUTTONACT( saveButton, close() );
    QPushButton *cancelButton = new QPushButton( qtr( "Cancel" ) );
    ui.buttonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );
    BUTTONACT( cancelButton, reject() );

    CONNECT( ui.valueholder_video_copy, stateChanged( int ),
             this, activatePanels() );
    CONNECT( ui.valueholder_audio_copy, stateChanged( int ),
             this, activatePanels() );
    CONNECT( ui.valueholder_vcodec_bitrate, editingFinished( ),
             this, fixBirateState() );
    CONNECT( ui.valueholder_vcodec_qp, editingFinished( ),
             this, fixQPState() );
    CONNECT( ui.valueholder_video_codec, currentIndexChanged( int ),
             this, codecSelected() );
    reset();

    fillProfile( value );
    muxSelected();
    codecSelected();
}

void VLCProfileEditor::loadCapabilities()
{
    size_t count;
    module_t **p_all = module_list_get (&count);

    /* Parse the module list for capabilities and probe each of them */
    for (size_t i = 0; i < count; i++)
    {
         module_t *p_module = p_all[i];

         if( module_provides( p_module, "sout mux" ) )
             caps["muxers"].insert( module_get_object( p_module ) );
//        else if ( module_provides( p_module, "encoder" ) )
//            caps["encoders"].insert( module_get_object( p_module ) );
    }
    module_list_free (p_all);
}

inline void VLCProfileEditor::registerFilters()
{
    size_t count;
    module_t **p_all = module_list_get (&count);

    for (size_t i = 0; i < count; i++)
    {
        module_t *p_module = p_all[i];
        if ( module_get_score( p_module ) > 0 ) continue;

        QString capability = module_get_capability( p_module );
        QListWidget *listWidget = NULL;
        QListWidgetItem *item;

        if ( capability == "video filter2" )
            listWidget = ui.valueholder_video_filters;
        else if ( capability == "audio filter" )
            listWidget = ui.valueholder_audio_filters;

        if ( !listWidget ) continue;

        item = new QListWidgetItem( module_get_name( p_module, true ) );
        item->setCheckState( Qt::Unchecked );
        item->setToolTip( QString( module_get_help( p_module ) ) );
        item->setData( Qt::UserRole, QString( module_get_object( p_module ) ) );
        listWidget->addItem( item );
    }
    module_list_free (p_all);

    ui.valueholder_video_filters->sortItems();
    ui.valueholder_audio_filters->sortItems();
}

inline void VLCProfileEditor::registerCodecs()
{
#define SETMUX( button, val,    vid, aud, men, sub, stream, chaps ) \
    ui.button->setProperty( "sout", val );\
    ui.button->setProperty( "capvideo", vid );\
    ui.button->setProperty( "capaudio", aud );\
    ui.button->setProperty( "capmenu", men );\
    ui.button->setProperty( "capsubs", sub );\
    ui.button->setProperty( "capstream", stream );\
    ui.button->setProperty( "capchaps", chaps );\
    CONNECT( ui.button, clicked(bool), this, muxSelected() );
    SETMUX( PSMux, "ps",        true, true, false, true, false, true )
    SETMUX( TSMux, "ts",        true, true, false, true, true, false )
    SETMUX( WEBMux, "webm",     true, true, false, false, true, false )
    SETMUX( MPEG1Mux, "mpeg1",  true, true, false, false, false, false )
    SETMUX( OggMux, "ogg",      true, true, false, false, true, true )
    SETMUX( ASFMux, "asf",      true, true, false, true, true, true )
    SETMUX( MOVMux, "mp4",      true, true, true, true, true, false )
    SETMUX( WAVMux, "wav",      false, true, false, false, false, false )
    SETMUX( RAWMux, "raw",      true, true, false, false, false, false )
    SETMUX( FLVMux, "flv",      true, true, false, false, true, false )
    SETMUX( MKVMux, "mkv",      true, true, true, true, true, true )
    SETMUX( AVIMux, "avi",      true, true, false, false, false, false )
    SETMUX( MJPEGMux, "mpjpeg", true, false, false, false, false, false )
#undef SETMUX

#define ADD_VCODEC( name, fourcc ) \
            ui.valueholder_video_codec->addItem( name, QVariant( fourcc ) );
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
    /* can do quality */
    qpcodecsList << "h264";

#define ADD_ACODEC( name, fourcc ) ui.valueholder_audio_codec->addItem( name, QVariant( fourcc ) );
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

#define ADD_SCALING( factor ) ui.valueholder_vcodec_scale->addItem( factor );
    ADD_SCALING( qtr("Auto") );
    ADD_SCALING( "1" )
    ADD_SCALING( "0.25" )
    ADD_SCALING( "0.5" )
    ADD_SCALING( "0.75" )
    ADD_SCALING( "1.25" )
    ADD_SCALING( "1.5" )
    ADD_SCALING( "1.75" )
    ADD_SCALING( "2" )
#undef ADD_SCALING

#define ADD_SAMPLERATE( sample, val ) ui.valueholder_acodec_samplerate->addItem( sample, val );
    ADD_SAMPLERATE( "8000 Hz", 8000 )
    ADD_SAMPLERATE( "11025 Hz", 11025 )
    ADD_SAMPLERATE( "22050 Hz", 22050 )
    ADD_SAMPLERATE( "44100 Hz", 44100 )
    ADD_SAMPLERATE( "48000 Hz", 48000 )
#undef ADD_SAMPLERATE

#define ADD_SCODEC( name, fourcc ) ui.valueholder_subtitles_codec->addItem( name, QVariant( fourcc ) );
    ADD_SCODEC( "DVB subtitle", "dvbs" )
    ADD_SCODEC( "T.140", "t140" )
#undef ADD_SCODEC
}

void VLCProfileEditor::muxSelected()
{
    QRadioButton *current =
            qobject_cast<QRadioButton *>( ui.buttonGroup->checkedButton() );

#define SETYESNOSTATE( name, prop ) \
    ui.name->setChecked( current->property( prop ).toBool() )

    /* dumb :/ */
    SETYESNOSTATE( capvideo, "capvideo" );
    SETYESNOSTATE( capaudio, "capaudio" );
    SETYESNOSTATE( capmenu, "capmenu" );
    SETYESNOSTATE( capsubs, "capsubs" );
    SETYESNOSTATE( capstream, "capstream" );
    SETYESNOSTATE( capchaps, "capchaps" );
    bool b = caps["muxers"].contains( "mux_" + current->property("sout").toString() );
    if ( !b )
        ui.muxerwarning->setText(
                    QString( "<img src=\":/menu/info\"/> %1" )
                    .arg( qtr( "This muxer is not provided directly by VLC: It could be missing." ) )
                    );
    else
        ui.muxerwarning->setText("");
    return;

#undef SETYESNOSTATE
}

void VLCProfileEditor::codecSelected()
{
    /* Enable quality preset */
    QString currentcodec = ui.valueholder_video_codec->
            itemData(ui.valueholder_video_codec->currentIndex() ).toString();
    ui.valueholder_vcodec_qp->setEnabled( qpcodecsList.contains( currentcodec ) );
}

void VLCProfileEditor::fillProfile( const QString& qs )
{
    QRegExp rx(OLDFORMAT);
    if ( rx.indexIn( qs ) != -1 ) return fillProfileOldFormat( qs );

    QStringList tuples = qs.split( ";" );
    foreach ( const QString &tuple, tuples )
    {
        QStringList keyvalue = tuple.split( "=" );
        if ( keyvalue.count() != 2 ) continue;
        QString key = keyvalue[0];
        QString value = keyvalue[1];
        QObject *object = findChild<QObject *>( CATPROP2NAME( key ) );
        if ( object )
        {
            if( object->inherits( "QButtonGroup" ) )
            { /* Buttongroup for Radios */
                const QButtonGroup *group = qobject_cast<const QButtonGroup *>( object );
                foreach( QAbstractButton *button, group->buttons() )
                {
                    if ( button->property("sout").toString() == value )
                    {
                        button->setChecked( true );
                        break;/* radios are exclusive */
                    }
                }
            }
            else if( object->inherits( "QCheckBox" ) )
            {
                QCheckBox *box = qobject_cast<QCheckBox *>( object );
                box->setChecked( ! value.isEmpty() );
            }
            else if( object->inherits( "QGroupBox" ) )
            {
                QGroupBox *box = qobject_cast<QGroupBox *>( object );
                box->setChecked( ! value.isEmpty() );
            }
            else if( object->inherits( "QSpinBox" ) )
            {
                QSpinBox *box = qobject_cast<QSpinBox *>( object );
                box->setValue( value.toInt() );
            }
            else if( object->inherits( "QDoubleSpinBox" ) )
            {
                QDoubleSpinBox *box = qobject_cast<QDoubleSpinBox *>( object );
                box->setValue( value.toDouble() );
            }
            else if( object->inherits( "QComboBox" ) )
            {
                QComboBox *box = qobject_cast<QComboBox *>( object );
                box->setCurrentIndex( box->findData( value ) );
                if ( box->lineEdit() && box->currentIndex() == -1 )
                    box->lineEdit()->setText( value );
            }
            else if( object->inherits( "QLineEdit" ) )
            {
                QLineEdit *box = qobject_cast<QLineEdit *>( object );
                box->setText( QUrl::fromPercentEncoding( value.toLatin1() ) );
            }
            else if ( object->inherits( "QListWidget" ) )
            {
                QStringList valuesList = QUrl::fromPercentEncoding( value.toLatin1() ).split( ";" );
                const QListWidget *list = qobject_cast<const QListWidget *>( object );
                for( int i=0; i < list->count(); i++ )
                {
                    QListWidgetItem *item = list->item( i );
                    if ( valuesList.contains( item->data( Qt::UserRole ).toString() ) )
                        item->setCheckState( Qt::Checked );
                    else
                        item->setCheckState( Qt::Unchecked );
                }
            }
        }
    }
}

void VLCProfileEditor::fillProfileOldFormat( const QString& qs )
{
    QStringList options = qs.split( ";" );
    if( options.count() < 16 )
        return;

    const QString mux = options[0];
    for ( int i=0; i< ui.muxer->layout()->count(); i++ )
    {
        QRadioButton *current =
                qobject_cast<QRadioButton *>(ui.muxer->layout()->itemAt(i)->widget());
        if ( unlikely( !current ) ) continue;/* someone is messing up with ui */
        if ( current->property("sout").toString() == mux )
        {
            current->setChecked( true );
            break;/* radios are exclusive */
        }
    }

    ui.valueholder_video_copy->setChecked( !options[1].toInt() );
    ui.valueholder_video_enable->setChecked( ( options[4] != "none" ) );
    ui.valueholder_audio_copy->setChecked( !options[2].toInt() );
    ui.valueholder_audio_enable->setChecked( ( options[10] != "none" ) );
    ui.valueholder_subtitles_enable->setChecked( options[3].toInt() );

    ui.valueholder_video_codec->setCurrentIndex( ui.valueholder_video_codec->findData( options[4] ) );
    ui.valueholder_vcodec_bitrate->setValue( options[5].toInt() );
    if ( options[6].toInt() > 0 )
        ui.valueholder_vcodec_scale->setEditText( options[6] );
    else
        ui.valueholder_vcodec_scale->setCurrentIndex( 0 );
    ui.valueholder_vcodec_framerate->setValue( options[7].toDouble() );
    ui.valueholder_vcodec_width->setValue( options[8].toInt() );
    ui.valueholder_vcodec_height->setValue( options[9].toInt() );

    ui.valueholder_audio_codec->setCurrentIndex( ui.valueholder_audio_codec->findData( options[10] ) );
    ui.valueholder_acodec_bitrate->setValue( options[11].toInt() );
    ui.valueholder_acodec_channels->setValue( options[12].toInt() );

    int index = ui.valueholder_acodec_samplerate->findData( options[13] );
    if ( index == -1 ) index = ui.valueholder_acodec_samplerate->findData( 44100 );
    ui.valueholder_acodec_samplerate->setCurrentIndex( index );

    ui.valueholder_subtitles_codec->setCurrentIndex( ui.valueholder_subtitles_codec->findData( options[14] ) );
    ui.valueholder_subtitles_overlay->setChecked( options[15].toInt() );
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

#define currentData( box ) box->itemData( box->currentIndex() )

QString VLCProfileEditor::transcodeValue()
{
    QList<QObject *> allwidgets = findChildren<QObject *>();
    QStringList configuration;

    foreach( const QObject *object, allwidgets )
    {
        if ( ! object->objectName().startsWith( CATPROP2NAME( "" ) ) )
            continue;
        if ( object->inherits( "QWidget" ) &&
             ! qobject_cast<const QWidget *>(object)->isEnabled() ) continue;

        QString name = object->objectName();
        QStringList vals = object->objectName().split( "_" );
        if ( vals.count() != 3 ) continue;
        QString &categ = vals[1];
        QString &prop = vals[2];
        QString value;

        if( object->inherits( "QButtonGroup" ) )
        {
            const QButtonGroup *group = qobject_cast<const QButtonGroup *>( object );
            value = group->checkedButton()->property( "sout" ).toString();
        }
        else if( object->inherits( "QCheckBox" ) )
        {
            const QCheckBox *box = qobject_cast<const QCheckBox *>( object );
            value = box->isChecked() ? "yes" : "";
        }
        else if( object->inherits( "QGroupBox" ) )
        {
            const QGroupBox *box = qobject_cast<const QGroupBox *>( object );
            value = box->isChecked() ? "yes" : "";
        }
        else if( object->inherits( "QSpinBox" ) )
        {
            const QSpinBox *box = qobject_cast<const QSpinBox *>( object );
            value = QString::number( box->value() );
        }
        else if( object->inherits( "QDoubleSpinBox" ) )
        {
            const QDoubleSpinBox *box = qobject_cast<const QDoubleSpinBox *>( object );
            value =  QString::number( box->value() );
        }
        else if( object->inherits( "QComboBox" ) )
        {
            const QComboBox *box = qobject_cast<const QComboBox *>( object );
            value = currentData( box ).toString();
            if ( value.isEmpty() && box->lineEdit() ) value = box->lineEdit()->text();
        }
        else if( object->inherits( "QLineEdit" ) )
        {
            const QLineEdit *box = qobject_cast<const QLineEdit *>( object );
            value = QUrl::toPercentEncoding( box->text(), "", "_;" );
        }
        else if ( object->inherits( "QListWidget" ) )
        {
            const QListWidget *list = qobject_cast<const QListWidget *>( object );
            QStringList valuesList;
            for( int i=0; i < list->count(); i++ )
            {
                const QListWidgetItem *item = list->item( i );
                if ( item->checkState() == Qt::Checked )
                    valuesList.append( item->data( Qt::UserRole ).toString() );
            }
            value = QUrl::toPercentEncoding( valuesList.join( ";" ), "", "_;" );
        }

        if ( !value.isEmpty() )
            configuration << QString( "%1_%2=%3" )
                             .arg( categ ).arg( prop ).arg( value );
    }

    return configuration.join( ";" );
}

void VLCProfileEditor::reset()
{
    /* reset to default state as we can only check/enable existing values */
    ui.valueholder_video_copy->setChecked( false );
    ui.valueholder_audio_copy->setChecked( false );
    activatePanels();
    fixBirateState(); /* defaults to bitrate, not qp */
    /* end with top level ones for cascaded setEnabled() */
    ui.valueholder_video_enable->setChecked( false );
    ui.valueholder_audio_enable->setChecked( false );
    ui.valueholder_subtitles_enable->setChecked( false );
}

void VLCProfileEditor::activatePanels()
{
    ui.transcodevideo->setEnabled( ! ui.valueholder_video_copy->isChecked() );
    ui.transcodeaudio->setEnabled( ! ui.valueholder_audio_copy->isChecked() );
}

void VLCProfileEditor::fixBirateState()
{
    /* exclusive bitrate choice */
    ui.valueholder_vcodec_qp->setValue( 0 );
}

void VLCProfileEditor::fixQPState()
{
    /* exclusive bitrate choice */
    ui.valueholder_vcodec_bitrate->setValue( 0 );
}

#undef CATPROP2NAME
#undef CATANDPROP
#undef OLDFORMAT
