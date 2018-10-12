/*
 * ConnectionParams.hpp
 *****************************************************************************
 * Copyright (C) 2016 - VideoLAN and VLC Authors
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
#ifndef CONNECTIONPARAMS_HPP
#define CONNECTIONPARAMS_HPP

#include <vlc_common.h>
#include <string>

namespace adaptive
{
    namespace http
    {
        class Transport;

        enum RequestStatus
        {
            Success,
            Redirection,
            Unauthorized,
            NotFound,
            GenericError,
        };

        class BackendPrefInterface
        {
            /* Design Hack for now to force fallback on regular access
             * through hybrid connection factory */
            public:
                BackendPrefInterface() { useaccess = false; }
                bool usesAccess() const { return useaccess; }
                void setUseAccess(bool b) { useaccess = b; }
            private:
                bool useaccess;
        };

        class ConnectionParams : public BackendPrefInterface
        {
            public:
                ConnectionParams();
                ConnectionParams(const std::string &);
                const std::string & getUrl() const;
                const std::string & getScheme() const;
                const std::string & getHostname() const;
                const std::string & getPath() const;
                bool isLocal() const;
                void setPath(const std::string &);
                uint16_t getPort() const;

            private:
                void parse();
                std::string uri;
                std::string scheme;
                std::string hostname;
                std::string path;
                uint16_t port;
        };
    }
}

#endif // CONNECTIONPARAMS_HPP
