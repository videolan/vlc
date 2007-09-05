/*****************************************************************************
 * sout.cpp : Stream output dialog ( old-style )
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

    /* UI stuff */
    ui.setupUi( this );

#define ADD_PROFILE( name ) ui.comboBox->addItem( name );
    ADD_PROFILE( "" )
    ADD_PROFILE( "Ipod" )
    ADD_PROFILE( "PSP" )
    ADD_PROFILE( "GSM" )
    ADD_PROFILE( "Custom" )

#define ADD_VCODEC( name, fourcc ) ui.vCodec_2->addItem( name, QVariant( fourcc ) );
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

#define ADD_ACODEC( name, fourcc ) ui.aCodec_2->addItem( name, QVariant( fourcc ) );
    ADD_ACODEC( "MPEG Audio", "mpga" )
    ADD_ACODEC( "MP3", "mp3" )
    ADD_ACODEC( "MPEG 4 Audio ( AAC )", "mp4a" )
    ADD_ACODEC( "A52/AC-3", "a52" )
    ADD_ACODEC( "Vorbis", "vorb" )
    ADD_ACODEC( "Flac", "flac" )
    ADD_ACODEC( "Speex", "spx" )
    ADD_ACODEC( "WAV", "s16l" )
    ADD_ACODEC( "WMA", "wma" )

#define ADD_SCALING( factor ) ui.vScale_2->addItem( factor );
    ADD_SCALING( "0.25" )
    ADD_SCALING( "0.5" )
    ADD_SCALING( "0.75" )
    ADD_SCALING( "1" )
    ADD_SCALING( "1.25" )
    ADD_SCALING( "1.5" )
    ADD_SCALING( "1.75" )
    ADD_SCALING( "2" )

    ui.mrlEdit->setToolTip ( qtr( "Stream output string.\n This is automatically generated when you change the above settings,\n but you can update it manually." ) ) ;

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
     CC( vCodec_2 ); CC( sCodec_2 ); CC( aCodec_2 ) ;
     CB( transcodeVideo_2 ); CB( transcodeAudio_2 ); CB( transcodeSubs_2 );
//     CB( sOverlay );
     CS( vBitrate_2 ); CS( aBitrate_2 ); CS( aChannels_2 ); CC( vScale_2 );
//     /* Mux */
     CB( PSMux ); CB( TSMux ); CB( MPEG1Mux ); CB( OggMux ); CB( ASFMux );
     CB( MP4Mux ); CB( MOVMux ); CB( WAVMux ); CB( RAWMux ); CB( FLVMux );
//     /* Misc */
     CB( soutAll ); CS( ttl ); CT( sapName ); CT( sapGroup );
//
    connect( ui.comboBox, SIGNAL( activated( const QString & ) ), this, SLOT( setOptions() ) );
    connect( ui.fileSelectButton, SIGNAL( clicked() ), this, SLOT( fileBrowse() ) );
    connect( ui.transcodeVideo_2,SIGNAL( toggled( bool ) ),this,SLOT( setVTranscodeOptions( bool ) ) );
    connect( ui.transcodeAudio_2,SIGNAL( toggled( bool ) ),this,SLOT( setATranscodeOptions( bool ) ) );
    connect( ui.transcodeSubs_2,SIGNAL( toggled( bool ) ),this,SLOT( setSTranscodeOptions( bool ) ) );
    connect( ui.rawInput,SIGNAL( toggled( bool ) ),this,SLOT( setRawOptions( bool ) ) );

    QPushButton *okButton = new QPushButton( qtr( "&Stream" ) );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ) );

    okButton->setDefault( true );
    ui.acceptButtonBox->addButton( okButton, QDialogButtonBox::AcceptRole );
    ui.acceptButtonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    BUTTONACT( okButton, ok() );
    BUTTONACT( cancelButton, cancel() );

    if( _transcode_only ) toggleSout();
}

void SoutDialog::fileBrowse()
{ui.tabWidget->setTabEnabled( 0,false );
    QString f = QFileDialog::getOpenFileName( this, qtr( "Save file" ), "", "" );
    ui.fileEdit->setText( f );
    updateMRL();
}

void SoutDialog::setVTranscodeOptions( bool b_trans )
{
    ui.label_2->setEnabled( b_trans );
    ui.vCodec_2->setEnabled( b_trans );
    ui.vBitrateLabel_2->setEnabled( b_trans );
    ui.vScaleLabel_2->setEnabled( b_trans );
    ui.vBitrate_2->setEnabled( b_trans );
    ui.vScaleLabel_2->setEnabled( b_trans );
    ui.vScale_2->setEnabled( b_trans );
}

void SoutDialog::setATranscodeOptions( bool b_trans )
{
    ui.label->setEnabled( b_trans );
    ui.aCodec_2->setEnabled( b_trans );
    ui.aBitrateLabel_2->setEnabled( b_trans );
    ui.aBitrate_2->setEnabled( b_trans );
    ui.s_3->setEnabled( b_trans );
    ui.aChannels_2->setEnabled( b_trans );
}

void SoutDialog::setSTranscodeOptions( bool b_trans )
{
    ui.sCodec_2->setEnabled( b_trans );
    ui.sOverlay_2->setEnabled( b_trans );
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

void SoutDialog::setOptions()
{
    /* The test is currently done with a QString, it could be done with the index, it'd depend how translation works */
    if ( ui.comboBox->currentText() == "Custom" )
    {
        ui.tabWidget->setEnabled( true );
    }
    else
    {
        ui.tabWidget->setDisabled( true );
    }
}

void SoutDialog::toggleSout()
{
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
    sout.psz_vcodec = strdup( qtu( ui.vCodec_2->itemData( ui.vCodec_2->currentIndex() ).toString() ) );
    sout.psz_acodec = strdup( qtu( ui.aCodec_2->itemData( ui.vCodec_2->currentIndex() ).toString() ) );
    sout.psz_scodec = strdup( qtu( ui.sCodec_2->itemData( ui.vCodec_2->currentIndex() ).toString() ) );
    sout.psz_file = strdup( qtu( ui.fileEdit->text() ) );
    sout.psz_http = strdup( qtu( ui.HTTPEdit->text() ) );
    sout.psz_mms = strdup( qtu( ui.MMSHEdit->text() ) );
    sout.psz_udp = strdup( qtu( ui.UDPEdit->text() ) );
    sout.i_http = ui.HTTPPort->value();
    sout.i_mms = ui.MMSHPort->value();
    sout.i_udp = ui.UDPPort->value();
    sout.i_ab = ui.aBitrate_2->value();
    sout.i_vb = ui.vBitrate_2->value();
    sout.i_channels = ui.aChannels_2->value();
    sout.f_scale = atof( qta( ui.vScale_2->currentText() ) );
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

if ( ui.transcodeVideo_2->isChecked() || ui.transcodeAudio_2->isChecked() )
{
     if ( ui.transcodeVideo_2->isChecked() )
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

    if ( ui.transcodeAudio_2->isChecked() )
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

// void SoutDialog::updateMRL()
// {
//     sout_gui_descr_t pd;
//     memset( &pd, 0, sizeof( sout_gui_descr_t ) );
//
//     /* Output */
//     pd.b_dump = ui.rawInput->isChecked();
//     if( pd.b_dump ) goto end;
//
//     pd.b_local = ui.localOutput->isChecked();
//     pd.b_file = ui.fileOutput->isChecked();
//     pd.b_http = ui.HTTPOutput->isChecked();
//     pd.b_mms = ui.MMSHOutput->isChecked();
//     pd.b_udp = ui.UDPOutput->isChecked();
//
//     pd.psz_file = ui.fileOutput->isChecked() ?
//                             strdup( qtu( ui.fileEdit->text() ) ): NULL;
//     pd.psz_http = ui.HTTPOutput->isChecked() ?
//                             strdup( qtu( ui.HTTPEdit->text() ) ) : NULL;
//     pd.psz_mms = ui.MMSHOutput->isChecked() ?
//                             strdup( qtu( ui.MMSHEdit->text() ) ): NULL;
//     pd.psz_udp = ui.UDPOutput->isChecked() ?
//                             strdup( qtu( ui.UDPEdit->text() ) ): NULL;
//
//     pd.i_http = ui.HTTPPort->value();
//     pd.i_mms = ui.MMSHPort->value();
//     pd.i_udp = ui.UDPPort->value();
//
//     /* Mux */
// #define SMUX( x, txt ) if( ui.x##Mux->isChecked() ) pd.psz_mux = strdup( txt );
//     SMUX( PS, "ps" );
//     SMUX( TS, "ts" );
//     SMUX( MPEG1, "mpeg" );
//     SMUX( Ogg, "ogg" );
//     SMUX( ASF, "asf" );
//     SMUX( MP4, "mp4" );
//     SMUX( MOV, "mov" );
//     SMUX( WAV, "wav" );
//     SMUX( RAW, "raw" );
//     SMUX( FLV, "flv" );
//
//
//
//     /* Transcode */
// //     pd.b_soverlay = ui.sOverlay->isChecked();
// //     pd.i_vb = ui.vBitrate->value();
// //     pd.i_ab = ui.aBitrate->value();
// //     pd.i_channels = ui.aChannels->value();
// //     pd.f_scale = atof( qta( ui.vScale->currentText() ) );
// //
// //     pd.psz_vcodec = ui.transcodeVideo->isChecked() ?
// //                      strdup( qtu( ui.vCodec->itemData( 
// //                             ui.vCodec->currentIndex() ). toString() ) ) : NULL;
// //     pd.psz_acodec = ui.transcodeAudio->isChecked() ?
// //                      strdup( qtu( ui.aCodec->itemData( 
// //                             ui.aCodec->currentIndex() ).toString() ) ) : NULL;
// //     pd.psz_scodec = ui.transcodeSubs->isChecked() ?
// //                      strdup( qtu( ui.sCodec->itemData( 
// //                             ui.sCodec->currentIndex() ).toString() ) ) : NULL;
// //     pd.b_sap = ui.sap->isChecked();
// //     pd.b_all_es = ui.soutAll->isChecked();
// //     pd.psz_name = qtu( ui.sapName->text() );
// //     pd.psz_group = qtu( ui.sapGroup->text() );
// //     pd.i_ttl = ui.ttl->value() ;
// end:
//     sout_chain_t* p_chain = streaming_ChainNew();
//     streaming_GuiDescToChain( VLC_OBJECT( p_intf ), p_chain, &pd );
//     char *psz_mrl = streaming_ChainToPsz( p_chain );
//
//     ui.mrlEdit->setText( qfu( strdup( psz_mrl ) ) );
//     free( pd.psz_acodec ); free( pd.psz_vcodec ); free( pd.psz_scodec );
//     free( pd.psz_file );free( pd.psz_http ); free( pd.psz_mms );
//     free( pd.psz_udp ); free( pd.psz_mux );
// }
