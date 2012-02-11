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

function descriptor()
    return { title="Channels.com" }
end

function search( string )
    -- Do the query
    query = string.gsub( string, ' ', '+' )
    local feed = simplexml.parse_url( "http://www.metachannels.com/api/search?apikey=54868d5d73af69d6afa12d55db6f3d18735baa7d&searchTerms=" .. query )
    local channel = feed.children[1]

    -- List all answers
    local node = vlc.sd.add_node( { path = "", title = string } )
    for _,item in ipairs( channel.children ) do
        if( item.name == 'item' ) then
            simplexml.add_name_maps( item )
            local url = vlc.strings.resolve_xml_special_chars( item.children_map['link'][1].children[1] )
            local title = vlc.strings.resolve_xml_special_chars( item.children_map['title'][1].children[1] )
            local arturl = nil
            if item.children_map['media:thumbnail'] ~= nil then
                arturl = vlc.strings.resolve_xml_special_chars( item.children_map['media:thumbnail'][1].attributes['url'] )
                if( arturl == '/images/thumb_channel_default.jpg' ) then
                    arturl = 'http://www.metachannels.com/images/thumb_channel_default.jpg'
                end
            end
            node:add_subitem( { path = url, title = title, arturl = arturl } )
        end
    end
end

function main()
    -- get the primary feed and parse the <channel> tag
    local feed = simplexml.parse_url( "http://metachannels.com/meta_channels?device=vlc&lang=en,es,fr,de,it,other&format=rss&adult_ok=y" )
    local channel = feed.children[1]

    -- list all children that are items
    for _,item in ipairs( channel.children ) do
        if( item.name == 'item' ) then
            simplexml.add_name_maps( item )
            local url = vlc.strings.resolve_xml_special_chars( item.children_map['link'][1].children[1] )
            local title = vlc.strings.resolve_xml_special_chars( item.children_map['title'][1].children[1] )
            local arturl = nil
            if item.children_map['image'] ~= nil then
                arturl = vlc.strings.resolve_xml_special_chars( item.children_map['image'][1].children_map['url'][1].children[1] )
            end
            local node = vlc.sd.add_item( { path = url,
                                            title = title,
                                            arturl = arturl } )
        end
    end
end

