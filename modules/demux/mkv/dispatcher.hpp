/*****************************************************************************
 * dispatcher.hpp : matroska demuxer
 *****************************************************************************
 * Copyright (C) 2016 VLC authors, VideoLAN, Videolabs SAS
 *
 * Authors: Filip Roseen <filip@videolabs.io>
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
#ifndef VLC_MKV_DISPATCHER_HPP_
#define VLC_MKV_DISPATCHER_HPP_

#include <vlc_cxx_helpers.hpp>

// ----------------------------------------------------------------------------
// This header contains helpers to simulate lambdas in C++03.
//
// It will be used to create "dispatchers" which can be thought of like a
// `switch` but only more dynamic.
// ----------------------------------------------------------------------------

namespace {

  template<class Impl, class Processor>
  class Dispatcher {
    protected:
      Dispatcher() : _default_handler (NULL) { }
    public:
      template<class It>
      void iterate (It beg, It end, void* const& payload) const {
        for (; beg != end; ++beg)
          static_cast<Impl const*> (this)->Impl::send (*beg, payload);
      }

      void set_default_handler (Processor const& callback) {
        _default_handler = callback;
      }

      void on_create () {
        /* empty default implementation */
      }

      Processor _default_handler;
  };

  template<int>
  struct DispatcherTag;

  template<class T, class DispatcherType>
  class DispatchContainer {
    public:    static DispatcherType dispatcher;
    protected: static vlc::threads::mutex _dispatcher_lock;
  };

  template<class T, class DT>
  DT DispatchContainer<T, DT>::dispatcher;

  template<class T, class DT>
  vlc::threads::mutex DispatchContainer<T, DT>::_dispatcher_lock;
}

// ----------------------------------------------------------------------------
//   * `GroupName_##_tag` is used so that we can refer to a static dispatcher
//      of the correct type without instantiating DispatchContainer with a
//      locally declared type (since it is illegal in C++03).
//
//      We are however allowed to pass the variable of a variable declared
//      `extern`, and this will effectively be our handle to the "foreign"
//      static dispatcher.
//
//      We make the variable have type `DispatcherTag<__LINE__>` so that you
//      can use MKV_SWITCH_CREATE with the same name in different parts of the
//      translation-unit (without collision).
//
//   *  `GroupName_ ## _base` is used to declare a bunch of helpers; names that
//       must be available in our fake "lambdas" (C++03 is a pain).
// ----------------------------------------------------------------------------

#define MKV_SWITCH_CREATE(DispatchType_, GroupName_, PayloadType_) \
  typedef DispatcherTag<__LINE__> GroupName_ ## _tag_t; \
  struct GroupName_; \
  struct GroupName_##_base : DispatchContainer<GroupName_##_tag_t, DispatchType_> { \
      typedef      PayloadType_ payload_t;                         \
      typedef     DispatchType_ dispatch_t;                        \
      typedef struct GroupName_ handler_t;                         \
  };                                                               \
  struct GroupName_ : GroupName_ ## _base

// ----------------------------------------------------------------------------
//   * `Dispatcher` is a static function used to access the dispatcher in a
//      thread-safe manner. We only want _one_ thread to actually construct
//      and initialize it, hence the lock.
// ----------------------------------------------------------------------------

#define MKV_SWITCH_INIT()                     \
  static dispatch_t& Dispatcher () {          \
      static handler_t * p_handler = NULL;    \
      _dispatcher_lock.lock();                \
      if (unlikely( p_handler == NULL) ) {    \
          static handler_t handler;           \
          p_handler = &handler;               \
          p_handler->dispatcher.on_create (); \
      }                                       \
       _dispatcher_lock.unlock();             \
      return p_handler->dispatcher;           \
  } struct PleaseAddSemicolon {}

// ----------------------------------------------------------------------------
//   * The following is to be used inside `struct GroupName_`, effectivelly
//     declaring a local struct and a data-member, `ClassName_ ## _processor`,
//     of that type.
//
//     When the data-member is constructed it will run `InitializationExpr_`,
//     meaning that it can access the static dispatcher and register itself (or
//     whatever is desired).
//
//   * Since we need to do type-erasure, once again, because of the fact that
//     C++03 does not support locally declared types as template-arguments, we
//     declare a static function `ClassName_ ## _callback` that will cast our
//     payload from `void*` to the appropriate type.
//
//   * The body of `ClassName_ ## _handler` will be written by the user of the
//     MACRO, and the implementation will effectively be invoked whenever the
//     dispatcher decides to (through `ClassName_ ## _callback`).
//
//   * Since the type of the `VariableName_` argument to `ClassName_ ## _handler`
//     might not necessarily be the same as the type passed from the dispatcher
//     (because of the type-erasure), the macro provides a way to change the
//     type with a `UnwrapExpr_` (see the function call within `ClassName_ ##
//     _callback`).
// ----------------------------------------------------------------------------

#define MKV_SWITCH_CASE_DEFINITION(ClassName_, RealType_, Type_, VariableName_, PayloadName_, InitializationExpr_, UnwrapExpr_) \
  struct ClassName_##_processor {                                             \
    ClassName_##_processor () { InitializationExpr_; }                        \
  } ClassName_##__wrapper;                                                    \
  static inline void ClassName_##_callback (Type_ data, void* PayloadName_) { \
    ClassName_##_handler (UnwrapExpr_, *static_cast<payload_t*>(PayloadName_));      \
  }                                                                             \
  static inline void ClassName_##_handler (RealType_& VariableName_, payload_t& PayloadName_)

#endif
