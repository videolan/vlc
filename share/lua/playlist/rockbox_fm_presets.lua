--[[
 $Id$

 Copyright © 2009, 2016 the VideoLAN team

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

-- Parser script from Rockbox FM radio presets
-- See http://www.rockbox.org/wiki/FmPresets

local MRL_base = "v4l2c:///dev/radio0:tuner-frequency="

function probe()
	if not string.match( vlc.path, "%.[fF][mM][rR]$" ) then return false end
	local line = vlc.peek(256)
	local freq = tonumber(string.match( line, "^[^%d]?[^%d]?[^%d]?[^%d]?(%d+):" )) -- handle BOM
	return freq and freq > 80000000 and freq < 110000000
end

function parse()
	local p = {}
	while true do
		local line = vlc.readline()
		if not line then break end
		local freq, name = string.match( line, "(%d+):(.*)" )
                if freq then
			table.insert( p, { path = MRL_base..freq, name = name } )
		end
	end
	return p
end
