/*****************************************************************************
 * Help.hpp : Help and About dialogs
 ****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
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

#ifndef QVLC_HELP_DIALOG_H_
#define QVLC_HELP_DIALOG_H_ 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "qt4.hpp"

#include "util/qvlcframe.hpp"

class QPushButton;
class QTextBrowser;
class QLabel;
class QEvent;
class QPushButton;
class QTextEdit;

class HelpDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static HelpDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new HelpDialog( p_intf );
        return instance;
    }
    static void killInstance()
    { delete instance; instance = NULL;}

private:
    HelpDialog( intf_thread_t * );
    virtual ~HelpDialog();

    static HelpDialog *instance;

public slots:
    void close();
};


class AboutDialog : public QVLCDialog
{
    Q_OBJECT;
public:
    static AboutDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
            instance = new AboutDialog( (QWidget *)p_intf->p_sys->p_mi,
                                        p_intf );
        return instance;
    }

private:
    AboutDialog( QWidget *, intf_thread_t * );
    virtual ~AboutDialog();

    static AboutDialog *instance;

public slots:
    void close();
};

#ifdef UPDATE_CHECK

static const int UDOkEvent = QEvent::User + DialogEventType + 21;
static const int UDErrorEvent = QEvent::User + DialogEventType + 22;

class UpdateDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static UpdateDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance )
            instance = new UpdateDialog( p_intf );
        return instance;
    }
    static void killInstance()
    { delete instance; instance = NULL;}

    void updateNotify( bool );

private:
    UpdateDialog( intf_thread_t * );
    virtual ~UpdateDialog();

    static UpdateDialog *instance;

    update_t *p_update;
    QPushButton *updateButton;
    QLabel *updateLabelTop;
    QLabel *updateLabelDown;
    QTextEdit *updateText;
    void customEvent( QEvent * );
    bool b_checked;

private slots:
    void close();
    void UpdateOrDownload();
};
#endif

#endif
