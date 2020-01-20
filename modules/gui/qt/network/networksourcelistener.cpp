#include "networksourcelistener.hpp"

NetworkSourceListener::SourceListenerCb::~SourceListenerCb()
{
}

NetworkSourceListener::NetworkSourceListener(MediaSourcePtr s, SourceListenerCb* m)
    : source( s )
    , listener( nullptr, [s]( vlc_media_tree_listener_id* l ) {
            vlc_media_tree_RemoveListener( s->tree, l );
        } )
    , cb( m )
{
    static const vlc_media_tree_callbacks cbs {
        &NetworkSourceListener::onItemCleared,
        &NetworkSourceListener::onItemAdded,
        &NetworkSourceListener::onItemRemoved,
        &NetworkSourceListener::onItemPreparseEnded
    };
    auto l = vlc_media_tree_AddListener( s->tree, &cbs, this, true );
    if ( l == nullptr )
        return;
    listener.reset( l );
}

NetworkSourceListener::NetworkSourceListener()
{
}

void NetworkSourceListener::onItemCleared( vlc_media_tree_t*, input_item_node_t* node,
                                                    void* userdata)
{
    auto* self = static_cast<NetworkSourceListener*>( userdata );
    self->cb->onItemCleared( self->source, node );
}

void NetworkSourceListener::onItemAdded( vlc_media_tree_t *, input_item_node_t * parent,
                                  input_item_node_t *const children[], size_t count,
                                  void *userdata )
{
    auto* self = static_cast<NetworkSourceListener*>( userdata );
    auto source = self->source;
    self->cb->onItemAdded( self->source, parent, children, count );
}

void NetworkSourceListener::onItemRemoved( vlc_media_tree_t *, input_item_node_t * node,
                                    input_item_node_t *const children[], size_t count,
                                    void *userdata )
{
    auto* self = static_cast<NetworkSourceListener*>( userdata );
    self->cb->onItemRemoved( self->source, node, children, count );
}

void NetworkSourceListener::onItemPreparseEnded(vlc_media_tree_t *, input_item_node_t * node, enum input_item_preparse_status status, void *userdata)
{
    auto* self = static_cast<NetworkSourceListener*>( userdata );
    self->cb->onItemPreparseEnded( self->source, node, status );
}
