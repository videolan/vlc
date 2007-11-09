/*****************************************************************************
 * open.hpp : advanced open dialog
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
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

#include "util/qvlcframe.hpp"
#include "dialogs_provider.hpp"
#include "ui/open.h"
#include "components/open.hpp"

class QString;
class QToolButton;
class QTabWidget;

class OpenDialog : public QVLCDialog
{
    Q_OBJECT;
public:
    static OpenDialog * getInstance( QWidget *parent, intf_thread_t *p_intf,
                 int _action_flag = 0 )
    {
        if( !instance)
            instance = new OpenDialog( parent, p_intf, false, _action_flag );
        else
        {
            instance->i_action_flag = _action_flag;
            instance->setMenuAction();
        }
        return instance;
    }
    OpenDialog( QWidget *parent, intf_thread_t *, bool modal,
                int _action_flag = 0 );
    virtual ~OpenDialog();

    void showTab( int );

    QString mrl;
    QString mainMRL;

public slots:
    void play();
    void stream( bool b_transode_only = false );
    void enqueue();
    void transcode();
private:
    static OpenDialog *instance;
    input_thread_t *p_input;

    Ui::Open ui;
    FileOpenPanel *fileOpenPanel;
    NetOpenPanel *netOpenPanel;
    DiscOpenPanel *discOpenPanel;
    CaptureOpenPanel *captureOpenPanel;

    QString storedMethod;
    QString mrlSub;
    int advHeight, mainHeight;
    int i_action_flag;
    QStringList SeparateEntries( QString );

    QPushButton *cancelButton;
    QToolButton *playButton;
    void finish( bool );

private slots:
    void setMenuAction();
    void cancel();
    void close();
    void toggleAdvancedPanel();
    void updateMRL( QString );
    void updateMRL();
    void newCachingMethod( QString );
    void signalCurrent();
};

#endif
