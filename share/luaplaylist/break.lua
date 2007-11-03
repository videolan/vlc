-- $Id$

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "www.break.com" ) 
end

-- Parse function.
function parse()
    filepath = ""
    filename = ""
    filetitle = ""
    arturl = ""
    while true do
        line = vlc.readline()
        if not line then break end
        if string.match( line, "sGlobalContentFilePath=" ) then
            filepath= string.gsub( line, ".*sGlobalContentFilePath='([^']*).*", "%1" )
        end
        if string.match( line, "sGlobalFileName=" ) then
            filename = string.gsub( line, ".*sGlobalFileName='([^']*).*", "%1")
        end
        if string.match( line, "sGlobalContentTitle=" ) then
            filetitle = string.gsub( line, ".*sGlobalContentTitle='([^']*).*", "%1")
        end
        if string.match( line, "el=\"videothumbnail\" href=\"" ) then
            arturl = string.gsub( line, ".*el=\"videothumbnail\" href=\"([^\"]*).*", "%1" )
        end
        if string.match( line, "videoPath" ) then
            return { { path = ( string.gsub( line, ".*videoPath', '([^']*).*", "%1" ) )..filepath.."/"..filename..".flv"; title = filetitle; arturl = arturl } }
        end
    end
end
