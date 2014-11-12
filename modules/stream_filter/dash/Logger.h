/*
 * Logger.h
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
#ifndef LOGGER_H
#define LOGGER_H

#include <vlc_common.h>
#include <string>

namespace dash {

    class Logger
    {
    public:
        static void Error( const std::string &msg );
        static void Debug( const std::string &msg );
        static void setObject( vlc_object_t *p_obj ) { Logger::p_object = p_obj; }
    private:
        static vlc_object_t *p_object;
    };

}

#endif // LOGGER_H
