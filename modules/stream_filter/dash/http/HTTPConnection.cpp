/*
 * HTTPConnection.cpp
 *****************************************************************************
 * Copyright (C) 2010 - 2011 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "HTTPConnection.h"
#include <vlc_url.h>

using namespace dash::http;

HTTPConnection::HTTPConnection  (Chunk *chunk, stream_t *stream) :
                stream          (stream),
                chunk           (chunk),
                peekBufferLen   (0)
{
    this->url        = chunk->getUrl();
    this->peekBuffer = new uint8_t[PEEKBUFFER];
}
HTTPConnection::~HTTPConnection ()
{
    delete[] this->peekBuffer;
    this->closeSocket();
}

int             HTTPConnection::read            (void *p_buffer, size_t len)
{
    if(this->peekBufferLen == 0)
    {
        int size = net_Read(this->stream, this->httpSocket, NULL, p_buffer, len, false);

        if(size <= 0)
            return 0;

        return size;
    }

    memcpy(p_buffer, this->peekBuffer, this->peekBufferLen);
    int ret = this->peekBufferLen;
    this->peekBufferLen = 0;
    return ret;
}
int             HTTPConnection::peek            (const uint8_t **pp_peek, size_t i_peek)
{
    if(this->peekBufferLen == 0)
        this->peekBufferLen = this->read(this->peekBuffer, PEEKBUFFER);

    int size = i_peek > this->peekBufferLen ? this->peekBufferLen : i_peek;

    uint8_t *peek = new uint8_t [size];
    memcpy(peek, this->peekBuffer, size);
    *pp_peek = peek;
    return size;
}
void            HTTPConnection::parseURL        ()
{
    vlc_url_t url_components;
    vlc_UrlParse(&url_components, this->url.c_str(), 0);
    this->path = url_components.psz_path;
    this->port = url_components.i_port ? url_components.i_port : 80;

    if(this->url.compare(0, 4, "http"))
        this->hostname = Helper::combinePaths(Helper::getDirectoryPath(stream->psz_path), this->url);
    else
        this->hostname = url_components.psz_host;

    this->request = "GET " + this->path + " HTTP/1.1\r\n" +
                    "Host: " + this->hostname + "\r\nConnection: close\r\n\r\n";
}
void            HTTPConnection::prepareRequest  ()
{
    if(!chunk->useByteRange())
    {
        this->request = "GET "          + this->path     + " HTTP/1.1" + "\r\n" +
                        "Host: "        + this->hostname + "\r\n" +
                        "Connection: close\r\n\r\n";
    }
    else
    {
        std::stringstream req;
        req << "GET " << this->path << " HTTP/1.1\r\n" <<
               "Host: " << this->hostname << "\r\n" <<
               "Range: bytes=" << this->chunk->getStartByte() << "-" << this->chunk->getEndByte() << "\r\n" <<
               "Connection: close\r\n\r\n";

        this->request = req.str();
    }
}
bool            HTTPConnection::init            ()
{
    this->parseURL();
    this->prepareRequest();

    this->httpSocket = net_ConnectTCP(this->stream, this->hostname.c_str(), this->port);

    if(this->sendData(this->request))
        return this->parseHeader();

    return false;
}
bool            HTTPConnection::parseHeader     ()
{
    std::string line = this->readLine();

    while(line.compare("\r\n"))
    {
        line = this->readLine();
    }

    return true;
}
std::string     HTTPConnection::readLine        ()
{
    std::stringstream ss;
    char c[1];
    size_t size = net_Read(this->stream, this->httpSocket, NULL, c, 1, false);

    while(size)
    {
        ss << c[0];
        if(c[0] == '\n')
            break;

        size = net_Read(this->stream, this->httpSocket, NULL, c, 1, false);
    }

    if(size > 0)
        return ss.str();

    return "\r\n";
}
bool            HTTPConnection::sendData        (const std::string& data)
{
    ssize_t size = net_Write(this->stream, this->httpSocket, NULL, data.c_str(), data.size());
    if (size == -1)
    {
        return false;
    }
    if ((size_t)size != data.length())
    {
        this->sendData(data.substr(size, data.size()));
    }

    return true;
}
void            HTTPConnection::closeSocket     ()
{
    net_Close(this->httpSocket);
}
