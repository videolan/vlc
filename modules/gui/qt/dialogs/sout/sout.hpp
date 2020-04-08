/*****************************************************************************
 * sout.hpp : Stream output dialog ( old-style, ala WX )
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
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

#ifndef QVLC_SOUT_DIALOG_H_
#define QVLC_SOUT_DIALOG_H_ 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h> /* Gettext functions */

/* Auto-generated from .ui files */
#include "ui_sout.h"

#include "widgets/native/qvlcframe.hpp"
#include "util/soutchain.hpp"

#include <QWizard>

class QPushButton;


class SoutDialog : public QWizard
{
    Q_OBJECT
public:
    SoutDialog( QWidget* parent, intf_thread_t *, const QString& chain = "");
    virtual ~SoutDialog(){}

    QString getChain(){ return chain; }

protected:
    virtual void done( int );
private:
    Ui::Sout ui;

    QString chain;
    QPushButton *okButton;

    intf_thread_t* p_intf;

public slots:
    void updateChain();

private slots:
    void closeTab( int );
    void addDest();
};

#endif
