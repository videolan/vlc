#include "networksourcelistener.hpp"

static void onItemCleared(vlc_media_tree_t*, input_item_node_t *node,
                          void* userdata)
{
    auto* self = static_cast<NetworkSourceListener*>( userdata );
    self->cb->onItemCleared( self->source, node );
}

static void onItemAdded(vlc_media_tree_t *, input_item_node_t *parent,
                        input_item_node_t *const children[], size_t count,
                        void *userdata)
{
    auto* self = static_cast<NetworkSourceListener*>( userdata );
    auto source = self->source;
    self->cb->onItemAdded( self->source, parent, children, count );
}

static void onItemRemoved(vlc_media_tree_t *, input_item_node_t *node,
                          input_item_node_t *const children[], size_t count,
                          void *userdata)
{
    auto* self = static_cast<NetworkSourceListener*>( userdata );
    self->cb->onItemRemoved( self->source, node, children, count );
}

static void onItemPreparseEnded(vlc_media_tree_t *, input_item_node_t * node,
                                enum input_item_preparse_status status,
                                void *userdata)
{
    auto* self = static_cast<NetworkSourceListener*>( userdata );
    self->cb->onItemPreparseEnded( self->source, node, status );
}

NetworkSourceListener::NetworkSourceListener(MediaSourcePtr s, std::unique_ptr<SourceListenerCb> &&cb)
    : source( s )
    , listener( nullptr, [s]( vlc_media_tree_listener_id* l ) {
            vlc_media_tree_RemoveListener( s->tree, l );
        } )
    , cb( std::move( cb ) )
{
    static const vlc_media_tree_callbacks cbs {
        &onItemCleared,
        &onItemAdded,
        &onItemRemoved,
        &onItemPreparseEnded
    };
    auto l = vlc_media_tree_AddListener( s->tree, &cbs, this, true );
    if ( l == nullptr )
        return;
    listener.reset( l );
}
