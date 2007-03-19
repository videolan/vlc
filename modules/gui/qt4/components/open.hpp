/*****************************************************************************
 * open.hpp : Panels for the open dialogs
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef _OPENPANELS_H_
#define _OPENPANELS_H_

#include <vlc/vlc.h>
#include <QWidget>
#include <QString>
#include "ui/open_file.h"
#include "ui/open_disk.h"
#include "ui/open_net.h"
#include "ui/open_capture.h"

class QFileDialog;
class QLineEdit;

class OpenPanel: public QWidget
{
    Q_OBJECT;
public:
    OpenPanel( QWidget *p, intf_thread_t *_p_intf ) : QWidget( p )
    {
        p_intf = _p_intf;
    }
    virtual ~OpenPanel() {};
    virtual void clear() = 0;
protected:
    intf_thread_t *p_intf;
public slots:
    virtual void updateMRL() = 0;
signals:
    void mrlUpdated(QString);
    void methodChanged( QString method );
};

class FileOpenPanel: public OpenPanel
{
    Q_OBJECT;
public:
    FileOpenPanel( QWidget *, intf_thread_t * );
    virtual ~FileOpenPanel();
    virtual void clear() ;
    virtual void accept() ;
private:
    Ui::OpenFile ui;
    QStringList browse( QString );
    QFileDialog *dialogBox;
    QLineEdit *lineFileEdit;
    bool eventFilter(QObject *, QEvent *);
public slots:
    virtual void updateMRL();
private slots:
    void browseFile();
    void browseFileSub();

};

class NetOpenPanel: public OpenPanel
{
    Q_OBJECT;
public:
    NetOpenPanel( QWidget *, intf_thread_t * );
    virtual ~NetOpenPanel();
    virtual void clear() ;
private:
    Ui::OpenNetwork ui;
public slots:
    virtual void updateMRL();
private slots:
    void updateProtocol(int);
    void updateAddress();
};

class DiskOpenPanel: public OpenPanel
{
    Q_OBJECT;
public:
    DiskOpenPanel( QWidget *, intf_thread_t * );
    virtual ~DiskOpenPanel();
    virtual void clear() ;
private:
    Ui::OpenDisk ui;
public slots:
    virtual void updateMRL() ;
};


class CaptureOpenPanel: public OpenPanel
{
    Q_OBJECT;
public:
    CaptureOpenPanel( QWidget *, intf_thread_t * );
    virtual ~CaptureOpenPanel();
    virtual void clear() ;
private:
    Ui::OpenCapture ui;
public slots:
    virtual void updateMRL();
};

#endif
