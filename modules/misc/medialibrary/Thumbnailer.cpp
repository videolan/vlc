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

#include <stdexcept>

Thumbnailer::Thumbnailer( vlc_medialibrary_module_t* ml )
    : m_ml( ml )
    , m_currentContext( nullptr )
    , m_thumbnailer( nullptr, &vlc_thumbnailer_Release )
{
    m_thumbnailer.reset( vlc_thumbnailer_Create( VLC_OBJECT( ml ) ) );
    if ( unlikely( m_thumbnailer == nullptr ) )
        throw std::runtime_error( "Failed to instantiate a vlc_thumbnailer_t" );
}

void Thumbnailer::onThumbnailComplete( void* data, picture_t* thumbnail )
{
    ThumbnailerCtx* ctx = static_cast<ThumbnailerCtx*>( data );

    {
        vlc::threads::mutex_locker lock( ctx->thumbnailer->m_mutex );
        ctx->done = true;
        ctx->thumbnail = thumbnail ? picture_Hold( thumbnail ) : nullptr;
        ctx->thumbnailer->m_currentContext = nullptr;
    }
    ctx->thumbnailer->m_cond.signal();
}

bool Thumbnailer::generate( const medialibrary::IMedia&, const std::string& mrl,
                            uint32_t desiredWidth, uint32_t desiredHeight,
                            float position, const std::string& dest )
{
    ThumbnailerCtx ctx{};
    auto item = vlc::wrap_cptr( input_item_New( mrl.c_str(), nullptr ),
                                &input_item_Release );
    if ( unlikely( item == nullptr ) )
        return false;

    input_item_AddOption( item.get(), "no-hwdec", VLC_INPUT_OPTION_TRUSTED );
    ctx.done = false;
    ctx.thumbnailer = this;
    {
        vlc::threads::mutex_locker lock( m_mutex );
        m_currentContext = &ctx;
        ctx.request = vlc_thumbnailer_RequestByPos( m_thumbnailer.get(), position,
                                      VLC_THUMBNAILER_SEEK_FAST, item.get(),
                                      VLC_TICK_FROM_SEC( 3 ),
                                      &onThumbnailComplete, &ctx );

        while ( ctx.done == false )
            m_cond.wait( m_mutex );
        m_currentContext = nullptr;
    }
    if ( ctx.thumbnail == nullptr )
        return false;

    block_t* block;
    if ( picture_Export( VLC_OBJECT( m_ml ), &block, nullptr, ctx.thumbnail,
                         VLC_CODEC_JPEG, desiredWidth, desiredHeight, true ) != VLC_SUCCESS )
        return false;
    auto blockPtr = vlc::wrap_cptr( block, &block_Release );

    auto f = vlc::wrap_cptr( vlc_fopen( dest.c_str(), "wb" ), &fclose );
    if ( f == nullptr )
        return false;
    if ( fwrite( block->p_buffer, block->i_buffer, 1, f.get() ) != 1 )
        return false;
    return true;
}

void Thumbnailer::stop()
{
    vlc::threads::mutex_locker lock{ m_mutex };
    if ( m_currentContext != nullptr )
    {
        vlc_thumbnailer_Cancel( m_thumbnailer.get(), m_currentContext->request );
        m_currentContext->done = true;
        m_cond.signal();
    }
}
