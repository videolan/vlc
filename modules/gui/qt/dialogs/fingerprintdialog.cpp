/*****************************************************************************
 * fingerprintdialog.cpp: Fingerprinter Dialog
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "fingerprintdialog.hpp"
#include "ui/fingerprintdialog.h"

#include "adapters/chromaprint.hpp"
#include <vlc_url.h>

#include <QLabel>
#include <QListWidgetItem>
#include <new>

FingerprintDialog::FingerprintDialog(QWidget *parent, intf_thread_t *p_intf,
                                     input_item_t *p_item ) :
    QDialog(parent),
    ui(new Ui::FingerprintDialog), p_r( NULL )
{
    ui->setupUi(this);

    ui->stackedWidget->setCurrentWidget( ui->wait );

    ui->buttonBox->addButton( "&Close",
                              QDialogButtonBox::RejectRole );

    ui->buttonsBox->addButton( "&Apply this identity to the file",
                                QDialogButtonBox::AcceptRole );

    ui->buttonsBox->addButton( "&Discard all identities",
                                QDialogButtonBox::RejectRole );

    CONNECT( ui->buttonsBox, accepted(), this, applyIdentity() );
    CONNECT( ui->buttonBox, rejected(), this, close() );
    CONNECT( ui->buttonsBox, rejected(), this, close() );

    t = new (std::nothrow) Chromaprint( p_intf );
    if ( t )
    {
        CONNECT( t, finished(), this, handleResults() );
        t->enqueue( p_item );
    }
}

void FingerprintDialog::applyIdentity()
{
    Q_ASSERT( p_r );
    if ( ui->recordsList->currentIndex().isValid() )
        t->apply( p_r, ui->recordsList->currentIndex().row() );
    emit metaApplied( p_r->p_item );
    close();
}

void FingerprintDialog::handleResults()
{
    p_r = t->fetchResults();

    if ( ! p_r )
    {
        ui->stackedWidget->setCurrentWidget( ui->error );
        return;
    }

    if ( vlc_array_count( & p_r->results.metas_array ) == 0 )
    {
        fingerprint_request_Delete( p_r );
        p_r = NULL;
        ui->stackedWidget->setCurrentWidget( ui->error );
        return;
    }

    ui->stackedWidget->setCurrentWidget( ui->results );

    for ( size_t i = 0; i< vlc_array_count( & p_r->results.metas_array ) ; i++ )
    {
        vlc_meta_t *p_meta =
                (vlc_meta_t *) vlc_array_item_at_index( & p_r->results.metas_array, i );
        QListWidgetItem *item = new QListWidgetItem();
        ui->recordsList->addItem( item );
        QString mb_id( vlc_meta_GetExtra( p_meta, "musicbrainz-id" ) );
        QLabel *label = new QLabel(
                    QString( "<h3 style=\"margin: 0\"><a style=\"text-decoration:none\" href=\"%1\">%2</a></h3>"
                             "<span style=\"padding-left:20px\">%3</span>" )
                    .arg( QString( "http://mb.videolan.org/recording/%1" ).arg( mb_id ) )
                    .arg( qfu( vlc_meta_Get( p_meta, vlc_meta_Title ) ) )
                    .arg( qfu( vlc_meta_Get( p_meta, vlc_meta_Artist ) ) )
        );
        label->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
        label->setOpenExternalLinks( true );
        item->setSizeHint( label->sizeHint() );
        ui->recordsList->setItemWidget( item, label );
    }
    ui->recordsList->setCurrentIndex( ui->recordsList->model()->index( 0, 0 ) );
}

FingerprintDialog::~FingerprintDialog()
{
    if ( t ) delete t;
    if ( p_r ) fingerprint_request_Delete( p_r );
    delete ui;
}
