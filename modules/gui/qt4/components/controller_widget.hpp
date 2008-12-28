/*****************************************************************************
 * interface_widgets.hpp : Custom widgets for the main interface
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Rafaël Carré <funman@videolanorg>
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

#include <vlc_common.h>
#include <vlc_interface.h>

#include "qt4.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"

#include <QWidget>
#include <QFrame>
#include <QToolButton>

#define I_PLAY_TOOLTIP N_("Play\nIf the playlist is empty, open a media")

class QPixmap;
class QLabel;
class QGridLayout;

class InputSlider;
class QAbstractSlider;

class QAbstractButton;

class VolumeClickHandler;
class QSignalMapper;

class QTimer;
class WidgetListing;

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
    void updateButton( bool );
};

class AtoB_Button : public QToolButton
{
    Q_OBJECT
private slots:
    void setIcons( bool, bool );
};

class TeletextController : public QWidget
{
    Q_OBJECT
    friend class AbstractController;
private:
    QToolButton         *telexTransparent, *telexOn;
    QSpinBox            *telexPage;

private slots:
    void enableTeletextButtons( bool );
    void toggleTeletextTransparency( bool );
};

class SoundWidget : public QWidget
{
    Q_OBJECT
    friend class VolumeClickHandler;

public:
    SoundWidget( QWidget *parent, intf_thread_t  *_p_i, bool );

private:
    intf_thread_t       *p_intf;
    QLabel              *volMuteLabel;
    QAbstractSlider     *volumeSlider;
    VolumeClickHandler  *hVolLabel;
    bool                 b_my_volume;

protected slots:
    void updateVolume( int );
    void updateVolume( void );
};

#endif
