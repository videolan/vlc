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

-- Probe function.
function probe()
    local path = vlc.path
    path = path:gsub("^www%.", "")
    return ( vlc.access == "http" or vlc.access == "https" )
        and ( string.match( path, "^vimeo%.com/%d+$" )
              or string.match( path, "^vimeo%.com/channels/(.-)/%d+$" )
              or string.match( path, "^player%.vimeo%.com/" ) )
        -- do not match other addresses,
        -- else we'll also try to decode the actual video url
end

-- Parse function.
function parse()
    if not string.match( vlc.path, "player%.vimeo%.com" ) then -- Web page URL
        while true do
            local line = vlc.readline()
            if not line then break end

            -- Get the appropriate ubiquitous meta tag
            -- <meta name="twitter:player" content="https://player.vimeo.com/video/123456789">
            local meta = string.match( line, "(<meta[^>]- name=\"twitter:player\"[^>]->)" )
            if meta then
                local path = string.match( meta, " content=\"(.-)\"" )
                if path then
                    path = vlc.strings.resolve_xml_special_chars( path )
                    return { { path = path } }
                end
            end
        end

        vlc.msg.err( "Couldn't extract vimeo video URL, please check for updates to this script" )
        return { }

    else -- API URL

        local prefres = vlc.var.inherit(nil, "preferred-resolution")
        local bestres = nil
        local line = vlc.readline() -- data is on one line only

        for stream in string.gmatch( line, "{([^}]*\"profile\":[^}]*)}" ) do
            local url = string.match( stream, "\"url\":\"(.-)\"" )
            if url then
                -- Apparently the different formats available are listed
                -- in uncertain order of quality, so compare with what
                -- we have so far.
                local height = string.match( stream, "\"height\":(%d+)" )
                height = tonumber( height )

                -- Better than nothing
                if not path or ( height and ( not bestres
            -- Better quality within limits
            or ( ( prefres < 0 or height <= prefres ) and height > bestres )
            -- Lower quality more suited to limits
            or ( prefres > -1 and bestres > prefres and height < bestres )
                ) ) then
                    path = url
                    bestres = height
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
