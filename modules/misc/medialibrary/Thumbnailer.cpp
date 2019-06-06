/*****************************************************************************
 * Thumbnailer.cpp: medialibrary thumbnailer implementation using libvlccore
 *****************************************************************************
 * Copyright © 2018 VLC authors, VideoLAN and VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "medialibrary.h"

#include <vlc_thumbnailer.h>
#include <vlc_fs.h>
#include <vlc_block.h>
#include <vlc_url.h>
#include <vlc_cxx_helpers.hpp>

Thumbnailer::Thumbnailer( vlc_medialibrary_module_t* ml, std::string thumbnailsDir )
    : m_ml( ml )
    , m_thumbnailDir( std::move( thumbnailsDir ) )
    , m_thumbnailer( nullptr, &vlc_thumbnailer_Release )
{
    m_thumbnailer.reset( vlc_thumbnailer_Create( VLC_OBJECT( ml ) ) );
    if ( unlikely( m_thumbnailer == nullptr ) )
        throw std::runtime_error( "Failed to instantiate a vlc_thumbnailer_t" );
}

struct ThumbnailerCtx
{
    ~ThumbnailerCtx()
    {
        if ( item != nullptr )
            input_item_Release( item );
        if ( thumbnail != nullptr )
            picture_Release( thumbnail );
    }
    vlc::threads::condition_variable cond;
    vlc::threads::mutex mutex;
    input_item_t* item;
    bool done;
    picture_t* thumbnail;
};

static void onThumbnailComplete( void* data, picture_t* thumbnail )
{
    ThumbnailerCtx* ctx = static_cast<ThumbnailerCtx*>( data );

    {
        vlc::threads::mutex_locker lock( ctx->mutex );
        ctx->done = true;
        ctx->thumbnail = thumbnail ? picture_Hold( thumbnail ) : nullptr;
    }
    ctx->cond.signal();
}

bool Thumbnailer::generate( medialibrary::MediaPtr media, const std::string& mrl )
{
    ThumbnailerCtx ctx{};
    ctx.item = input_item_New( mrl.c_str(), media->title().c_str() );
    if ( unlikely( ctx.item == nullptr ) )
        return false;

    ctx.done = false;
    {
        vlc::threads::mutex_locker lock( ctx.mutex );
        vlc_thumbnailer_RequestByPos( m_thumbnailer.get(), .3f,
                                      VLC_THUMBNAILER_SEEK_FAST, ctx.item,
                                      VLC_TICK_FROM_SEC( 3 ),
                                      &onThumbnailComplete, &ctx );

        while ( ctx.done == false )
            ctx.cond.wait( ctx.mutex );
    }
    if ( ctx.thumbnail == nullptr )
        return false;

    block_t* block;
    if ( picture_Export( VLC_OBJECT( m_ml ), &block, nullptr, ctx.thumbnail,
                         VLC_CODEC_JPEG, 512, 320 ) != VLC_SUCCESS )
        return false;
    auto blockPtr = vlc::wrap_cptr( block, &block_Release );

    std::string outputPath = m_thumbnailDir + std::to_string( media->id() ) + ".jpg";
    auto f = vlc::wrap_cptr( vlc_fopen( outputPath.c_str(), "wb" ), &fclose );
    if ( f == nullptr )
        return false;
    if ( fwrite( block->p_buffer, block->i_buffer, 1, f.get() ) != 1 )
        return false;
    auto thumbnailMrl = vlc::wrap_cptr( vlc_path2uri( outputPath.c_str(), nullptr ) );
    if ( thumbnailMrl == nullptr )
        return false;

    return media->setThumbnail( thumbnailMrl.get() );
}
