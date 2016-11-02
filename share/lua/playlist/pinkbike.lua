--[[
 $Id$

 Copyright © 2009 the VideoLAN team

 Authors: Konstantin Pavlov (thresh@videolan.org)

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
    local path = vlc.path:gsub("^www%.", "")
    return vlc.access == "http"
        and string.match( path, "^pinkbike%.com/video/%d+" )
end

-- Parse function.
function parse()
    p = {}

	if string.match ( vlc.path, "pinkbike.com/video/%d+" ) then
		while true do
			line = vlc.readline()
			if not line then break end
			-- Try to find video id
			if string.match( line, "video_src.+swf.id=(.*)\"") then
				_,_,videoid = string.find( line, "video_src.+swf.id=(.*)\"")
				catalog = math.floor( tonumber( videoid ) / 10000 )
			end
			-- Try to find the video's title
			if string.match( line, "<title>(.*)</title>" ) then
				_,_,name = string.find (line, "<title>(.*)</title>")
			end
			-- Try to find server which has our video
			if string.match( line, "<link rel=\"videothumbnail\" href=\"http://(.*)/vt/svt-") then
				_,_,server = string.find (line, '<link rel="videothumbnail" href="http://(.*)/vt/svt-' )
			end
			if string.match( line, "<link rel=\"videothumbnail\" href=\"(.*)\" type=\"image/jpeg\"") then
				_,_,arturl = string.find (line, '<link rel="videothumbnail" href="(.*)" type="image/jpeg"')
			end
		end

	end
	table.insert( p, { path = "http://" .. server .. "/vf/" .. catalog .. "/pbvid-" .. videoid .. ".flv"; name = name; arturl = arturl } )
	return p
end
