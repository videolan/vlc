/*****************************************************************************
 * controller_widget.hpp : Controller Widget for the controllers
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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

#ifndef VLC_QT_CONTROLLER_WIDGET_HPP_
#define VLC_QT_CONTROLLER_WIDGET_HPP_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "components/player_controller.hpp"
#include "components/playlist/playlist_controller.hpp"

#include <QWidget>
#include <QToolButton>
#include <QComboBox>

class QLabel;
class QFrame;
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
public slots:
    void updateButtonIcons( bool );
};

class LoopButton : public QToolButton
{
    Q_OBJECT
public slots:
    void updateButtonIcons( vlc::playlist::PlaylistControllerModel::PlaybackRepeat );
};

class AtoB_Button : public QToolButton
{
    Q_OBJECT
public slots:
    void updateButtonIcons( PlayerController::ABLoopState state );
};

class AspectRatioComboBox : public QComboBox
{
    Q_OBJECT
public:
    AspectRatioComboBox( intf_thread_t* _p_intf, QAbstractListModel* model )
        : p_intf( _p_intf )
        , m_aspectRatioModel(model)
    {
        connect( model, &QAbstractListModel::rowsInserted, this, &AspectRatioComboBox::onRowInserted );
        connect( model, &QAbstractListModel::rowsRemoved, this, &AspectRatioComboBox::onRowRemoved );
        connect( model, &QAbstractListModel::modelAboutToBeReset, this, &AspectRatioComboBox::onModelAboutToReset);
        connect( model, &QAbstractListModel::modelReset, this, &AspectRatioComboBox::onModelReset);
        connect( model, &QAbstractListModel::dataChanged, this, &AspectRatioComboBox::onDataChanged);
        connect( this,QOverload<int>::of(&AspectRatioComboBox::currentIndexChanged), this, &AspectRatioComboBox::updateAspectRatio );

        onModelReset();
    }

public slots:
    void onRowInserted(const QModelIndex &parent, int first, int last);
    void onRowRemoved(const QModelIndex &parent, int first, int last);
    void onModelAboutToReset();
    void onModelReset();
    void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

    void updateAspectRatio( int );

private:
    intf_thread_t* p_intf;
    QAbstractListModel* m_aspectRatioModel;
};

class SoundWidget : public QWidget
{
    Q_OBJECT

public:
    SoundWidget( QWidget *parent, intf_thread_t  *_p_i, bool,
                 bool b_special = false );
    void setMuted( bool );

protected:
    bool eventFilter( QObject *obj, QEvent *e ) Q_DECL_OVERRIDE;

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
    void libUpdateVolume( float );
    void updateMuteStatus( bool );
    void refreshLabels( void );
    void showVolumeMenu( QPoint pos );
    void valueChangedFilter( int );

signals:
    void valueReallyChanged( int );
};

#endif
