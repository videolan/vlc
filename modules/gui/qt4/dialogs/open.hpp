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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>

#include "util/qvlcframe.hpp"
#include "dialogs_provider.hpp"
#include "ui/open.h"
#include "components/open_panels.hpp"

class QString;
class QTabWidget;

class OpenDialog : public QVLCDialog
{
    Q_OBJECT;
public:
    static OpenDialog * getInstance( QWidget *parent, intf_thread_t *p_intf,
                                     int _action_flag = 0, bool modal = false  );

    static void killInstance()
    {
        if( instance ) delete instance;
        instance = NULL;
    }
    virtual ~OpenDialog();

    void showTab( int );
    QString getMRL(){ return mrl; }

public slots:
    void selectSlots();
    void play();
    void stream( bool b_transode_only = false );
    void enqueue();
    void transcode();

private:
    OpenDialog( QWidget *parent, intf_thread_t *, bool modal,
                int _action_flag = 0 );

    static OpenDialog *instance;
    input_thread_t *p_input;

    QString mrl;
    QString mainMRL;
    QString storedMethod;

    Ui::Open ui;
    FileOpenPanel *fileOpenPanel;
    NetOpenPanel *netOpenPanel;
    DiscOpenPanel *discOpenPanel;
    CaptureOpenPanel *captureOpenPanel;

    int i_action_flag;
    QStringList SeparateEntries( QString );

    QPushButton *cancelButton, *selectButton;
    QPushButton *playButton;

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
