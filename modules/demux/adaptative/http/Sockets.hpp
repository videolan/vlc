/*
 * Sockets.hpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN and VLC authors
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
#ifndef SOCKETS_HPP
#define SOCKETS_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_tls.h>
#include <string>

namespace adaptative
{
    namespace http
    {
        class Socket
        {
            public:
                Socket();
                virtual ~Socket();
                virtual bool    connect     (vlc_object_t *, const std::string&, int port = 80);
                virtual bool    connected   () const;
                virtual bool    send        (vlc_object_t *, const void *buf, size_t size);
                virtual ssize_t read        (vlc_object_t *, void *p_buffer, size_t len);
                virtual std::string readline(vlc_object_t *);
                virtual void    disconnect  ();

            protected:
                int netfd;
        };

        class TLSSocket : public Socket
        {
            public:
                TLSSocket();
                virtual ~TLSSocket();
                virtual bool    connect     (vlc_object_t *, const std::string&, int port = 443);
                virtual bool    connected   () const;
                virtual bool    send        (vlc_object_t *, const void *buf, size_t size);
                virtual ssize_t read        (vlc_object_t *, void *p_buffer, size_t len);
                virtual std::string readline(vlc_object_t *);
                virtual void    disconnect  ();

            private:
                vlc_tls_creds_t *creds;
                vlc_tls_t *tls;
        };
    }
}


#endif // SOCKETS_HPP
