/*****************************************************************************
 * parser_context.hpp
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: parser_context.hpp,v 1.1 2004/01/03 23:31:33 asmax Exp $
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef PARSER_CONTEXT_HPP
#define PARSER_CONTEXT_HPP

#include <vlc/intf.h>
#include "builder_data.hpp"
#include <list>


/// Context for the FLEX parser
class ParserContext
{
    public:
        ParserContext( intf_thread_t *pIntf ):
            m_pIntf( pIntf ), m_xOffset( 0 ), m_yOffset( 0 ) {}

        intf_thread_t *m_pIntf;

        /// Container for mapping data from the XML
        BuilderData m_data;

        /// Current IDs
        string m_curWindowId;
        string m_curLayoutId;
        string m_curListId;

        /// Current offset of the controls
        int m_xOffset, m_yOffset;
        list<int> m_xOffsetList, m_yOffsetList;

        /// Layer of the current control in the layout
        int m_curLayer;
};

#endif
