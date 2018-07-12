/*****************************************************************************
 * vlc_cxx_helpers.hpp: C++ helpers
 *****************************************************************************
 * Copyright (C) 1998-2018 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#ifndef VLC_CXX_HELPERS_HPP
#define VLC_CXX_HELPERS_HPP

/******************************************************************************
 * C++ memory management helpers
 ******************************************************************************/

#ifdef __cplusplus

#include <memory>
#include <utility>
#include <type_traits>

namespace vlc
{

namespace
{
// This helpers need static linkage to avoid their signature to change when
// building as C++17 (noexcept becomes part of the function signature stating there)

// Wraps a pointer with a custom releaser
// ex: auto ptr = vlc_wrap_cptr( input_item, &input_item_Release );

///
/// Wraps a C pointer into a std::unique_ptr
///
/// This will convert a C pointer of type T to a std::unique_ptr<T, R> where
/// T is the pointee type, and R is an arbitrary releaser type.
///
/// ptr will be automatically released by calling r( ptr ) when falling out of
/// scope (whether by returning of by throwing an exception
///
/// @param ptr a C pointer
/// @param r An instance of a Callable type, that will be invoked with ptr
///          as its first and only parameter.
template <typename T, typename Releaser>
inline auto wrap_cptr( T* ptr, Releaser&& r ) noexcept
    -> std::unique_ptr<T, typename std::decay<decltype( r )>::type>
{
    return std::unique_ptr<T, typename std::decay<decltype( r )>::type>{
                ptr, std::forward<Releaser>( r )
    };
}

///
/// Wraps a C pointer into a std::unique_ptr
///
/// This will convert a C pointer to an array of type T to a
/// std::unique_ptr<T[], R> where T is the pointee type, and R is an arbitrary
/// releaser type.
///
/// ptr will be automatically released by calling r( ptr ) when falling out of
/// scope (whether by returning of by throwing an exception
///
/// This function is equivalent to wrap_cptr, except that the returned
/// unique_ptr provides an operator[] for array access instead of operator* and
/// operator->
///
/// @param ptr a C pointer
/// @param r An instance of a Callable type, that will be invoked with ptr
///          as its first and only parameter.
template <typename T, typename Releaser>
inline auto wrap_carray( T* ptr, Releaser&& r ) noexcept
    -> std::unique_ptr<T[], typename std::decay<decltype( r )>::type>
{
    return std::unique_ptr<T[], typename std::decay<decltype( r )>::type>{
        ptr, std::forward<Releaser>( r )
    };
}

///
/// Wraps a C pointer into a std::unique_ptr
///
/// This is a convenience wrapper that will use free() as its releaser
///
template <typename T>
inline std::unique_ptr<T, void (*)(void*)> wrap_cptr( T* ptr ) noexcept
{
    return wrap_cptr( ptr, &free );
}

///
/// Wraps a C pointer into a std::unique_ptr
///
/// This is a convenience wrapper that will use free() as its releaser
///
template <typename T>
inline std::unique_ptr<T[], void (*)(void*)> wrap_carray( T* ptr ) noexcept
{
    return wrap_carray( ptr, &free );
}

} // anonymous namespace

} // namespace vlc

#endif

#endif // VLC_CXX_HELPERS_HPP
