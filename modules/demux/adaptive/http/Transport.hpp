/*
 * Transport.hpp
 *****************************************************************************
 * Copyright (C) 2015-2018 - VideoLabs, VideoLAN and VLC authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP

#include <vlc_common.h>
#include <vlc_tls.h>
#include <string>

namespace adaptive
{
    namespace http
    {
        class Transport
        {
            public:
                Transport(bool b_secure = false);
                ~Transport();
                bool    connect     (vlc_object_t *, const std::string&, int port = 80);
                bool    connected   () const;
                bool    send        (const void *buf, size_t size);
                ssize_t read        (void *p_buffer, size_t len);
                std::string readline();
                void    disconnect  ();

            protected:
                vlc_tls_client_t *creds;
                vlc_tls_t *tls;
                bool b_secure;
        };
    }
}

#endif // TRANSPORT_HPP
