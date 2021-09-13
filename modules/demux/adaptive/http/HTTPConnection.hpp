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
    class ChunksSourceStream;

    namespace http
    {
        class AuthStorage;

        constexpr unsigned MAX_REDIRECTS = 3;

        class AbstractConnection
        {
            public:
                AbstractConnection(vlc_object_t *);
                virtual ~AbstractConnection();

                virtual bool    prepare     (const ConnectionParams &);
                virtual bool    canReuse     (const ConnectionParams &) const = 0;

                virtual RequestStatus request(const std::string& path,
                                              const BytesRange & = BytesRange()) = 0;
                virtual ssize_t read        (void *p_buffer, size_t len) = 0;

                virtual size_t  getContentLength() const;
                virtual size_t  getBytesRead() const;
                virtual const std::string & getContentType() const;
                virtual const ConnectionParams &getRedirection() const;
                virtual void    setUsed( bool ) = 0;

            protected:
                vlc_object_t      *p_object;
                ConnectionParams   locationparams;
                ConnectionParams   params;
                bool               available;
                size_t             contentLength;
                std::string        contentType;
                BytesRange         bytesRange;
                size_t             bytesRead;
        };

       class LibVLCHTTPSource;

       class LibVLCHTTPConnection : public AbstractConnection
       {
            public:
               LibVLCHTTPConnection(vlc_object_t *, AuthStorage *);
               virtual ~LibVLCHTTPConnection();
               virtual bool    canReuse     (const ConnectionParams &) const override;
               virtual RequestStatus request(const std::string& path,
                                             const BytesRange & = BytesRange()) override;
               virtual ssize_t read         (void *p_buffer, size_t len) override;
               virtual void    setUsed      ( bool ) override;

            private:
               void reset();
               std::string useragent;
               std::string referer;
               LibVLCHTTPSource *source;
               ChunksSourceStream *sourceStream;
               stream_t *stream;
       };

       class StreamUrlConnection : public AbstractConnection
       {
            public:
                StreamUrlConnection(vlc_object_t *);
                virtual ~StreamUrlConnection();

                virtual bool    canReuse     (const ConnectionParams &) const override;

                virtual RequestStatus request(const std::string& path,
                                              const BytesRange & = BytesRange()) override;
                virtual ssize_t read        (void *p_buffer, size_t len) override;

                virtual void    setUsed( bool ) override;

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

       class LibVLCHTTPConnectionFactory : public AbstractConnectionFactory
       {
           public:
               LibVLCHTTPConnectionFactory( AuthStorage * );
               virtual ~LibVLCHTTPConnectionFactory() = default;
               virtual AbstractConnection * createConnection(vlc_object_t *, const ConnectionParams &) override;
           private:
               AuthStorage *authStorage;
       };

       class StreamUrlConnectionFactory : public AbstractConnectionFactory
       {
           public:
               StreamUrlConnectionFactory();
               virtual ~StreamUrlConnectionFactory() {}
               virtual AbstractConnection * createConnection(vlc_object_t *, const ConnectionParams &) override;
       };
    }
}

#endif /* HTTPCONNECTION_H_ */
