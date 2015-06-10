/*
 * HTTPConnection.hpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *               2014 - 2015 VideoLAN and VLC Authors
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
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
#ifndef HTTPCONNECTION_H_
#define HTTPCONNECTION_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <string>

namespace adaptative
{
    namespace http
    {
        class Socket;
        class Chunk;

        class HTTPConnection
        {
            public:
                HTTPConnection(vlc_object_t *stream, Socket *, Chunk * = NULL, bool = false);
                virtual ~HTTPConnection();

                virtual bool    connect     (const std::string& hostname, int port = 80);
                virtual bool    connected   () const;
                virtual int     query       (const std::string& path);
                virtual bool    send        (const void *buf, size_t size);
                virtual ssize_t read        (void *p_buffer, size_t len);
                virtual void    disconnect  ();
                virtual bool    send        (const std::string &data);

                const std::string&  getHostname () const;
                virtual void    bindChunk   (Chunk *chunk);
                virtual bool    isAvailable () const;
                virtual void    releaseChunk();

            protected:

                virtual void    onHeader    (const std::string &line,
                                             const std::string &value);
                virtual std::string extraRequestHeaders() const;
                virtual std::string buildRequestHeader(const std::string &path) const;

                int parseReply();
                std::string readLine();
                std::string hostname;
                char * psz_useragent;
                vlc_object_t *stream;
                size_t toRead;
                Chunk *chunk;

                bool                connectionClose;
                bool                queryOk;
                int                 retries;
                static const int    retryCount = 5;

            private:
                Socket *socket;
       };
    }
}

#endif /* HTTPCONNECTION_H_ */
