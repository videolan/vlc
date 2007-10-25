-- $Id$

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "video.google.com" ) 
        and string.match( vlc.path, "videoplay" )
end

-- Parse function.
function parse()
    while true
    do
        line = vlc.readline()
        if not line then break end
        if string.match( line, "^<title>" ) then
            title = string.gsub( line, "<title>([^<]*).*", "%1" )
        end
        if string.match( line, "src=\"/googleplayer.swf" ) then
            url = string.gsub( line, ".*videoUrl=([^&]*).*" ,"%1" )
            arturl = string.gsub( line, ".*thumbnailUrl=([^\"]*).*", "%1" )
            return { { path = vlc.decode_uri(url), title = title, arturl = vlc.decode_uri(arturl) } }
        end
    end
end
