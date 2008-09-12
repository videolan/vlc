--[[
 $Id$

 Copyright © 2007 the VideoLAN team

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

function get_url_param( url, name )
    local _,_,ret = string.find( url, "[&?]"..name.."=([^&]*)" )
    return ret
end

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "video.google.com" ) 
        and ( string.match( vlc.path, "videoplay" )
            or string.match( vlc.path, "videofeed" ) )
end

function get_arg( line, arg )
    return string.gsub( line, "^.*"..arg.."=\"(.-)\".*$", "%1" )
end

-- Parse function.
function parse()
    local docid = get_url_param( vlc.path, "docid" ) 
    if string.match( vlc.path, "videoplay" ) then
        return { { path = "http://video.google.com/videofeed?docid=" .. docid } }
    elseif string.match( vlc.path, "videofeed" ) then
        local path = nil
        local arturl
        local duration
        local name
        local description
        while true
        do
            local line = vlc.readline()
            if not line then break end
            if string.match( line, "media:content.*flv" )
            then
                local _,_,s = string.find( line, "<media:content(.-)/>" )
                path = vlc.strings.resolve_xml_special_chars(get_arg( s, "url" ))
                duration = get_arg( s, "duration" )
            end
            if string.match( line, "media:thumbnail" )
            then
                local _,_,s = string.find( line, "<media:thumbnail(.-)/>" )
                arturl = vlc.strings.resolve_xml_special_chars(get_arg( s, "url" ))
            end
            if string.match( line, "media:title" )
            then
                _,_,name = string.find( line, "<media:title>(.-)</media:title>" )
            end
            if string.match( line, "media:description" )
            then
                _,_,description = string.find( line, "<media:description>(.-)</media:description>" )
            end
        end
        return { { path = path; name = name; arturl = arturl; duration = duration; description = description } }
    end
end
