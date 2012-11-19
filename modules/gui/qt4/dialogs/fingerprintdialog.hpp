/*****************************************************************************
 * fingerprintdialog.hpp: Fingerprinter Dialog
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
#ifndef FINGERPRINTDIALOG_HPP
#define FINGERPRINTDIALOG_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"

#include <QDialog>
#include <vlc_interface.h>
#include <vlc_fingerprinter.h>

namespace Ui {
class FingerprintDialog;
}

class Chromaprint;

class FingerprintDialog : public QDialog
{
    Q_OBJECT

public:
    FingerprintDialog( QWidget *parent, intf_thread_t *p_intf,
                                input_item_t *p_item );
    ~FingerprintDialog();

private:
    Ui::FingerprintDialog *ui;
    Chromaprint *t;
    fingerprint_request_t *p_r;

private slots:
    void handleResults();
    void applyIdentity();

signals:
    void metaApplied( input_item_t * );
};

#endif // FINGERPRINTDIALOG_HPP
