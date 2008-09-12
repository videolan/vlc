--[[
 $Id$

 Copyright © 2007 the VideoLAN team

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

-- Helper function to get a parameter's value in a URL
function get_url_param( url, name )
    local _, _, res = string.find( url, "[&?]"..name.."=([^&]*)" )
    return res
end

function get_arturl( path, video_id )
    if string.match( vlc.path, "iurl=" ) then
        return vlc.strings.decode_uri( get_url_param( vlc.path, "iurl" ) )
    end
    if not arturl then
        return "http://img.youtube.com/vi/"..video_id.."/default.jpg"
    end
end

-- Probe function.
function probe()
    if vlc.access ~= "http" then
        return false
    end
    youtube_site = string.match( string.sub( vlc.path, 1, 8 ), "youtube" )
    if not youtube_site then
        -- FIXME we should be using a builtin list of known youtube websites
        -- like "fr.youtube.com", "uk.youtube.com" etc..
        youtube_site = string.find( vlc.path, ".youtube.com" )
        if youtube_site == nil then
            return false
        end
    end
    return (  string.match( vlc.path, "watch%?v=" ) -- the html page
            or string.match( vlc.path, "watch_fullscreen%?video_id=" ) -- the fullscreen page
            or string.match( vlc.path, "p.swf" ) -- the (old?) player url
            or string.match( vlc.path, "jp.swf" ) -- the (new?) player url (as of 24/08/2007)
            or string.match( vlc.path, "player2.swf" ) ) -- another player url
end

-- Parse function.
function parse()
    if string.match( vlc.path, "watch%?v=" )
    then -- This is the HTML page's URL
        while true do
            -- Try to find the video's title
            line = vlc.readline()
            if not line then break end
            if string.match( line, "<meta name=\"title\"" ) then
                _,_,name = string.find( line, "content=\"(.-)\"" )
            end
            if string.match( line, "<meta name=\"description\"" ) then
               -- Don't ask me why they double encode ...
                _,_,description = vlc.strings.resolve_xml_special_chars(vlc.strings.resolve_xml_special_chars(string.find( line, "content=\"(.-)\"" )))
            end
            if string.match( line, "subscribe_to_user=" ) then
                _,_,artist = string.find( line, "subscribe_to_user=([^&]*)" )
            end
            -- OLD: var swfArgs = {hl:'en',BASE_YT_URL:'http://youtube.com/',video_id:'XPJ7d8dq0t8',l:'292',t:'OEgsToPDskLFdOYrrlDm3FQPoQBYaCP1',sk:'0gnr-AE6QZJEZmCMd3lq_AC'};
            -- NEW: var swfArgs = { "BASE_YT_URL": "http://youtube.com", "video_id": "OHVvVmUNBFc", "l": 88, "sk": "WswKuJzDBsdD6oG3IakCXgC", "t": "OEgsToPDskK3zO44y0QN8Fr5ZSAZwCQp", "plid": "AARGnwWMrmGkbpOxAAAA4AT4IAA", "tk": "mEL4E7PqHeaZp5OG19NQThHt9mXJU4PbRTOw6lz9osHi4Hixp7RE1w=="};
            if string.match( line, "swfArgs" ) and string.match( line, "video_id" ) then
                if string.match( line, "BASE_YT_URL" ) then
                    _,_,base_yt_url = string.find( line, "\"BASE_YT_URL\": \"(.-)\"" )
                end
                _,_,t = string.find( line, "\"t\": \"(.-)\"" )
                -- vlc.msg.err( t )
                -- video_id = string.gsub( line, ".*&video_id:'([^']*)'.*", "%1" )
            end
            if name and description and artist --[[and video_id]] then break end
        end
        if not video_id then
            video_id = get_url_param( vlc.path, "v" )
        end
        if not base_yt_url then
            base_yt_url = "http://youtube.com/"
        end
        arturl = get_arturl( vlc.path, video_id )
        if t then
            return { { path = base_yt_url .. "get_video?video_id="..video_id.."&t="..t; name = name; description = description; artist = artist; arturl = arturl } }
        else
            -- This shouldn't happen ... but keep it as a backup.
            return { { path = "http://www.youtube.com/v/"..video_id; name = name; description = description; artist = artist; arturl = arturl } }
        end
    else -- This is the flash player's URL
        if string.match( vlc.path, "title=" ) then
            name = get_url_param( vlc.path, "title" )
        end
        video_id = get_url_param( vlc.path, "video_id" )
        arturl = get_arturl( vlc.path, video_id )
        if not string.match( vlc.path, "t=" ) then
            -- This sucks, we're missing "t" which is now mandatory. Let's
            -- try using another url
            return { { path = "http://www.youtube.com/v/"..video_id; name = name; arturl = arturl } }
        end
        return { { path = "http://www.youtube.com/get_video.php?video_id="..video_id.."&t="..get_url_param( vlc.path, "t" ); name = name; arturl = arturl } }
    end
end
