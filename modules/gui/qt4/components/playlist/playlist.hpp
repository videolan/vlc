/*****************************************************************************
 * interface_widgets.hpp : Playlist Widgets
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

#ifndef _PLAYLISTWIDGET_H_
#define _PLAYLISTWIDGET_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"

#include "dialogs_provider.hpp" /* Media Info from ArtLabel */
#include "components/interface_widgets.hpp"

#include <QSplitter>
#include <QLabel>

class PLSelector;
class PLPanel;
class QPushButton;
class CoverArtLabel;
class ArtLabel;

class PlaylistWidget : public QSplitter
{
    Q_OBJECT;
public:
    PlaylistWidget( intf_thread_t *_p_i );
    virtual ~PlaylistWidget();
private:
    PLSelector *selector;
    PLPanel *rightPanel;
    QPushButton *addButton;
    ArtLabel *art;
protected:
    intf_thread_t *p_intf;
    virtual void dropEvent( QDropEvent *);
    virtual void dragEnterEvent( QDragEnterEvent * );
    virtual void closeEvent( QCloseEvent * );

signals:
    void rootChanged( int );
};

class ArtLabel : public CoverArtLabel
{
    Q_OBJECT
public:
    ArtLabel( QWidget *parent, intf_thread_t *intf )
            : CoverArtLabel( parent, intf ) {};
    virtual void mouseDoubleClickEvent( QMouseEvent *event )
    {
        THEDP->mediaInfoDialog();
        event->accept();
    }
};


#endif
