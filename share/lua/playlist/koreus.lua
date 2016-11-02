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
    local path = vlc.path:gsub("^www%.", "")
    return ( vlc.access == "http" or vlc.access == "https" )
        and string.match( path, "^koreus%.com/video/.+" )
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

        if string.match( line, "<meta property=\"og:description\"" ) then
            _,_,description = string.find( line, "content=\"(.-)\"" )
            if (description ~= nil) then
                description = vlc.strings.resolve_xml_special_chars( description )
            end
        end
        if string.match( line, "<span id=\"spoil\" style=\"display:none\">" ) then
            _,_,desc_spoil = string.find( line, "<span id=\"spoil\" style=\"display:none\">(.-)</span>" )
            desc_spoil = vlc.strings.resolve_xml_special_chars( desc_spoil )
            description = description .. "\n\r" .. desc_spoil
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
            path_url = vid_url
        end

        vid_url_hd = string.match( line, '(http://embed%.koreus%.com/%d+/%d+/[%w-]*%-hd%.mp4)' )
        if vid_url_hd then
            path_url_hd = vid_url_hd
        end

        vid_url_webm = string.match( line, '(http://embed%.koreus%.com/%d+/%d+/[%w-]*%.webm)' )
        if vid_url_webm then
            path_url_webm = vid_url_webm
        end

        vid_url_flv = string.match( line, '(http://embed%.koreus%.com/%d+/%d+/[%w-]*%.flv)' )
        if vid_ulr_flv then
            path_url_flv = vid_url_flv
        end

    end

    if path_url_hd then
        if vlc.access == 'https' then path_url_hd = path_url_hd:gsub('http','https') end
        return { { path = path_url_hd; name = name; description = description; artist = artist; arturl = arturl } }
    elseif path_url then
        if vlc.access == 'https' then path_url = path_url:gsub('http','https') end
        return { { path = path_url; name = name; description = description; artist = artist; arturl = arturl } }
    elseif path_url_webm then
        if vlc.access == 'https' then path_url_webm = path_url_webm:gsub('http','https') end
        return { { path = path_url_webm; name = name; description = description; artist = artist; arturl = arturl } }
    elseif path_url_flv then
        if vlc.access == 'https' then path_url_flv = path_url_flv:gsub('http','https') end
        return { { path = path_url_flv; name = name; description = description; artist = artist; arturl = arturl } }
    else
        return {}
    end
end
