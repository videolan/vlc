-- $Id$
-- French humor site: http://lelombrik.net

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "lelombrik.net/videos" ) 
end

-- Parse function.
function parse()
    vidtitle = ""
    while true do
        line = vlc.readline()
        if not line then break end
        if string.match( line, "id=\"nom_fichier\">" ) then
            vidtitle = string.gsub( line, ".*\"nom_fichier\">([^<]*).*", "%1" )
        end
        if string.match( line, "flvplayer.swf" ) then
            -- fallback: retrieve the title from the url if we didn't find it
            if vidtitle == "" then
                vidtitle = string.gsub( vlc.path, ".*/([^.]*).*", "%1" )
            end
            return { { path = string.gsub( line, ".*flashvars=\"&file=([^&]*).*", "%1" ); arturl = string.gsub( line, ".*&image=([^&]*).*", "%1" ); title = vidtitle } }
        end
    end
end
