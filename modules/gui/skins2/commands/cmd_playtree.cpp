/*****************************************************************************
 * cmd_playtree.cpp
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 * $Id: cmd_playlist.cpp 10101 2005-03-02 16:47:31Z robux4 $
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
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

#include "cmd_playtree.hpp"
#include "../src/vlcproc.hpp"
#include "../utils/var_bool.hpp"

void CmdPlaytreeDel::execute()
{
    m_rTree.delSelected();
}

void CmdPlaytreeSort::execute()
{
    // TODO
}

void CmdPlaytreeNext::execute()
{
    // TODO
}

void CmdPlaytreePrevious::execute()
{
    // TODO
}

void CmdPlaytreeRandom::execute()
{
    // TODO
}

void CmdPlaytreeLoop::execute()
{
    // TODO
}

void CmdPlaytreeRepeat::execute()
{
    // TODO
}

void CmdPlaytreeLoad::execute()
{
    // TODO
}

void CmdPlaytreeSave::execute()
{
    // TODO
}
