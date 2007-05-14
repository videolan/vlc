-- $Id$

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "video.google.com" ) 
        and string.match( vlc.path, "videoplay" )
end

-- Parse function.
function parse()
    return { { url = string.gsub( vlc.path, "^.*(docid=[^&]*).*$", "http://video.google.com/videogvp?%1" ) } }
end
