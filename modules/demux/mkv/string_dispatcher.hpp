/*****************************************************************************
 * string_dispatcher.hpp : matroska demuxer
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
#ifndef VLC_MKV_STRING_DISPATCHER_HPP_
#define VLC_MKV_STRING_DISPATCHER_HPP_

#include "dispatcher.hpp"

#include <map>
#include <vector>
#include <string>
#include <cstring>
#include <sstream>

namespace {
  namespace detail {
    struct CStringCompare {
      bool operator () (char const * const& s1, char const * const& s2) const {
        return std::strcmp (s1, s2) < 0;
      }
    };
  }

  class StringDispatcher : public Dispatcher<StringDispatcher, void(*)(char const*, void*)> {
    public:
      typedef void(*Processor)(char const*, void*);

      typedef std::pair<char const *, Processor>                         ProcessorEntry;
      typedef std::map <char const *, Processor, detail::CStringCompare> ProcessorContainer;

      typedef std::vector<std::string>                      GlobParts;
      typedef std::vector<std::pair<GlobParts, Processor> > GlobContainer;

    public:
      void insert (ProcessorEntry const& data) {
        _processors.insert (data);
      }

      void insert_glob (ProcessorEntry const& data) {
        std::istringstream iss (data.first);
        std::vector<std::string> parts;
        std::string s1;

        for (std::string s1; std::getline (iss, s1, '*'); )
          parts.push_back (s1);

        iss.clear ();
        iss.unget ();

        if (iss.get () == '*')
          parts.push_back (std::string ());

        _glob_processors.push_back (std::make_pair (parts, data.second));
      }

      Processor find_glob_match (char const* const& haystack) const {
        GlobContainer::const_iterator glob_it     = _glob_processors.begin ();
        GlobContainer::const_iterator glob_it_end = _glob_processors.end ();

        for ( ; glob_it != glob_it_end; ++glob_it) {
          Processor const& callback   = glob_it->second;
          GlobParts const& parts      = glob_it->first;
          char      const* haystack_p = haystack;

          /* No parts? match empty haystack only */

          if (parts.size() == 0) {
            if (*haystack_p) continue;
            else             return callback;
          }

          /* make sure first part is located at the beginning of our haystack */
          {
            std::string const& first_part = parts.front ();

            if (strncmp( first_part.c_str(), haystack_p, first_part.size() ))
              continue;

            haystack_p += first_part.size();
          }
          /* locate every remaining part in the haystack */
          {
            GlobParts::const_iterator it  = parts.begin() + 1;
            GlobParts::const_iterator end = parts.end();

            for ( ; it != end; ++it) {
              haystack_p = strstr (haystack_p, it->c_str ());

              if (haystack_p == NULL)
                break;

              haystack_p += it->size ();
            }

            if ( haystack_p == NULL)             continue; // last search failed
            if (*haystack_p == '\0')      return callback; // end of parts + end of haystack? success.
            if (parts.back().size() == 0) return callback; // endof parts + glob at end? success
          }
        }

        return NULL;
      }

      bool send (char const* const& str, void* const& payload) const
      {
        ProcessorContainer::const_iterator cit     = _processors.begin ();
        ProcessorContainer::const_iterator cit_end = _processors.end ();

        if ((cit = _processors.find (str)) != cit_end) {
            cit->second (str, payload);
            return true;
        }

        if (Processor glob = find_glob_match (str)) {
          glob (str, payload);
          return true;
        }

        if (_default_handler != NULL) {
            _default_handler (str, payload);
            return true;
        }

        return false;
      }

    private:
      ProcessorContainer _processors;
      GlobContainer _glob_processors;
  };

} /* end-of-namespace */

#define STRD_T0KENPASTE_(a, b) a ## b
#define STRD_TOKENPASTE_(a, b) STRD_T0KENPASTE_(a, b)
#define STRD_UNIQUE_NAME_      STRD_TOKENPASTE_(StringProcessor_, __LINE__)

#define STRING_CASE_DEF(ClassName_, VariableName_, InitializationExpr_ )                    \
    MKV_SWITCH_CASE_DEFINITION(ClassName_, char const*, char const*, VariableName_, vars, InitializationExpr_, data)

#define S_CASE(Str_)                                                                         \
  STRING_CASE_DEF(STRD_UNIQUE_NAME_, /* ignored */,                                         \
    (dispatcher.insert( StringDispatcher::ProcessorEntry(Str_, STRD_TOKENPASTE_(STRD_UNIQUE_NAME_, _callback) ) )) \
  )

#define S_CASE_GLOB(Str_) \
  STRING_CASE_DEF(STRD_UNIQUE_NAME_, /* ignored */,                                         \
    (dispatcher.insert_glob( StringDispatcher::ProcessorEntry(Str_, STRD_TOKENPASTE_(STRD_UNIQUE_NAME_, _callback) ) )) \
  )

#define S_CASE_DEFAULT(VariableName_)                                                        \
  STRING_CASE_DEF(default_handler, VariableName_,                                           \
    dispatcher.set_default_handler (&default_handler_callback)                                      \
  )

#endif
