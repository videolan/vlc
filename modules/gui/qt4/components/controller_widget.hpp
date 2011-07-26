/*****************************************************************************
 * Controller_widget.cpp : Controller Widget for the controllers
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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
 *****************************************************************************/

#ifndef _CONTROLLER_WIDGET_H_
#define _CONTROLLER_WIDGET_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"

#include <QWidget>
#include <QToolButton>

class QLabel;
class QFrame;
class QSpinBox;
class QAbstractSlider;

/**
 * SPECIAL Widgets that are a bit more than just a ToolButton
 * and have an icon/behaviour that changes depending on the context:
 * - playButton
 * - A->B Button
 * - Teletext group buttons
 * - Sound Widget group
 **/
class PlayButton : public QToolButton
{
    Q_OBJECT
private slots:
    void updateButtonIcons( bool );
};

class LoopButton : public QToolButton
{
    Q_OBJECT
public slots:
    void updateButtonIcons( int );
};

class AtoB_Button : public QToolButton
{
    Q_OBJECT
private slots:
    void updateButtonIcons( bool, bool );
};

#define VOLUME_MAX (QT_VOLUME_MAX * 100 / QT_VOLUME_DEFAULT)
class SoundWidget : public QWidget
{
    Q_OBJECT

public:
    SoundWidget( QWidget *parent, intf_thread_t  *_p_i, bool,
                 bool b_special = false );
    virtual ~SoundWidget();
    void setMuted( bool );

protected:
    virtual bool eventFilter( QObject *obj, QEvent *e );

private:
    intf_thread_t       *p_intf;
    QLabel              *volMuteLabel;
    QAbstractSlider     *volumeSlider;
    QFrame              *volumeControlWidget;
    QMenu               *volumeMenu;

    bool                b_is_muted;
    bool                b_ignore_valuechanged;

protected slots:
    void userUpdateVolume( int );
    void libUpdateVolume( void );
    void updateMuteStatus( void );
    void refreshLabels( void );
    void showVolumeMenu( QPoint pos );
    void valueChangedFilter( int );

signals:
    void valueReallyChanged( int );
};

#endif
