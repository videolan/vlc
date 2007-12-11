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
class QDateEdit;
class QTimeEdit;
class QSpinBox;
class VLMObject;

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

    QList<VLMObject *> vlmItems;
    int currentIndex;

    QVBoxLayout *vlmItemLayout;
    QWidget *vlmItemWidget;
  
    QComboBox *mediatype;
    QTimeEdit *time;
    QDateEdit *date;
    QSpinBox *scherepeatnumber;
    bool isNameGenuine( QString );
public slots:
    void removeVLMItem( VLMObject * );
    void startModifyVLMItem( VLMObject * );
private slots:
    void addVLMItem();
    void clearWidgets();
    void saveModifications();
    void showScheduleWidget( int );
    void selectVLMItem( int );
};

class VLMObject : public QGroupBox 
{
    Q_OBJECT
    friend class VLMDialog;
public:
    VLMObject( int type, QString name, QString input, QString output, bool _enable, VLMDialog *parent );
private:
    QString name;
    QString input;
    QString output;
    bool b_looped;
    bool b_enabled;
    VLMDialog *parent;
protected:
    virtual void enterEvent( QEvent * );
private slots:
    void modify();
    void stop();
    void del();
    void togglePlayPause();
    void toggleLoop();
};

#endif
