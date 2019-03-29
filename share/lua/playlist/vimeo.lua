--[[

 Copyright © 2009-2019 the VideoLAN team

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
    return ( vlc.access == "http" or vlc.access == "https" )
        and ( string.match( vlc.path, "^vimeo%.com/%d+" )
              or string.match( vlc.path, "^vimeo%.com/channels/.-/%d+" )
              or string.match( vlc.path, "^player%.vimeo%.com/" ) )
end

-- Parse function.
function parse()
    if string.match( vlc.path, "^player%.vimeo%.com/" ) then
        -- The /config API will return the data on a single line.
        -- Otherwise, search the web page for the config.
        local config = vlc.readline()
        while true do
            local line = vlc.readline()
            if not line then break end
            if string.match( line, "var config = {" ) then
                config = line
                break
            end
        end

        local prefres = vlc.var.inherit(nil, "preferred-resolution")
        local bestres = nil

        for stream in string.gmatch( config, "{([^}]*\"profile\":[^}]*)}" ) do
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

        local name = string.match( config, "\"title\":\"(.-)\"" )
        local artist = string.match( config, "\"owner\":{[^}]-\"name\":\"(.-)\"" )
        local arturl = string.match( config, "\"thumbs\":{\"[^\"]+\":\"(.-)\"" )
        local duration = string.match( config, "\"duration\":(%d+)[,}]" )

        return { { path = path; name = name; artist = artist; arturl = arturl; duration = duration } }

    else -- Video web page
        local api
        while true do
            local line = vlc.readline()
            if not line then break end

            if string.match( line, "clip_page_config = {" ) then
                api = string.match( line, '"config_url":"(.-)"' )
                if api then
                    api = string.gsub( api, "\\/", "/" )
                    break
                end
            end
        end

        if not api then
            vlc.msg.warn( "Couldn't extract vimeo API URL, falling back to preprogrammed URL pattern" )

            local path = string.gsub( vlc.path, "^vimeo%.com/channels/.-/(%d+)", "/%1" )
            local video_id = string.match( path, "/(%d+)" )

            api = vlc.access.."://player.vimeo.com/video/"..video_id.."/config"
        end

        return { { path = api } }
    end
end
