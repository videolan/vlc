#include "renderer_manager.hpp"

#include <QApplication>

const QEvent::Type RendererManagerEvent::AddedEvent =
        (QEvent::Type)QEvent::registerEventType();
const QEvent::Type RendererManagerEvent::RemovedEvent =
        (QEvent::Type)QEvent::registerEventType();

RendererManager::RendererManager( intf_thread_t *p_intf_ ) :
    p_intf( p_intf_ ), p_selected_item( NULL )
{
    CONNECT( &m_stop_scan_timer, timeout(), this, RendererMenuCountdown() );
}

RendererManager::~RendererManager()
{
    StopScan();
    foreach( ItemEntry entry, m_items )
    {
        emit rendererItemRemoved( entry.second );
        vlc_renderer_item_release( entry.second );
    }
}

void RendererManager::customEvent( QEvent *event )
{
    if( event->type() == RendererManagerEvent::AddedEvent ||
        event->type() == RendererManagerEvent::RemovedEvent )
    {
        RendererManagerEvent *ev = static_cast<RendererManagerEvent *>(event);
        QString souturi( vlc_renderer_item_sout( ev->getItem() ) );
        vlc_renderer_item_t *p_item = ev->getItem();

        if( event->type() == RendererManagerEvent::AddedEvent )
        {
            if( !m_items.contains( souturi ) )
            {
                vlc_renderer_item_hold( p_item );
                ItemEntry entry( true, p_item );
                m_items.insert( souturi, entry );
                emit rendererItemAdded( p_item );
            }
            else /* incref for now */
            {
                ItemEntry &entry = m_items[ souturi ];
                entry.first = true; /* to be kept */
            }
        }
        else /* remove event */
        {
            if( m_items.contains( souturi ) )
            {
                p_item = m_items[ souturi ].second;
                if( p_selected_item != p_item )
                {
                    m_items.remove( souturi );
                    emit rendererItemRemoved( p_item );
                    vlc_renderer_item_release( p_item );
                }
                else m_items[ souturi ].first = true; /* keep */
            }
            /* else ignore */
        }
    }
}

void RendererManager::StartScan()
{
    if( m_stop_scan_timer.isActive() )
        return;

    /* SD subnodes */
    char **ppsz_longnames;
    char **ppsz_names;
    if( vlc_rd_get_names( p_intf, &ppsz_names, &ppsz_longnames ) != VLC_SUCCESS )
    {
        emit statusUpdated( RendererManager::RendererStatus::FAILED );
        return;
    }

    struct vlc_renderer_discovery_owner owner =
    {
        this,
        renderer_event_item_added,
        renderer_event_item_removed,
    };

    char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
    for( ; *ppsz_name; ppsz_name++, ppsz_longname++ )
    {
        msg_Dbg( p_intf, "starting renderer discovery service %s", *ppsz_longname );
        vlc_renderer_discovery_t* p_rd = vlc_rd_new( VLC_OBJECT(p_intf), *ppsz_name, &owner );
        if( p_rd != NULL )
            m_rds.push_back( p_rd );
        free( *ppsz_name );
        free( *ppsz_longname );
    }
    free( ppsz_names );
    free( ppsz_longnames );

    emit statusUpdated( RendererManager::RendererStatus::RUNNING );
    m_scan_remain = 20000;
    m_stop_scan_timer.setInterval( 1000 );
    m_stop_scan_timer.start();
}

void RendererManager::StopScan()
{
    m_stop_scan_timer.stop();
    foreach ( vlc_renderer_discovery_t* p_rd, m_rds )
        vlc_rd_release( p_rd );
    /* Cleanup of outdated items, and notify removal */
    QHash<QString, ItemEntry>::iterator it = m_items.begin();
    while ( it != m_items.end() )
    {
        ItemEntry &entry = it.value();
        if( !entry.first /* don't keep */ && entry.second != p_selected_item )
        {
            emit rendererItemRemoved( entry.second );
            vlc_renderer_item_release( entry.second );
            it = m_items.erase( it );
        }
        else
        {
            entry.first = false; /* don't keep if not updated by new detect */
            assert( it.value().first == false );
            ++it;
        }
    }
    m_rds.clear();
    emit statusUpdated( RendererManager::RendererStatus::IDLE );
}

void RendererManager::RendererMenuCountdown()
{
    if( m_stop_scan_timer.isActive() && m_scan_remain > 0 )
    {
        m_scan_remain -= 1000;
        emit statusUpdated( RendererManager::RendererStatus::RUNNING +  m_scan_remain / 1000 );
    }
    else
    {
        StopScan();
    }
}

void RendererManager::SelectRenderer( vlc_renderer_item_t *p_item )
{
    p_selected_item = p_item;
    vlc_player_locker lock{ p_intf->p_sys->p_player };
    vlc_player_SetRenderer( p_intf->p_sys->p_player, p_item );
}

void RendererManager::renderer_event_item_added( vlc_renderer_discovery_t* p_rd,
                                                 vlc_renderer_item_t *p_item )
{
    RendererManager *self = reinterpret_cast<RendererManager*>( p_rd->owner.sys );
    QEvent *ev = new RendererManagerEvent( RendererManagerEvent::AddedEvent,
                                           p_item );
    QApplication::postEvent( self, ev );
}

void RendererManager::renderer_event_item_removed( vlc_renderer_discovery_t *p_rd,
                                                   vlc_renderer_item_t *p_item )
{
    RendererManager *self = reinterpret_cast<RendererManager*>( p_rd->owner.sys );
    QEvent *ev = new RendererManagerEvent( RendererManagerEvent::RemovedEvent,
                                           p_item );
    QApplication::postEvent( self, ev );
}
