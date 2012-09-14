/*****************************************************************************
 * ml_configuration.cpp: ML's configuration dialog (folder view)
 *****************************************************************************
 * Copyright (C) 2008-2010 the VideoLAN Team and AUTHORS
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
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
# include <config.h>
#endif

#ifdef MEDIA_LIBRARY

#include "ml_configuration.hpp"

#include <QTreeView>
#include <QPushButton>
#include <QDialogButtonBox>

MLConfDialog *MLConfDialog::instance = NULL;


/** **************************************************************************
 * MODEL FOR THE FILESYSTEM
 *****************************************************************************/

Qt::ItemFlags MLDirModel::flags( const QModelIndex &index ) const
{
    Qt::ItemFlags flags = QDirModel::flags( index );
    flags |= Qt::ItemIsUserCheckable;
    if( b_recursive )
    {
        for( int i = 0; i < monitoredDirs.count(); i++ )
        {
            if( filePath( index ).startsWith( monitoredDirs.at( i ) ) )
            {
                if( monitoredDirs.at( i ) != filePath( index ) )
                    flags ^= Qt::ItemIsEnabled;
                break;
            }
        }
    }
    return flags;
}

QVariant MLDirModel::data( const QModelIndex &index, int role ) const
{
    if( index.column() == 0 && role == Qt::CheckStateRole )
    {
        if( itemCheckState.contains( filePath( index ) ) )
            return itemCheckState.value( filePath( index ) );
        else if( b_recursive )
        {
            for( int i = 0; i < monitoredDirs.count(); i++ )
            {
                if( filePath( index ).startsWith( monitoredDirs.at( i ) ) )
                    return Qt::Checked;
            }
            return Qt::Unchecked;
        }
        else
            return Qt::Unchecked;
    }

    return QDirModel::data( index, role );
}

bool MLDirModel::setData( const QModelIndex &index, const QVariant &value,
                          int role )
{
    QModelIndex idx;
    QModelIndex topLeft, bottomRight; // index to signal they have changed
    if( role == Qt::CheckStateRole )
    {
        if( value == Qt::Checked )
        {
            monitoredDirs << filePath( index );
            itemCheckState[filePath( index )] = Qt::Checked;
            /* We have to make his parents Qt::PartiallyChecked */
            idx = index.parent();
            while( idx != QModelIndex() )
            {
                if( !( !b_recursive && monitoredDirs.contains( filePath(idx) ) ) )
                    itemCheckState[filePath(idx)] = Qt::PartiallyChecked;
                topLeft = idx;
                idx = idx.parent();
            }
            /* We have to remove his children that are monitored if we are
               in recursive mode */
            if( b_recursive )
            {
                for( int i = 0; i < monitoredDirs.count()-1; i++ )
                {
                    if( monitoredDirs.at( i ).startsWith( filePath( index ) ) )
                    {
                        itemCheckState.take( monitoredDirs.at( i ) );
                        monitoredDirs.removeAt( i );
                        i--;
                    }
                }
            }
        }
        else if( monitoredDirs.removeOne( filePath( index ) ) )
        {
            itemCheckState.take( filePath( index ) );
            /* We have to make his parent Qt::Unchecked
               if index is his only child */
            for( idx = index.parent(); idx != QModelIndex(); idx = idx.parent() )
            {
                if( monitoredDirs.count() == 0 )
                {
                    itemCheckState.take( filePath(idx) );
                    topLeft = idx;
                    continue;
                }
                for( int i = 0; i < monitoredDirs.count(); i++ )
                {
                    if( monitoredDirs.at( i ).startsWith( filePath( idx ) ) )
                        break;
                    itemCheckState.take( filePath(idx) );
                    topLeft = idx;
                }
            }
        }

        emit dataChanged( topLeft, index );
        return true;
    }
    return QDirModel::setData( index, value, role );
}

int MLDirModel::columnCount( const QModelIndex & ) const
{
    return 1;
}

void MLDirModel::reset( bool _b_recursive, vlc_array_t *p_array )
{
    b_recursive = _b_recursive;

    itemCheckState.clear();
    monitoredDirs.clear();
    for( int i = 0; i < vlc_array_count( p_array ); i++ )
    {
        setData( index( qfu((char*)vlc_array_item_at_index(p_array, i)) ),
                 Qt::Checked, Qt::CheckStateRole );
    }
    emit layoutChanged();
}

void MLDirModel::setRecursivity( bool _b_recursive )
{
    /* If the selection becomes recursive, we may have to delete from
       monitoredDirs some directories  */
    if( !b_recursive && _b_recursive )
    {
        for( int i = 0; i < monitoredDirs.count(); i++ )
        {
            for( int j = i+1; j < monitoredDirs.count(); j++ )
            {
                if( monitoredDirs.at( i ).startsWith( monitoredDirs.at( j ) ) )
                {
                    setData( index( monitoredDirs.at( i ) ),
                             Qt::Unchecked, Qt::CheckStateRole );
                    i--;
                }
                else if( monitoredDirs.at( j ).startsWith( monitoredDirs.at( i ) ) )
                    setData( index( monitoredDirs.at( j ) ),
                             Qt::Unchecked, Qt::CheckStateRole );
            }
        }
    }
    b_recursive = _b_recursive;
    emit layoutChanged();
}

/** **************************************************************************
 * PREFERENCES DIALOG FOR THE MEDIA LIBRARY
 *****************************************************************************/

MLConfDialog::MLConfDialog( QWidget *parent, intf_thread_t *_p_intf )
            : QVLCDialog( parent, _p_intf ), p_intf( _p_intf )
{
    p_monitored_dirs = NULL;

    setWindowTitle( qtr( "Media library Preferences" ) );
    setMinimumSize( 400, 300 );
    setParent( parent, Qt::Window );
    setWindowModality( Qt::NonModal );
    resize( 550, 450 );

    QGridLayout *main_layout = new QGridLayout( this );

    /* Directories selection */
    QStringList nameFilters;
    model = new MLDirModel( nameFilters,
                            QDir::Dirs | QDir::NoDotAndDotDot,
                            QDir::Name, this );
    QTreeView *tree = new QTreeView( this );
    tree->setModel( model );

    /* recursivity */
    recursivity = new QCheckBox( qtr( "Subdirectory recursive scanning" ) );

    synchronous = new QCheckBox( qtr( "Use safe transactions" ) );

    /* Buttons */
    QDialogButtonBox *buttonsBox = new QDialogButtonBox();
    QPushButton *save = new QPushButton( qtr( "&Save" ) );
    QPushButton *cancel = new QPushButton( qtr( "&Cancel" ) );
    QPushButton *reset = new QPushButton( qtr( "&Reset" ) );

    buttonsBox->addButton( save, QDialogButtonBox::AcceptRole );
    buttonsBox->addButton( cancel, QDialogButtonBox::RejectRole );
    buttonsBox->addButton( reset, QDialogButtonBox::ResetRole );

    main_layout->addWidget( tree, 0, 0 );
    main_layout->addWidget( recursivity, 1, 0 );
    main_layout->addWidget( synchronous, 2, 0 );
    main_layout->addWidget( buttonsBox, 3, 0 );

    p_ml = ml_Get( p_intf );
    init();

    BUTTONACT( save, save() );
    BUTTONACT( cancel, cancel() );
    BUTTONACT( reset, reset() );
    CONNECT( recursivity, toggled( bool ), model, setRecursivity( bool ) );
}

void MLConfDialog::init()
{
    bool b_recursive = var_CreateGetBool( p_ml, "ml-recursive-scan" );
    recursivity->setChecked( b_recursive );

    bool b_sync = var_CreateGetBool( p_ml, "ml-synchronous" );
    synchronous->setChecked( b_sync );

    if( p_monitored_dirs )
        vlc_array_destroy( p_monitored_dirs );
    p_monitored_dirs = vlc_array_new();
    ml_Control( p_ml, ML_GET_MONITORED, p_monitored_dirs );

    model->reset( b_recursive, p_monitored_dirs );
}

void MLConfDialog::save()
{
    QStringList newDirs = model->monitoredDirs;
    QStringList toDelete;

    for( int i = 0; i < vlc_array_count( p_monitored_dirs ); i++ )
    {
        if( newDirs.removeAll(
            qfu((char*)vlc_array_item_at_index(p_monitored_dirs, i)) ) == 0 )
        {
            toDelete << qfu((char*)vlc_array_item_at_index(p_monitored_dirs, i));
        }
    }

    for( int i = 0; i < toDelete.count(); i++ )
    {
        ml_Control( p_ml, ML_DEL_MONITORED, qtu( toDelete.at( i ) ) );
    }
    for( int i = 0; i < newDirs.count(); i++ )
    {
        ml_Control( p_ml, ML_ADD_MONITORED, qtu( newDirs.at( i ) ) );
    }

    var_SetBool( p_ml, "ml-recursive-scan", recursivity->isChecked() );
    var_SetBool( p_ml, "ml-synchronous", synchronous->isChecked() );

    init();
    hide();
}

void MLConfDialog::cancel()
{
    init();
    hide();
}

void MLConfDialog::reset()
{
    init();
}

#endif

