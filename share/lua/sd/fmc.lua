--SD_Description=Free Music Charts
--[[
 $Id$

 Copyright © 2010 the VideoLAN team
 
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

function main()
    local tree = simplexml.parse_url("http://www.archive.org/download/freemusiccharts.songs/fmc.xml")
    for i = 1, table.getn(tree.children) do
        simplexml.add_name_maps( tree.children[i] )
        local node = vlc.sd.add_node( {title=tree.children[i].children_map["description"][1].children[1]} )
        if tonumber( tree.children[i].children_map["songcount"][1].children[1] ) > 0 then
            local songs_node = node:add_node( {title="Songs"} )
            for j = 1, table.getn( tree.children[i].children_map["songs"][1].children ) do
                _, _, artist, title = string.find( tree.children[i].children_map["songs"][1].children[j].children_map["name"][1].children[1], "(.+)%s*-%s*(.+)" )
                local rank = tree.children[i].children_map["songs"][1].children[j].children_map["rank"][1].children[1]
                if rank ~= nil then
                    rank = "Rank: " .. rank
                else
                    rank = "Rank: N/A"
                end
                local votes = tree.children[i].children_map["songs"][1].children[j].children_map["votes"][1].children[1]
                if votes ~= nil then
                    votes = "Votes: " .. rank
                else
                    votes = "Votes: N/A"
                end
                songs_node:add_subitem( {url=tree.children[i].children_map["songs"][1].children[j].children_map["url"][1].children[1],title=title,artist=artist,description=rank .. ", " .. votes} )
            end
        end
        node:add_subitem( {title=tree.children[i].children_map["date"][1].children[1] .. " MP3 Podcast",url=tree.children[i].children_map["podcastmp3"][1].children[1]} )
        node:add_subitem( {title=tree.children[i].children_map["date"][1].children[1] .. " OGG Podcast",url=tree.children[i].children_map["podcastogg"][1].children[1]} )
    end
end
