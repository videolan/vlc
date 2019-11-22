--[[
 $Id$

 Copyright © 2016, 2019 the VideoLAN team

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

-- Set to "mp3", "ogg", "flac" or "wav"
local fmt = "mp3"

-- Probe function.
function probe()
    return ( vlc.access == "http" or vlc.access == "https" )
        and ( string.match( vlc.path, "^old%.vocaroo%.com/i/" )
            or string.match( vlc.path, "^beta%.vocaroo%.com/." )
            or string.match( vlc.path, "^vocaroo%.com/." ) )
end

-- Parse function.
function parse()
    -- At the moment, a new/beta platform coexists with the old one:
    -- classic URLs for old media are redirected to the old platform,
    -- while new media seems accessible only through the new platform.

    -- With either platform, HTML pages contain no metadata and are not
    -- worth parsing.

    if string.match( vlc.path, "^old%.vocaroo%.com/" ) then -- Old platform
        local id = string.match( vlc.path, "vocaroo%.com/i/([^?]*)" )
        local path = vlc.access.."://old.vocaroo.com/media_command.php?media="..id.."&command=download_"..fmt
        return { { path = path } }
    else -- New/beta platform
        local id = string.match( vlc.path, "vocaroo%.com/([^?]+)" )
        local path = vlc.access.."://media.vocaroo.com/mp3/"..id
        return { { path = path } }
    end
end

