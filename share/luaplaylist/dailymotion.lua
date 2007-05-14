-- $Id$

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "dailymotion.com" ) 
        and vlc.peek( 9 ) == "<!DOCTYPE"
end

-- Parse function.
function parse()
    while true
    do 
        line = vlc.readline()
        if not line then break end
        if string.match( line, "param name=\"flashvars\" value=\"url=" )
        then
            url = vlc.decode_uri( string.gsub( line, "^.*param name=\"flashvars\" value=\"url=([^&]*).*$", "%1" ) )
        end
        if string.match( line, "<title>" )
        then
            title = vlc.decode_uri( string.gsub( line, "^.*<title>([^<]*).*$", "%1" ) )
        end
        if url and title then break end
    end
    return { { url = url; title = title } }
end
