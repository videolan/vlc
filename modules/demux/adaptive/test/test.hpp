/*****************************************************************************
 * test.hpp
 *****************************************************************************
 * Copyright (C) 2020 VideoLabs, VideoLAN and VLC Authors
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
#ifndef ADAPTIVE_TEST_H
#define ADAPTIVE_TEST_H
#include <exception>
#include <iostream>

#define DoExpect(testcond, testcontext, testline) \
    try {\
        if (!(testcond)) \
            throw 1;\
    }\
    catch (...)\
    {\
        std::cerr << testcontext << ": failed " \
            " line " << testline << std::endl;\
        std::rethrow_exception(std::current_exception());\
    }

#define Expect(testcond) DoExpect((testcond), __FUNCTION__, __LINE__)

int Inheritables_test();
int TemplatedUri_test();
int SegmentBase_test();
int SegmentList_test();
int SegmentTemplate_test();
int Timeline_test();
int Conversions_test();
int M3U8MasterPlaylist_test();
int M3U8Playlist_test();
int CommandsQueue_test();
int BufferingLogic_test();

#endif
