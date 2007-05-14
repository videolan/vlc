-- $Id$

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "metacafe.com" ) 
        and (  string.match( vlc.path, "watch/" )
            or string.match( vlc.path, "mediaURL=" ) )
end

-- Parse function.
function parse()
    if string.match( vlc.path, "watch/" )
    then -- This is the HTML page's URL
        return { { url = string.gsub( vlc.path, "^.*watch/(.*[^/])/?$", "http://www.metacafe.com/fplayer/%1.swf" ) } }
    else -- This is the flash player's URL
        return { { url = string.gsub( vlc.path, "^.*mediaURL=([^&]*).*$", "%1" ) } }
    end
end
