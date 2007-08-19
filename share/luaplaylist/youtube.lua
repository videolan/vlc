-- $Id$

-- Helper function to get a parameter's value in a URL
function get_url_param( url, name )
    return string.gsub( vlc.path, "^.*"..name.."=([^&]*).*$", "%1" )
end

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "youtube.com" ) 
        and (  string.match( vlc.path, "watch%?v=" )
            or string.match( vlc.path, "watch_fullscreen%?video_id=" )
            or string.match( vlc.path, "p.swf" )
            or string.match( vlc.path, "player2.swf" ) )
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
            if string.match( line, "player2.swf" ) then
                video_id = string.gsub( line, ".*&video_id=([^\"]*).*", "%1" )
            end
            if name and description and artist and video_id then break end
        end
        return { { path = "http://www.youtube.com/get_video.php?video_id="..video_id; name = name; description = description; artist = artist } }
    else -- This is the flash player's URL
        if string.match( vlc.path, "title=" ) then
            name = get_url_param( vlc.path, "title" )
        end
        return { { path = "http://www.youtube.com/get_video.php?video_id="..get_url_param( vlc.path, "video_id" ).."&t="..get_url_param( vlc.patch, "t" ); name = name } }
    end
end
