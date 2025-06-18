/*
 * Compatibility.hpp
 *****************************************************************************
 * Copyright (C) 2025 - VideoLabs, VideoLAN and VLC Authors
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
#ifndef COMPATIBILITY_HPP
#define COMPATIBILITY_HPP

/* Provide std::optional compatibility for builds with c++17 or
   incomplete/bogus c++17 with MacOS <= 10.13 and iOS < 12 */

#ifdef __APPLE__
# include <TargetConditionals.h>
# if (TARGET_OS_IPHONE && __IPHONE_OS_VERSION_MIN_REQUIRED < 120000) || \
     (TARGET_OS_MAC && __MAC_OS_X_VERSION_MIN_REQUIRED < 101400)
#  define IOS_INCOMPLETE_CPP17
# endif
#endif

#if __cplusplus >= 201703L && !defined(IOS_INCOMPLETE_CPP17)
# include <optional>
#else
# include <utility>
# include <type_traits>
# include <stdexcept>
#endif

namespace adaptive
{

#if __cplusplus >= 201703L && !defined(IOS_INCOMPLETE_CPP17)

template <typename T>
using optional = std::optional<T>;
using nullopt_t = std::nullopt_t;
constexpr auto nullopt = std::nullopt;
using in_place_t = std::in_place_t;
constexpr auto in_place = std::in_place;
using bad_optional_access = std::bad_optional_access;
#else

struct nullopt_t {
    explicit nullopt_t() = default;
};
constexpr nullopt_t nullopt{};

struct in_place_t {
    explicit in_place_t() = default;
};
constexpr in_place_t in_place{};

class bad_optional_access : public std::exception {
public:
    const char* what() const noexcept override {
        return "Bad optional access";
    }
};

template <typename T>
class optional
{
private:
    alignas(T) unsigned char storage[sizeof(T)];
    bool has_value_ = false;

    T* ptr() noexcept
    {
        return reinterpret_cast<T*>(storage);
    }
    const T* ptr() const noexcept
    {
        return reinterpret_cast<const T*>(storage);
    }

    void destroy() noexcept(std::is_nothrow_destructible<T>::value)
    {
        if (has_value_)
        {
            ptr()->~T();
            has_value_ = false;
        }
    }

public:
    optional() noexcept = default;

    optional(nullopt_t) noexcept : optional() {}

    optional(const T& value) : has_value_(true)
    {
        new (storage) T(value);
    }
    optional(T&& value) : has_value_(true)
    {
        new (storage) T(std::move(value));
    }
    optional(optional&& other) noexcept(std::is_nothrow_move_constructible<T>::value)
        : has_value_(other.has_value_) {
        if (has_value_) { new (storage) T(std::move(*other.ptr())); other.destroy(); }
    }

    ~optional() {destroy();}

    optional& operator=(const optional& other)
    {
        if (this != &other)
        {
            destroy();
            has_value_ = other.has_value_;
            if (has_value_)
                new (storage) T(*other.ptr());
        }
        return *this;
    }

    optional& operator=(optional&& other) noexcept(
        std::is_nothrow_move_constructible<T>::value &&
        std::is_nothrow_move_assignable<T>::value)
    {
        if (this != &other)
        {
            destroy();
            has_value_ = other.has_value_;
            if (has_value_)
            {
                new (storage) T(std::move(*other.ptr()));
                other.destroy();
            }
        }
        return *this;
    }

    optional& operator=(nullopt_t) noexcept
    {
        destroy();
        return *this;
    }

    optional& operator=(const T& value)
    {
        destroy();
        has_value_ = true;
        new (storage) T(value);
        return *this;
    }
    optional& operator=(T&& value)
    {
        destroy();
        has_value_ = true;
        new (storage) T(std::move(value));
        return *this;
    }

    template <typename... Args>
    void emplace(Args&&... args)
    {
        destroy();
        has_value_ = true;
        new (storage) T(std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept { return has_value_; }
    bool has_value() const noexcept { return has_value_; }

    T& value()
    {
        if (!has_value_) throw bad_optional_access{};
        return *ptr();
    }
    const T& value() const
    {
        if (!has_value_) throw bad_optional_access{};
        return *ptr();
    }

    T* operator->() { return ptr(); }
    const T* operator->() const { return ptr(); }
    T& operator*() { return *ptr(); }
    const T& operator*() const { return *ptr(); }

    template <typename U>
    T value_or(U&& default_value) const& {
        return has_value_ ? *ptr() : static_cast<T>(std::forward<U>(default_value));
    }
    template <typename U>
    T value_or(U&& default_value) && {
        return has_value_ ? std::move(*ptr()) : static_cast<T>(std::forward<U>(default_value));
    }

    void reset() noexcept
    {
        destroy();
    }
};

#endif // #if __cplusplus < 201703L

}

#endif // COMPATIBILITY_HPP
