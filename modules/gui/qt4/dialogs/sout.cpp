/*****************************************************************************
 * sout.cpp : Stream output dialog ( old-style )
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Jean-François Massol <jf.massol -at- gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#include "dialogs/sout.hpp"
#include "qt4.hpp"
#include <vlc_streaming.h>

#include <iostream>
#include <QString>
#include <QFileDialog>

SoutDialog::SoutDialog( QWidget *parent, intf_thread_t *_p_intf,
                     bool _transcode_only ) : QVLCDialog( parent,  _p_intf )
{
    setWindowTitle( qtr( "Stream output" ) );

    b_transcode_only = _transcode_only;

    /* UI stuff */
    ui.setupUi( this );

/* ADD HERE for new profiles */
#define ADD_PROFILE( name, shortname ) ui.profileBox->addItem( qtr( name ), QVariant( QString( shortname ) ) );
    ADD_PROFILE( "Custom" , "Custom" )
    ADD_PROFILE( "IPod (mp4/aac)", "IPod" )
    ADD_PROFILE( "XBox", "XBox" )
    ADD_PROFILE( "Windows (wmv/asf)", "Windows" )
    ADD_PROFILE( "PSP", "PSP")
    ADD_PROFILE( "GSM", "GSM" )

#define ADD_VCODEC( name, fourcc ) ui.vCodecBox->addItem( name, QVariant( fourcc ) );
    ADD_VCODEC( "MPEG-1", "mp1v" )
    ADD_VCODEC( "MPEG-2", "mp2v" )
    ADD_VCODEC( "MPEG-4", "mp4v" )
    ADD_VCODEC( "DIVX 1" , "DIV1" )
    ADD_VCODEC( "DIVX 2" , "DIV1" )
    ADD_VCODEC( "DIVX 3" , "DIV1" )
    ADD_VCODEC( "H-263", "H263" )
    ADD_VCODEC( "H-264", "h264" )
    ADD_VCODEC( "WMV1", "WMV1" )
    ADD_VCODEC( "WMV2" , "WMV2" )
    ADD_VCODEC( "M-JPEG", "MJPG" )
    ADD_VCODEC( "Theora", "theo" )

#define ADD_ACODEC( name, fourcc ) ui.aCodecBox->addItem( name, QVariant( fourcc ) );
    ADD_ACODEC( "MPEG Audio", "mpga" )
    ADD_ACODEC( "MP3", "mp3" )
    ADD_ACODEC( "MPEG 4 Audio ( AAC )", "mp4a" )
    ADD_ACODEC( "A52/AC-3", "a52" )
    ADD_ACODEC( "Vorbis", "vorb" )
    ADD_ACODEC( "Flac", "flac" )
    ADD_ACODEC( "Speex", "spx" )
    ADD_ACODEC( "WAV", "s16l" )
    ADD_ACODEC( "WMA", "wma" )

#define ADD_SCALING( factor ) ui.vScaleBox->addItem( factor );
    ADD_SCALING( "0.25" )
    ADD_SCALING( "0.5" )
    ADD_SCALING( "0.75" )
    ADD_SCALING( "1" )
    ADD_SCALING( "1.25" )
    ADD_SCALING( "1.5" )
    ADD_SCALING( "1.75" )
    ADD_SCALING( "2" )

    ui.mrlEdit->setToolTip ( qtr( "Stream output string.\n This is automatically generated "
                                                "when you change the above settings,\n but you can update it manually." ) ) ;

//     /* Connect everything to the updateMRL function */
 #define CB( x ) CONNECT( ui.x, clicked( bool ), this, updateMRL() );
 #define CT( x ) CONNECT( ui.x, textChanged( const QString ), this, updateMRL() );
 #define CS( x ) CONNECT( ui.x, valueChanged( int ), this, updateMRL() );
 #define CC( x ) CONNECT( ui.x, currentIndexChanged( int ), this, updateMRL() );
//     /* Output */
     CB( fileOutput ); CB( HTTPOutput ); CB( localOutput );
     CB( UDPOutput ); CB( MMSHOutput ); CB( rawInput );
     CT( fileEdit ); CT( HTTPEdit ); CT( UDPEdit ); CT( MMSHEdit );
     CS( HTTPPort ); CS( UDPPort ); CS( MMSHPort );
//     /* Transcode */
     CC( vCodecBox ); CC( subsCodecBox ); CC( aCodecBox ) ;
     CB( transcodeVideo ); CB( transcodeAudio ); CB( transcodeSubs );
//     CB( sOverlay );
     CS( vBitrateSpin ); CS( aBitrateSpin ); CS( aChannelsSpin ); CC( vScaleBox );
//     /* Mux */
     CB( PSMux ); CB( TSMux ); CB( MPEG1Mux ); CB( OggMux ); CB( ASFMux );
     CB( MP4Mux ); CB( MOVMux ); CB( WAVMux ); CB( RAWMux ); CB( FLVMux );
//     /* Misc */
     CB( soutAll ); CS( ttl ); CT( sapName ); CT( sapGroup );
//
    CONNECT( ui.profileBox, activated( const QString & ), this, setOptions() );
    CONNECT( ui.fileSelectButton, clicked() , this, fileBrowse()  );
    CONNECT( ui.transcodeVideo, toggled( bool ), this, setVTranscodeOptions( bool ) );
    CONNECT( ui.transcodeAudio, toggled( bool ), this, setATranscodeOptions( bool ) );
    CONNECT( ui.transcodeSubs, toggled( bool ), this, setSTranscodeOptions( bool ) );
    CONNECT( ui.rawInput, toggled( bool ), this, setRawOptions( bool ) );

    okButton = new QPushButton( qtr( "&Stream" ) );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ) );

    okButton->setDefault( true );
    ui.acceptButtonBox->addButton( okButton, QDialogButtonBox::AcceptRole );
    ui.acceptButtonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    BUTTONACT( okButton, ok() );
    BUTTONACT( cancelButton, cancel() );

    if( b_transcode_only ) toggleSout();
}

void SoutDialog::fileBrowse()
{
    ui.tabWidget->setTabEnabled( 0,false );
    QString f = QFileDialog::getOpenFileName( this, qtr( "Save file" ), "", "" );
    ui.fileEdit->setText( f );
    updateMRL();
}

void SoutDialog::setVTranscodeOptions( bool b_trans )
{
    ui.vCodecLabel->setEnabled( b_trans );
    ui.vCodecBox->setEnabled( b_trans );
    ui.vBitrateLabel->setEnabled( b_trans );
    ui.vBitrateSpin->setEnabled( b_trans );
    ui.vScaleLabel->setEnabled( b_trans );
    ui.vScaleBox->setEnabled( b_trans );
}

void SoutDialog::setATranscodeOptions( bool b_trans )
{
    ui.aCodecLabel->setEnabled( b_trans );
    ui.aCodecBox->setEnabled( b_trans );
    ui.aBitrateLabel->setEnabled( b_trans );
    ui.aBitrateSpin->setEnabled( b_trans );
    ui.aChannelsLabel->setEnabled( b_trans );
    ui.aChannelsSpin->setEnabled( b_trans );
}

void SoutDialog::setSTranscodeOptions( bool b_trans )
{
    ui.subsCodecBox->setEnabled( b_trans );
    ui.subsOverlay->setEnabled( b_trans );
}

void SoutDialog::setRawOptions( bool b_raw )
{
    if ( b_raw )
    {
        ui.tabWidget->setDisabled( true );
    }
    else
    {
        SoutDialog::setOptions();
    }
}

int indexFromItemData( QComboBox *aCombo, QString aString )
{
    for( int i=0; i < aCombo->count(); i++ )
    {
        if( aCombo->itemData( i ).toString() == aString ) return i;
    }
    return -1;
}

void SoutDialog::setOptions()
{
    QString profileString = ui.profileBox->itemData( ui.profileBox->currentIndex() ).toString();
    msg_Dbg( p_intf, "Profile Used: %s",  qta( profileString ));
    int index;

#define setProfile( muxName, hasVideo, vCodecName, hasAudio, aCodecName ) \
    { \
        ui.muxName ##Mux->setChecked( true ); \
        \
        ui.transcodeAudio->setChecked( hasAudio ); \
        index = indexFromItemData( ui.aCodecBox, vCodecName );  \
        if( index >= 0 ) ui.aCodecBox->setCurrentIndex( index ); \
        \
        ui.transcodeVideo->setChecked( hasVideo ); \
        index = indexFromItemData( ui.aCodecBox, vCodecName );  \
        if( index >=0 ) ui.vCodecBox->setCurrentIndex( index ); \
    }

    /* ADD HERE the profiles you want and need */
    if( profileString == "IPod" ) setProfile( MP4, true, "mp4a", true, "mp4v" )
    else if( profileString == "XBox" ) setProfile( ASF, true, "wma", true, "WMV2" )

        /* If the profile is not a custom one, then disable the tabWidget */
        if ( profileString == "Custom" )
        {
            ui.tabWidget->setEnabled( true );
        }
        else
        {
            ui.tabWidget->setDisabled( true );
        }

    /* Update the MRL !! */
    updateMRL();
}

void SoutDialog::toggleSout()
{
    //Toggle all the streaming options.
    #define TGV( x ) { \
        if( ( x->isHidden() ) )  \
        x->show();          \
        else  x->hide();\
    }
    TGV( ui.HTTPOutput ) ; TGV( ui.UDPOutput ) ; TGV( ui.MMSHOutput ) ;
    TGV( ui.HTTPEdit ) ; TGV( ui.UDPEdit ) ; TGV( ui.MMSHEdit ) ;
    TGV( ui.HTTPLabel ) ; TGV( ui.UDPLabel ) ; TGV( ui.MMSHLabel ) ;
    TGV( ui.HTTPPortLabel ) ; TGV( ui.UDPPortLabel ) ; TGV( ui.MMSHPortLabel ) ;
    TGV( ui.HTTPPort ) ; TGV( ui.UDPPort ) ; TGV( ui.MMSHPort ) ;

    TGV( ui.sap ); TGV( ui.sapName );
    TGV( ui.sapGroup ); TGV( ui.sapGroupLabel );
    TGV( ui.ttlLabel ); TGV( ui.ttl );

    if( b_transcode_only ) okButton->setText( "&Save" );
    else okButton->setText( "&Stream" );

    updateGeometry();
}

void SoutDialog::ok()
{
    mrl = ui.mrlEdit->text();
    accept();
}
void SoutDialog::cancel()
{
    mrl = ui.mrlEdit->text();
    reject();
}

void SoutDialog::updateMRL()
{
    sout_gui_descr_t sout;
    memset( &sout, 0, sizeof( sout_gui_descr_t ) );

    sout.b_local = ui.localOutput->isChecked();
    sout.b_file = ui.fileOutput->isChecked();
    sout.b_http = ui.HTTPOutput->isChecked();
    sout.b_mms = ui.MMSHOutput->isChecked();
    sout.b_udp = ui.UDPOutput->isChecked();
    sout.b_sap = ui.sap->isChecked();
    sout.b_all_es = ui.soutAll->isChecked();
    sout.psz_vcodec = strdup( qtu( ui.vCodecBox->itemData( ui.vCodecBox->currentIndex() ).toString() ) );
    sout.psz_acodec = strdup( qtu( ui.aCodecBox->itemData( ui.aCodecBox->currentIndex() ).toString() ) );
    sout.psz_scodec = strdup( qtu( ui.subsCodecBox->itemData( ui.subsCodecBox->currentIndex() ).toString() ) );
    sout.psz_file = strdup( qtu( ui.fileEdit->text() ) );
    sout.psz_http = strdup( qtu( ui.HTTPEdit->text() ) );
    sout.psz_mms = strdup( qtu( ui.MMSHEdit->text() ) );
    sout.psz_udp = strdup( qtu( ui.UDPEdit->text() ) );
    sout.i_http = ui.HTTPPort->value();
    sout.i_mms = ui.MMSHPort->value();
    sout.i_udp = ui.UDPPort->value();
    sout.i_ab = ui.aBitrateSpin->value();
    sout.i_vb = ui.vBitrateSpin->value();
    sout.i_channels = ui.aChannelsSpin->value();
    sout.f_scale = atof( qta( ui.vScaleBox->currentText() ) );
    sout.psz_group = strdup( qtu( ui.sapGroup->text() ) );
    sout.psz_name = strdup( qtu( ui.sapName->text() ) );

#define SMUX( x, txt ) if( ui.x->isChecked() ) sout.psz_mux = strdup( txt );
    SMUX( PSMux, "ps" );
    SMUX( TSMux, "ts" );
    SMUX( MPEG1Mux, "mpeg" );
    SMUX( OggMux, "ogg" );
    SMUX( ASFMux, "asf" );
    SMUX( MP4Mux, "mp4" );
    SMUX( MOVMux, "mov" );
    SMUX( WAVMux, "wav" );
    SMUX( RAWMux, "raw" );
    SMUX( FLVMux, "flv" );

    bool trans = false;
    bool more = false;

    if ( ui.transcodeVideo->isChecked() || ui.transcodeAudio->isChecked() )
    {
        if ( ui.transcodeVideo->isChecked() )
        {
            mrl = ":sout=#transcode{";
            mrl.append( "vcodec=" );
            mrl.append( sout.psz_vcodec );
            mrl.append( "," );
            mrl.append( "vb=" );
            mrl.append( QString::number( sout.i_vb,10 ) );
            mrl.append( "," );
            mrl.append( "scale=" );
            mrl.append( QString::number( sout.f_scale ) );
            trans = true;
        }

        if ( ui.transcodeAudio->isChecked() )
        {
            if ( trans )
            {
                mrl.append( "," );
            }
            else
            {
                mrl = ":sout=#transcode{";
            }
            mrl.append( "acodec=" );
            mrl.append( sout.psz_acodec );
            mrl.append( "," );
            mrl.append( "ab=" );
            mrl.append( QString::number( sout.i_ab,10 ) );
            mrl.append( "," );
            mrl.append( "channels=" );
            mrl.append( QString::number( sout.i_channels,10 ) );
            trans = true;
        }
        mrl.append( "}" );
    }

    if ( sout.b_local || sout.b_file || sout.b_http || sout.b_mms || sout.b_udp )
    {

#define ISMORE() if ( more ) mrl.append( "," );

        if ( trans )
        {
            mrl.append( ":duplicate{" );
        }
        else
        {
            mrl = ":sout=#";
        }

        if ( sout.b_local )
        {
            ISMORE();
            mrl.append( "dst=display" );
            more = true;
        }

        if ( sout.b_file )
        {
            ISMORE();
            mrl.append( "dst=std{access=file,mux=" );
            mrl.append( sout.psz_mux );
            mrl.append( ",dst=" );
            mrl.append( sout.psz_file );
            mrl.append( "}" );
            more = true;
        }

        if ( sout.b_http )
        {
            ISMORE();
            mrl.append( "dst=std{access=http,mux=" );
            mrl.append( sout.psz_mux );
            mrl.append( ",dst=" );
            mrl.append( sout.psz_http );
            mrl.append( ":" );
            mrl.append( QString::number( sout.i_http,10 ) );
            mrl.append( "}" );
            more = true;
        }

        if ( sout.b_mms )
        {
            ISMORE();
            mrl.append( "dst=std{access=mmsh,mux=" );
            mrl.append( sout.psz_mux );
            mrl.append( ",dst=" );
            mrl.append( sout.psz_mms );
            mrl.append( ":" );
            mrl.append( QString::number( sout.i_mms,10 ) );
            mrl.append( "}" );
            more = true;
        }

        if ( sout.b_udp )
        {
            ISMORE();
            mrl.append( "dst=std{access=udp,mux=" );
            mrl.append( sout.psz_mux );
            mrl.append( ",dst=" );
            mrl.append( sout.psz_udp );
            mrl.append( ":" );
            mrl.append( QString::number( sout.i_udp,10 ) );
            if ( sout.b_sap )
            {
                mrl.append( ",sap," );
                mrl.append( "group=\"" );
                mrl.append( sout.psz_group );
                mrl.append( "\"," );
                mrl.append( "name=\"" );
                mrl.append( sout.psz_name );
                mrl.append( "\"" );
            }
            mrl.append( "}" );
            more = true;
        }

        if ( trans )
        {
            mrl.append( "}" );
        }
    }

    if ( sout.b_all_es )
        mrl.append( ":sout-all" );

    ui.mrlEdit->setText( mrl );
    free( sout.psz_acodec ); free( sout.psz_vcodec ); free( sout.psz_scodec );
    free( sout.psz_file );free( sout.psz_http ); free( sout.psz_mms );
    free( sout.psz_udp ); free( sout.psz_mux );
    free( sout.psz_name ); free( sout.psz_group );
}
