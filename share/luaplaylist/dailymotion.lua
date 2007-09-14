-- $Id$

--[[
    Translate Daily Motion video webpages URLs to the corresponding
    FLV URL.
--]]

-- Probe function.
function probe()
    return vlc.access == "http"
        and string.match( vlc.path, "dailymotion.com" ) 
        and string.match( vlc.peek( 256 ), "<!DOCTYPE.*<title>Video " )
end

-- Parse function.
function parse()
    while true
    do 
        line = vlc.readline()
        if not line then break end
        if string.match( line, "param name=\"flashvars\" value=\".*url=http" )
        then
            path = vlc.decode_uri( string.gsub( line, "^.*param name=\"flashvars\" value=\".*url=(http[^&]*).*$", "%1" ) )
        end
        --[[ if string.match( line, "<title>" )
        then
            title = vlc.decode_uri( string.gsub( line, "^.*<title>([^<]*).*$", "%1" ) )
        end ]]
        if string.match( line, "<meta name=\"description\"" )
        then
            name = vlc.resolve_xml_special_chars( string.gsub( line, "^.*name=\"description\" content=\"%w+ (.*) %w+ %w+ %w+ %w+ Videos\..*$", "%1" ) )
            description = vlc.resolve_xml_special_chars( string.gsub( line, "^.*name=\"description\" content=\"%w+ .* %w+ %w+ %w+ %w+ Videos\. ([^\"]*)\".*$", "%1" ) )
        end
        if string.match( line, "<link rel=\"thumbnail\"" )
        then
            arturl = string.gsub( line, "^.*\"thumbnail\" type=\"([^\"]*)\".*$", "http://%1" ) -- looks like the dailymotion people mixed up type and href here ...
        end
        if path and name and description and arturl then break end
    end
    return { { path = path; name = name; description = description; url = vlc.path; arturl = arturl } }
end
