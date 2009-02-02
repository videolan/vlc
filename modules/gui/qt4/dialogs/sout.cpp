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
#include "util/qt_dirs.hpp"

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

    /* Misc */
    bool b_sap;   /*< send SAP announcement */
    bool b_all_es;/*< send all elementary streams from source stream */
    bool b_sout_keep;
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
/*    ADD_PROFILE( "Custom" , "Custom" )
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

*/
    ui.mrlEdit->setToolTip ( qtr( "Stream output string.\n"
                "This is automatically generated "
                 "when you change the above settings,\n"
                 "but you can change it manually." ) ) ;

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
    /* Misc */
    CB( soutAll ); CB( soutKeep );  CS( ttl ); CT( sapName ); CT( sapGroup );

//    CONNECT( ui.profileSelect, optionsChanged(), this, updateMRL() );
    CONNECT( ui.fileSelectButton, clicked() , this, fileBrowse()  );
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
    QString fileName = QFileDialog::getSaveFileName( this, qtr( "Save file..." ),
            "", qtr( "Containers (*.ps *.ts *.mpg *.ogg *.asf *.mp4 *.mov *.wav *.raw *.flv)" ) );
    ui.fileEdit->setText( toNativeSeparators( fileName ) );
    updateMRL();
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
        ;
//        ui.tabWidget->setDisabled( true );
    else
        setOptions();
}

void SoutDialog::setOptions()
{
/*    QString profileString =
        ui.profileBox->itemData( ui.profileBox->currentIndex() ).toString();
    msg_Dbg( p_intf, "Profile Used: %s",  qtu( profileString )); */
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
/*    if( profileString == "IPod" ) setProfile( MP4, true, "mp4v", true, "mp4a" )
    else if( profileString == "theora" ) setProfile( Ogg, true, "theo", true, "vorb" )
    else if( profileString == "vorbis" ) setProfile( Ogg, false, "", true, "vorb" )
    else if( profileString == "mpeg2" ) setProfile( TS, true, "mp2v", true, "mpga" )
    else if( profileString == "mp3" ) setProfile( RAW, false, "", true, "mp3" )
    else if( profileString == "aac" ) setProfile( MP4, false, "", true, "mp4a" )
    else if( profileString == "mp4" ) setProfile( MP4, true, "mp4v", true, "mp4a" )
    else if( profileString == "h264" ) setProfile( TS, true, "h264", true, "mp4a" )
    else if( profileString == "XBox" ) setProfile( ASF, true, "WMV2", true, "wma" )
    else if( profileString == "Windows" ) setProfile( ASF, true, "WMV2", true, "wma" )
    else if( profileString == "PSP" ) setProfile( MP4, true, "mp4v", true, "mp4a" )*/

        /* If the profile is not a custom one, then disable the tabWidget */
      /*  if ( profileString == "Custom" )
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
    HIDEORSHOW( ui.soutKeep );

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
    ui.UDPRTPLabel->setEnabled( b_en );
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
    sout.b_sout_keep = ui.soutKeep->isChecked();
/*    sout.psz_vcodec = strdup( qtu( ui.vCodecBox->itemData( ui.vCodecBox->currentIndex() ).toString() ) );
    sout.psz_acodec = strdup( qtu( ui.aCodecBox->itemData( ui.aCodecBox->currentIndex() ).toString() ) );
    sout.psz_scodec = strdup( qtu( ui.subsCodecBox->itemData( ui.subsCodecBox->currentIndex() ).toString() ) );*/
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
/*    sout.i_ab = ui.aBitrateSpin->value();
    sout.i_vb = ui.vBitrateSpin->value();
    sout.i_channels = ui.aChannelsSpin->value();
    sout.f_scale = atof( qtu( ui.vScaleBox->currentText() ) ); */
    sout.psz_group = strdup( qtu( ui.sapGroup->text() ) );
    sout.psz_name = strdup( qtu( ui.sapName->text() ) );

    if ( sout.b_local ) counter++ ;
    if ( sout.b_file ) counter++ ;
    if ( sout.b_http ) counter++ ;
    if ( sout.b_mms ) counter++ ;
    if ( sout.b_rtp ) counter++ ;
    if ( sout.b_udp ) counter ++;
    if ( sout.b_icecast ) counter ++;

    sout.psz_mux = strdup( qtu( ui.profileSelect->getMux() ) );

    bool trans = false;
    bool more = false;

    SoutMrl smrl( ":sout=#" );

    /* Special case for demuxdump */
    if ( sout.b_file && sout.b_dump )
    {
        mrl = ":demux=dump :demuxdump-file=";
        mrl.append( qfu( sout.psz_file ) );
    }
    else {
    if( !ui.profileSelect->getTranscode().isEmpty() )
    {
        smrl.begin( ui.profileSelect->getTranscode() );
        smrl.end();
    }

    /* Protocol output */
    if ( sout.b_local || sout.b_file || sout.b_http ||
         sout.b_mms || sout.b_rtp || sout.b_udp || sout.b_icecast )
    {
        if( counter > 1 )
            smrl.begin( "duplicate" );

#define ADD(m) do { if( counter > 1 ) { \
                smrl.option( "dst", m.getMrl() ); \
            } else { \
                smrl.begin( m.getMrl() ); \
                smrl.end(); \
            } } while(0)

        if ( sout.b_local )
        {
            SoutMrl m;
            m.begin( "display" );
            m.end();

            ADD( m );
            more = true;
        }

        if ( sout.b_file )
        {
            SoutMrl m;

            m.begin( "std" );
            m.option( "access", "file" );
            if( sout.psz_mux )
                m.option( "mux", qfu( sout.psz_mux ) );
            m.option( "dst", qfu( sout.psz_file ) );
            m.end();

            ADD( m );
            more = true;
        }

        if ( sout.b_http )
        {
            SoutMrl m;

            m.begin( "std" );
            m.option(  "access", "http" );
            if( sout.psz_mux )
                m.option( "mux", qfu( sout.psz_mux ) );
            m.option( "dst", qfu( sout.psz_http ), sout.i_http );
            m.end();

            ADD( m );
            more = true;
        }

        if ( sout.b_mms )
        {
            SoutMrl m;

            m.begin( "std" );
            m.option(  "access", "mmsh" );
            m.option( "mux", "asfh" );
            m.option( "dst", qfu( sout.psz_mms ), sout.i_mms );
            m.end();

            ADD( m );
            more = true;
        }

        if ( sout.b_rtp )
        {
            SoutMrl m;
            if ( sout.b_udp )
            {
                m.begin( "std" );
                m.option(  "access", "udp" );
                if( sout.psz_mux )
                    m.option( "mux", qfu( sout.psz_mux ) );
                m.option( "dst", qfu( sout.psz_udp ), sout.i_udp );
            }
            else
            {
                m.begin( "rtp" );

                if( sout.psz_rtp && *sout.psz_rtp )
                    m.option( "dst", qfu( sout.psz_rtp ) );
                if( sout.psz_mux )
                    m.option( "mux", qfu( sout.psz_mux ) );

                m.option( "port", sout.i_rtp );
                if( !sout.psz_mux || strncmp( sout.psz_mux, "ts", 2 ) )
                {
                    m.option( "port-audio", sout.i_rtp_audio );
                    m.option( "port-video", sout.i_rtp_video );
                }
            }

            /* SAP */
            if ( sout.b_sap )
            {
                m.option( "sap" );
                m.option( "group", qfu( sout.psz_group ) );
                m.option( "name", qfu( sout.psz_name ) );
            }

            m.end();
            ADD( m );
            more = true;
        }

        if( sout.b_icecast )
        {
            SoutMrl m;
            QString url;

            url = qfu(sout.sa_icecast.psz_username) + "@"
                + qfu( sout.psz_icecast )
                + ":" + QString::number( sout.i_icecast, 10 )
                + "/" + qfu( sout.psz_icecast_mountpoint );

            m.begin( "std" );
            m.option( "access", "shout" );
            m.option( "mux", "ogg" );
            m.option( "dst", url );
            m.end();

            ADD( m );
            more = true;
        }

        if ( counter )
            smrl.end();

        mrl = smrl.getMrl();
    }
    }

    if ( sout.b_all_es )
        mrl.append( " :sout-all" );

    if ( sout.b_sout_keep )
        mrl.append( " :sout-keep" );

    ui.mrlEdit->setText( mrl );
    free( sout.psz_file );free( sout.psz_http ); free( sout.psz_mms );
    free( sout.psz_rtp ); free( sout.psz_udp ); free( sout.psz_mux );
    free( sout.psz_name ); free( sout.psz_group );
    free( sout.psz_icecast ); free( sout.psz_icecast_mountpoint );
    free( sout.sa_icecast.psz_password ); free( sout.sa_icecast.psz_username );
}


