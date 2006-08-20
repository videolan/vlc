/*****************************************************************************
 * Messages.hpp : Information about a stream
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: Messages.hpp 16024 2006-07-13 13:51:05Z xtophe $
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#ifndef _MESSAGES_DIALOG_H_
#define _MESSAGES_DIALOG_H_

#include "util/qvlcframe.hpp"
#include <QSpinBox>
#include <QLabel>
#include <QTextEdit>
#include <QTextCursor>
#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>

class QPushButton;
class QSpinBox;
class QGridLayout;
class QLabel;
class QTextEdit;

class MessagesDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static MessagesDialog * getInstance( intf_thread_t *p_intf, bool a )
    {
        if( !instance)
            instance = new MessagesDialog( p_intf, a );
        return instance;
    }
    virtual ~MessagesDialog();

private:
    MessagesDialog( intf_thread_t *,  bool );
    input_thread_t *p_input;
    bool main_input;
    static MessagesDialog *instance;

    QTextEdit *messages;

public slots:
    void updateLog();
    void onCloseButton();
    void onClearButton();
    bool onSaveButton();
    void onVerbosityChanged(int verbosityLevel);
};

#endif
