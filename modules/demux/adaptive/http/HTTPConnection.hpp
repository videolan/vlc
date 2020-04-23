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

#include "ConnectionParams.hpp"
#include "BytesRange.hpp"
#include <vlc_common.h>
#include <string>

namespace adaptive
{
    namespace http
    {
        class Transport;
        class AuthStorage;

        class AbstractConnection
        {
            public:
                AbstractConnection(vlc_object_t *);
                virtual ~AbstractConnection();

                virtual bool    prepare     (const ConnectionParams &);
                virtual bool    canReuse     (const ConnectionParams &) const = 0;

                virtual enum RequestStatus
                                request     (const std::string& path, const BytesRange & = BytesRange()) = 0;
                virtual ssize_t read        (void *p_buffer, size_t len) = 0;

                virtual size_t  getContentLength() const;
                virtual const std::string & getContentType() const;
                virtual void    setUsed( bool ) = 0;

            protected:
                vlc_object_t      *p_object;
                ConnectionParams   params;
                bool               available;
                size_t             contentLength;
                std::string        contentType;
                BytesRange         bytesRange;
                size_t             bytesRead;
        };

        class HTTPConnection : public AbstractConnection
        {
            public:
                HTTPConnection(vlc_object_t *, AuthStorage *,  Transport *,
                               const ConnectionParams &, bool = false);
                virtual ~HTTPConnection();

                virtual bool    canReuse     (const ConnectionParams &) const;
                virtual enum RequestStatus
                                request     (const std::string& path, const BytesRange & = BytesRange());
                virtual ssize_t read        (void *p_buffer, size_t len);

                void setUsed( bool );
                const ConnectionParams &getRedirection() const;
                static const unsigned MAX_REDIRECTS = 3;

            protected:
                virtual bool    connected   () const;
                virtual bool    connect     ();
                virtual void    disconnect  ();
                virtual bool    send        (const void *buf, size_t size);
                virtual bool    send        (const std::string &data);

                virtual void    onHeader    (const std::string &line,
                                             const std::string &value);
                virtual std::string extraRequestHeaders() const;
                virtual std::string buildRequestHeader(const std::string &path) const;

                ssize_t         readChunk   (void *p_buffer, size_t len);
                enum RequestStatus parseReply();
                std::string readLine();
                std::string useragent;
                std::string referer;

                AuthStorage        *authStorage;
                ConnectionParams    locationparams;
                ConnectionParams    proxyparams;
                bool                connectionClose;
                bool                chunked;
                bool                chunked_eof;
                size_t              chunkLength;
                bool                queryOk;
                int                 retries;
                static const int    retryCount = 5;

            private:
                Transport *transport;
       };

       class StreamUrlConnection : public AbstractConnection
       {
            public:
                StreamUrlConnection(vlc_object_t *);
                virtual ~StreamUrlConnection();

                virtual bool    canReuse     (const ConnectionParams &) const;

                virtual enum RequestStatus
                                request     (const std::string& path, const BytesRange & = BytesRange());
                virtual ssize_t read        (void *p_buffer, size_t len);

                virtual void    setUsed( bool );

            protected:
                void reset();
                stream_t *p_streamurl;
       };

       class AbstractConnectionFactory
       {
           public:
               AbstractConnectionFactory() {}
               virtual ~AbstractConnectionFactory() {}
               virtual AbstractConnection * createConnection(vlc_object_t *, const ConnectionParams &) = 0;
       };

       class NativeConnectionFactory : public AbstractConnectionFactory
       {
           public:
               NativeConnectionFactory( AuthStorage * );
               virtual ~NativeConnectionFactory();
               virtual AbstractConnection * createConnection(vlc_object_t *, const ConnectionParams &);
           private:
               AuthStorage *authStorage;
       };

       class StreamUrlConnectionFactory : public AbstractConnectionFactory
       {
           public:
               StreamUrlConnectionFactory();
               virtual ~StreamUrlConnectionFactory() {}
               virtual AbstractConnection * createConnection(vlc_object_t *, const ConnectionParams &);
       };

       class ConnectionFactory : public AbstractConnectionFactory
       {
           public:
               ConnectionFactory( AuthStorage * );
               virtual ~ConnectionFactory();
               virtual AbstractConnection * createConnection(vlc_object_t *, const ConnectionParams &);
           private:
               NativeConnectionFactory *native;
               StreamUrlConnectionFactory *streamurl;
       };
    }
}

#endif /* HTTPCONNECTION_H_ */
