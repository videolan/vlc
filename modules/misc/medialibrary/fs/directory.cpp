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

#include <assert.h>
#include <vector>
#include <system_error>
#include <vlc_common.h>
#include <vlc_input_item.h>
#include <vlc_input.h>
#include <vlc_threads.h>
#include <vlc_cxx_helpers.hpp>

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
    if (!m_read_done)
        read();
    return m_files;
}

const std::vector<std::shared_ptr<IDirectory>> &
SDDirectory::dirs() const
{
    if (!m_read_done)
        read();
    return m_dirs;
}

std::shared_ptr<IDevice>
SDDirectory::device() const
{
    if (!m_device)
        m_device = m_fs.createDeviceFromMrl(mrl());
    return m_device;
}

struct metadata_request {
    vlc::threads::mutex lock;
    vlc::threads::condition_variable cond;
    /* results */
    input_state_e state;
    bool probe;
    std::vector<InputItemPtr> *children;
};

  } /* namespace medialibrary */
} /* namespace vlc */

extern "C" {

static void onInputEvent( input_thread_t*, const struct vlc_input_event *event,
                          void *data )
{
    auto req = static_cast<vlc::medialibrary::metadata_request*>( data );
    switch ( event->type )
    {
        case INPUT_EVENT_SUBITEMS:
        {
            for (int i = 0; i < event->subitems->i_children; ++i)
            {
                input_item_node_t *child = event->subitems->pp_children[i];
                /* this class assumes we always receive a flat list */
               assert(child->i_children == 0);
               input_item_t *media = child->p_item;
               req->children->emplace_back( media );
            }
            break;
        }
        case INPUT_EVENT_STATE:
            {
                vlc::threads::mutex_locker lock( req->lock );
                req->state = event->state;
            }
            break;
        case INPUT_EVENT_DEAD:
            {
                vlc::threads::mutex_locker lock( req->lock );
                // We need to probe the item now, but not from the input thread
                req->probe = true;
            }
            req->cond.signal();
            break;
        default:
            break;
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
    auto deadline = vlc_tick_now() + VLC_TICK_FROM_SEC( 5 );

    media->i_preparse_depth = 1;
    auto inputThread = vlc::wrap_cptr(
        input_CreatePreparser( VLC_OBJECT( libvlc ), onInputEvent, &req, media ),
        &input_Close );

    if ( inputThread == nullptr )
        return false;

    vlc::threads::mutex_locker lock( req.lock );
    if ( input_Start( inputThread.get() ) != VLC_SUCCESS )
        return false;
    while ( req.probe == false )
    {
        auto res = req.cond.timedwait( req.lock, deadline );
        if (res != 0 )
        {
            input_Stop( inputThread.get() );
            throw std::system_error( ETIMEDOUT, std::generic_category(),
                                     "Failed to browse network directory: "
                                     "Network is too slow");
        }
        if ( req.probe == true )
        {
            if ( req.state == END_S || req.state == ERROR_S )
                break;
            req.probe = false;
        }
    }
    return req.state == END_S;
}

void
SDDirectory::read() const
{
    auto media = vlc::wrap_cptr( input_item_New(m_mrl.c_str(), m_mrl.c_str()),
                                 &input_item_Release );
    if (!media)
        throw std::bad_alloc();

    std::vector<InputItemPtr> children;

    auto status = request_metadata_sync( m_fs.libvlc(), media.get(), &children);

    if ( status == false )
        throw std::system_error(EIO, std::generic_category(),
                                "Failed to browse network directory: "
                                "Unknown error");

    for (const InputItemPtr &m : children)
    {
        const char *mrl = m.get()->psz_uri;
        enum input_item_type_e type = m->i_type;
        if (type == ITEM_TYPE_DIRECTORY)
            m_dirs.push_back(std::make_shared<SDDirectory>(mrl, m_fs));
        else if (type == ITEM_TYPE_FILE)
            m_files.push_back(std::make_shared<SDFile>(mrl));
    }

    m_read_done = true;
}

  } /* namespace medialibrary */
} /* namespace vlc */
