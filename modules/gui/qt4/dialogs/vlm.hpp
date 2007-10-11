/*****************************************************************************
 * vlm.hpp : Stream output dialog ( old-style, ala WX )
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

class QComboBox;
class QVBoxLayout;
class QStackedWidget;
class QLabel;
class QWidget;
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
    VLMDialog( intf_thread_t *);
    static VLMDialog *instance;
    Ui::Vlm ui;

    void makeBcastPage();
    void makeVODPage();
    void makeSchedulePage();

    QComboBox *mediatype;
    QVBoxLayout *layout;
    QHBoxLayout *bcastgbox, ;
    QStackedWidget *slayout;
    QWidget *pBcast, *pVod, *pSchedule;
    QGridLayout *bcastlayout, *vodlayout, *schelayout, *schetimelayout;
    QLabel *bcastname, *vodname, *schename,*bcastinput, *vodinput, *scheinput;
    QLabel *bcastoutput, *vodoutput, *scheoutput;
    QCheckBox *bcastenable, *vodenable, *scheenable;
    QLineEdit *bcastnameledit, *vodnameledit, *schenameledit, *bcastinputledit, *vodinputledit, *scheinputledit;
    QLineEdit *bcastoutputledit, *vodoutputledit, *scheoutputledit;
    QToolButton *bcastinputtbutton, *vodinputtbutton, *scheinputtbutton;
    QToolButton *bcastoutputtbutton, *vodoutputtbutton, *scheoutputtbutton;
    QGroupBox *bcastcontrol, *schecontrol;
    QPushButton *bcastplay, *bcastpause, *bcaststop;
    QPushButton *bcastadd, *vodadd, *scheadd, *bcastremove, *vodremove, *scheremove;
    QTimeEdit *time;
    QDateEdit *date;
    QLabel *schetimelabel, *schedatelabel, *schetimerepeat;
    QSpinBox *scherepeatnumber;
};

#endif
