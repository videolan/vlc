--[[

 Copyright © 2016, 2019-2020, 2022 the VideoLAN team

 Authors: Pierre Ynard

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
        and string.match( vlc.path, "^vocaroo%.com/." )
end

-- Parse function.
function parse()
    -- The HTML page contains no metadata and is not worth parsing
    local id = string.match( vlc.path, "^vocaroo%.com/([^?]+)" )

    -- Dispatch media to correct CDN server
    -- function Ic(e){return function(e){if(e.length){if(11==e.length)return ControlConfig.mediaMp3FileUrl;if(12==e.length&&"1"==e[0])return ControlConfig.mediaMp3FileUrl1;if(10==e.length)return ControlConfig.mediaMp3FileUrl1}return ControlConfig.mediaMp3FileUrl}(e)+e}
    local cdn = ( string.len( id ) == 10 or
                  ( string.len( id ) == 12 and string.match( id, "^1" ) ) )
        and "//media1.vocaroo.com/mp3/"
        or "//media.vocaroo.com/mp3/"

    local path = vlc.access..":"..cdn..id
    return { { path = path; options = { ":http-referrer="..vlc.access.."://"..vlc.path } } }
end

