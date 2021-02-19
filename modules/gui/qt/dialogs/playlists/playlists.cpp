/*****************************************************************************
 * playlists.cpp : Playlists
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "playlists.hpp"

// VLC includes
#include <vlc_media_library.h>
#include <maininterface/main_interface.hpp>
#include <medialibrary/mlplaylistlistmodel.hpp>
#include <medialibrary/mlqmltypes.hpp>

// Qt includes
#include <QGroupBox>
#include <QVBoxLayout>
#include <QTreeView>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>

//-------------------------------------------------------------------------------------------------
// Ctor / dtor
//-------------------------------------------------------------------------------------------------

PlaylistsDialog::PlaylistsDialog(intf_thread_t * _p_intf) : QVLCFrame(_p_intf)
{
    MainInterface * mainInterface = p_intf->p_sys->p_mi;

    assert(mainInterface->hasMediaLibrary());

    setWindowFlags(Qt::Tool);

    setWindowRole("vlc-playlists");

    setWindowTitle(qtr("Add to Playlist"));

    setWindowOpacity(var_InheritFloat(p_intf, "qt-opacity"));

    //---------------------------------------------------------------------------------------------
    // Existing Playlists

    QGroupBox * groupBox = new QGroupBox(qtr("Existing Playlist"));

    m_playlists = new QTreeView(this);

    connect(m_playlists, &QTreeView::clicked,       this, &PlaylistsDialog::onClicked);
    connect(m_playlists, &QTreeView::doubleClicked, this, &PlaylistsDialog::onDoubleClicked);

    m_model = new MLPlaylistListModel(vlc_ml_instance_get(_p_intf), m_playlists);

    m_model->setMl(mainInterface->getMediaLibrary());

    m_playlists->setModel(m_model);

    QVBoxLayout * layoutPlaylists = new QVBoxLayout(groupBox);

    layoutPlaylists->addWidget(m_playlists);

    //---------------------------------------------------------------------------------------------
    // New Playlist

    QLabel * labelNew = new QLabel(this);

    labelNew->setText(qtr("New Playlist"));

    // FIXME: Add and configure a QValidator.
    m_lineEdit = new QLineEdit(this);

    connect(m_lineEdit, &QLineEdit::textEdited, this, &PlaylistsDialog::onTextEdited);

    QHBoxLayout * layoutNew = new QHBoxLayout;

    layoutNew->addWidget(labelNew);
    layoutNew->addWidget(m_lineEdit);

    //---------------------------------------------------------------------------------------------
    // Buttons

    QDialogButtonBox * buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok |
                                                        QDialogButtonBox::Cancel,
                                                        Qt::Horizontal, this);

    m_button = buttonBox->button(QDialogButtonBox::Ok);

    m_button->setEnabled(false);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &PlaylistsDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &PlaylistsDialog::close);

    m_label = new QLabel(this);

    QHBoxLayout * layoutButtons = new QHBoxLayout;

    layoutButtons->addWidget(m_label);
    layoutButtons->addWidget(buttonBox);

    //---------------------------------------------------------------------------------------------

    QVBoxLayout * layout = new QVBoxLayout(this);

    layout->addWidget(groupBox);
    layout->addLayout(layoutNew);
    layout->addLayout(layoutButtons);

    // FIXME: Is this the right default size ?
    restoreWidgetPosition("Playlists", QSize(320, 320));

    updateGeometry();
}

PlaylistsDialog::~PlaylistsDialog() /* override */
{
    saveWidgetPosition("Playlists");
}

//-------------------------------------------------------------------------------------------------
// Interface
//-------------------------------------------------------------------------------------------------

/* Q_INVOKABLE */ void PlaylistsDialog::setMedias(const QVariantList & medias)
{
    m_ids = medias;

    m_label->setText(qtr("%1 Media(s)").arg(m_ids.count()));
}

//-------------------------------------------------------------------------------------------------
// Events
//-------------------------------------------------------------------------------------------------

void PlaylistsDialog::hideEvent(QHideEvent *) /* override */
{
    // NOTE: We clear the lineEdit when hiding the dialog.
    m_lineEdit->clear();
}

void PlaylistsDialog::keyPressEvent(QKeyEvent * event) /* override */
{
    int key = event->key();

    // FIXME: For some reason we have to do this manually on here. Shouldn't QDialogButtonBox do
    //        this automatically ?
    if (key == Qt::Key_Return || key == Qt::Key_Enter)
    {
        if (m_button->isEnabled())
            onAccepted();

        return;
    }

    QVLCFrame::keyPressEvent(event);
}

//-------------------------------------------------------------------------------------------------
// Private slots
//-------------------------------------------------------------------------------------------------

void PlaylistsDialog::onClicked()
{
    // NOTE: We clear the lineEdit when the list is selected.
    m_lineEdit->clear();

    m_button->setEnabled(true);
}

void PlaylistsDialog::onDoubleClicked()
{
    if (m_button->isEnabled() == false)
        return;

    onAccepted();
}

//-------------------------------------------------------------------------------------------------

void PlaylistsDialog::onTextEdited()
{
    // NOTE: We clear the list selection when we edit the 'new playlist' field.
    m_playlists->clearSelection();

    if (m_lineEdit->text().isEmpty())
        m_button->setEnabled(false);
    else
        m_button->setEnabled(true);
}

void PlaylistsDialog::onAccepted()
{
    MLItemId id;

    QString text = m_lineEdit->text();

    if (text.isEmpty())
        id = m_model->getItemId(m_playlists->currentIndex().row());
    else
        id = m_model->create(text);

    m_model->append(id, m_ids);

    close();
}
