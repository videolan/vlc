/*
 * IHTTPConnection.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN Authors
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

#include "IHTTPConnection.h"
#include "Chunk.h"
#include "Helper.h"
#include "dash.hpp"

#include <vlc_network.h>
#include <vlc_stream.h>

#include <sstream>

using namespace dash::http;

IHTTPConnection::IHTTPConnection(stream_t *stream_)
{
    stream = stream_;
    httpSocket = -1;
    psz_useragent = var_InheritString(stream, "http-user-agent");
}

IHTTPConnection::~IHTTPConnection()
{
    disconnect();
    free(psz_useragent);
}

bool IHTTPConnection::connect(const std::string &hostname, int port)
{
    httpSocket = net_ConnectTCP(stream, hostname.c_str(), port);
    this->hostname = hostname;

    if(httpSocket == -1)
        return false;

    return true;
}

bool IHTTPConnection::connected() const
{
    return (httpSocket != -1);
}

void IHTTPConnection::disconnect()
{
    if (httpSocket >= 0)
    {
        net_Close(httpSocket);
        httpSocket = -1;
    }
}

bool IHTTPConnection::query(const std::string &path)
{
    std::string header = buildRequestHeader(path);
    header.append("\r\n");
    if (!send( header ) || !parseReply())
        return false;
    return true;
}

ssize_t IHTTPConnection::read(void *p_buffer, size_t len)
{
    ssize_t size = net_Read(stream, httpSocket, NULL, p_buffer, len, true);
    if(size <= 0)
        return -1;
    else
        return size;
}

bool IHTTPConnection::send(const std::string &data)
{
    return send(data.c_str(), data.length());
}

bool IHTTPConnection::send(const void *buf, size_t size)
{
    if (size == 0)
        return true;

    if (httpSocket == -1)
        return false;

    ssize_t ret = net_Write(stream, httpSocket, NULL, buf, size);
    if (ret <= 0)
        return false;

    if ( (size_t)ret < size )
        send( ((uint8_t*)buf) + ret, size - ret );

    return true;
}

bool IHTTPConnection::parseReply()
{
    std::string line = readLine();

    if(line.empty())
        return false;

    if (line.compare(0, 9, "HTTP/1.1 ")!=0)
        return false;

    std::istringstream ss(line.substr(9));
    int replycode;
    ss >> replycode;
    if (replycode != 200 && replycode != 206)
        return false;

    readLine();

    while(!line.empty() && line.compare("\r\n"))
    {
        size_t split = line.find_first_of(':');
        size_t value = split + 1;

        while(line.at(value) == ' ')
            value++;

        onHeader(line.substr(0, split), line.substr(value));
        line = readLine();
    }

    return true;
}

std::string IHTTPConnection::readLine()
{
    std::stringstream ss;
    char c[1];
    ssize_t size = net_Read(stream, httpSocket, NULL, c, 1, false);

    while(size >= 0)
    {
        ss << c[0];
        if(c[0] == '\n')
            break;

        size = net_Read(stream, httpSocket, NULL, c, 1, false);
    }

    if(size > 0)
        return ss.str();

    return "";
}

std::string IHTTPConnection::buildRequestHeader(const std::string &path) const
{
    std::stringstream req;
    req << "GET " << path << " HTTP/1.1\r\n" <<
           "Host: " << hostname << "\r\n" <<
           "User-Agent: " << std::string(psz_useragent) << "\r\n";
    req << extraRequestHeaders();
    return req.str();
}
