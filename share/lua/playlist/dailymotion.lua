--[[
    Translate Daily Motion video webpages URLs to the corresponding
    FLV URL.

 $Id$

 Copyright © 2007-2011 the VideoLAN team

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
	if vlc.access ~= "http" and vlc.access ~= "https" then
		return false
	end

	return ( string.match( vlc.path, "www.dailymotion.com/video/" ) )
end

-- Parse function.
function parse()
	while true
	do
		line = vlc.readline()
		if not line then break end
		if string.match( line, "<meta property=\"og:title\"" ) then
			_,_,name = string.find( line, "content=\"(.-)\"" )
			name = vlc.strings.resolve_xml_special_chars( name )
		end
		if string.match( line, "<meta property=\"og:description\"" ) then
			_,_,description = string.find( line, "content=\"(.-)\"" )
			if (description ~= nil) then
				description = vlc.strings.resolve_xml_special_chars( description )
			end
		end
		if string.match( line, "<meta name=\"author\"" ) then
			_,_,artist = string.find( line, "content=\"(.-)\"" )
			artist = vlc.strings.resolve_xml_special_chars( artist )
		end
		if string.match( line, "<link rel=\"thumbnail\" type=\"image/jpeg\"" ) then
			_,_,arturl = string.find( line, "href=\"(.-)\"" )
		end
	end

	page_embed = string.gsub(vlc.path, "dailymotion.com/video/", "dailymotion.com/embed/video/")
	page_url = vlc.stream(vlc.access .. "://" .. page_embed )
	if not page_url then return nil end
	page = page_url:read( 90000 )

	m3u8_url = string.match( page, "https:\\/\\/www.dailymotion.com\\/cdn\\/manifest\\/video\\/[^\"]+" )
	m3u8_url = string.gsub( m3u8_url, "\\/", "/" )
	return { { path = m3u8_url; name = name; description = description; url = vlc.path; arturl = arturl; artist = artist} }
end
