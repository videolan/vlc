#ifndef RENDERER_MANAGER_HPP
#define RENDERER_MANAGER_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "util/singleton.hpp"

#include <vlc_common.h>
#include <vlc_renderer_discovery.h>

#include <QObject>
#include <QEvent>
#include <QTimer>
#include <QVector>
#include <QHash>
#include <QPair>

class RendererManagerEvent : public QEvent
{
public:
    static const QEvent::Type AddedEvent;
    static const QEvent::Type RemovedEvent;

    RendererManagerEvent( QEvent::Type type, vlc_renderer_item_t *p_item_ )
        : QEvent( type ), p_item( p_item_ )
    {
        vlc_renderer_item_hold( p_item );
    }
    virtual ~RendererManagerEvent()
    {
        vlc_renderer_item_release( p_item );
    }

    vlc_renderer_item_t * getItem() const { return p_item; }

private:
    vlc_renderer_item_t *p_item;
};

class RendererManager : public QObject, public Singleton<RendererManager>
{
    Q_OBJECT
    friend class Singleton<RendererManager>;

public:
    enum RendererStatus
    {
        FAILED = -2,
        IDLE = -1,
        RUNNING,
    };
    RendererManager( intf_thread_t * );
    virtual ~RendererManager();
    void customEvent( QEvent * );

public slots:
    void SelectRenderer( vlc_renderer_item_t * );
    void StartScan();
    void StopScan();

signals:
    void rendererItemAdded( vlc_renderer_item_t * );   /* For non queued only */
    void rendererItemRemoved( vlc_renderer_item_t * ); /* For non queued only */
    void statusUpdated( int );

private:
    static void renderer_event_item_added( vlc_renderer_discovery_t *,
                                           vlc_renderer_item_t * );
    static void renderer_event_item_removed( vlc_renderer_discovery_t *,
                                             vlc_renderer_item_t * );

    typedef std::pair<bool, vlc_renderer_item_t *> ItemEntry;
    intf_thread_t* const p_intf;
    const vlc_renderer_item_t *p_selected_item;
    QVector<vlc_renderer_discovery_t*> m_rds;
    QHash<QString, ItemEntry> m_items;
    QTimer m_stop_scan_timer;
    unsigned m_scan_remain;

private slots:
    void RendererMenuCountdown();
};

#endif // RENDERER_MANAGER_HPP
