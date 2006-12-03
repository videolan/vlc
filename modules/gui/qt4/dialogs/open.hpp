/*****************************************************************************
 * open.hpp : advanced open dialog
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id: streaminfo.hpp 16806 2006-09-23 13:37:50Z zorglub $
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef _OPEN_DIALOG_H_
#define _OPEN_DIALOG_H_

#include <vlc/vlc.h>

#include "ui/open.h"
#include "util/qvlcframe.hpp"
#include "components/open.hpp"

#include <QTabWidget>
#include <QBoxLayout>
#include <QString>

class OpenDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static OpenDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new OpenDialog( p_intf);
        return instance;
    }
    virtual ~OpenDialog();

    void showTab( int );

    QString mainMRL;
private:
    OpenDialog( intf_thread_t * );
    static OpenDialog *instance;
    input_thread_t *p_input;
    QString mrlSub;

    Ui::Open ui;
    FileOpenPanel *fileOpenPanel;
    NetOpenPanel *netOpenPanel;
    DiskOpenPanel *diskOpenPanel;

    QString storedMethod;
    int advHeight, mainHeight;
private slots:
    void cancel();
    void ok();
    void changedTab();
    void toggleAdvancedPanel();
    void updateMRL(QString);
    void updateMRL();
    void newMethod(QString);
    void signalCurrent();
};

#endif
