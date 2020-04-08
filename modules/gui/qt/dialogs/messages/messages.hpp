/*****************************************************************************
 * messages.hpp : Information about a stream
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_MESSAGES_DIALOG_H_
#define QVLC_MESSAGES_DIALOG_H_ 1

#include "widgets/native/qvlcframe.hpp"
#include "util/singleton.hpp"

/* Auto-generated from .ui files */
#include "ui_messages_panel.h"

#include <stdarg.h>
#include <QMutex>
#include <atomic>

class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class MsgEvent;

class MessagesDialog : public QVLCFrame, public Singleton<MessagesDialog>
{
    Q_OBJECT
private:
    MessagesDialog( intf_thread_t * );
    virtual ~MessagesDialog();

    Ui::messagesPanelWidget ui;
    static void sinkMessage( void *, vlc_log_t *, unsigned );
    void customEvent( QEvent * );
    void sinkMessage( const MsgEvent * );
    bool matchFilter( const QString& );

    std::atomic<int> verbosity;
    static void MsgCallback( void *, int, const vlc_log_t *, const char *,
                             va_list );

private slots:
    bool save();
    void updateConfig();
    void changeVerbosity( int );
    void updateOrClear();
    void tabChanged( int );
    void filterMessages();

private:
    void buildTree( QTreeWidgetItem *, vlc_object_t * );

    friend class    Singleton<MessagesDialog>;
    QPushButton *updateButton;
    QMutex messageLocker;
#ifndef NDEBUG
    QTreeWidget *pldebugTree;
    void updatePLTree();
#endif
};

#endif
