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

#include <vlc_fs.h>
#include <vlc_block.h>
#include <vlc_url.h>
#include <vlc_cxx_helpers.hpp>
#include <vlc_preparser.h>

#include <climits>
#include <stdexcept>

Thumbnailer::Thumbnailer( vlc_medialibrary_module_t* ml )
    : m_currentContext( nullptr )
    , m_thumbnailer( nullptr, &vlc_preparser_Delete )
{
    const struct vlc_preparser_cfg cfg = []{
        struct vlc_preparser_cfg cfg{};
        cfg.types = VLC_PREPARSER_TYPE_THUMBNAIL_TO_FILES;
        cfg.timeout = VLC_TICK_FROM_SEC( 3 );
        cfg.max_thumbnailer_threads = 1;
        return cfg;
    }();
    m_thumbnailer.reset( vlc_preparser_New( VLC_OBJECT( ml ), &cfg ) );
    if ( unlikely( m_thumbnailer == nullptr ) )
        throw std::runtime_error( "Failed to instantiate a vlc_preparser_t" );
}

void Thumbnailer::onThumbnailToFilesComplete(vlc_preparser_req *req, int ,
                                             const bool *result_array,
                                             size_t result_count, void *data)
{
    ThumbnailerCtx* ctx = static_cast<ThumbnailerCtx*>( data );

    vlc::threads::mutex_locker lock( ctx->thumbnailer->m_mutex );
    ctx->done = true;
    if (result_count != 1) {
        ctx->error = true;
    } else {
        ctx->error = !result_array[0];
    }
    ctx->thumbnailer->m_currentContext = nullptr;
    ctx->thumbnailer->m_cond.broadcast();
    vlc_preparser_req_Release( req );
}

bool Thumbnailer::generate( const medialibrary::IMedia&, const std::string& mrl,
                            uint32_t desiredWidth, uint32_t desiredHeight,
                            float position, const std::string& dest )
{
#if INT_MAX < UINT32_MAX
    assert(desiredWidth < (uint32_t)INT_MAX);
    assert(desiredHeight < (uint32_t)INT_MAX);
#endif

    ThumbnailerCtx ctx{};

    auto item = vlc::wrap_cptr( input_item_New( mrl.c_str(), nullptr ),
                                &input_item_Release );
    if ( unlikely( item == nullptr ) )
        return false;

    ctx.done = false;
    ctx.thumbnailer = this;
    ctx.error = true;
    {
        vlc::threads::mutex_locker lock( m_mutex );
        m_currentContext = &ctx;
        struct vlc_thumbnailer_arg thumb_arg = {
            .seek = {
                .type = vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_POS,
                .pos = position,
                .speed = vlc_thumbnailer_arg::seek::VLC_THUMBNAILER_SEEK_FAST,
            },
            .hw_dec = false,
        };

        struct vlc_thumbnailer_output thumb_out = {
            .format = VLC_THUMBNAILER_FORMAT_JPEG,
            .width = (int)desiredWidth,
            .height = (int)desiredHeight,
            .crop = true,
            .file_path = dest.c_str(),
            .creat_mode = 0600,
        };

        static const struct vlc_thumbnailer_to_files_cbs cbs = {
            .on_ended = onThumbnailToFilesComplete,
        };

        vlc_preparser_req *preparserReq;
        preparserReq = vlc_preparser_GenerateThumbnailToFiles(m_thumbnailer.get(),
                                                              item.get(),
                                                              &thumb_arg,
                                                              &thumb_out, 1,
                                                              &cbs, &ctx);

        if (preparserReq == NULL)
        {
            m_currentContext = nullptr;
            return false;
        }
        while ( ctx.done == false )
            m_cond.wait( m_mutex );
        m_currentContext = nullptr;
    }

    return !ctx.error;
}

void Thumbnailer::stop()
{
    vlc_preparser_Cancel(m_thumbnailer.get(), NULL);

    vlc::threads::mutex_locker lock{ m_mutex };
    if ( m_currentContext != nullptr )
    {
        while (m_currentContext != nullptr && m_currentContext->done == false)
            m_cond.wait( m_mutex );
    }
}
