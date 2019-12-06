/*****************************************************************************
 * openurl.hpp: Open a MRL or clipboard content
 ****************************************************************************
 * Copyright © 2009 the VideoLAN team
 *
 * Authors: Jean-Philippe André <jpeg@videolan.org>
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
 ******************************************************************************/

#ifndef QVLC_OPEN_URL_H_
#define QVLC_OPEN_URL_H_ 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "widgets/native/qvlcframe.hpp"
#include "util/singleton.hpp"

class ClickLineEdit;

class OpenUrlDialog : public QVLCDialog
{
    Q_OBJECT

private:
    QString lastUrl;
    bool bClipboard, bShouldEnqueue;
    ClickLineEdit *edit;

private slots:
    void enqueue();
    void play();

public:
    OpenUrlDialog( intf_thread_t *, bool bClipboard = true );
    virtual ~OpenUrlDialog() {}

    QString url() const;
    bool shouldEnqueue() const;
    virtual void showEvent( QShowEvent *ev ) Q_DECL_OVERRIDE;

public slots:
    void close() Q_DECL_OVERRIDE { play(); }

};

#endif
