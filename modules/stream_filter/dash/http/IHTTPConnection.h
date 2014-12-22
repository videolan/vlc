/*
 * IHTTPConnection.h
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
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

#ifndef IHTTPCONNECTION_H_
#define IHTTPCONNECTION_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <string>

namespace dash
{
    namespace http
    {
        class IHTTPConnection
        {
            public:
                IHTTPConnection(stream_t *stream);
                virtual ~IHTTPConnection();

                virtual bool    connect     (const std::string& hostname, int port = 80);
                virtual bool    connected   () const;
                virtual bool    query       (const std::string& path);
                virtual bool    send        (const void *buf, size_t size);
                virtual ssize_t read        (void *p_buffer, size_t len);
                virtual void    disconnect  ();
                virtual bool    send        (const std::string &data);

            protected:

                virtual void    onHeader    (const std::string &key,
                                             const std::string &value) = 0;
                virtual std::string extraRequestHeaders() const = 0;
                virtual std::string buildRequestHeader(const std::string &path) const;

                bool parseReply();
                std::string readLine();
                std::string hostname;
                char * psz_useragent;
                stream_t   *stream;

            private:
                int         httpSocket;
        };
    }
}

#endif /* IHTTPCONNECTION_H_ */
