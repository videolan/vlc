/*****************************************************************************
 * playlists.hpp : playlists
 ****************************************************************************
 * Copyright (C) 2021 the VideoLAN team
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

#ifndef QVLC_PLAYLISTS_H_
#define QVLC_PLAYLISTS_H_ 1

// VLC includes
#include <widgets/native/qvlcframe.hpp>
#include <util/singleton.hpp>

// Forward declarations
class MLPlaylistListModel;
class QTreeView;
class QLineEdit;
class QLabel;
class QPushButton;

class PlaylistsDialog : public QVLCFrame, public Singleton<PlaylistsDialog>
{
    Q_OBJECT

private: // Ctor / dtor
    PlaylistsDialog(qt_intf_t *);

    ~PlaylistsDialog() override;

public: // Interface
    Q_INVOKABLE void setMedias(const QVariantList & medias);

protected: // Events
    void hideEvent(QHideEvent * event) override;

    void keyPressEvent(QKeyEvent * event) override;

private slots:
    void onClicked      ();
    void onDoubleClicked();

    void onTextEdited();

    void onAccepted();

private:
    QVariantList m_ids;

    MLPlaylistListModel * m_model;

    QTreeView * m_playlists;
    QLineEdit * m_lineEdit;

    QLabel * m_label;

    QPushButton * m_button;

private:
    friend class Singleton<PlaylistsDialog>;
};

#endif
