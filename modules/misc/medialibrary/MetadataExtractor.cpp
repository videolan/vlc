/*****************************************************************************
 * MetadataExtractor.cpp: IParserService implementation using libvlccore
 *****************************************************************************
 * Copyright © 2008-2018 VLC authors, VideoLAN and VideoLabs
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

MetadataExtractor::MetadataExtractor( vlc_object_t* parent )
    : m_currentCtx( nullptr )
    , m_obj( parent )
{
}

void MetadataExtractor::onParserEnded( ParseContext& ctx, int status )
{
    vlc::threads::mutex_locker lock( ctx.mde->m_mutex );

    // We need to probe the item now, but not from the input thread
    ctx.success = status == VLC_SUCCESS;
    ctx.needsProbing = true;
    ctx.mde->m_cond.signal();
}

void MetadataExtractor::populateItem( medialibrary::parser::IItem& item, input_item_t* inputItem )
{
    vlc_mutex_locker lock( &inputItem->lock );

    const auto emptyStringWrapper = []( const char* psz ) {
        return psz != nullptr ? std::string{ psz } : std::string{};
    };

    using metadata_t = medialibrary::parser::IItem::Metadata;

    static const std::pair<metadata_t, vlc_meta_type_t> fields[] =
    {
        { metadata_t::Title, vlc_meta_Title },
        { metadata_t::ArtworkUrl, vlc_meta_ArtworkURL },
        { metadata_t::ShowName, vlc_meta_ShowName },
        { metadata_t::Episode, vlc_meta_Episode },
        { metadata_t::Album, vlc_meta_Album },
        { metadata_t::Genre, vlc_meta_Genre },
        { metadata_t::Date, vlc_meta_Date },
        { metadata_t::AlbumArtist, vlc_meta_AlbumArtist },
        { metadata_t::Artist, vlc_meta_Artist },
        { metadata_t::TrackNumber, vlc_meta_TrackNumber },
        { metadata_t::DiscNumber, vlc_meta_DiscNumber },
        { metadata_t::DiscTotal, vlc_meta_DiscTotal }
    };

    if ( inputItem->p_meta != nullptr )
    {
        for( auto pair : fields )
            item.setMeta( pair.first,
                emptyStringWrapper( vlc_meta_Get( inputItem->p_meta, pair.second ) ) );
    }

    item.setDuration( MS_FROM_VLC_TICK(inputItem->i_duration) );

    for ( auto i = 0; i < inputItem->i_es; ++i )
    {
        medialibrary::parser::IItem::Track t;
        const es_format_t *p_es = inputItem->es[i];

        switch ( p_es->i_cat )
        {
            case AUDIO_ES:
                t.type = medialibrary::parser::IItem::Track::Type::Audio;
                t.a.nbChannels = p_es->audio.i_channels;
                t.a.rate = p_es->audio.i_rate;
                break;
            case VIDEO_ES:
                t.type = medialibrary::parser::IItem::Track::Type::Video;
                t.v.fpsNum = p_es->video.i_frame_rate;
                t.v.fpsDen = p_es->video.i_frame_rate_base;
                t.v.width = p_es->video.i_width;
                t.v.height = p_es->video.i_height;
                t.v.sarNum = p_es->video.i_sar_num;
                t.v.sarDen = p_es->video.i_sar_den;
                break;
            default:
                continue;
        }

        char fourcc[4];
        vlc_fourcc_to_char( p_es->i_codec, fourcc );
        t.codec = std::string{ fourcc, 4 };

        t.bitrate = p_es->i_bitrate;
        t.language = emptyStringWrapper( p_es->psz_language );
        t.description = emptyStringWrapper( p_es->psz_description );

        item.addTrack( std::move( t ) );
    }
}

void MetadataExtractor::onParserEnded( input_item_t *, int status, void *data )
{
    auto* ctx = static_cast<ParseContext*>( data );
    ctx->mde->onParserEnded( *ctx, status );
}

void MetadataExtractor::onParserSubtreeAdded( input_item_t *,
                                              input_item_node_t *subtree,
                                              void *data )
{
    auto* ctx = static_cast<ParseContext*>( data );
    ctx->mde->addSubtree( *ctx, subtree );
}

void MetadataExtractor::addSubtree( ParseContext& ctx, input_item_node_t *root )
{
    for ( auto i = 0; i < root->i_children; ++i )
    {
        auto it = root->pp_children[i]->p_item;
        auto& subItem = ctx.item.createSubItem( it->psz_uri, i );
        populateItem( subItem, it );
    }
}

medialibrary::parser::Status MetadataExtractor::run( medialibrary::parser::IItem& item )
{
    ParseContext ctx( this, item );

    ctx.inputItem = {
        input_item_New( item.mrl().c_str(), NULL ),
        &input_item_Release
    };
    if ( ctx.inputItem == nullptr )
        return medialibrary::parser::Status::Fatal;

    const input_item_parser_cbs_t cbs = {
        &MetadataExtractor::onParserEnded,
        &MetadataExtractor::onParserSubtreeAdded,
    };
    m_currentCtx = &ctx;
    ctx.inputItem->i_preparse_depth = 1;
    ctx.inputParser = {
        input_item_Parse( ctx.inputItem.get(), m_obj, &cbs,
                          std::addressof( ctx ) ),
        &input_item_parser_id_Release
    };
    if ( ctx.inputParser == nullptr )
    {
        m_currentCtx = nullptr;
        return medialibrary::parser::Status::Fatal;
    }

    {
        vlc::threads::mutex_locker lock( m_mutex );
        auto deadline = vlc_tick_now() + VLC_TICK_FROM_SEC( 5 );
        while ( ctx.needsProbing == false && ctx.inputParser != nullptr )
        {
            auto res = m_cond.timedwait( m_mutex, deadline );
            if ( res != 0 )
            {
                msg_Dbg( m_obj, "Timed out while extracting %s metadata",
                         item.mrl().c_str() );
                break;
            }
        }
        m_currentCtx = nullptr;
    }

    if ( !ctx.success || ctx.inputParser == nullptr )
        return medialibrary::parser::Status::Fatal;

    if ( item.fileType() == medialibrary::IFile::Type::Playlist &&
         item.nbSubItems() == 0 )
        return medialibrary::parser::Status::Fatal;

    populateItem( item, ctx.inputItem.get() );

    return medialibrary::parser::Status::Success;
}

const char* MetadataExtractor::name() const
{
    return "libvlccore extraction";
}

medialibrary::parser::Step MetadataExtractor::targetedStep() const
{
    return medialibrary::parser::Step::MetadataExtraction;
}

bool MetadataExtractor::initialize( medialibrary::IMediaLibrary* )
{
    return true;
}

void MetadataExtractor::onFlushing()
{
}

void MetadataExtractor::onRestarted()
{
}

void MetadataExtractor::stop()
{
    vlc::threads::mutex_locker lock{ m_mutex };
    if ( m_currentCtx != nullptr )
        input_item_parser_id_Interrupt( m_currentCtx->inputParser.get() );
}
