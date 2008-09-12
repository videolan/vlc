--[[
 $Id$

 Copyright © 2008 the VideoLAN team

 Authors: Antoine Cellerier <dionoea at videolan dot org>

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

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "jt.france2.fr/player/" )
end

-- Parse function.
function parse()
    p = {}
    while true do
        -- Try to find the video's title
        line = vlc.readline()
        if not line then break end
        if string.match( line, "class=\"editiondate\"" ) then
            _,_,editiondate = string.find( line, "<h1>(.-)</h1>" )
        end
        if string.match( line, "mms.*%.wmv" ) then
            _,_,video = string.find( line, "mms(.-%.wmv)" )
            video = "mmsh"..video
            table.insert( p, { path = video; name = editiondate } )
        end
        if string.match( line, "class=\"subjecttimer\"" ) then
            oldtime = time
            _,_,time = string.find( line, "href=\"(.-)\"" )
            if oldtime then
                table.insert( p, { path = video; name = name; duration = time - oldtime; options = { ':start-time='..tostring(oldtime); ':stop-time='..tostring(time) } } )
            end
            name = vlc.strings.resolve_xml_special_chars( string.gsub( line, "^.*>(.*)<..*$", "%1" ) )
        end
    end
    if oldtime then
        table.insert( p, { path = video; name = name; options = { ':start-time='..tostring(time) } } )
    end
    return p
end
