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
    return { title="Apple Trailers" }
end

function find( haystack, needle )
    local _,_,r = string.find( haystack, needle )
    return r
end

function main()
    fd = vlc.stream( "http://trailers.apple.com/trailers/home/feeds/just_hd.json" )
    if not fd then return nil end
    line = fd:readline()
    while line ~= nil
    do
         if string.match( line, "title" ) then 
            title = vlc.strings.resolve_xml_special_chars( find( line, "title\":\"(.-)\""))
            art = find( line, "poster\":\"(.-)\"")
            if string.match( art, "http://" ) then
            else
                art = "http://trailers.apple.com"..art
            end

            url = find( line, "location\":\"(.-)\"")
            node = vlc.sd.add_item( {title  = title,
                                     path   = "http://trailers.apple.com"..url.."includes/playlists/web.inc",
                                     arturl = art})

         end
         line = fd:readline()
    end
end
