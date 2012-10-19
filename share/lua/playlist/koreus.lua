--[[

 Copyright © 2009 the VideoLAN team

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
    if vlc.access ~= "http" then
        return false
    end
	koreus_site = string.match( vlc.path, "koreus" )
    if not koreus_site then
        return false
    end
    return (  string.match( vlc.path, "video" )  ) -- http://www.koreus.com/video/pouet.html
end

-- Parse function.
function parse()
	while true do
		line = vlc.readline()
		if not line then break end
		if string.match( line, "<meta name=\"title\"" ) then
			_,_,name = string.find( line, "content=\"(.-)\"" )
			name = vlc.strings.resolve_xml_special_chars( name )
		end
		if string.match( line, "<meta name=\"description\"" ) then
			_,_,description = string.find( line, "content=\"(.-)\"" )
            if (description ~= nil) then
                description = vlc.strings.resolve_xml_special_chars( description )
            end
		end
		if string.match( line, "<meta name=\"author\"" ) then
			_,_,artist = string.find( line, "content=\"(.-)\"" )
			artist = vlc.strings.resolve_xml_special_chars( artist )
		end
		if string.match( line, "link rel=\"image_src\"" ) then
			_,_,arturl = string.find( line, "href=\"(.-)\"" )
		end

        vid_url = string.match( line, '(http://embed%.koreus%.com/%d+/%d+/[%w-]*%.mp4)' )
		if vid_url then
			return { { path = vid_url; name = name; description = description; artist = artist; arturl = arturl } }
		end
	end
    return {}
end
