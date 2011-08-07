--[[
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

-- Helper function to get a parameter's value in a URL
function get_url_param( url, name )
    local _, _, res = string.find( url, "[&?]"..name.."=([^&]*)" )
    return res
end

function get_arturl( path, video_id )
    if string.match( vlc.path, "iurl=" ) then
        return vlc.strings( get_url_param( vlc.path, "iurl" ) )
    end
    if not arturl then
        return "http://img.youtube.com/vi/"..video_id.."/default.jpg"
    end
end

-- Probe function.
function probe()
    if vlc.access ~= "http" and vlc.access ~= "https" then
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
        -- fmt is the format of the video
        -- (cf. http://en.wikipedia.org/wiki/YouTube#Quality_and_codecs)
        fmt = get_url_param( vlc.path, "fmt" )
        while true do
            -- Try to find the video's title
            line = vlc.readline()
            if not line then break end
            if string.match( line, "<meta name=\"title\"" ) then
                _,_,name = string.find( line, "content=\"(.-)\"" )
                name = vlc.strings.resolve_xml_special_chars( name )
                name = vlc.strings.resolve_xml_special_chars( name )
            end
            if string.match( line, "<meta name=\"description\"" ) then
               -- Don't ask me why they double encode ...
                _,_,description = string.find( line, "content=\"(.-)\"" )
                description = vlc.strings.resolve_xml_special_chars( description )
                description = vlc.strings.resolve_xml_special_chars( description )
            end
            if string.match( line, "<meta property=\"og:image\"" ) then
                _,_,arturl = string.find( line, "content=\"(.-)\"" )
            end
            if string.match( line, " rel=\"author\"" ) then
                _,_,artist = string.find( line, "href=\"/user/([^\"]*)\"" )
            end
            -- JSON parameters, also formerly known as "swfConfig",
            -- "SWF_ARGS", "swfArgs" ...
            if string.match( line, "PLAYER_CONFIG" ) then
                url_map = string.match( line, "\"url_encoded_fmt_stream_map\": \"(.-)\"" )
                if url_map then
                    -- FIXME: do this properly
                    url_map = string.gsub( url_map, "\\u0026", "&" )
                    for url,itag in string.gmatch( url_map, "url=([^&,]+).-&itag=(%d+)" ) do
                        -- Apparently formats are listed in quality order,
                        -- so we can afford to simply take the first one
                        if not fmt or tonumber( itag ) == tonumber( fmt ) then
                            url = vlc.strings.decode_uri( url )
                            path = url
                            break
                        end
                    end
                end
            -- There is also another version of the parameters, encoded
            -- differently, as an HTML attribute of an <object> or <embed>
            -- tag; but we don't need it now
            end
        end

        if not arturl then
            video_id = get_url_param( vlc.path, "v" )
            arturl = get_arturl( vlc.path, video_id )
        end

        if not path then
            vlc.msg.err( "Couldn't extract youtube video URL, please check for updates to this script" )
            return { }
        end
        return { { path = path; name = name; description = description; artist = artist; arturl = arturl } }
    else -- This is the flash player's URL
        if string.match( vlc.path, "title=" ) then
            name = vlc.strings.decode_uri(get_url_param( vlc.path, "title" ))
        end
        video_id = get_url_param( vlc.path, "video_id" )
        arturl = get_arturl( vlc.path, video_id )
        fmt = get_url_param( vlc.path, "fmt" )
        if fmt then
            format = "&fmt=" .. fmt
        else
            format = ""
        end
        if not string.match( vlc.path, "t=" ) then
            -- This sucks, we're missing "t" which is now mandatory. Let's
            -- try using another url
            return { { path = "http://www.youtube.com/v/"..video_id; name = name; arturl = arturl } }
        end
        return { { path = "http://www.youtube.com/get_video.php?video_id="..video_id.."&t="..get_url_param( vlc.path, "t" )..format; name = name; arturl = arturl } }
    end
end
