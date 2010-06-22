--[[
 $Id$

 Copyright © 2010 VideoLAN and AUTHORS

 Authors: Ilkka Ollakka <ileoo at videolan dot org >

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
--]]

function descriptor()
    return { title="Katsomo.fi"}
end

function find( haystack, needle )
    local _,_,r = string.find( haystack, needle )
    return r
end

function main()
    vlc.sd.add_item( {title="Uutiset ja fakta", path="http://www.katsomo.fi/?treeId=1"} )
    vlc.sd.add_item( {title="Urheilu", path="http://www.katsomo.fi/?treeId=2000021"} )
    vlc.sd.add_item( {title="Viihde ja sarjat", path="http://www.katsomo.fi/?treeId=3"} )
    vlc.sd.add_item( {title="Lifestyle", path="http://www.katsomo.fi/?treeId=8"} )
end
