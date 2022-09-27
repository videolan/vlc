#include "mediatreelistener.hpp"

static void onItemCleared(vlc_media_tree_t*, input_item_node_t *node,
                          void* userdata)
{
    auto* self = static_cast<MediaTreeListener*>( userdata );
    self->cb->onItemCleared( self->tree, node );
}

static void onItemAdded(vlc_media_tree_t *, input_item_node_t *parent,
                        input_item_node_t *const children[], size_t count,
                        void *userdata)
{
    auto* self = static_cast<MediaTreeListener*>( userdata );
    self->cb->onItemAdded( self->tree, parent, children, count );
}

static void onItemRemoved(vlc_media_tree_t *, input_item_node_t *node,
                          input_item_node_t *const children[], size_t count,
                          void *userdata)
{
    auto* self = static_cast<MediaTreeListener*>( userdata );
    self->cb->onItemRemoved( self->tree, node, children, count );
}

static void onItemPreparseEnded(vlc_media_tree_t *, input_item_node_t * node,
                                enum input_item_preparse_status status,
                                void *userdata)
{
    auto* self = static_cast<MediaTreeListener*>( userdata );
    self->cb->onItemPreparseEnded( self->tree, node, status );
}

MediaTreeListener::MediaTreeListener(MediaTreePtr tree, std::unique_ptr<MediaTreeListenerCb> &&cb )
    : tree( tree )
    , listener( nullptr, [tree]( vlc_media_tree_listener_id* l ) {
            vlc_media_tree_RemoveListener( tree.get(), l );
        } )
    , cb( std::move( cb ) )
{
    static const vlc_media_tree_callbacks cbs {
        &onItemCleared,
        &onItemAdded,
        &onItemRemoved,
        &onItemPreparseEnded
    };
    auto l = vlc_media_tree_AddListener( tree.get(), &cbs, this, true );
    if ( l == nullptr )
        return;
    listener.reset( l );
}
