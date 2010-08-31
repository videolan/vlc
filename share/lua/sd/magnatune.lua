--[[
 $Id$

 Copyright © 2010 VideoLAN and AUTHORS

 Authors: Rémi Duraffort  <ivoire at videolan dot org>

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

require "simplexml"

function descriptor()
    return { title="Magnatune" }
end

function main()
    add_top_albums()
end

function add_top_albums()
    local fd = vlc.stream( "http://magnatune.com/genres/m3u/ranked_all.xspf" )
    if not fd then return end
    local file = ''
    local line = ''
    while line ~= nil do
        line = fd:readline()
        if line ~= nil then
            -- repair the XML stream if needed
            file = file .. string.gsub(line, ' & ', ' &amp; ')
        end
    end

    local node = vlc.sd.add_node( {title = "Most popular albums"} )

    local tree = simplexml.parse_string( file )
    simplexml.add_name_maps( tree )
    local track_list = tree.children_map['trackList'][1]
    for _, track in ipairs( track_list.children ) do
        simplexml.add_name_maps( track )
        local track_item = node:add_subitem(
                { path     = string.gsub(track.children_map['location'][1].children[1], ' ', '%%20'),
                  title    = track.children_map['annotation'][1].children[1],
                  arturl   = track.children_map['image'][1].children[1] })
    end
end

