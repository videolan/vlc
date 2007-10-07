-- $Id$

-- Helper function to get a parameter's value in a URL
function get_url_param( url, name )
    return string.gsub( url, "^.*[&?]"..name.."=([^&]*).*$", "%1" )
end

function get_arturl( path, video_id )
    if string.match( vlc.path, "iurl=" ) then
        return vlc.decode_uri( get_url_param( vlc.path, "iurl" ) )
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
                name = string.gsub( line, "^.*content=\"([^\"]*).*$", "%1" )
            end
            if string.match( line, "<meta name=\"description\"" ) then
                description = string.gsub( line, "^.*content=\"([^\"]*).*$", "%1" )
            end
            if string.match( line, "subscribe_to_user=" ) then
                artist = string.gsub( line, ".*subscribe_to_user=([^&]*).*", "%1" )
            end
            -- var swfArgs = {hl:'en',BASE_YT_URL:'http://youtube.com/',video_id:'XPJ7d8dq0t8',l:'292',t:'OEgsToPDskLFdOYrrlDm3FQPoQBYaCP1',sk:'0gnr-AE6QZJEZmCMd3lq_AC'};
            if string.match( line, "swfArgs" ) and string.match( line, "video_id" ) then
                if string.match( line, "BASE_YT_URL" ) then
                    base_yt_url = string.gsub( line, ".*BASE_YT_URL:'([^']*)'.*", "%1" )
                end
                t = string.gsub( line, ".*t:'([^']*)'.*", "%1" )
                -- vlc.msg_err( t )
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
        art_url = get_arturl( vlc.path, video_id )
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
        art_url = get_arturl( vlc.path, video_id )
        if not string.match( vlc.path, "t=" ) then
            -- This sucks, we're missing "t" which is now mandatory. Let's
            -- try using another url
            return { { path = "http://www.youtube.com/v/"..video_id; name = name; arturl = arturl } }
        end
        return { { path = "http://www.youtube.com/get_video.php?video_id="..video_id.."&t="..get_url_param( vlc.path, "t" ); name = name; arturl = arturl } }
    end
end
