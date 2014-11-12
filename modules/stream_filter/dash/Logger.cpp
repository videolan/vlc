/*
 * Logger.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN authors
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
#include "Logger.h"

#include <iostream>

using namespace dash;

vlc_object_t * Logger::p_object = NULL;

void Logger::Debug( const std::string &msg )
{
    if ( Logger::p_object )
        msg_Dbg( Logger::p_object, "%s", msg.c_str() );
    else
        std::cout << msg << std::endl;
}

void Logger::Error( const std::string &msg )
{
    if ( Logger::p_object )
        msg_Err( Logger::p_object, "%s", msg.c_str() );
    else
        std::cerr << msg << std::endl;
}
