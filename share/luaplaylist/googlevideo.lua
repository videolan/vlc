-- $Id$

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "video.google.com" ) 
        and string.match( vlc.path, "videoplay" )
end

-- Parse function.
function parse()
    -- We don't need to get the meta data here since it'll be available in
    -- the GVP file.
    return { { path = string.gsub( vlc.path, "^.*(docid=[^&]*).*$", "http://video.google.com/videogvp?%1" ) } }
end
