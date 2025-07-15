/*
 * Macros.hpp
 *****************************************************************************
 * Copyright (C) 2025 VideoLabs, VideoLAN and VLC Authors
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
#ifndef MACROS_HPP
#define MACROS_HPP

#define PREREQ_COPYMOVEASSIGN(classname, op)\
public:\
    classname(classname&&) = op;\
    classname(const classname&) = op;\
    classname& operator=(classname&&) = op;\
    classname& operator=(const classname&) = op

#define PREREQ_VIRTUAL(classname)\
    PREREQ_COPYMOVEASSIGN(classname, delete)

#define PREREQ_INTERFACE(classname)\
public:\
    classname() = default;\
    virtual ~classname() = default;\
    PREREQ_COPYMOVEASSIGN(classname, default)

#endif // MACROS_HPP
