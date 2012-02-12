--[[
 $Id$

 Copyright © 2010 VideoLAN and AUTHORS

 Authors: Rémi Duraffort <ivoire at videolan dot org>

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

function probe()
    return vlc.access == 'http' and string.match( vlc.path, 'metachannels.com' )
end

function parse()
    local webpage = ''
    while true do
        local line = vlc.readline()
        if line == nil then break end
        webpage = webpage .. line
    end

    local feed = simplexml.parse_string( webpage )
    local channel = feed.children[1]

    -- list all children that are items
    local tracks = {}
    for _,item in ipairs( channel.children ) do
        if( item.name == 'item' ) then
            simplexml.add_name_maps( item )
            local url = vlc.strings.resolve_xml_special_chars( item.children_map['link'][1].children[1] )
            local title = vlc.strings.resolve_xml_special_chars( item.children_map['title'][1].children[1] )
            local arturl = nil
            if item.children_map['media:thumbnail'] then
                arturl = vlc.strings.resolve_xml_special_chars( item.children_map['media:thumbnail'][1].attributes['url'] )
            end
            table.insert( tracks, { path = url,
                                    title = title,
                                    arturl = arturl,
                                    options = {':play-and-pause'} } )
        end
    end

    return tracks
end

