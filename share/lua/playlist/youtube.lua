--[[
 $Id$

 Copyright © 2007-2009 the VideoLAN team

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
        -- fmt is the format of the video: 18 is HQ (mp4)
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
                _,_,description = vlc.strings.resolve_xml_special_chars(vlc.strings.resolve_xml_special_chars(string.find( line, "content=\"(.-)\"" )))
            end
            if string.match( line, "subscribe_to_user=" ) then
                _,_,artist = string.find( line, "subscribe_to_user=([^&]*)" )
            end
            -- CURRENT: var swfConfig = { [a lot of stuff...], "video_id": "OHVvVmUNBFc", "sk": "WswKuJzDBsdD6oG3IakCXgC", "t": "OEgsToPDskK3zO44y0QN8Fr5ZSAZwCQp", "plid": "AARGnwWMrmGkbpOxAAAA4AT4IAA"};
            -- OLD 1: var swfArgs = {hl:'en',BASE_YT_URL:'http://youtube.com/',video_id:'XPJ7d8dq0t8',l:'292',t:'OEgsToPDskLFdOYrrlDm3FQPoQBYaCP1',sk:'0gnr-AE6QZJEZmCMd3lq_AC'};
            -- OLD 2: var swfArgs = { "BASE_YT_URL": "http://youtube.com", "video_id": "OHVvVmUNBFc", "l": 88, "sk": "WswKuJzDBsdD6oG3IakCXgC", "t": "OEgsToPDskK3zO44y0QN8Fr5ZSAZwCQp", "plid": "AARGnwWMrmGkbpOxAAAA4AT4IAA", "tk": "mEL4E7PqHeaZp5OG19NQThHt9mXJU4PbRTOw6lz9osHi4Hixp7RE1w=="};
            -- OLD 3: 'SWF_ARGS': { [a lot of stuff...], "video_id": "OHVvVmUNBFc", "sk": "WswKuJzDBsdD6oG3IakCXgC", "t": "OEgsToPDskK3zO44y0QN8Fr5ZSAZwCQp", "plid": "AARGnwWMrmGkbpOxAAAA4AT4IAA"};
            if ( string.match( line, "PLAYER_CONFIG" ) or string.match( line, "swfConfig" ) or string.match( line, "SWF_ARGS" ) or string.match( line, "swfArgs" ) ) and string.match( line, "video_id" ) then
                if string.match( line, "BASE_YT_URL" ) then
                    _,_,base_yt_url = string.find( line, "\"BASE_YT_URL\": \"(.-)\"" )
                end
                _,_,t = string.find( line, "\"t\": \"(.-)\"" )
                -- vlc.msg.err( t )
                -- video_id = string.gsub( line, ".*&video_id:'([^']*)'.*", "%1" )
                fmt_url_map = string.match( line, "\"url_encoded_fmt_stream_map\": \"(.-)\"" )
                if fmt_url_map then
                    -- FIXME: do this properly
                    fmt_url_map = string.gsub( fmt_url_map, "\\u0026", "&" )
                    for url,itag in string.gmatch( fmt_url_map, "url=([^&,]+).-&itag=(%d+)" ) do
                        -- Apparently formats are listed in quality order,
                        -- so we can afford to simply take the first one
                        if not fmt or tonumber( itag ) == tonumber( fmt ) then
                            url = vlc.strings.decode_uri( url )
                            path = url
                            break
                        end
                    end
                end
            -- Also available on non-HTML5 pages: var swfHTML = (isIE) ? "<object [...]><param name=\"flashvars\" value=\"rv.2.thumbnailUrl=http%3A%2F%2Fi4.ytimg.com%2Fvi%2F3MLp7YNTznE%2Fdefault.jpg&rv.7.length_seconds=384 [...] &video_id=OHVvVmUNBFc [...] &t=OEgsToPDskK3zO44y0QN8Fr5ZSAZwCQp [...]
            elseif string.match( line, "swfHTML" ) and string.match( line, "video_id" ) then
                _,_,t = string.find( line, "&t=(.-)&" )
            -- Also available in HTML5 pages: videoPlayer.setAvailableFormat("http://v6.lscache4.c.youtube.com/videoplayback?ip=82.0.0.0&sparams=id%2Cexpire%2Cip%2Cipbits%2Citag%2Calgorithm%2Cburst%2Cfactor&algorithm=throttle-factor&itag=45&ipbits=8&burst=40&sver=3&expire=1275688800&key=yt1&signature=6ED860441298D1157FF3013A5D72727F25831F09.4C196BEA9F8F9B83CE678D79AD918B83D5E98B46&factor=1.25&id=7117715cf57d18d4", "video/webm; codecs=&quot;vp8.0, vorbis&quot;", "hd720");
            elseif string.match( line, "videoPlayer%.setAvailableFormat" ) then
                url,itag = string.match( line, "videoPlayer%.setAvailableFormat%(\"(.-itag=(%d+).-)\",.+%)" )
                if url then
                    -- For now, WebM formats are listed only in the HTML5
                    -- section, that is also only when HTML5 is enabled.
                    -- Format 45 is 720p, and 43 is lower resolution.
                    if tonumber( itag ) == 45  or ( tonumber( itag ) == 43 and not webm_path ) then
                        webm_path = url
                    end
                    -- Grab something if fmt_url_map failed
                    if not path and ( not fmt or tonumber( itag ) == tonumber( fmt ) ) then
                        path = url
                    end
                end
            end
        end

        if not video_id then
            video_id = get_url_param( vlc.path, "v" )
        end
        arturl = get_arturl( vlc.path, video_id )

        if not fmt then
            -- Prefer WebM formats if this is an &html5=True URL
            html5 = get_url_param( vlc.path, "html5" )
            if html5 == "True" and webm_path then
                path = webm_path
            end
        end

        if not path then
            if not base_yt_url then
                base_yt_url = "http://youtube.com/"
            end
            if fmt then
                format = "&fmt=" .. fmt
            else
                format = ""
            end

            if t then
                path = base_yt_url .. "get_video?video_id="..video_id.."&t="..t..format
            else
                -- This shouldn't happen ... but keep it as a backup.
                path = "http://www.youtube.com/v/"..video_id
            end
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
