--[[
 $Id$

 Copyright © 2010 VideoLAN and AUTHORS

 Authors: Fabio Ritrovato <sephiroth87 at videolan dot org>

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
    return { title="Free Music Charts" }
end

function main()
    local tree = simplexml.parse_url("http://www.archive.org/download/freemusiccharts.songs/fmc.xml")
    for _, show_node in ipairs( tree.children ) do
        simplexml.add_name_maps( show_node )
        local node = vlc.sd.add_node( {title=show_node.children_map["description"][1].children[1]} )
        if tonumber( show_node.children_map["songcount"][1].children[1] ) > 0 then
            local songs_node = node:add_node( {title="Songs"} )
            for _, song_node in ipairs( show_node.children_map["songs"][1].children ) do
                _, _, artist, title = string.find( song_node.children_map["name"][1].children[1], "(.+)%s*-%s*(.+)" )
                local rank = song_node.children_map["rank"][1].children[1]
                if rank ~= nil then
                    rank = "Rank: " .. rank
                else
                    rank = "Rank: N/A"
                end
                local votes = song_node.children_map["votes"][1].children[1]
                if votes ~= nil then
                    votes = "Votes: " .. votes
                else
                    votes = "Votes: N/A"
                end
                songs_node:add_subitem( {path=song_node.children_map["url"][1].children[1],title=title,artist=artist,description=rank .. ", " .. votes} )
            end
        end
        node:add_subitem( {title=show_node.children_map["date"][1].children[1] .. " MP3 Podcast",path=show_node.children_map["podcastmp3"][1].children[1]} )
        node:add_subitem( {title=show_node.children_map["date"][1].children[1] .. " OGG Podcast",path=show_node.children_map["podcastogg"][1].children[1]} )
    end
end
