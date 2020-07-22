--[[
    Translate Dailymotion video webpages URLs to corresponding
    video stream URLs.

 Copyright © 2007-2020 the VideoLAN team

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
			name = string.gsub( name, " %- [^ ]+ [Dd]ailymotion$", "" )
		end
		if string.match( line, "<meta name=\"description\"" ) then
			_,_,description = string.find( line, "content=\"(.-)\"" )
            if (description ~= nil) then
                description = vlc.strings.resolve_xml_special_chars( description )
            end
		end
		if string.match( line, "<meta property=\"og:image\"" ) then
			arturl = string.match( line, "content=\"(.-)\"" )
		end
    end

    local video_id = string.match( vlc.path, "^www%.dailymotion%.com/video/([^/?#]+)" )
    if video_id then
        local metadata = vlc.stream( vlc.access.."://www.dailymotion.com/player/metadata/video/"..video_id )
        if metadata then
            local line = metadata:readline() -- data is on one line only

            -- TODO: fetch "title" and resolve \u escape sequences
            -- FIXME: use "screenname" instead and resolve \u escape sequences
            artist = string.match( line, '"username":"([^"]+)"' )

            local poster = string.match( line, '"poster_url":"([^"]+)"' )
            if poster then
                arturl = string.gsub( poster, "\\/", "/")
            end

            local streams = string.match( line, "\"qualities\":{(.-%])}" )
            if streams then
                -- Most of this has become unused, as in practice Dailymotion
                -- has currently stopped offering progressive download and
                -- been offering only adaptive streaming for a while now.
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
        vlc.msg.err("Couldn't extract dailymotion video URL, please check for updates to this script")
        return { }
    end

    return { { path = path; name = name; description = description; arturl = arturl; artist = artist } }
end
