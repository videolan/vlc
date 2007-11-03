--[[
    Translate Daily Motion video webpages URLs to the corresponding
    FLV URL.

 $Id$

 Copyright © 2007 the VideoLAN team

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
