/*****************************************************************************
 * sout.cpp : Stream output dialog (old-style)
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include "dialogs/sout.hpp"
#include "qt4.hpp"
#include <vlc_streaming.h>

#include <QFileDialog>

SoutDialog::SoutDialog( QWidget *parent, intf_thread_t *_p_intf,
                     bool _transcode_only ) : QVLCDialog( parent,  _p_intf )
{
    setWindowTitle( qtr( "Stream output") );

    /* UI stuff */
    ui.setupUi( this );

#define ADD_VCODEC( name, fcc) ui.vCodec->addItem( name, QVariant( fcc ) );
    ADD_VCODEC( "MPEG-1", "mp1v" );
    ADD_VCODEC( "MPEG-2", "mp2v" );
    ADD_VCODEC( "MPEG-4", "mp4v" );
    ADD_VCODEC( "DIVX 1" , "DIV1" );
    ADD_VCODEC( "DIVX 2" , "DIV1" );
    ADD_VCODEC( "DIVX 3" , "DIV1" );
    ADD_VCODEC( "H-263", "H263" );
    ADD_VCODEC( "H-264", "h264" );
    ADD_VCODEC( "WMV1", "WMV1" );
    ADD_VCODEC( "WMV2" , "WMV2" );
    ADD_VCODEC( "M-JPEG", "MJPG" );
    ADD_VCODEC( "Theora", "theo" );

#define ADD_ACODEC( name, fcc) ui.aCodec->addItem( name, QVariant( fcc ) );
    ADD_ACODEC( "MPEG Audio", "mpga" );
    ADD_ACODEC( "MP3", "mp3" );
    ADD_ACODEC( "MPEG 4 Audio (AAC)", "mp4a");
    ADD_ACODEC( "A52/AC-3", "a52");
    ADD_ACODEC( "Vorbis", "vorb" );
    ADD_ACODEC( "Flac", "flac" );
    ADD_ACODEC( "Speex", "spx" );
    ADD_ACODEC( "WAV", "s16l" );
    ADD_ACODEC( "WMA", "wma" );

    ui.vScale->addItem( "0.25" );
    ui.vScale->addItem( "0.5" );
    ui.vScale->addItem( "0.75" );
    ui.vScale->addItem( "1" );
    ui.vScale->addItem( "1.25" );
    ui.vScale->addItem( "1.5" );
    ui.vScale->addItem( "1.75" );
    ui.vScale->addItem( "2" );

    ui.mrlEdit->setToolTip ( qtr("Stream output string.\n This is automatically generated when you change the above settings,\n but you can update it manually." ) ) ;

    /* Connect everything to the updateMRL function */
#define CB(x) CONNECT( ui.x, clicked(bool), this, updateMRL() );
#define CT(x) CONNECT( ui.x, textChanged(const QString), this, updateMRL() );
#define CS(x) CONNECT( ui.x, valueChanged(int), this, updateMRL() );
#define CC(x) CONNECT( ui.x, currentIndexChanged(int), this, updateMRL() );
    /* Output */
    CB( fileOutput ); CB( HTTPOutput ); CB( localOutput );
    CB( UDPOutput ); CB( MMSHOutput ); CB( rawInput );
    CT( fileEdit ); CT( HTTPEdit ); CT( UDPEdit ); CT( MMSHEdit );
    CS( HTTPPort ); CS( UDPPort ); CS( MMSHPort );
    /* Transcode */
    CC( vCodec ); CC( sCodec ); CC( aCodec ) ;
    CB( transcodeVideo ); CB( transcodeAudio ); CB( transcodeSubs );
    CB( sOverlay );
    CS( vBitrate ); CS( aBitrate ); CS( aChannels ); CC( vScale );
    /* Mux */
    CB( PSMux ); CB( TSMux ); CB( MPEG1Mux ); CB( OggMux ); CB( ASFMux );
    CB( MP4Mux ); CB( MOVMux ); CB( WAVMux ); CB( RAWMux ); CB( FLVMux );
    /* Misc */
    CB( soutAll ); CS( ttl ); CT( sapName ); CT( sapGroup );

    CONNECT( ui.fileSelectButton, clicked(), this, fileBrowse() );

    QPushButton *okButton = new QPushButton( qtr( "&Stream" ) );
    QPushButton *cancelButton = new QPushButton( qtr( "&Cancel" ) );

    okButton->setDefault( true );
    ui.acceptButtonBox->addButton( okButton, QDialogButtonBox::AcceptRole );
    ui.acceptButtonBox->addButton( cancelButton, QDialogButtonBox::RejectRole );

    BUTTONACT( okButton, ok());
    BUTTONACT( cancelButton, cancel());

    if( _transcode_only ) toggleSout();
}

void SoutDialog::fileBrowse()
{
    QString f = QFileDialog::getOpenFileName( this, qtr("Save file"), "", "" );
    ui.fileEdit->setText( f );
    updateMRL();
}

void SoutDialog::toggleSout()
{
#define TGV(x) { \
     if( (x->isHidden()) )  \
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
    sout_gui_descr_t pd;
    memset( &pd, 0, sizeof( sout_gui_descr_t ) );

    /* Output */
    pd.b_dump = ui.rawInput->isChecked();
    if( pd.b_dump ) goto end;

    pd.b_local = ui.localOutput->isChecked();
    pd.b_file = ui.fileOutput->isChecked();
    pd.b_http = ui.HTTPOutput->isChecked();
    pd.b_mms = ui.MMSHOutput->isChecked();
    pd.b_udp = ui.UDPOutput->isChecked();

    pd.psz_file = ui.fileOutput->isChecked() ?
                            strdup(qtu( ui.fileEdit->text() ) ): NULL;
    pd.psz_http = ui.HTTPOutput->isChecked() ?
                            strdup(qtu( ui.HTTPEdit->text() ) ) : NULL;
    pd.psz_mms = ui.MMSHOutput->isChecked() ?
                            strdup(qtu( ui.MMSHEdit->text() ) ): NULL;
    pd.psz_udp = ui.UDPOutput->isChecked() ?
                            strdup( qtu( ui.UDPEdit->text() ) ): NULL;

    pd.i_http = ui.HTTPPort->value();
    pd.i_mms = ui.MMSHPort->value();
    pd.i_udp = ui.UDPPort->value();

    /* Mux */
#define SMUX(x, txt) if( ui.x##Mux->isChecked() ) pd.psz_mux = strdup(txt);
    SMUX( PS, "ps" );
    SMUX( TS, "ts" );
    SMUX( MPEG1, "mpeg" );
    SMUX( Ogg, "ogg" );
    SMUX( ASF, "asf" );
    SMUX( MP4, "mp4" );
    SMUX( MOV, "mov" );
    SMUX( WAV, "wav" );
    SMUX( RAW, "raw" );
    SMUX( FLV, "flv" );

    /* Transcode */
    pd.b_soverlay = ui.sOverlay->isChecked();
    pd.i_vb = ui.vBitrate->value();
    pd.i_ab = ui.aBitrate->value();
    pd.i_channels = ui.aChannels->value();
    pd.f_scale = atof( qta( ui.vScale->currentText() ) );

    pd.psz_vcodec = ui.transcodeVideo->isChecked() ?
                     strdup( qtu( ui.vCodec->itemData(
                            ui.vCodec->currentIndex() ). toString() ) ) : NULL;
    pd.psz_acodec = ui.transcodeAudio->isChecked() ?
                     strdup( qtu( ui.aCodec->itemData(
                            ui.aCodec->currentIndex() ).toString() ) ) : NULL;
    pd.psz_scodec = ui.transcodeSubs->isChecked() ?
                     strdup( qtu( ui.sCodec->itemData(
                            ui.sCodec->currentIndex() ).toString() ) ) : NULL;
    pd.b_sap = ui.sap->isChecked();
    pd.b_all_es = ui.soutAll->isChecked();
    pd.psz_name = qtu( ui.sapName->text() );
    pd.psz_group = qtu( ui.sapGroup->text() );
    pd.i_ttl = ui.ttl->value() ;
end:
    sout_chain_t* p_chain = streaming_ChainNew();
    streaming_GuiDescToChain( VLC_OBJECT(p_intf), p_chain, &pd );
    char *psz_mrl = streaming_ChainToPsz( p_chain );

    ui.mrlEdit->setText( qfu( strdup(psz_mrl) ) );
    free( pd.psz_acodec ); free( pd.psz_vcodec ); free( pd.psz_scodec );
    free( pd.psz_file );free( pd.psz_http ); free( pd.psz_mms );
    free( pd.psz_udp ); free( pd.psz_mux );
}
