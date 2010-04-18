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
    fd = vlc.stream( "http://trailers.apple.com/trailers/iphone/home/feeds/just_added.json" )
    if not fd then return nil end
    options = {"http-user-agent='iPhone'"}
    while true
    do
         line = fd:readline()
         if not line then break end
         if string.match( line, "title" ) then
            title = vlc.strings.resolve_xml_special_chars( find( line, "title\":\"(.-)\""))
            art = find( line, "poster\":\"(.-)\"")
            url = find( line, "url\":\"(.-)\"")
            url = string.gsub( url, "trailers/","trailers/iphone/")
            url = "http://trailers.apple.com"..url.."trailer/"
            vlc.sd.add_item( { path = url, name=title, title=title, options=options, arturl=art})
         end
    end
end
