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

using namespace dash::http;

HTTPConnection::HTTPConnection  (const std::string& url, stream_t *stream)
{
    this->url       = url;
    this->stream    = stream;
}
HTTPConnection::~HTTPConnection ()
{

}

int             HTTPConnection::read            (void *p_buffer, size_t len)
{
    int size = stream_Read(this->urlStream, p_buffer, len);

    if(size <= 0)
        return 0;

    return size;
}
int             HTTPConnection::peek            (const uint8_t **pp_peek, size_t i_peek)
{
    return stream_Peek(this->urlStream, pp_peek, i_peek);
}
void            HTTPConnection::parseURL        ()
{
    this->hostname = this->url;
    this->hostname.erase(0, 7);
    this->path = this->hostname;

    size_t pos = this->hostname.find("/");

    this->hostname  = this->hostname.substr(0, pos);
    this->path      = this->path.substr(pos, this->path.size());

    this->request = "GET " + this->path + " HTTP/1.1\r\n" +
                    "Host: " + this->hostname + "\r\nConnection: close\r\n\r\n";
}
bool            HTTPConnection::init()
{
    this->urlStream = stream_UrlNew( this->stream, this->url.c_str() );

    if( this->urlStream == NULL )
        return false;

    return true;

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
    stream_Delete(this->urlStream);
}
