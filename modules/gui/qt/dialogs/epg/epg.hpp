/*****************************************************************************
 * epg.hpp : EPG Viewer dialog
 ****************************************************************************
 * Copyright © 2010 VideoLAN and AUTHORS
 *
 * Authors:    Jean-Baptiste Kempf <jb@videolan.org>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_EPG_DIALOG_H_
#define QVLC_EPG_DIALOG_H_ 1

#include "widgets/native/qvlcframe.hpp"

#include "util/singleton.hpp"

class QLabel;
class QTextEdit;
class QTimer;
class EPGItem;
class EPGWidget;

class EpgDialog : public QVLCFrame, public Singleton<EpgDialog>
{
    Q_OBJECT
protected:
    virtual void showEvent(QShowEvent * event) Q_DECL_OVERRIDE;

private:
    EpgDialog( intf_thread_t * );
    virtual ~EpgDialog();

    EPGWidget *epg;
    QTextEdit *description;
    QLabel *title;
    QTimer *timer;

    friend class    Singleton<EpgDialog>;

private slots:
    void scheduleUpdate();
    void inputChanged();
    void updateInfos();
    void timeout();
    void displayEvent( EPGItem * );
};

#endif

