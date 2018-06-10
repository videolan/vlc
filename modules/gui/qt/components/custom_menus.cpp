/*****************************************************************************
 * custom_menus.cpp : Qt custom menus classes
 *****************************************************************************
 * Copyright © 2006-2018 VideoLAN authors
 *                  2018 VideoLabs
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_renderer_discovery.h>

#include "custom_menus.hpp"
#include "../managers/renderer_manager.hpp"

#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QWidgetAction>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>

RendererAction::RendererAction( vlc_renderer_item_t *p_item_ )
    : QAction()
{
    p_item = p_item_;
    vlc_renderer_item_hold( p_item );
    if( vlc_renderer_item_flags( p_item ) & VLC_RENDERER_CAN_VIDEO )
        setIcon( QIcon( ":/sidebar/movie.svg" ) );
    else
        setIcon( QIcon( ":/sidebar/music.svg" ) );
    setText( vlc_renderer_item_name( p_item ) );
    setCheckable(true);
}

RendererAction::~RendererAction()
{
    vlc_renderer_item_release( p_item );
}

vlc_renderer_item_t * RendererAction::getItem()
{
    return p_item;
}

RendererMenu::RendererMenu( QMenu *parent, intf_thread_t *p_intf_ )
    : QMenu( parent ), p_intf( p_intf_ )
{
    setTitle( qtr("&Renderer") );

    group = new QActionGroup( this );

    QAction *action = new QAction( qtr("<Local>") );
    action->setCheckable(true);
    addAction( action );
    group->addAction(action);

    char *psz_renderer = var_InheritString( THEPL, "sout" );
    if ( psz_renderer == NULL )
        action->setChecked( true );
    else
        free( psz_renderer );

    addSeparator();

    QWidget *statusWidget = new QWidget();
    statusWidget->setLayout( new QVBoxLayout );
    QLabel *label = new QLabel();
    label->setObjectName( "statuslabel" );
    statusWidget->layout()->addWidget( label );
    QProgressBar *pb = new QProgressBar();
    pb->setObjectName( "statusprogressbar" );
    pb->setMaximumHeight( 10 );
    pb->setStyleSheet( QString("\
        QProgressBar:horizontal {\
            border: none;\
            background: transparent;\
            padding: 1px;\
        }\
        QProgressBar::chunk:horizontal {\
            background: qlineargradient(x1: 0, y1: 0.5, x2: 1, y2: 0.5, \
                        stop: 0 white, stop: 0.4 orange, stop: 0.6 orange, stop: 1 white);\
        }") );
    pb->setRange( 0, 0 );
    pb->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Maximum );
    statusWidget->layout()->addWidget( pb );
    QWidgetAction *qwa = new QWidgetAction( this );
    qwa->setDefaultWidget( statusWidget );
    qwa->setDisabled( true );
    addAction( qwa );
    status = qwa;

    RendererManager *manager = RendererManager::getInstance( p_intf );
    CONNECT( this, aboutToShow(), manager, StartScan() );
    CONNECT( group, triggered(QAction*), this, RendererSelected( QAction* ) );
    DCONNECT( manager, rendererItemAdded( vlc_renderer_item_t * ),
              this, addRendererItem( vlc_renderer_item_t * ) );
    DCONNECT( manager, rendererItemRemoved( vlc_renderer_item_t * ),
              this, removeRendererItem( vlc_renderer_item_t * ) );
    CONNECT( manager, statusUpdated( int ),
             this, updateStatus( int ) );
}

RendererMenu::~RendererMenu()
{
    reset();
}

void RendererMenu::updateStatus( int val )
{
    QProgressBar *pb = findChild<QProgressBar *>("statusprogressbar");
    QLabel *label = findChild<QLabel *>("statuslabel");
    if( val >= RendererManager::RendererStatus::RUNNING )
    {
        label->setText( qtr("Scanning...").
               append( QString(" (%1s)").arg( val ) ) );
        pb->setVisible( true );
        status->setVisible( true );
    }
    else if( val == RendererManager::RendererStatus::FAILED )
    {
        label->setText( "Failed (no discovery module available)" );
        pb->setVisible( false );
        status->setVisible( true );
    }
    else status->setVisible( false );
}

void RendererMenu::addRendererItem( vlc_renderer_item_t *p_item )
{
    QAction *action = new RendererAction( p_item );
    insertAction( status, action );
    group->addAction( action );
}

void RendererMenu::removeRendererItem( vlc_renderer_item_t *p_item )
{
    foreach (QAction* action, group->actions())
    {
        RendererAction *ra = qobject_cast<RendererAction *>( action );
        if( !ra || ra->getItem() != p_item )
            continue;
        removeRendererAction( ra );
        delete ra;
        break;
    }
}

void RendererMenu::addRendererAction(QAction *action)
{
    insertAction( status, action );
    group->addAction( action );
}

void RendererMenu::removeRendererAction(QAction *action)
{
    removeAction( action );
    group->removeAction( action );
}

void RendererMenu::reset()
{
    /* reset the list of renderers */
    foreach (QAction* action, group->actions())
    {
        RendererAction *ra = qobject_cast<RendererAction *>( action );
        if( ra )
        {
            removeRendererAction( ra );
            delete ra;
        }
    }
}

void RendererMenu::RendererSelected(QAction *action)
{
    RendererAction *ra = qobject_cast<RendererAction *>( action );
    if( ra )
        RendererManager::getInstance( p_intf )->SelectRenderer( ra->getItem() );
    else
        RendererManager::getInstance( p_intf )->SelectRenderer( NULL );
}
