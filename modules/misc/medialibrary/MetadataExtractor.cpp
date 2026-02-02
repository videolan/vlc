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

#ifdef __APPLE__
# include <TargetConditionals.h>
#endif

#include "medialibrary.h"

#include <vlc_image.h>
#include <vlc_hash.h>
#include <vlc_fs.h>
#include <vlc_preparser.h>

EmbeddedThumbnail::EmbeddedThumbnail( input_attachment_t* a, vlc_fourcc_t fcc )
    : m_attachment( vlc_input_attachment_Hold( a ) )
    , m_fcc( fcc )
{
}

EmbeddedThumbnail::~EmbeddedThumbnail()
{
    vlc_input_attachment_Release( m_attachment );
}

bool EmbeddedThumbnail::save( const std::string& path )
{
    FILE* f = vlc_fopen( path.c_str(), "wb" );

    if ( f == nullptr )
        return false;
    auto res = fwrite( m_attachment->p_data, m_attachment->i_data, 1, f );

    if ( fclose( f ) != 0 )
        return false;
    return res == 1;
}

size_t EmbeddedThumbnail::size() const
{
    return m_attachment->i_data;
}

std::string EmbeddedThumbnail::hash() const
{
    vlc_hash_md5_t md5;
    vlc_hash_md5_Init( &md5 );
    vlc_hash_md5_Update( &md5, m_attachment->p_data, m_attachment->i_data );
    uint8_t bytes[VLC_HASH_MD5_DIGEST_SIZE];
    vlc_hash_md5_Finish( &md5, bytes, sizeof(bytes) );
    std::string res;
    res.reserve( VLC_HASH_MD5_DIGEST_HEX_SIZE );
    const char* hex = "0123456789ABCDEF";
    for ( auto i = 0u; i < VLC_HASH_MD5_DIGEST_SIZE; ++i )
        res.append( { hex[bytes[i] >> 4], hex[bytes[i] & 0xF] } );
    return res;
}

std::string EmbeddedThumbnail::extension() const
{
    switch ( m_fcc )
    {
    case VLC_CODEC_JPEG:
        return "jpg";
    case VLC_CODEC_PNG:
        return "png";
    default:
        vlc_assert_unreachable();
    }
}

MetadataExtractor::MetadataExtractor( vlc_object_t* parent )
    : m_currentCtx( nullptr )
    , m_obj( parent )
    , m_parser(parent, {
        .types = VLC_PREPARSER_TYPE_PARSE | VLC_PREPARSER_OPTION_SUBITEMS,
        .max_parser_threads = 1,
        .max_thumbnailer_threads = 0,
        .timeout = VLC_TICK_FROM_SEC(5),
#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
        .external_process = false,
#else
        .external_process = true,
#endif
    })
{
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

    for ( auto i = 0u; i < inputItem->es_vec.size; ++i )
    {
        medialibrary::parser::IItem::Track t;
        const es_format_t *p_es = &inputItem->es_vec.data[i].es;

        switch ( p_es->i_cat )
        {
            case AUDIO_ES:
                t.type = medialibrary::parser::IItem::Track::Type::Audio;
                t.u.a.nbChannels = p_es->audio.i_channels;
                t.u.a.rate = p_es->audio.i_rate;
                break;
            case VIDEO_ES:
                t.type = medialibrary::parser::IItem::Track::Type::Video;
                t.u.v.fpsNum = p_es->video.i_frame_rate;
                t.u.v.fpsDen = p_es->video.i_frame_rate_base;
                t.u.v.width = p_es->video.i_width;
                t.u.v.height = p_es->video.i_height;
                t.u.v.sarNum = p_es->video.i_sar_num;
                t.u.v.sarDen = p_es->video.i_sar_den;
                break;
            case SPU_ES:
                t.type = medialibrary::parser::IItem::Track::Type::Subtitle;
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

void MetadataExtractor::onParserEnded(vlc_preparser_req *req, int status, void *data)
{
    auto* ctx = static_cast<ParseContext*>( data );

    vlc::threads::mutex_locker lock( ctx->mde->m_mutex );
    if (status == VLC_SUCCESS) {
        if (ctx->item.fileType() == medialibrary::IFile::Type::Playlist
                && ctx->item.nbLinkedItems() == 0) {
            ctx->status =  medialibrary::parser::Status::Fatal;
        } else {
            ctx->mde->populateItem(ctx->item, vlc_preparser_req_GetItem(req));
            ctx->status = medialibrary::parser::Status::Success;
        }
    } else {
        ctx->status = medialibrary::parser::Status::Fatal;
    }
    ctx->done = true;
    ctx->mde->m_currentCtx = nullptr;
    ctx->mde->m_cond.broadcast();
    vlc_preparser_req_Release(req);
}

void MetadataExtractor::onParserSubtreeAdded( vlc_preparser_req *,
                                              input_item_node_t *subtree,
                                              void *data )
{
    auto* ctx = static_cast<ParseContext*>( data );

    for ( auto i = 0; i < subtree->i_children; ++i )
    {
        auto it = subtree->pp_children[i]->p_item;
        auto& subItem = ctx->item.createLinkedItem( it->psz_uri,
                                                   medialibrary::IFile::Type::Main, i );
        ctx->mde->populateItem( subItem, it );
    }
    input_item_node_Delete(subtree);
}

void MetadataExtractor::onAttachmentsAdded( vlc_preparser_req *,
                                            input_attachment_t *const *array,
                                            size_t count, void *data )
{
    auto ctx = static_cast<ParseContext*>( data );
    for ( auto i = 0u; i < count; ++i )
    {
        auto a = array[i];
        auto fcc = image_Mime2Fourcc( a->psz_mime );
        if ( fcc != VLC_CODEC_PNG && fcc != VLC_CODEC_JPEG )
            continue;
        ctx->item.addEmbeddedThumbnail( std::make_shared<EmbeddedThumbnail>( a, fcc ) );
    }
}

medialibrary::parser::Status MetadataExtractor::run( medialibrary::parser::IItem& item )
{
    ParseContext ctx( this, item );

    auto inputItem = vlc::wrap_cptr(input_item_New(item.mrl().c_str(), NULL),
                                    &input_item_Release);
    if (inputItem == nullptr)
        return medialibrary::parser::Status::Fatal;

    {
        vlc::threads::mutex_locker lock( m_mutex );
        m_currentCtx = &ctx;

        int options = VLC_PREPARSER_TYPE_PARSE | VLC_PREPARSER_OPTION_SUBITEMS;

        static const struct vlc_preparser_cbs cbs = {
            .on_ended = onParserEnded,
            .on_subtree_added = onParserSubtreeAdded,
            .on_attachments_added = onAttachmentsAdded,
        };

        vlc_preparser_t *preparser = m_parser.instance();
        if (preparser == nullptr) {
            return medialibrary::parser::Status::Fatal;
        }

        vlc_preparser_req *req = vlc_preparser_Push(preparser,
                                                   inputItem.get(), options,
                                                   &cbs, &ctx);
        if (req == nullptr)
        {
            m_currentCtx = nullptr;
            return medialibrary::parser::Status::Fatal;
        }
        while (ctx.done == false)
            m_cond.wait(m_mutex);
        m_currentCtx = nullptr;
    }

    return ctx.status;
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
    vlc_preparser_t *preparser = m_parser.get();
    if (preparser == nullptr) {
        return;
    }

    /* vlc_preparser_Cancel can call the callback from this thread so the mutex
     * must be unlock */
    vlc_preparser_Cancel(preparser, nullptr);

    vlc::threads::mutex_locker lock(m_mutex);
    if (m_currentCtx != nullptr) {
        while (m_currentCtx != nullptr && m_currentCtx->done == false)
            m_cond.wait(m_mutex);
    }
}
