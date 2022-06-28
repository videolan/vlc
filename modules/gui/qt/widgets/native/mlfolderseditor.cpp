/*****************************************************************************
 * roundimage.cpp: Custom widgets
 ****************************************************************************
 * Copyright (C) 2021 the VideoLAN team
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

#include "mlfolderseditor.hpp"
#include "medialibrary/mlhelper.hpp"

#include <QBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>

MLFoldersEditor::MLFoldersEditor(QWidget *parent)
    : QTableWidget(0, 2, parent)
{
    setHorizontalHeaderLabels({ qtr("Path"), qtr("Remove") });
    horizontalHeader()->setMinimumSectionSize( 100 );
    horizontalHeader()->setSectionResizeMode( 0 , QHeaderView::Stretch );
    horizontalHeader()->setFixedHeight( 24 );
}

void MLFoldersEditor::setMLFoldersModel(MLFoldersBaseModel *foldersModel)
{
    if (m_foldersModel)
        disconnect(m_foldersModel, nullptr, this, nullptr);

    m_foldersModel = foldersModel;
    m_newEntries.clear();
    m_removeEntries.clear();

    resetFolders();

    connect(m_foldersModel, &QAbstractItemModel::modelReset, this, &MLFoldersEditor::resetFolders);
    connect(m_foldersModel, &QAbstractItemModel::rowsInserted, this, &MLFoldersEditor::resetFolders);
    connect(m_foldersModel, &QAbstractItemModel::rowsRemoved, this, &MLFoldersEditor::resetFolders);
    connect(m_foldersModel, &QAbstractItemModel::rowsMoved, this, &MLFoldersEditor::resetFolders);
    connect(m_foldersModel, &MLFoldersBaseModel::operationFailed, this, &MLFoldersEditor::handleOpFailure );
}

void MLFoldersEditor::add(const QUrl &mrl)
{
    m_newEntries.push_back(mrl);
    newRow(mrl);
}

void MLFoldersEditor::commit()
{
    for ( const auto &removeEntry : m_removeEntries )
        m_foldersModel->remove( removeEntry );

    for ( const auto &newEntry : m_newEntries )
        m_foldersModel->add( newEntry );

    m_removeEntries.clear();
    m_newEntries.clear();
}

void MLFoldersEditor::handleOpFailure(int operation, const QUrl &url)
{
    const QString entryPoint = urlToDisplayString(url);

    QString msg;
    switch (operation)
    {
    case MLFoldersBaseModel::Add:
        msg = qtr("Failed to add \"%1\"").arg( entryPoint );
        break;
    case MLFoldersBaseModel::Remove:
        msg = qtr("Failed to remove \"%1\"").arg( entryPoint );
        break;
    case MLFoldersBaseModel::Ban:
        msg = qtr("Failed to ban \"%1\"").arg( entryPoint );
        break;
    case MLFoldersBaseModel::Unban:
        msg = qtr("Failed to unban \"%1\"").arg( entryPoint );
        break;
    }

    QMessageBox::warning( this, qtr( "Medialibrary error" ), msg );
}

void MLFoldersEditor::resetFolders()
{
    setRowCount(0);

    for ( int i = 0; i < m_foldersModel->rowCount(); ++i )
    {
        const auto url = m_foldersModel->data(m_foldersModel->index(i), MLFoldersBaseModel::MRL).toUrl();
        if (!m_removeEntries.contains(url))
            newRow(url);
    }

    for ( const auto &newEntry : m_newEntries )
        newRow(newEntry);

}

void MLFoldersEditor::newRow(const QUrl &mrl)
{
    const int row = rowCount();
    setRowCount(row + 1);

    const QString text = urlToDisplayString(mrl);
    auto col1 = new QTableWidgetItem( text );
    col1->setData(Qt::UserRole, mrl);
    col1->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable );
    setItem( row, 0, col1 );

    QWidget *wid = new QWidget( this );
    QBoxLayout* layout = new QBoxLayout( QBoxLayout::LeftToRight , wid );
    QPushButton *pb = new QPushButton( "-" , wid );
    pb->setFixedSize( 16 , 16 );

    layout->addWidget( pb , Qt::AlignCenter );
    wid->setLayout( layout );

    connect( pb , &QPushButton::clicked , this, [this, col1]()
    {
        int row = col1->row();
        vlc_assert( row >= 0 );

        const QUrl mrl = col1->data( Qt::UserRole ).toUrl();
        const auto index = m_newEntries.indexOf( mrl );
        if ( index == -1 )
            m_removeEntries.push_back( mrl );
        else
            m_newEntries.remove( index );

        removeRow( row );
    });

    setCellWidget( row, 1, wid );
}
