--[[
 $Id$

 Copyright © 2009-2013 the VideoLAN team

 Authors: Konstantin Pavlov (thresh@videolan.org)
          François Revol (revol@free.fr)
          Pierre Ynard

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

function get_prefres()
    local prefres = -1
    if vlc.var and vlc.var.inherit then
        prefres = vlc.var.inherit(nil, "preferred-resolution")
        if prefres == nil then
            prefres = -1
        end
    end
    return prefres
end

-- Probe function.
function probe()
    return ( vlc.access == "http" or vlc.access == "https" )
        and ( string.match( vlc.path, "vimeo%.com/%d+$" )
              or string.match( vlc.path, "player%.vimeo%.com" ) )
        -- do not match other addresses,
        -- else we'll also try to decode the actual video url
end

-- Parse function.
function parse()
    if not string.match( vlc.path, "player%.vimeo%.com" ) then -- Web page URL
        while true do
            local line = vlc.readline()
            if not line then break end
            path = string.match( line, "data%-config%-url=\"(.-)\"" )
            if path then
                path = vlc.strings.resolve_xml_special_chars( path )
                return { { path = path } }
            end
        end

        vlc.msg.err( "Couldn't extract vimeo video URL, please check for updates to this script" )
        return { }

    else -- API URL

        local prefres = get_prefres()
        local line = vlc.readline() -- data is on one line only

        for stream in string.gmatch( line, "{([^}]*\"profile\":[^}]*)}" ) do
            local url = string.match( stream, "\"url\":\"(.-)\"" )
            if url then
                path = url
                if prefres < 0 then
                    break
                end
                local height = string.match( stream, "\"height\":(%d+)[,}]" )
                if not height or tonumber(height) <= prefres then
                    break
                end
            end
        end

        if not path then
            vlc.msg.err( "Couldn't extract vimeo video URL, please check for updates to this script" )
            return { }
        end

        local name = string.match( line, "\"title\":\"(.-)\"" )
        local artist = string.match( line, "\"owner\":{[^}]-\"name\":\"(.-)\"" )
        local arturl = string.match( line, "\"thumbs\":{\"[^\"]+\":\"(.-)\"" )
        local duration = string.match( line, "\"duration\":(%d+)[,}]" )

        return { { path = path; name = name; artist = artist; arturl = arturl; duration = duration } }
    end
end
