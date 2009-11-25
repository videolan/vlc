/*****************************************************************************
 * open.hpp : Panels for the open dialogs
 ****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "components/preferences_widgets.hpp"

#include "ui/open_file.h"
#include "ui/open_disk.h"
#include "ui/open_net.h"
#include "ui/open_capture.h"

#include <QFileDialog>

#include <limits.h>

#define setSpinBoxFreq( spinbox ){ spinbox->setRange ( 0, INT_MAX ); \
    spinbox->setAccelerated( true ); }

enum
{
    V4L_DEVICE,
    V4L2_DEVICE,
    PVR_DEVICE,
    DVB_DEVICE,
    BDA_DEVICE,
    DSHOW_DEVICE,
    SCREEN_DEVICE,
    JACK_DEVICE
};

class QWidget;
class QLineEdit;
class QString;
class QStringListModel;

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
    void mrlUpdated( const QStringList&, const QString& );
    void methodChanged( const QString& method );
};

class FileOpenBox: public QFileDialog
{
    Q_OBJECT;
public:
    FileOpenBox( QWidget *parent, const QString &caption,
                 const QString &directory, const QString &filter ):
                QFileDialog( parent, caption, directory, filter ) {}
public slots:
    void accept(){}
    void reject(){}
};


class FileOpenPanel: public OpenPanel
{
    Q_OBJECT;
public:
    FileOpenPanel( QWidget *, intf_thread_t * );
    virtual ~FileOpenPanel();
    virtual void clear() ;
    virtual void accept() ;
protected:
    bool eventFilter(QObject *obj, QEvent *event)
    {
        if( event->type() == QEvent::Hide ||
            event->type() == QEvent::HideToParent )
        {
            event->accept();
            return true;
        }
        return false;
    }
private:
    Ui::OpenFile ui;
    FileOpenBox *dialogBox;
    void BuildOldPanel();
public slots:
    virtual void updateMRL();
private slots:
    void browseFileSub();
    void browseFile();
    void removeFile();
    void updateButtons();
    void toggleSubtitleFrame( bool );
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
    QStringListModel *mrlList;
public slots:
    virtual void updateMRL();
private slots:
    void updateCompleter();
};

class DiscOpenPanel: public OpenPanel
{
    Q_OBJECT;
public:
    DiscOpenPanel( QWidget *, intf_thread_t * );
    virtual ~DiscOpenPanel();
    virtual void clear() ;
    virtual void accept() ;
private:
    Ui::OpenDisk ui;
    char *psz_dvddiscpath, *psz_vcddiscpath, *psz_cddadiscpath;
    bool b_firstdvd, b_firstvcd, b_firstcdda;
public slots:
    virtual void updateMRL() ;
private slots:
    void browseDevice();
    void updateButtons() ;
    void eject();
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
    bool isInitialized;

    QString advMRL;
    QDialog *adv;
#ifdef WIN32
    QRadioButton *bdas, *bdat, *bdac, *bdaa;
    QSpinBox *bdaCard, *bdaFreq, *bdaSrate;
    QLabel *bdaSrateLabel, *bdaBandLabel;
    QComboBox *bdaBandBox;
    StringListConfigControl *vdevDshowW, *adevDshowW;
    QLineEdit *dshowVSizeLine;
#else
    QRadioButton *dvbs, *dvbt, *dvbc;
    QLabel *dvbBandLabel, *dvbSrateLabel;
    QSpinBox  *v4lFreq, *pvrFreq, *pvrBitr;
    QLineEdit *v4lVideoDevice, *v4lAudioDevice;
    QLineEdit *v4l2VideoDevice, *v4l2AudioDevice;
    QLineEdit *pvrDevice, *pvrRadioDevice;
    QComboBox *v4lNormBox, *v4l2StdBox, *pvrNormBox, *dvbBandBox;
    QSpinBox *dvbCard, *dvbFreq, *dvbSrate;
    QSpinBox *jackChannels, *jackCaching;
    QCheckBox *jackPace, *jackConnect;
    QLineEdit *jackPortsSelected;
#endif
    QSpinBox *screenFPS;

public slots:
    virtual void updateMRL();
    void initialize();
private slots:
    void updateButtons();
    void advancedDialog();
};

#endif
