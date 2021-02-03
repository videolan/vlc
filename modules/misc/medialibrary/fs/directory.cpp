/*****************************************************************************
 * directory.cpp: Media library network directory
 *****************************************************************************
 * Copyright (C) 2018 VLC authors, VideoLAN and VideoLabs
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

#include "directory.h"
#include "file.h"
#include "util.h"

#include <vlc_common.h>
#include <vlc_url.h>
#include <vlc_input_item.h>
#include <vlc_fs.h>
#include <vlc_input.h>
#include <vlc_threads.h>
#include <vlc_cxx_helpers.hpp>

#include <algorithm>
#include <assert.h>
#include <medialibrary/filesystem/Errors.h>
#include <sys/stat.h>
#include <system_error>
#include <vector>

using InputItemPtr = vlc_shared_data_ptr_type(input_item_t,
                                              input_item_Hold,
                                              input_item_Release);

namespace vlc {
  namespace medialibrary {

SDDirectory::SDDirectory(const std::string &mrl, SDFileSystemFactory &fs)
    : m_mrl(mrl)
    , m_fs(fs)
{
    if ( *m_mrl.crbegin() != '/' )
        m_mrl += '/';
}

const std::string &
SDDirectory::mrl() const
{
    return m_mrl;
}

const std::vector<std::shared_ptr<IFile>> &
SDDirectory::files() const
{
    if ( !m_read_done )
        read();
    return m_files;
}

const std::vector<std::shared_ptr<IDirectory>> &
SDDirectory::dirs() const
{
    if ( !m_read_done )
        read();
    return m_dirs;
}

std::shared_ptr<IDevice>
SDDirectory::device() const
{
    if ( !m_device )
        m_device = m_fs.createDeviceFromMrl( mrl() );
    return m_device;
}

std::shared_ptr<IFile> SDDirectory::file(const std::string& mrl) const
{
    auto fs = files();
    // Don't compare entire mrls, this might yield false negative when a
    // device has multiple mountpoints.
    auto fileName = utils::fileName( mrl );
    auto it = std::find_if( cbegin( fs ), cend( fs ),
                            [&fileName]( const std::shared_ptr<fs::IFile> f ) {
                                return f->name() == fileName;
                            });
    if ( it == cend( fs ) )
        throw medialibrary::fs::errors::NotFound( mrl, m_mrl );
    return *it;
}

bool SDDirectory::contains(const std::string& fileName) const
{
    auto fs = files();
    return std::find_if( cbegin( fs ), cend( fs ),
                         [&fileName]( const std::shared_ptr<fs::IFile> f ) {
                             return f->name() == fileName;
                         }) != cend( fs );
}

struct metadata_request {
    vlc::threads::mutex lock;
    vlc::threads::condition_variable cond;
    /* results */
    bool success;
    bool probe;
    std::vector<InputItemPtr> *children;
};

  } /* namespace medialibrary */
} /* namespace vlc */

extern "C" {

static void onParserEnded( input_item_t *, int status, void *data )
{
    auto req = static_cast<vlc::medialibrary::metadata_request*>( data );

    vlc::threads::mutex_locker lock( req->lock );
    req->success = status == VLC_SUCCESS;
    req->probe = true;
    req->cond.signal();
}

static void onParserSubtreeAdded( input_item_t *, input_item_node_t *subtree,
                                  void *data )
{
    auto req = static_cast<vlc::medialibrary::metadata_request*>( data );

    for ( int i = 0; i < subtree->i_children; ++i )
    {
        input_item_node_t* child = subtree->pp_children[i];
        /* this class assumes we always receive a flat list */
        assert( child->i_children == 0 );
        input_item_t* media = child->p_item;
        req->children->emplace_back( media );
    }
}

} /* extern C */

namespace vlc {
  namespace medialibrary {

static bool request_metadata_sync( libvlc_int_t *libvlc, input_item_t *media,
                                   std::vector<InputItemPtr> *out_children )
{
    metadata_request req;
    req.children = out_children;
    req.probe = false;
    auto deadline = vlc_tick_now() + VLC_TICK_FROM_SEC( 15 );

    media->i_preparse_depth = 1;

    static const input_item_parser_cbs_t cbs = {
        onParserEnded,
        onParserSubtreeAdded,
    };

    auto inputParser = vlc::wrap_cptr( input_item_Parse( media, VLC_OBJECT( libvlc ), &cbs, &req ),
                                       &input_item_parser_id_Release );

    if ( inputParser == nullptr )
        return false;

    vlc::threads::mutex_locker lock( req.lock );
    while ( req.probe == false )
    {
        auto res = req.cond.timedwait( req.lock, deadline );
        if ( res != 0 )
        {
            throw medialibrary::fs::errors::System(
                ETIMEDOUT, "Failed to browse directory: Operation timed out" );
        }
    }
    return req.success;
}

void
SDDirectory::read() const
{
    auto media =
        vlc::wrap_cptr( input_item_New( m_mrl.c_str(), m_mrl.c_str() ), &input_item_Release );
    if ( !media )
        throw std::bad_alloc();

    std::vector<InputItemPtr> children;

    input_item_AddOption( media.get(), "show-hiddenfiles", VLC_INPUT_OPTION_TRUSTED );
    input_item_AddOption( media.get(), "ignore-filetypes=''", VLC_INPUT_OPTION_TRUSTED );
    input_item_AddOption( media.get(), "sub-autodetect-fuzzy=2", VLC_INPUT_OPTION_TRUSTED );
    auto status = request_metadata_sync( m_fs.libvlc(), media.get(), &children );

    if ( status == false )
        throw medialibrary::fs::errors::System(
            EIO, "Failed to browse directory: Unknown error" );

    for ( const InputItemPtr& m : children )
    {
        const char* mrl = m.get()->psz_uri;
        enum input_item_type_e type = m->i_type;
        if ( type == ITEM_TYPE_DIRECTORY )
        {
            m_dirs.push_back( std::make_shared<SDDirectory>( mrl, m_fs ) );
        }
        else if ( type == ITEM_TYPE_FILE )
        {
            addFile( mrl, IFile::LinkedFileType::None, {} );
            for ( auto i = 0; i < m->i_slaves; ++i )
            {
                const auto* slave = m->pp_slaves[i];
                const auto linked_type = slave->i_type == SLAVE_TYPE_AUDIO
                                             ? IFile::LinkedFileType::SoundTrack
                                             : IFile::LinkedFileType::Subtitles;

                addFile( slave->psz_uri, linked_type, mrl );
            }
        }
    }

    m_read_done = true;
}

void
SDDirectory::addFile(std::string mrl, IFile::LinkedFileType fType, std::string linkedFile) const
{
    time_t lastModificationDate = 0;
    int64_t fileSize = 0;

    if ( m_fs.isNetworkFileSystem() == false )
    {
        const auto path = vlc::wrap_cptr( vlc_uri2path( mrl.c_str() ) );
        struct stat stat;

        if ( vlc_stat( path.get(), &stat ) != 0 )
        {
            if ( errno == EACCES )
                return;
            throw errors::System{ errno, "Failed to get file info" };
        }
        lastModificationDate = stat.st_mtime;
        fileSize = stat.st_size;
    }

    if ( fType == IFile::LinkedFileType::None )
    {
        m_files.push_back(
            std::make_shared<SDFile>( std::move( mrl ), fileSize, lastModificationDate ) );
    }
    else
    {
        m_files.push_back( std::make_shared<SDFile>(
            std::move( mrl ), fType, std::move( linkedFile ), fileSize, lastModificationDate ) );
    }
}
  } /* namespace medialibrary */
} /* namespace vlc */
