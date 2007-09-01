/*****************************************************************************
 * open.hpp : Panels for the open dialogs
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * Copyright (C) 2007 Société des arts technologiques
 * Copyright (C) 2007 Savoir-faire Linux
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#include <QFileDialog>

#include "ui/open_file.h"
#include "ui/open_disk.h"
#include "ui/open_net.h"
#include "ui/open_capture.h"

#ifdef HAVE_LIMITS_H
#   include <limits.h>
#endif

#define setSpinBoxFreq( spinbox ){ spinbox->setRange ( 0, INT_MAX ); \
    spinbox->setAccelerated( true ); }

#define V4L_DEVICE         0x0
#define PVR_DEVICE         0x1
#define DVB_DEVICE         0x2
#define BDA_DEVICE         0x4
#define DSHOW_DEVICE       0x8
#define SCREEN_DEVICE      0x16
#define JACK_DEVICE        0x32

class QWidget;
class QLineEdit;
class QString;

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
    void mrlUpdated( QString );
    void methodChanged( QString method );
};

class FileOpenBox: public QFileDialog
{
    Q_OBJECT;
public:
    FileOpenBox( QWidget *parent, const QString &caption,
        const QString &directory, const QString &filter ):
        QFileDialog( parent, caption, directory, filter ) {}
public slots:
    void accept();
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
    FileOpenBox *dialogBox;
    QLineEdit *lineFileEdit;
public slots:
    virtual void updateMRL();
private slots:
    void browseFile();
    void browseFileSub();
    void toggleSubtitleFrame();
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

class DiscOpenPanel: public OpenPanel
{
    Q_OBJECT;
public:
    DiscOpenPanel( QWidget *, intf_thread_t * );
    virtual ~DiscOpenPanel();
    virtual void clear() ;
private:
    Ui::OpenDisk ui;
public slots:
    virtual void updateMRL() ;
    virtual void updateButtons() ;
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
    QRadioButton *dvbs, *dvbt, *dvbc;
    QRadioButton *bdas, *bdat, *bdac;
    QSpinBox  *v4lFreq, *pvrFreq, *pvrBitr;
    QLineEdit *v4lVideoDevice, *v4lAudioDevice;
    QLineEdit *pvrDevice, *pvrRadioDevice;
    QComboBox *v4lNormBox, *pvrNormBox, *bdaBandBox;
    QSpinBox *dvbCard, *dvbFreq, *dvbSrate;
    QSpinBox *bdaCard, *bdaFreq, *bdaSrate;
    QSpinBox *jackChannels, *jackCaching;
    QCheckBox *jackPace, *jackConnect;
    QLineEdit *jackPortsSelected;

    QLabel *bdaSrateLabel, *bdaBandLabel;

public slots:
    virtual void updateMRL();
private slots:
    void updateButtons();
};

#endif
