-- $Id$

if vlc.access ~= "http"
then
    return false
end 

url = nil
title = nil

function get_url_param( url, name )
    return string.gsub( vlc.path, "^.*"..name.."=([^&]*).*$", "%1" )
end

if string.match( vlc.path, "youtube.com" ) 
then
    if string.match( vlc.path, "watch%?v=" )
    then
        url = string.gsub( vlc.path, "^(.*)watch%?v=([^&]*).*$", "http://%1v/%2" )
        while not title
        do
            line = vlc.readline()
            if not line
            then
                break
            end
            if string.match( line, "<meta name=\"title\"" )
            then
                title = string.gsub( line, "^.*content=\"([^\"]*).*$", "%1" )
            end
        end
    elseif string.match( vlc.path, "watch_fullscreen%?video_id=" ) or string.match( vlc.path, "p.swf" ) or string.match( vlc.path, "player2.swf" )
    then
        video_id = get_url_param( vlc.path, "video_id" )
        t = get_url_param( vlc.path, "t" )
        url = "http://www.youtube.com/get_video.php?video_id="..video_id.."&t="..t
        if string.match( vlc.path, "title=" )
        then
            title = get_url_param( vlc.path, "title" )
        end
    end
elseif string.match( vlc.path, "dailymotion.com" )
then
    len, str = vlc.peek( 9 )
    if str == "<!DOCTYPE"
    then
        while not url
        do
            line = vlc.readline()
            if string.match( line, "param name=\"flashvars\" value=\"url=" )
            then
                url = vlc.decode_uri( string.gsub( line, "^.*param name=\"flashvars\" value=\"url=([^&]*).*$", "%1" ) )
            end
        end
    end
elseif string.match( vlc.path, "video.google.com" ) and string.match( vlc.path, "videoplay" )
then
    url = string.gsub( vlc.path, "^.*(docid=[^&]*).*$", "http://video.google.com/videogvp?%1" )
elseif string.match( vlc.path, "metacafe.com" )
then
    if string.match( vlc.path, "watch/" )
    then
        url = string.gsub( vlc.path, "^.*watch/(.*[^/])/?$", "http://www.metacafe.com/fplayer/%1.swf" )
    elseif string.match( vlc.path, "mediaURL=" )
    then
        url = string.gsub( vlc.path, "^.*mediaURL=([^&]*).*$", "%1" )
    end
end

if url == nil
then
    return false
else
    return true, url, title
end
