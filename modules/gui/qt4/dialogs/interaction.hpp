/*****************************************************************************
 * interaction.hpp : Interaction dialogs
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

#ifndef _INTERACTION_H_
#define _INTERACTION_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>

#include <QWidget>

class QPushButton;
class QLabel;
class QProgressBar;
class QLineEdit;

class InteractionDialog : public QObject
{
    Q_OBJECT
public:
    InteractionDialog( intf_thread_t *, interaction_dialog_t * );
    virtual ~InteractionDialog();

    void update();
    void show() { if( dialog ) dialog->show(); }
    void hide() { if( dialog ) dialog->hide(); }

private:
    QWidget *panel;
    QWidget *dialog;
    intf_thread_t *p_intf;
    interaction_dialog_t *p_dialog;

    QPushButton *defaultButton, *otherButton, *altButton;
    QLabel *description;
    QProgressBar *progressBar;
    QLineEdit *inputEdit, *loginEdit, *passwordEdit;

    void Finish( int );

private slots:
    void defaultB();
    void altB();
    void otherB();
};

#endif
