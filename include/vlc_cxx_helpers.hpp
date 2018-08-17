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

#ifdef VLC_THREADS_H_
// Ensure we can use vlc_sem_wait_i11e. We can't declare different versions
// of the semaphore helper based on vlc_interrupt inclusion, as it would
// violate ODR
# include <vlc_interrupt.h>
#endif

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

#ifdef VLC_THREADS_H_

namespace threads
{

class mutex
{
public:
    mutex() noexcept
    {
        vlc_mutex_init( &m_mutex );
    }
    ~mutex()
    {
        vlc_mutex_destroy( &m_mutex );
    }

    mutex( const mutex& ) = delete;
    mutex& operator=( const mutex& ) = delete;
    mutex( mutex&& ) = delete;
    mutex& operator=( mutex&& ) = delete;

    void lock() noexcept
    {
        vlc_mutex_lock( &m_mutex );
    }
    void unlock() noexcept
    {
        vlc_mutex_unlock( &m_mutex );
    }

private:
    vlc_mutex_t m_mutex;
    friend class condition_variable;
    friend class mutex_locker;
};

class condition_variable
{
public:
    condition_variable() noexcept
    {
        vlc_cond_init( &m_cond );
    }
    ~condition_variable()
    {
        vlc_cond_destroy( &m_cond );
    }
    void signal() noexcept
    {
        vlc_cond_signal( &m_cond );
    }
    void broadcast() noexcept
    {
        vlc_cond_broadcast( &m_cond );
    }
    void wait( mutex& mutex ) noexcept
    {
        vlc_cond_wait( &m_cond, &mutex.m_mutex );
    }
    int timedwait( mutex& mutex, vlc_tick_t deadline ) noexcept
    {
        return vlc_cond_timedwait( &m_cond, &mutex.m_mutex, deadline );
    }

private:
    vlc_cond_t m_cond;
};

class mutex_locker
{
public:
    mutex_locker( vlc_mutex_t* m ) noexcept
        : m_mutex( m )
    {
        vlc_mutex_lock( m_mutex );
    }
    mutex_locker( mutex& m ) noexcept
        : mutex_locker( &m.m_mutex )
    {
    }
    ~mutex_locker()
    {
        vlc_mutex_unlock( m_mutex );
    }
    mutex_locker( const mutex_locker& ) = delete;
    mutex_locker& operator=( const mutex_locker& ) = delete;
    mutex_locker( mutex_locker&& ) = delete;
    mutex_locker& operator=( mutex_locker&& ) = delete;

private:
    vlc_mutex_t* m_mutex;
};

class semaphore
{
public:
    semaphore() noexcept
    {
        vlc_sem_init( &m_sem, 0 );
    }
    semaphore( unsigned int count ) noexcept
    {
        vlc_sem_init( &m_sem, count );
    }
    ~semaphore()
    {
        vlc_sem_destroy( &m_sem );
    }

    semaphore( const semaphore& ) = delete;
    semaphore& operator=( const semaphore& ) = delete;
    semaphore( semaphore&& ) = delete;
    semaphore& operator=( semaphore&& ) = delete;

    int post() noexcept
    {
        return vlc_sem_post( &m_sem );
    }
    void wait() noexcept
    {
        vlc_sem_wait( &m_sem );
    }

    int wait_i11e() noexcept
    {
        return vlc_sem_wait_i11e( &m_sem );
    }

private:
    vlc_sem_t m_sem;
};

}

#endif // VLC_THREADS_H_

} // namespace vlc

#endif

#endif // VLC_CXX_HELPERS_HPP
