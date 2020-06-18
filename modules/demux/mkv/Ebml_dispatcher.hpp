/*****************************************************************************
 * Ebml_dispatcher.hpp : matroska demuxer
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
#ifndef VLC_MKV_EBML_DISPATCHER_HPP_
#define VLC_MKV_EBML_DISPATCHER_HPP_

#include "dispatcher.hpp"

#include "ebml/EbmlElement.h"
#include "ebml/EbmlId.h"

#include <vlc_threads.h>

#include <algorithm>
#include <typeinfo>
#include <vector>

namespace {
  struct EbmlProcessorEntry {
    typedef void (*EbmlProcessor) (EbmlElement*, void*);

    EbmlId         const* p_ebmlid;
    EbmlProcessor         callback;

    EbmlProcessorEntry (EbmlId const& id, EbmlProcessor cb)
      : p_ebmlid (&id), callback (cb)
    { }

  };

  bool operator<( EbmlProcessorEntry const& lhs, EbmlProcessorEntry const& rhs )
  {
      EbmlId const& lid = *lhs.p_ebmlid;
      EbmlId const& rid = *rhs.p_ebmlid;

      return lid.GetLength() < rid.GetLength() || (
        !( rid.GetLength() < lid.GetLength() ) && lid.GetValue() < rid.GetValue()
      );
  }

  class EbmlTypeDispatcher : public Dispatcher<EbmlTypeDispatcher, EbmlProcessorEntry::EbmlProcessor> {
    protected:
      typedef std::vector<EbmlProcessorEntry> ProcessorContainer;

    public:
      void insert (EbmlProcessorEntry const& data) {
        _processors.push_back (data);
      }

      void on_create () {
        std::sort (_processors.begin(), _processors.end());
      }

      bool send (EbmlElement * const& element, void* payload) const
      {
        if ( element == nullptr )
            return false;

        EbmlProcessorEntry eb = EbmlProcessorEntry (
          static_cast<EbmlId const&> (*element), NULL
        );

        // --------------------------------------------------------------
        // Find the appropriate callback for the received EbmlElement
        // --------------------------------------------------------------

        ProcessorContainer::const_iterator cit_end = _processors.end();
        ProcessorContainer::const_iterator cit     = std::lower_bound (
            _processors.begin(), cit_end, eb
        );

        /* Check that the processor is valid and unique. */
        if (cit != cit_end &&
            cit->p_ebmlid == eb.p_ebmlid &&
            (*cit->p_ebmlid == *eb.p_ebmlid))
        {
            cit->callback (element, payload);
            return true;
        }

        if (_default_handler == NULL)
            return false;

        _default_handler (element, payload);
        return true;
      }

    public:
      ProcessorContainer _processors;
  };

} /* end-of-namespace */

#define EBML_ELEMENT_CASE_DEF(EbmlType_, ClassName_, VariableName_, InitializationExpr_) \
    MKV_SWITCH_CASE_DEFINITION( ClassName_, EbmlType_, EbmlElement*, VariableName_, vars,          \
      InitializationExpr_, static_cast<EbmlType_&> (*data)                            \
    )

#define E_CASE(EbmlType_, VariableName_)            \
    EBML_ELEMENT_CASE_DEF(EbmlType_, EbmlType_, VariableName_, \
      (dispatcher.insert( EbmlProcessorEntry( EbmlType_ ::ClassInfos.ClassId(), &EbmlType_ ## _callback) ) ) \
    )

#define E_CASE_DEFAULT(VariableName_)                    \
    EBML_ELEMENT_CASE_DEF(EbmlElement, ebml_default, VariableName_, \
      dispatcher.set_default_handler (&ebml_default_callback)    \
    )

#endif
