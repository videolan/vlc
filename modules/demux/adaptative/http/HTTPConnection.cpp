/*
 * HTTPConnection.cpp
 *****************************************************************************
 * Copyright (C) 2014-2015 - VideoLAN and VLC Authors
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

#include "HTTPConnection.hpp"
#include "Sockets.hpp"
#include "../adaptative/tools/Helper.h"

#include <sstream>

using namespace adaptative::http;

HTTPConnection::HTTPConnection(vlc_object_t *stream_, Socket *socket_, bool persistent)
{
    socket = socket_;
    stream = stream_;
    psz_useragent = var_InheritString(stream, "http-user-agent");
    bytesRead = 0;
    contentLength = 0;
    queryOk = false;
    retries = 0;
    connectionClose = !persistent;
    port = 80;
    available = true;
}

HTTPConnection::~HTTPConnection()
{
    free(psz_useragent);
    delete socket;
}

bool HTTPConnection::compare(const std::string &hostname, uint16_t port, int type) const
{
    return ( hostname == this->hostname &&
            (socket && socket->getType() == type) &&
             port == this->port );
}

bool HTTPConnection::connect(const std::string &hostname, uint16_t port)
{
    if(!socket->connect(stream, hostname.c_str(), port))
        return false;

    this->hostname = hostname;
    this->port = port;

    return true;
}

bool HTTPConnection::connected() const
{
    return socket->connected();
}

void HTTPConnection::disconnect()
{
    queryOk = false;
    bytesRead = 0;
    contentLength = 0;
    bytesRange = BytesRange();
    socket->disconnect();
}

int HTTPConnection::query(const std::string &path, const BytesRange &range)
{
    queryOk = false;

    msg_Dbg(stream, "Retrieving ://%s:%u%s @%zu", hostname.c_str(), port, path.c_str(),
            range.isValid() ? range.getStartByte() : 0);

    if(!connected() && ( hostname.empty() || !connect(hostname, port) ))
        return VLC_EGENERIC;

    bytesRange = range;

    std::string header = buildRequestHeader(path);
    if(connectionClose)
        header.append("Connection: close\r\n");
    header.append("\r\n");

    if(!send( header ))
    {
        socket->disconnect();
        if(!connectionClose)
        {
            /* server closed connection pipeline after last req. need new */
            connectionClose = true;
            return query(path, range);
        }
        return VLC_EGENERIC;
    }

    int i_ret = parseReply();
    if(i_ret == VLC_SUCCESS)
    {
        queryOk = true;
    }
    else if(i_ret == VLC_EGENERIC)
    {
        socket->disconnect();
        if(!connectionClose)
        {
            connectionClose = true;
            return query(path, range);
        }
    }

    return i_ret;
}

ssize_t HTTPConnection::read(void *p_buffer, size_t len)
{
    if( !connected() ||
       (!queryOk && bytesRead == 0) )
        return VLC_EGENERIC;

    if(len == 0)
        return VLC_SUCCESS;

    queryOk = false;

    const size_t toRead = contentLength - bytesRead;

    if (toRead == 0)
        return VLC_SUCCESS;

    if(len > toRead)
        len = toRead;

    ssize_t ret = socket->read(stream, p_buffer, len);
    if(ret >= 0)
        bytesRead += ret;

    if(ret < 0 || (size_t)ret < len) /* set EOF */
    {
        socket->disconnect();
        return VLC_EGENERIC;
    }

    return ret;
}

bool HTTPConnection::send(const std::string &data)
{
    return send(data.c_str(), data.length());
}

bool HTTPConnection::send(const void *buf, size_t size)
{
    return socket->send(stream, buf, size);
}

int HTTPConnection::parseReply()
{
    std::string line = readLine();

    if(line.empty())
        return VLC_EGENERIC;

    if (line.compare(0, 9, "HTTP/1.1 ")!=0)
        return VLC_ENOOBJ;

    std::istringstream ss(line.substr(9));
    int replycode;
    ss >> replycode;
    if (replycode != 200 && replycode != 206)
        return VLC_ENOOBJ;

    line = readLine();

    while(!line.empty() && line.compare("\r\n"))
    {
        size_t split = line.find_first_of(':');
        size_t value = split + 1;

        while(line.at(value) == ' ')
            value++;

        onHeader(line.substr(0, split), line.substr(value));
        line = readLine();
    }

    return VLC_SUCCESS;
}

std::string HTTPConnection::readLine()
{
    return socket->readline(stream);
}

bool HTTPConnection::isAvailable() const
{
    return available;
}

void HTTPConnection::setUsed( bool b )
{
    available = !b;
    if(available)
    {
        if(!connectionClose && contentLength == bytesRead )
        {
            queryOk = false;
            bytesRead = 0;
            contentLength = 0;
            bytesRange = BytesRange();
        }
        else  /* We can't resend request if we haven't finished reading */
            disconnect();
    }
}

size_t HTTPConnection::getContentLength() const
{
    return contentLength;
}

void HTTPConnection::onHeader(const std::string &key,
                              const std::string &value)
{
    if(key == "Content-Length")
    {
        std::istringstream ss(value);
        size_t length;
        ss >> length;
        contentLength = length;
    }
    else if (key == "Connection" && value =="close")
    {
        connectionClose = true;
    }
}

std::string HTTPConnection::buildRequestHeader(const std::string &path) const
{
    std::stringstream req;
    req << "GET " << path << " HTTP/1.1\r\n" <<
           "Host: " << hostname << "\r\n" <<
           "Cache-Control: no-cache" << "\r\n" <<
           "User-Agent: " << std::string(psz_useragent) << "\r\n";
    req << extraRequestHeaders();
    return req.str();
}

std::string HTTPConnection::extraRequestHeaders() const
{
    std::stringstream ss;
    if(bytesRange.isValid())
    {
        ss << "Range: bytes=" << bytesRange.getStartByte() << "-";
        if(bytesRange.getEndByte())
            ss << bytesRange.getEndByte();
        ss << "\r\n";
    }
    return ss.str();
}
