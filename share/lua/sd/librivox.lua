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
    return { title = 'librivox' }
end

-- Transform a duration 'mm:ss' or 'hh::mm::ss' into an integer
function string_2_duration(str)
    local index = string.find( str, ':' )
    if( index == nil ) then return str
    else
        if( index == 1 ) then
            return string.sub( str, 2 )
        end
        local index2 = string.find( str, ':', index + 1 )
        if( index2 == nil ) then
            return string.sub( str, 0, index - 1 ) * 60 + string.sub( str, index + 1 )
        else
            return string.sub( str, 0, index - 1 ) * 3600 + string.sub( str, index + 1, index2 - 1 ) * 60 + string.sub( str, index2 + 1 )
        end
    end
end

function main()
    local podcast = simplexml.parse_url( 'http://librivox.org/podcast.xml' )
    simplexml.add_name_maps( podcast )

    local channel = podcast.children_map['channel'][1]
    local arturl = ''
    local books = {}

    for _, item in ipairs( channel.children ) do
        if( item.name == 'item' ) then
            simplexml.add_name_maps( item )

            -- If the book title is unknown, create a node for it in the sd
            local book_title = item.children_map['itunes:subtitle'][1].children[1]
            if( books[book_title] == nil ) then
                books[book_title] = vlc.sd.add_node( { title = book_title,
                                                       arturl = arturl } )
            end

            -- Add the new chapter to the book
            books[book_title]:add_subitem( { path = item.children_map['link'][1].children[1],
                                             title = item.children_map['title'][1].children[1],
                                             album = item.children_map['itunes:subtitle'][1].children[1],
                                             artist = item.children_map['itunes:author'][1].children[1],
                                             duration = string_2_duration( item.children_map['itunes:duration'][1].children[1] ),
                                             arturl = arturl } )

        elseif( item.name == 'itunes:image' ) then
            arturl = item.attributes['href']
        end
    end
end

