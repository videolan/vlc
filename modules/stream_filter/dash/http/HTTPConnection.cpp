/*
 * HTTPConnection.cpp
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "HTTPConnection.h"

using namespace dash::http;

HTTPConnection::HTTPConnection  (stream_t *stream) :
                stream          (stream),
                peekBufferLen   (0),
                contentLength   (0)
{
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
std::string     HTTPConnection::prepareRequest  (Chunk *chunk)
{
    std::string request;

    if(!chunk->useByteRange())
    {
        request = "GET "    + chunk->getPath()    + " HTTP/1.1" + "\r\n" +
                  "Host: "  + chunk->getHostname() + "\r\n" +
                  "Connection: close\r\n\r\n";
    }
    else
    {
        std::stringstream req;
        req << "GET " << chunk->getPath() << " HTTP/1.1\r\n" <<
               "Host: " << chunk->getHostname() << "\r\n" <<
               "Range: bytes=" << chunk->getStartByte() << "-" << chunk->getEndByte() << "\r\n" <<
               "Connection: close\r\n\r\n";

        request = req.str();
    }

    return request;
}
bool            HTTPConnection::init            (Chunk *chunk)
{
    if(!chunk->hasHostname())
        if(!this->setUrlRelative(chunk))
            return false;

    this->httpSocket = net_ConnectTCP(this->stream, chunk->getHostname().c_str(), chunk->getPort());

    if(this->httpSocket == -1)
        return false;

    if(this->sendData(this->prepareRequest(chunk)))
        return this->parseHeader();

    return false;
}
bool            HTTPConnection::parseHeader     ()
{
    std::string line = this->readLine();

    if(line.size() == 0)
        return false;

    while(line.compare("\r\n"))
    {
        if(!strncasecmp(line.c_str(), "Content-Length", 14))
            this->contentLength = atoi(line.substr(15,line.size()).c_str());

        line = this->readLine();

        if(line.size() == 0)
            return false;
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

    return "";
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
bool            HTTPConnection::setUrlRelative  (Chunk *chunk)
{
    std::stringstream ss;
    ss << stream->psz_access << "://" << Helper::combinePaths(Helper::getDirectoryPath(stream->psz_path), chunk->getUrl());
    chunk->setUrl(ss.str());

    return chunk->hasHostname();
}
