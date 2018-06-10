/*****************************************************************************
 * custom_menus.hpp : Qt custom menus classes
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
#ifndef CUSTOM_MENUS_HPP
#define CUSTOM_MENUS_HPP

#include "qt.hpp"

#include <QMenu>

class RendererAction : public QAction
{
    Q_OBJECT

    public:
        RendererAction( vlc_renderer_item_t * );
        ~RendererAction();
        vlc_renderer_item_t *getItem();

    private:
        vlc_renderer_item_t *p_item;
};

class RendererMenu : public QMenu
{
    Q_OBJECT

public:
    RendererMenu( QMenu *, intf_thread_t * );
    virtual ~RendererMenu();
    void reset();

private slots:
    void addRendererItem( vlc_renderer_item_t * );
    void removeRendererItem( vlc_renderer_item_t * );
    void updateStatus( int );
    void RendererSelected( QAction* );

private:
    void addRendererAction( QAction * );
    void removeRendererAction( QAction * );
    static vlc_renderer_item_t* getMatchingRenderer( const QVariant &,
                                                     vlc_renderer_item_t* );
    QAction *status;
    QActionGroup *group;
    intf_thread_t *p_intf;
};


#endif // CUSTOM_MENUS_HPP
