/*****************************************************************************
 * vlm.hpp : VLM Management
 ****************************************************************************
 * Copyright ( C ) 2006 the VideoLAN team
 * $Id: vlm.hpp 21875 2007-09-08 16:01:33Z jb $
 *
 * Authors: Jean-François Massol <jf.massol@gmail.com>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#ifndef _VLM_DIALOG_H_
#define _VLM_DIALOG_H_

#include <vlc/vlc.h>

#include "ui/vlm.h"
#include "util/qvlcframe.hpp"

enum{
    QVLM_Broadcast,
    QVLM_Schedule,
    QVLM_VOD
};

class QComboBox;
class QVBoxLayout;
class QStackedWidget;
class QLabel;
class QGridLayout;
class QLineEdit;
class QCheckBox;
class QToolButton;
class QGroupBox;
class QPushButton;
class QHBoxLayout;
class QDateTimeEdit;
class QSpinBox;
class VLMAWidget;

class VLMDialog : public QVLCFrame
{
    Q_OBJECT;
public:
    static VLMDialog * getInstance( intf_thread_t *p_intf )
    {
        if( !instance)
             instance = new VLMDialog( p_intf );
        return instance;
    };
    virtual ~VLMDialog();

private:
    VLMDialog( intf_thread_t * );
    static VLMDialog *instance;
    Ui::Vlm ui;

    QList<VLMAWidget *> vlmItems;
    int currentIndex;

    QVBoxLayout *vlmItemLayout;
    QWidget *vlmItemWidget;
  
    QComboBox *mediatype;
    QDateTimeEdit *time, *date, *repeatTime;
    QSpinBox *scherepeatnumber, *repeatDays;
    bool isNameGenuine( QString );
public slots:
    void removeVLMItem( VLMAWidget * );
    void startModifyVLMItem( VLMAWidget * );
private slots:
    void addVLMItem();
    void clearWidgets();
    void saveModifications();
    void showScheduleWidget( int );
    void selectVLMItem( int );
};

class VLMAWidget : public QGroupBox 
{
    Q_OBJECT
    friend class VLMDialog;
public:
    VLMAWidget( QString name, QString input, QString output, bool _enable, VLMDialog *parent, int _type = QVLM_Broadcast );
protected:
    QLabel *nameLabel;
    QString name;
    QString input;
    QString output;
    bool b_enabled;
    int type;
    VLMDialog *parent;
    virtual void enterEvent( QEvent * );
    QGridLayout *objLayout;
private slots:
    virtual void modify();
    virtual void del();
};

class VLMBroadcast : public VLMAWidget
{
    Q_OBJECT
public:
    VLMBroadcast( QString name, QString input, QString output, bool _enable, VLMDialog *parent );
private:
    bool b_looped;
private slots:
    void stop();
    void togglePlayPause();
    void toggleLoop();
};

class VLMVod : public VLMAWidget
{
public:
    VLMVod( QString name, QString input, QString output, bool _enable, VLMDialog *parent );
private:
    QString mux;
};

class VLMSchedule : public VLMAWidget
{
public:
    VLMSchedule( QString name, QString input, QString output, bool _enable, VLMDialog *parent );
private:
    
};

#endif
