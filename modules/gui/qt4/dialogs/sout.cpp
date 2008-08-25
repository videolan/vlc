/*****************************************************************************
 * sout.cpp : Stream output dialog ( old-style )
 ****************************************************************************
 * Copyright (C) 2007-2008 the VideoLAN team
 * Copyright (C) 2007 Société des arts technologiques
 * Copyright (C) 2007 Savoir-faire Linux
 *
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Jean-François Massol <jf.massol -at- gmail.com>
 *          Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/sout.hpp"

#include <QString>
#include <QFileDialog>

struct streaming_account_t
{
    char *psz_username; /*< username of account */
    char *psz_password; /*< password of account */
};

struct sout_gui_descr_t
{
    /* Access types */
    bool b_local;   /*< local access module */
    bool b_file;    /*< file access module */
    bool b_http;    /*< http access module */
    bool b_mms;     /*< mms access module */
    bool b_rtp;     /*< rtp access module */
    bool b_udp;     /*< udp access module */
    bool b_dump;    /*< dump access module */
    bool b_icecast; /*< icecast access module */

    char *psz_file;     /*< filename */
    char *psz_http;     /*< HTTP servername or ipaddress */
    char *psz_mms;      /*< MMS servername or ipaddress */
    char *psz_rtp;      /*< RTP servername or ipaddress */
    char *psz_udp;      /*< UDP servername or ipaddress */
    char *psz_icecast;  /*< Icecast servername or ipaddress*/

    int32_t i_http;     /*< http port number */
    int32_t i_mms;      /*< mms port number */
    int32_t i_rtp;      /*< rtp port number */
    int32_t i_rtp_audio;      /*< rtp port number */
    int32_t i_rtp_video;      /*< rtp port number */
    int32_t i_udp;      /*< udp port number */
    int32_t i_icecast;  /*< icecast port number */

    /* Mux */
    char *psz_mux;      /*< name of muxer to use in streaming */

    /* Transcode */
    bool b_soverlay; /*< enable burning overlay in the video */
    char *psz_vcodec;   /*< video codec to use in transcoding */
    char *psz_acodec;   /*< audio codec to use in transcoding */
    char *psz_scodec;   /*< subtitle codec to use in transcoding */
    int32_t i_vb;       /*< video bitrate to use in transcoding */
    int32_t i_ab;       /*< audio bitrate to use in transcoding */
    int32_t i_channels; /*< number of audio channels to use in transcoding */
    float f_scale;      /*< scaling factor to use in transcoding */

    /* Misc */
    bool b_sap;   /*< send SAP announcement */
    bool b_all_es;/*< send all elementary streams from source stream */
    char *psz_group;    /*< SAP Group name */
    char *psz_name;     /*< SAP name */
    int32_t i_ttl;      /*< Time To Live (TTL) for network traversal */

    /* Icecast */
    char *psz_icecast_mountpoint;/*< path to Icecast mountpoint */
    struct streaming_account_t sa_icecast;  /*< Icecast account information */
};

SoutDialog* SoutDialog::instance = NULL;

SoutDialog::SoutDialog( QWidget *parent, intf_thread_t *_p_intf,
                     bool _transcode_only ) : QVLCDialog( parent,  _p_intf )
{
    setWindowTitle( qtr( "Stream Output" ) );

    b_transcode_only = _transcode_only;

    /* UI stuff */
    ui.setupUi( this );

    changeUDPandRTPmess( false );

/* ADD HERE for new profiles */
#define ADD_PROFILE( name, shortname ) ui.profileBox->addItem( qtr( name ), QVariant( QString( shortname ) ) );
    ADD_PROFILE( "Custom" , "Custom" )
    ADD_PROFILE( "Ogg / Theora", "theora" )
    ADD_PROFILE( "Ogg / Vorbis", "vorbis" )
    ADD_PROFILE( "MPEG-2", "mpeg2" )
    ADD_PROFILE( "MP3", "mp3" )
    ADD_PROFILE( "MPEG-4 audio AAC", "aac" )
    ADD_PROFILE( "MPEG-4 / DivX", "mp4" )
    ADD_PROFILE( "H264", "h264" )
    ADD_PROFILE( "IPod (mp4/aac)", "IPod" )
    ADD_PROFILE( "XBox", "XBox" )
    ADD_PROFILE( "Windows (wmv/asf)", "Windows" )
    ADD_PROFILE( "PSP", "PSP")

#define ADD_VCODEC( name, fourcc ) ui.vCodecBox->addItem( name, QVariant( fourcc ) );
    ADD_VCODEC( "MPEG-1", "mp1v" )
    ADD_VCODEC( "MPEG-2", "mp2v" )
    ADD_VCODEC( "MPEG-4", "mp4v" )
    ADD_VCODEC( "DIVX 1" , "DIV1" )
    ADD_VCODEC( "DIVX 2" , "DIV2" )
    ADD_VCODEC( "DIVX 3" , "DIV3" )
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

    ui.mrlEdit->setToolTip ( qtr( "Stream output string.\n"
                "This is automatically generated "
                 "when you change the above settings,\n"
                 "but you can update it manually." ) ) ;

//     /* Connect everything to the updateMRL function */
 #define CB( x ) CONNECT( ui.x, toggled( bool ), this, updateMRL() );
 #define CT( x ) CONNECT( ui.x, textChanged( const QString ), this, updateMRL() );
 #define CS( x ) CONNECT( ui.x, valueChanged( int ), this, updateMRL() );
 #define CC( x ) CONNECT( ui.x, currentIndexChanged( int ), this, updateMRL() );
    /* Output */
    CB( fileOutput ); CB( HTTPOutput ); CB( localOutput );
    CB( RTPOutput ); CB( MMSHOutput ); CB( rawInput ); CB( UDPOutput );
    CT( fileEdit ); CT( HTTPEdit ); CT( RTPEdit ); CT( MMSHEdit ); CT( UDPEdit );
    CT( IcecastEdit ); CT( IcecastMountpointEdit ); CT( IcecastNamePassEdit );
    CS( HTTPPort ); CS( RTPPort ); CS( RTPPort2 ); CS( MMSHPort ); CS( UDPPort );
    /* Transcode */
    CC( vCodecBox ); CC( subsCodecBox ); CC( aCodecBox ) ;
    CB( transcodeVideo ); CB( transcodeAudio ); CB( transcodeSubs );
    /*   CB( sOverlay ); */
    CS( vBitrateSpin ); CS( aBitrateSpin ); CS( aChannelsSpin ); CC( vScaleBox );
    /* Mux */
    CB( PSMux ); CB( TSMux ); CB( MPEG1Mux ); CB( OggMux ); CB( ASFMux );
    CB( MP4Mux ); CB( MOVMux ); CB( WAVMux ); CB( RAWMux ); CB( FLVMux );
    /* Misc */
    CB( soutAll ); CS( ttl ); CT( sapName ); CT( sapGroup );

    CONNECT( ui.profileBox, activated( const QString & ), this, setOptions() );
    CONNECT( ui.fileSelectButton, clicked() , this, fileBrowse()  );
    CONNECT( ui.transcodeVideo, toggled( bool ),
            this, setVTranscodeOptions( bool ) );
    CONNECT( ui.transcodeAudio, toggled( bool ),
            this, setATranscodeOptions( bool ) );
    CONNECT( ui.transcodeSubs, toggled( bool ),
            this, setSTranscodeOptions( bool ) );
    CONNECT( ui.rawInput, toggled( bool ), this, setRawOptions( bool ) );

    okButton = new QPushButton( qtr( "&Stream" ) );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ) );

    okButton->setDefault( true );
    ui.acceptButtonBox->addButton( okButton, QDialogButtonBox::AcceptRole );
    ui.acceptButtonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    BUTTONACT( okButton, ok() );
    BUTTONACT( cancelButton, cancel() );

    CONNECT( ui.UDPOutput, toggled( bool ), this, changeUDPandRTPmess( bool ) );
    CONNECT( ui.RTPOutput, clicked(bool), this, RTPtoggled( bool ) );

    if( b_transcode_only ) toggleSout();
}

void SoutDialog::fileBrowse()
{
    QString fileName = QFileDialog::getSaveFileName( this, qtr( "Save file" ), "",
        qtr( "Containers (*.ps *.ts *.mpg *.ogg *.asf *.mp4 *.mov *.wav *.raw *.flv)" ) );
    ui.fileEdit->setText( fileName );
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
    ui.localOutput->setEnabled( !b_raw );
    ui.HTTPOutput->setEnabled( !b_raw );
    ui.MMSHOutput->setEnabled( !b_raw );
    ui.UDPOutput->setEnabled( !b_raw );
    ui.RTPOutput->setEnabled( !b_raw );
    ui.IcecastOutput->setEnabled( !b_raw );
    ui.UDPRTPLabel->setEnabled( !b_raw );

    if( b_raw )
        ui.tabWidget->setDisabled( true );
    else
        setOptions();
}

void SoutDialog::setOptions()
{
    QString profileString =
        ui.profileBox->itemData( ui.profileBox->currentIndex() ).toString();
    msg_Dbg( p_intf, "Profile Used: %s",  qta( profileString ));
    int index;

#define setProfile( muxName, hasVideo, vCodecName, hasAudio, aCodecName ) \
    { \
        ui.muxName ##Mux->setChecked( true ); \
        \
        ui.transcodeAudio->setChecked( hasAudio ); \
        index = ui.aCodecBox->findData( aCodecName );  \
        if( index >= 0 ) ui.aCodecBox->setCurrentIndex( index ); \
        \
        ui.transcodeVideo->setChecked( hasVideo ); \
        index = ui.vCodecBox->findData( vCodecName );  \
        if( index >=0 ) ui.vCodecBox->setCurrentIndex( index ); \
    }

    /* ADD HERE the profiles you want and need */
    if( profileString == "IPod" ) setProfile( MP4, true, "mp4v", true, "mp4a" )
    else if( profileString == "theora" ) setProfile( Ogg, true, "theo", true, "vorb" )
    else if( profileString == "vorbis" ) setProfile( Ogg, false, "", true, "vorb" )
    else if( profileString == "mpeg2" ) setProfile( TS, true, "mp2v", true, "mpga" )
    else if( profileString == "mp3" ) setProfile( RAW, false, "", true, "mp3" )
    else if( profileString == "aac" ) setProfile( MP4, false, "", true, "mp4a" )
    else if( profileString == "mp4" ) setProfile( MP4, true, "mp4v", true, "mp4a" )
    else if( profileString == "h264" ) setProfile( TS, true, "h264", true, "mp4a" )
    else if( profileString == "XBox" ) setProfile( ASF, true, "WMV2", true, "wma" )
    else if( profileString == "Windows" ) setProfile( ASF, true, "WMV2", true, "wma" )
    else if( profileString == "PSP" ) setProfile( Ogg, true, "DIV3", true, "vorb" )

        /* If the profile is not a custom one, then disable the tabWidget */
        if ( profileString == "Custom" )
            ui.tabWidget->setEnabled( true );
        else
            ui.tabWidget->setDisabled( true );

    /* Update the MRL !! */
    updateMRL();
}

void SoutDialog::toggleSout()
{
    //Toggle all the streaming options.
#define HIDEORSHOW(x) if( b_transcode_only ) x->hide(); else x->show();
    HIDEORSHOW( ui.HTTPOutput ) ; HIDEORSHOW( ui.RTPOutput ) ; HIDEORSHOW( ui.MMSHOutput ) ; HIDEORSHOW( ui.UDPOutput ) ;
    HIDEORSHOW( ui.HTTPEdit ) ; HIDEORSHOW( ui.RTPEdit ) ; HIDEORSHOW( ui.MMSHEdit ) ; HIDEORSHOW( ui.UDPEdit ) ;
    HIDEORSHOW( ui.HTTPLabel ) ; HIDEORSHOW( ui.RTPLabel ) ; HIDEORSHOW( ui.MMSHLabel ) ; HIDEORSHOW( ui.UDPLabel ) ;
    HIDEORSHOW( ui.HTTPPortLabel ) ; HIDEORSHOW( ui.RTPPortLabel ) ; HIDEORSHOW( ui.MMSHPortLabel ) ; HIDEORSHOW( ui.UDPPortLabel )
    HIDEORSHOW( ui.HTTPPort ) ; HIDEORSHOW( ui.RTPPort ) ; HIDEORSHOW( ui.MMSHPort ) ; HIDEORSHOW( ui.UDPPort ) ; HIDEORSHOW( ui.RTPPortLabel2 ); HIDEORSHOW( ui.RTPPort2 ); HIDEORSHOW( ui.UDPRTPLabel )

    HIDEORSHOW( ui.sap ); HIDEORSHOW( ui.sapName );
    HIDEORSHOW( ui.sapGroup ); HIDEORSHOW( ui.sapGroupLabel );
    HIDEORSHOW( ui.ttlLabel ); HIDEORSHOW( ui.ttl );

    HIDEORSHOW( ui.IcecastOutput ); HIDEORSHOW( ui.IcecastEdit );
    HIDEORSHOW( ui.IcecastNamePassEdit ); HIDEORSHOW( ui.IcecastMountpointEdit );
    HIDEORSHOW( ui.IcecastPort ); HIDEORSHOW( ui.IcecastLabel );
    HIDEORSHOW( ui.IcecastPortLabel );
    HIDEORSHOW( ui.IcecastMountpointLabel ); HIDEORSHOW( ui.IcecastNameLabel );
#undef HIDEORSHOW

    if( b_transcode_only ) okButton->setText( "&Save" );
    else okButton->setText( "&Stream" );

    setMinimumHeight( 500 );
    resize( width(), sizeHint().height() );
}

void SoutDialog::changeUDPandRTPmess( bool b_udp )
{
    ui.RTPEdit->setVisible( !b_udp );
    ui.RTPLabel->setVisible( !b_udp );
    ui.RTPPort->setVisible( !b_udp );
    ui.RTPPortLabel->setVisible( !b_udp );
    ui.UDPEdit->setVisible( b_udp );
    ui.UDPLabel->setVisible( b_udp );
    ui.UDPPortLabel->setText( b_udp ? qtr( "Port:") : qtr( "Audio Port:" ) );
    ui.RTPPort2->setVisible( !b_udp );
    ui.RTPPortLabel2->setVisible( !b_udp );
}

void SoutDialog::RTPtoggled( bool b_en )
{
    if( !b_en )
    {
        if( ui.RTPPort->value() == ui.UDPPort->value() )
        {
            ui.UDPPort->setValue( ui.UDPPort->value() + 1 );
        }

        while( ui.RTPPort2->value() == ui.UDPPort->value() ||
                ui.RTPPort2->value() == ui.RTPPort->value() )
        {
            ui.RTPPort2->setValue( ui.RTPPort2->value() + 1 );
        }
    }
    ui.sap->setEnabled( b_en );
    ui.RTPLabel->setEnabled( b_en );
    ui.RTPEdit->setEnabled( b_en );
    ui.UDPOutput->setEnabled( b_en );
    ui.UDPEdit->setEnabled( b_en );
    ui.UDPPort->setEnabled( b_en );
    ui.UDPPortLabel->setEnabled( b_en );
    ui.RTPPort2->setEnabled( b_en );
    ui.RTPPortLabel2->setEnabled( b_en );
}

void SoutDialog::ok()
{
    mrl = ui.mrlEdit->text();
    accept();
}

void SoutDialog::cancel()
{
    mrl.clear();
    reject();
}

void SoutDialog::updateMRL()
{
    sout_gui_descr_t sout;
    memset( &sout, 0, sizeof( sout_gui_descr_t ) );
    unsigned int counter = 0;

    sout.b_local = ui.localOutput->isChecked();
    sout.b_file = ui.fileOutput->isChecked();
    sout.b_http = ui.HTTPOutput->isChecked();
    sout.b_mms = ui.MMSHOutput->isChecked();
    sout.b_icecast = ui.IcecastOutput->isChecked();
    sout.b_rtp = ui.RTPOutput->isChecked();
    sout.b_udp = ui.UDPOutput->isChecked();
    sout.b_dump = ui.rawInput->isChecked();
    sout.b_sap = ui.sap->isChecked();
    sout.b_all_es = ui.soutAll->isChecked();
    sout.psz_vcodec = strdup( qtu( ui.vCodecBox->itemData( ui.vCodecBox->currentIndex() ).toString() ) );
    sout.psz_acodec = strdup( qtu( ui.aCodecBox->itemData( ui.aCodecBox->currentIndex() ).toString() ) );
    sout.psz_scodec = strdup( qtu( ui.subsCodecBox->itemData( ui.subsCodecBox->currentIndex() ).toString() ) );
    sout.psz_file = strdup( qtu( ui.fileEdit->text() ) );
    sout.psz_http = strdup( qtu( ui.HTTPEdit->text() ) );
    sout.psz_mms = strdup( qtu( ui.MMSHEdit->text() ) );
    sout.psz_rtp = strdup( qtu( ui.RTPEdit->text() ) );
    sout.psz_udp = strdup( qtu( ui.UDPEdit->text() ) );
    sout.psz_icecast = strdup( qtu( ui.IcecastEdit->text() ) );
    sout.sa_icecast.psz_username = strdup( qtu( ui.IcecastNamePassEdit->text() ) );
    sout.sa_icecast.psz_password = strdup( qtu( ui.IcecastNamePassEdit->text() ) );
    sout.psz_icecast_mountpoint = strdup( qtu( ui.IcecastMountpointEdit->text() ) );
    sout.i_http = ui.HTTPPort->value();
    sout.i_mms = ui.MMSHPort->value();
    sout.i_rtp = ui.RTPPort->value();
    sout.i_rtp_audio = sout.i_udp = ui.UDPPort->value();
    sout.i_rtp_video = ui.RTPPort2->value();
    sout.i_icecast = ui.IcecastPort->value();
    sout.i_ab = ui.aBitrateSpin->value();
    sout.i_vb = ui.vBitrateSpin->value();
    sout.i_channels = ui.aChannelsSpin->value();
    sout.f_scale = atof( qta( ui.vScaleBox->currentText() ) );
    sout.psz_group = strdup( qtu( ui.sapGroup->text() ) );
    sout.psz_name = strdup( qtu( ui.sapName->text() ) );

    if ( sout.b_local ) counter++ ;
    if ( sout.b_file ) counter++ ;
    if ( sout.b_http ) counter++ ;
    if ( sout.b_mms ) counter++ ;
    if ( sout.b_rtp ) counter++ ;
    if ( sout.b_udp ) counter ++;
    if ( sout.b_icecast ) counter ++;

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
    SMUX( MKVMux, "mkv" );

    bool trans = false;
    bool more = false;

    if ( ui.transcodeVideo->isChecked() || ui.transcodeAudio->isChecked()
         && !ui.rawInput->isChecked() /*demuxdump speciality*/ )
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

    /* Special case for demuxdump */
    if ( sout.b_file && sout.b_dump )
    {
        mrl = ":demux=dump :demuxdump-file=";
        mrl.append( sout.psz_file );
    }
    else


    /* Protocol output */
    if ( sout.b_local || sout.b_file || sout.b_http ||
         sout.b_mms || sout.b_rtp || sout.b_udp || sout.b_icecast )
    {

#define ISMORE() if ( more ) mrl.append( "," )
#define ATLEASTONE() if ( counter ) mrl.append( "dst=" )

#define CHECKMUX() \
       if( sout.psz_mux ) \
       {                  \
         mrl.append( ",mux=");\
         mrl.append( sout.psz_mux ); \
       }

        if ( trans )
            mrl.append( ":" );
        else
            mrl = ":sout=#";

        if ( counter )
            mrl.append( "duplicate{" );

        if ( sout.b_local )
        {
            ISMORE();
            ATLEASTONE();
            mrl.append( "display" );
            more = true;
        }

        if ( sout.b_file )
        {
            ISMORE();
            ATLEASTONE();
            mrl.append( "std{access=file" );
            CHECKMUX();
            mrl.append( ",dst=" );
            mrl.append( sout.psz_file );
            mrl.append( "}" );
            more = true;
        }

        if ( sout.b_http )
        {
            ISMORE();
            ATLEASTONE();
            mrl.append( "std{access=http" );
            CHECKMUX();
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
            ATLEASTONE();
            mrl.append( "std{access=mmsh" );
            CHECKMUX();
            mrl.append( ",dst=" );
            mrl.append( sout.psz_mms );
            mrl.append( ":" );
            mrl.append( QString::number( sout.i_mms,10 ) );
            mrl.append( "}" );
            more = true;
        }

        if ( sout.b_rtp )
        {
            ISMORE();
            ATLEASTONE();
            if ( sout.b_udp )
            {
                mrl.append( "std{access=udp" );
                CHECKMUX();
                mrl.append( ",dst=" );
                mrl.append( sout.psz_udp );
                mrl.append( ":" );
                mrl.append( QString::number( sout.i_udp,10 ) );
            }
            else
            {
                mrl.append( "rtp{" );
                mrl.append( "dst=" );
                mrl.append( sout.psz_rtp );
                CHECKMUX();
                mrl.append( ",port=" );
                mrl.append( QString::number( sout.i_rtp,10 ) );
                if( !sout.psz_mux || strncmp( sout.psz_mux, "ts", 2 ) )
                {
                    mrl.append( ",port-audio=" );
                    mrl.append( QString::number( sout.i_rtp_audio, 10 ) );
                    mrl.append( ",port-video=" );
                    mrl.append( QString::number( sout.i_rtp_video, 10 ) );
                }
            }

            /* SAP */
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

        if( sout.b_icecast )
        {
            ISMORE();
            ATLEASTONE();
            mrl.append( "std{access=shout,mux=ogg" );
            mrl.append( ",dst=" );
            mrl.append( sout.sa_icecast.psz_username );
            mrl.append( "@" );
            mrl.append( sout.psz_icecast );
            mrl.append( ":" );
            mrl.append( QString::number( sout.i_icecast, 10 ) );
            mrl.append( "/" );
            mrl.append( sout.psz_icecast_mountpoint );
            mrl.append( "}" );
            more = true;
        }

        if ( counter )
        {
            mrl.append( "}" );
        }
    }

#undef CHECKMUX

    if ( sout.b_all_es )
        mrl.append( ":sout-all" );

    ui.mrlEdit->setText( mrl );
    free( sout.psz_acodec ); free( sout.psz_vcodec ); free( sout.psz_scodec );
    free( sout.psz_file );free( sout.psz_http ); free( sout.psz_mms );
    free( sout.psz_rtp ); free( sout.psz_udp ); free( sout.psz_mux );
    free( sout.psz_name ); free( sout.psz_group );
    free( sout.psz_icecast ); free( sout.psz_icecast_mountpoint );
    free( sout.sa_icecast.psz_password ); free( sout.sa_icecast.psz_username );
}
