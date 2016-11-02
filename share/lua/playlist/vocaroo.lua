--[[
 $Id$

 Copyright © 2016 the VideoLAN team

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
        and string.match( vlc.path, "^vocaroo%.com/i/" )
end

-- Parse function.
function parse()
    -- The HTML page contains no metadata and is not worth parsing
    local id = string.match( vlc.path, "vocaroo%.com/i/([^?]*)" )
    local path = vlc.access.."://vocaroo.com/media_command.php?media="..id.."&command=download_"..fmt
    return { { path = path } }
end

