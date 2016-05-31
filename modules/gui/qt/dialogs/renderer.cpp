/*****************************************************************************
 * renderer.cpp : Renderer output dialog
 ****************************************************************************
 * Copyright (C) 2015-2016 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
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

#include <QListWidget>
#include <QListWidgetItem>
#include <sstream>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_renderer_discovery.h>
#include <vlc_url.h>

#include "dialogs/renderer.hpp"

class RendererItem : public QListWidgetItem
{
public:
    RendererItem(vlc_renderer_item *obj)
        : QListWidgetItem( vlc_renderer_item_flags(obj) & VLC_RENDERER_CAN_VIDEO ? QIcon( ":/sidebar/movie" ) : QIcon( ":/sidebar/music" ),
                           qfu( vlc_renderer_item_name(obj) ))
    {
        m_obj = vlc_renderer_item_hold(obj);
    }
    ~RendererItem()
    {
        vlc_renderer_item_release(m_obj);
    }

    bool isItemSout( const char *psz_sout, bool as_output ) const;

protected:
    vlc_renderer_item* m_obj;

    friend class RendererDialog;
};

void RendererDialog::renderer_event_received( const vlc_event_t * p_event, void * user_data )
{
    RendererDialog *p_this = reinterpret_cast<RendererDialog*>(user_data);
    p_this->discoveryEventReceived( p_event );
}

RendererDialog::RendererDialog( intf_thread_t *_p_intf )
               : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
               , p_rd( NULL )
               , b_rd_started( false )
{
    setWindowTitle( qtr( "Renderer Output" ) );
    setWindowRole( "vlc-renderer" );

    /* Build Ui */
    ui.setupUi( this );

    CONNECT( ui.buttonBox, accepted(), this, accept() );
    CONNECT( ui.buttonBox, rejected(), this, onReject() );
    CONNECT( ui.receiversListWidget, itemDoubleClicked(QListWidgetItem*), this, accept());

    QVLCTools::restoreWidgetPosition( p_intf, "Renderer", this, QSize( 400 , 440 ) );
}

RendererDialog::~RendererDialog()
{
    if ( p_rd != NULL )
        vlc_rd_release( p_rd );
}

void RendererDialog::onReject()
{
    setSout( NULL );

    QVLCDialog::reject();
}

void RendererDialog::close()
{
    QVLCTools::saveWidgetPosition( p_intf, "Renderer", this );

    QVLCDialog::close();
}

void RendererDialog::setVisible(bool visible)
{
    QVLCDialog::setVisible(visible);

    if (visible)
    {
        /* SD subnodes */
        char **ppsz_longnames;
        char **ppsz_names;
        if( vlc_rd_get_names( THEPL, &ppsz_names, &ppsz_longnames ) != VLC_SUCCESS )
            return;

        char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
        for( ; *ppsz_name; ppsz_name++, ppsz_longname++ )
        {
            /* TODO launch all discovery services for renderers */
            msg_Dbg( p_intf, "starting renderer discovery service %s", *ppsz_longname );
            if ( p_rd == NULL )
            {
                p_rd = vlc_rd_new( VLC_OBJECT(p_intf), *ppsz_name );
                if( !p_rd )
                    msg_Err( p_intf, "Could not start renderer discovery services" );
            }
            break;
        }
        free( ppsz_names );
        free( ppsz_longnames );

        if ( p_rd != NULL )
        {
            int row = -1;
            char *psz_renderer = var_InheritString( THEPL, "sout" );
            if ( psz_renderer != NULL )
            {
                for ( row = 0 ; row < ui.receiversListWidget->count(); row++ )
                {
                    RendererItem *rowItem = reinterpret_cast<RendererItem*>( ui.receiversListWidget->item( row ) );
                    if ( rowItem->isItemSout( psz_renderer, false ) )
                        break;
                }
                if ( row == ui.receiversListWidget->count() )
                    row = -1;
                free( psz_renderer );
            }
            ui.receiversListWidget->setCurrentRow( row );

            if ( !b_rd_started )
            {
                vlc_event_manager_t *em = vlc_rd_event_manager( p_rd );
                vlc_event_attach( em, vlc_RendererDiscoveryItemAdded, renderer_event_received, this );
                vlc_event_attach( em, vlc_RendererDiscoveryItemRemoved, renderer_event_received, this );

                b_rd_started = vlc_rd_start( p_rd ) == VLC_SUCCESS;
                if ( !b_rd_started )
                {
                    vlc_event_detach( em, vlc_RendererDiscoveryItemAdded, renderer_event_received, this);
                    vlc_event_detach( em, vlc_RendererDiscoveryItemRemoved, renderer_event_received, this);
                }
            }
        }
    }
    else
    {
        if ( p_rd != NULL )
        {
            if ( b_rd_started )
            {
                vlc_event_manager_t *em = vlc_rd_event_manager( p_rd );
                vlc_event_detach( em, vlc_RendererDiscoveryItemAdded, renderer_event_received, this);
                vlc_event_detach( em, vlc_RendererDiscoveryItemRemoved, renderer_event_received, this);

                vlc_rd_stop( p_rd );
                b_rd_started = false;
            }
        }
        ui.receiversListWidget->clear();
    }
}

void RendererDialog::accept()
{
    /* get the selected one in the listview if any */
    QListWidgetItem *current = ui.receiversListWidget->currentItem();
    if (current != NULL)
    {
        RendererItem *rowItem = reinterpret_cast<RendererItem*>(current);
        msg_Dbg( p_intf, "selecting Renderer %s", vlc_renderer_item_name(rowItem->m_obj) );

        setSout( rowItem->m_obj );
    }

    QVLCDialog::accept();
}

void RendererDialog::discoveryEventReceived( const vlc_event_t * p_event )
{
    if ( p_event->type == vlc_RendererDiscoveryItemAdded )
    {
        vlc_renderer_item *p_item =  p_event->u.renderer_discovery_item_added.p_new_item;

        int row = 0;
        for ( ; row < ui.receiversListWidget->count(); row++ )
        {
            RendererItem *rowItem = reinterpret_cast<RendererItem*>( ui.receiversListWidget->item( row ) );
            if ( rowItem->isItemSout( vlc_renderer_item_sout( p_item ), false ) )
                return;
        }

        RendererItem *newItem = new RendererItem(p_item);
        ui.receiversListWidget->addItem( newItem );

        char *psz_renderer = var_InheritString( THEPL, "sout" );
        if ( psz_renderer != NULL )
        {
            if ( newItem->isItemSout( psz_renderer, true ) )
                ui.receiversListWidget->setCurrentItem( newItem );
            free( psz_renderer );
        }
    }
}

void RendererDialog::setSout( const vlc_renderer_item *p_item )
{
    std::stringstream s_sout;
    if ( p_item )
    {
        const char *psz_out = vlc_renderer_item_sout( p_item );
        if ( psz_out )
            s_sout << '#' << psz_out;
    }

    msg_Dbg( p_intf, "using sout: '%s'", s_sout.str().c_str() );
    var_SetString( THEPL, "sout", s_sout.str().c_str() );
}

bool RendererItem::isItemSout( const char *psz_sout, bool as_output ) const
{
    std::stringstream s_sout;
    if ( psz_sout == NULL )
        psz_sout = "";
    if ( m_obj )
    {
        const char *psz_out = vlc_renderer_item_sout( m_obj );
        if ( likely( psz_out != NULL ) )
        {
            if ( as_output )
                s_sout << '#';
            s_sout << psz_out;
        }
    }
    return s_sout.str() == psz_sout;
}
