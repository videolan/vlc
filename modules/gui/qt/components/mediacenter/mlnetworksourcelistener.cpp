#include "mlnetworksourcelistener.hpp"

MLNetworkSourceListener::SourceListenerCb::~SourceListenerCb()
{
}

MLNetworkSourceListener::MLNetworkSourceListener(MediaSourcePtr s, SourceListenerCb* m)
    : source( s )
    , listener( nullptr, [s]( vlc_media_tree_listener_id* l ) {
            vlc_media_tree_RemoveListener( s->tree, l );
        } )
    , cb( m )
{
    static const vlc_media_tree_callbacks cbs {
        &MLNetworkSourceListener::onItemCleared,
        &MLNetworkSourceListener::onItemAdded,
        &MLNetworkSourceListener::onItemRemoved,
        &MLNetworkSourceListener::onItemPreparseEnded
    };
    auto l = vlc_media_tree_AddListener( s->tree, &cbs, this, true );
    if ( l == nullptr )
        return;
    listener.reset( l );
}

MLNetworkSourceListener::MLNetworkSourceListener()
{
}

void MLNetworkSourceListener::onItemCleared( vlc_media_tree_t*, input_item_node_t* node,
                                                    void* userdata)
{
    auto* self = static_cast<MLNetworkSourceListener*>( userdata );
    self->cb->onItemCleared( self->source, node );
}

void MLNetworkSourceListener::onItemAdded( vlc_media_tree_t *, input_item_node_t * parent,
                                  input_item_node_t *const children[], size_t count,
                                  void *userdata )
{
    auto* self = static_cast<MLNetworkSourceListener*>( userdata );
    auto source = self->source;
    self->cb->onItemAdded( self->source, parent, children, count );
}

void MLNetworkSourceListener::onItemRemoved( vlc_media_tree_t *, input_item_node_t *,
                                    input_item_node_t *const children[], size_t count,
                                    void *userdata )
{
    auto* self = static_cast<MLNetworkSourceListener*>( userdata );
    self->cb->onItemRemoved( self->source, children, count );
}

void MLNetworkSourceListener::onItemPreparseEnded(vlc_media_tree_t *, input_item_node_t * node, enum input_item_preparse_status status, void *userdata)
{
    auto* self = static_cast<MLNetworkSourceListener*>( userdata );
    self->cb->onItemPreparseEnded( self->source, node, status );
}
