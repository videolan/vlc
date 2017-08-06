--[[
    Translate Daily Motion video webpages URLs to the corresponding
    FLV URL.

 $Id$

 Copyright © 2007-2016 the VideoLAN team

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
        and string.match( vlc.path, "^www%.dailymotion%.com/video/" )
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
		if string.match( line, "<meta name=\"description\"" ) then
			_,_,description = string.find( line, "content=\"(.-)\"" )
            if (description ~= nil) then
                description = vlc.strings.resolve_xml_special_chars( description )
            end
		end
		if string.match( line, "<link rel=\"thumbnail\" type=\"image/jpeg\"" ) then
			_,_,arturl = string.find( line, "href=\"(.-)\"" )
		end

        if string.match( line, "var __PLAYER_CONFIG__ = {" ) then
            artist = string.match( line, '"username":"([^"]+)"' )

            local streams = string.match( line, "\"qualities\":{(.-%])}" )
            if streams then
                local prefres = vlc.var.inherit(nil, "preferred-resolution")
                local file = nil
                local live = nil
                for height,stream in string.gmatch( streams, "\"(%w+)\":%[(.-)%]" ) do
                    -- Apparently formats are listed in increasing quality
                    -- order, so we take the first, lowest quality as
                    -- fallback, then pick the last one that matches.
                    if string.match( height, "^(%d+)$" ) and ( ( not file ) or prefres < 0 or tonumber( height ) <= prefres ) then
                        local f = string.match( stream, '"type":"video\\/[^"]+","url":"([^"]+)"' )
                        if f then
                            file = f
                        end
                    end
                    if not live then
                        live = string.match( stream, '"type":"application\\/x%-mpegURL","url":"([^"]+)"' )
                    end
                end

                -- Pick live streaming only as a fallback
                path = file or live
                if path then
                    path = string.gsub( path, "\\/", "/")
                end
            end
        end
    end

    if not path then
        vlc.msg.err("Couldn't extract the video URL from dailymotion")
        return { }
    end

    return { { path = path; name = name; description = description; arturl = arturl; artist = artist } }
end
