--[[
 $Id$

 Copyright © 2007-2020 the VideoLAN team

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

-- Helper function to get a parameter's value in a URL
function get_url_param( url, name )
    local _, _, res = string.find( url, "[&?]"..name.."=([^&]*)" )
    return res
end

-- Helper function to copy a parameter when building a new URL
function copy_url_param( url, name )
    local value = get_url_param( url, name )
    return ( value and "&"..name.."="..value or "" ) -- Ternary operator
end

function get_arturl()
    local iurl = get_url_param( vlc.path, "iurl" )
    if iurl then
        return iurl
    end
    local video_id = get_url_param( vlc.path, "v" )
    if not video_id then
        return nil
    end
    return vlc.access.."://img.youtube.com/vi/"..video_id.."/default.jpg"
end

-- Pick the most suited format available
function get_fmt( fmt_list )
    local prefres = vlc.var.inherit(nil, "preferred-resolution")
    if prefres < 0 then
        return nil
    end

    local fmt = nil
    for itag,height in string.gmatch( fmt_list, "(%d+)/%d+x(%d+)[^,]*" ) do
        -- Apparently formats are listed in quality
        -- order, so we take the first one that works,
        -- or fallback to the lowest quality
        fmt = itag
        if tonumber(height) <= prefres then
            break
        end
    end
    return fmt
end

-- Buffering iterator to parse through the HTTP stream several times
-- without making several HTTP requests
function buf_iter( s )
    s.i = s.i + 1
    local line = s.lines[s.i]
    if not line then
        -- Put back together statements split across several lines,
        -- otherwise we won't be able to parse them
        repeat
            local l = s.stream:readline()
            if not l then break end
            line = line and line..l or l -- Ternary operator
        until string.match( line, "};$" )

        if line then
            s.lines[s.i] = line
        end
    end
    return line
end

-- Helper to search and extract code from javascript stream
function js_extract( js, pattern )
    js.i = 0 -- Reset to beginning
    for line in buf_iter, js do
        local ex = string.match( line, pattern )
        if ex then
            return ex
        end
    end
    vlc.msg.err( "Couldn't process youtube video URL, please check for updates to this script" )
    return nil
end

-- Descramble the URL signature using the javascript code that does that
-- in the web page
function js_descramble( sig, js_url )
    -- Fetch javascript code
    local js = { stream = vlc.stream( js_url ), lines = {}, i = 0 }
    if not js.stream then
        vlc.msg.err( "Couldn't process youtube video URL, please check for updates to this script" )
        return sig
    end

    -- Look for the descrambler function's name
    -- if(k.s){var l=k.sp,m=pt(decodeURIComponent(k.s));f.set(l,encodeURIComponent(m))}
    -- k.s (from stream map field "s") holds the input scrambled signature
    -- k.sp (from stream map field "sp") holds a parameter name (normally
    -- "signature" or "sig") to set with the output, descrambled signature
    local descrambler = js_extract( js, "[=%(,&|](..)%(decodeURIComponent%(.%.s%)%)" )
    if not descrambler then
        vlc.msg.dbg( "Couldn't extract youtube video URL signature descrambling function name" )
        return sig
    end

    -- Fetch the code of the descrambler function
    -- Go=function(a){a=a.split("");Fo.sH(a,2);Fo.TU(a,28);Fo.TU(a,44);Fo.TU(a,26);Fo.TU(a,40);Fo.TU(a,64);Fo.TR(a,26);Fo.sH(a,1);return a.join("")};
    local rules = js_extract( js, "^"..descrambler.."=function%([^)]*%){(.-)};" )
    if not rules then
        vlc.msg.dbg( "Couldn't extract youtube video URL signature descrambling rules" )
        return sig
    end

    -- Get the name of the helper object providing transformation definitions
    local helper = string.match( rules, ";(..)%...%(" )
    if not helper then
        vlc.msg.dbg( "Couldn't extract youtube video URL signature transformation helper name" )
        vlc.msg.err( "Couldn't process youtube video URL, please check for updates to this script" )
        return sig
    end

    -- Fetch the helper object code
    -- var Fo={TR:function(a){a.reverse()},TU:function(a,b){var c=a[0];a[0]=a[b%a.length];a[b]=c},sH:function(a,b){a.splice(0,b)}};
    local transformations = js_extract( js, "[ ,]"..helper.."={(.-)};" )
    if not transformations then
        vlc.msg.dbg( "Couldn't extract youtube video URL signature transformation code" )
        return sig
    end

    -- Parse the helper object to map available transformations
    local trans = {}
    for meth,code in string.gmatch( transformations, "(..):function%([^)]*%){([^}]*)}" ) do
        -- a=a.reverse()
        if string.match( code, "%.reverse%(" ) then
          trans[meth] = "reverse"

        -- a.splice(0,b)
        elseif string.match( code, "%.splice%(") then
          trans[meth] = "slice"

        -- var c=a[0];a[0]=a[b%a.length];a[b]=c
        elseif string.match( code, "var c=" ) then
          trans[meth] = "swap"
        else
            vlc.msg.warn("Couldn't parse unknown youtube video URL signature transformation")
        end
    end

    -- Parse descrambling rules, map them to known transformations
    -- and apply them on the signature
    local missing = false
    for meth,idx in string.gmatch( rules, "..%.(..)%([^,]+,(%d+)%)" ) do
        idx = tonumber( idx )

        if trans[meth] == "reverse" then
            sig = string.reverse( sig )

        elseif trans[meth] == "slice" then
            sig = string.sub( sig, idx + 1 )

        elseif trans[meth] == "swap" then
            if idx > 1 then
                sig = string.gsub( sig, "^(.)("..string.rep( ".", idx - 1 )..")(.)(.*)$", "%3%2%1%4" )
            elseif idx == 1 then
                sig = string.gsub( sig, "^(.)(.)", "%2%1" )
            end
        else
            vlc.msg.dbg("Couldn't apply unknown youtube video URL signature transformation")
            missing = true
        end
    end
    if missing then
        vlc.msg.err( "Couldn't process youtube video URL, please check for updates to this script" )
    end
    return sig
end

-- Parse and assemble video stream URL
function stream_url( params, js_url )
    local url = string.match( params, "url=([^&]+)" )
    if not url then
        return nil
    end
    url = vlc.strings.decode_uri( url )

    -- Descramble any scrambled signature and append it to URL
    local s = string.match( params, "s=([^&]+)" )
    if s then
        s = vlc.strings.decode_uri( s )
        vlc.msg.dbg( "Found "..string.len( s ).."-character scrambled signature for youtube video URL, attempting to descramble... " )
        if js_url then
            s = js_descramble( s, js_url )
        else
            vlc.msg.err( "Couldn't process youtube video URL, please check for updates to this script" )
        end

        local sp = string.match( params, "sp=([^&]+)" )
        if not sp then
            vlc.msg.warn( "Couldn't extract signature parameters for youtube video URL, guessing" )
            sp = "signature"
        end
        url = url.."&"..sp.."="..vlc.strings.encode_uri_component( s )
    end

    return url
end

-- Parse and pick our video stream URL (classic parameters)
function pick_url( url_map, fmt, js_url )
    for stream in string.gmatch( url_map, "[^,]+" ) do
        local itag = string.match( stream, "itag=(%d+)" )
        if not fmt or not itag or tonumber( itag ) == tonumber( fmt ) then
            return stream_url( stream, js_url )
        end
    end
    return nil
end

-- Parse and pick our video stream URL (new-style parameters)
function pick_stream( stream_map, js_url )
    local pick = nil

    local fmt = tonumber( get_url_param( vlc.path, "fmt" ) )
    if fmt then
        -- Legacy match from URL parameter
        for stream in string.gmatch( stream_map, '{(.-)}' ) do
            local itag = tonumber( string.match( stream, '"itag":(%d+)' ) )
            if fmt == itag then
                pick = stream
                break
            end
        end
    else
        -- Compare the different available formats listed with our
        -- quality targets
        local prefres = vlc.var.inherit( nil, "preferred-resolution" )
        local bestres = nil

        for stream in string.gmatch( stream_map, '{(.-)}' ) do
            local height = tonumber( string.match( stream, '"height":(%d+)' ) )

            -- Better than nothing
            if not pick or ( height and ( not bestres
                -- Better quality within limits
                or ( ( prefres < 0 or height <= prefres ) and height > bestres )
                -- Lower quality more suited to limits
                or ( prefres > -1 and bestres > prefres and height < bestres )
            ) ) then
                bestres = height
                pick = stream
            end
        end
    end

    if not pick then
        return nil
    end

    -- Either the "url" or the "signatureCipher" parameter is present,
    -- depending on whether the URL signature is scrambled.
    local cipher = string.match( pick, '"signatureCipher":"(.-)"' )
        or string.match( pick, '"%a*[Cc]ipher":"(.-)"' )
    if cipher then
        -- Scrambled signature: some assembly required
        local url = stream_url( cipher, js_url )
        if url then
            return url
        end
    end
    -- Unscrambled signature, already included in ready-to-use URL
    return string.match( pick, '"url":"(.-)"' )
end

-- Probe function.
function probe()
    return ( ( vlc.access == "http" or vlc.access == "https" )
             and (
               string.match( vlc.path, "^www%.youtube%.com/" )
            or string.match( vlc.path, "^gaming%.youtube%.com/" )
             ) and (
               string.match( vlc.path, "/watch%?" ) -- the html page
            or string.match( vlc.path, "/live$" ) -- user live stream html page
            or string.match( vlc.path, "/live%?" ) -- user live stream html page
            or string.match( vlc.path, "/get_video_info%?" ) -- info API
            or string.match( vlc.path, "/v/" ) -- video in swf player
            or string.match( vlc.path, "/embed/" ) -- embedded player iframe
             ) )
end

-- Parse function.
function parse()
    if string.match( vlc.path, "^gaming%.youtube%.com/" ) then
        url = string.gsub( vlc.path, "^gaming%.youtube%.com", "www.youtube.com" )
        return { { path = vlc.access.."://"..url } }
    end
    if string.match( vlc.path, "/watch%?" )
        or string.match( vlc.path, "/live$" )
        or string.match( vlc.path, "/live%?" )
    then -- This is the HTML page's URL
        -- fmt is the format of the video
        -- (cf. http://en.wikipedia.org/wiki/YouTube#Quality_and_formats)
        fmt = get_url_param( vlc.path, "fmt" )
        while true do
            -- Try to find the video's title
            line = vlc.readline()
            if not line then break end
            if string.match( line, "<meta property=\"og:title\"" ) then
                _,_,name = string.find( line, "content=\"(.-)\"" )
                name = vlc.strings.resolve_xml_special_chars( name )
                name = vlc.strings.resolve_xml_special_chars( name )
            end

            if not description then
                -- FIXME: there is another version of this available,
                -- without the double JSON string encoding, but we're
                -- unlikely to access it due to #24957
                description = string.match( line, '\\"shortDescription\\":\\"(.-[^\\])\\"')
                if description then
                    -- FIXME: do this properly
                    description = string.gsub( description, '\\(["\\/])', '%1' )
                    description = string.gsub( description, '\\(["\\/])', '%1' )
                    description = string.gsub( description, '\\n', '\n' )
                    description = string.gsub( description, "\\u0026", "&" )
                end
            end


            if string.match( line, "<meta property=\"og:image\"" ) then
                _,_,arturl = string.find( line, "content=\"(.-)\"" )
                arturl = vlc.strings.resolve_xml_special_chars( arturl )
            end

            if not artist then
                artist = string.match(line, '\\"author\\":\\"(.-)\\"')
                if artist then
                    -- FIXME: do this properly
                    artist = string.gsub( artist, "\\u0026", "&" )
                end
            end

            -- JSON parameters, also formerly known as "swfConfig",
            -- "SWF_ARGS", "swfArgs", "PLAYER_CONFIG", "playerConfig" ...
            if string.match( line, "ytplayer%.config" ) then

                local js_url = string.match( line, "\"js\": *\"(.-)\"" )
                if js_url then
                    js_url = string.gsub( js_url, "\\/", "/" )
                    -- Resolve URL
                    if string.match( js_url, "^/[^/]" ) then
                        local authority = string.match( vlc.path, "^([^/]*)/" )
                        js_url = "//"..authority..js_url
                    end
                    js_url = string.gsub( js_url, "^//", vlc.access.."://" )
                end

                -- Classic parameters - out of use since early 2020
                if not fmt then
                    fmt_list = string.match( line, "\"fmt_list\": *\"(.-)\"" )
                    if fmt_list then
                        fmt_list = string.gsub( fmt_list, "\\/", "/" )
                        fmt = get_fmt( fmt_list )
                    end
                end

                url_map = string.match( line, "\"url_encoded_fmt_stream_map\": *\"(.-)\"" )
                if url_map then
                    vlc.msg.dbg( "Found classic parameters for youtube video stream, parsing..." )
                    -- FIXME: do this properly
                    url_map = string.gsub( url_map, "\\u0026", "&" )
                    path = pick_url( url_map, fmt, js_url )
                end

                -- New-style parameters
                if not path then
                    local stream_map = string.match( line, '\\"formats\\":%[(.-)%]' )
                    if stream_map then
                        vlc.msg.dbg( "Found new-style parameters for youtube video stream, parsing..." )
                        -- FIXME: do this properly
                        stream_map = string.gsub( stream_map, '\\(["\\/])', '%1' )
                        stream_map = string.gsub( stream_map, "\\u0026", "&" )
                        path = pick_stream( stream_map, js_url )
                    end
                end

                if not path then
                    -- If this is a live stream, the URL map will be empty
                    -- and we get the URL from this field instead
                    local hlsvp = string.match( line, '\\"hlsManifestUrl\\": *\\"(.-)\\"' )
                    if hlsvp then
                        hlsvp = string.gsub( hlsvp, "\\/", "/" )
                        path = hlsvp
                    end
                end
            end
        end

        if not path then
            local video_id = get_url_param( vlc.path, "v" )
            if video_id then
                -- Passing no "el" parameter to /get_video_info seems to
                -- let it default to "embedded", and both known values
                -- of "embedded" and "detailpage" have historically been
                -- wrong and failed for various restricted videos.
                path = vlc.access.."://www.youtube.com/get_video_info?video_id="..video_id..copy_url_param( vlc.path, "fmt" )
                vlc.msg.warn( "Couldn't extract video URL, falling back to alternate youtube API" )
            end
        end

        if not path then
            vlc.msg.err( "Couldn't extract youtube video URL, please check for updates to this script" )
            return { }
        end

        if not arturl then
            arturl = get_arturl()
        end

        return { { path = path; name = name; description = description; artist = artist; arturl = arturl } }

    elseif string.match( vlc.path, "/get_video_info%?" ) then -- video info API
        local line = vlc.readline() -- data is on one line only

        -- Classic parameters - out of use since early 2020
        local fmt = get_url_param( vlc.path, "fmt" )
        if not fmt then
            local fmt_list = string.match( line, "&fmt_list=([^&]*)" )
            if fmt_list then
                fmt_list = vlc.strings.decode_uri( fmt_list )
                fmt = get_fmt( fmt_list )
            end
        end

        local url_map = string.match( line, "&url_encoded_fmt_stream_map=([^&]*)" )
        if url_map then
            vlc.msg.dbg( "Found classic parameters for youtube video stream, parsing..." )
            url_map = vlc.strings.decode_uri( url_map )
            path = pick_url( url_map, fmt )
        end

        -- New-style parameters
        if not path then
            local stream_map = string.match( line, '%%22formats%%22%%3A%%5B(.-)%%5D' )
            if stream_map then
                vlc.msg.dbg( "Found new-style parameters for youtube video stream, parsing..." )
                stream_map = vlc.strings.decode_uri( stream_map )
                -- FIXME: do this properly
                stream_map = string.gsub( stream_map, "\\u0026", "&" )
                path = pick_stream( stream_map )
            end
        end

        if not path then
            -- If this is a live stream, the URL map will be empty
            -- and we get the URL from this field instead
            local hlsvp = string.match( line, "%%22hlsManifestUrl%%22%%3A%%22(.-)%%22" )
            if hlsvp then
                hlsvp = vlc.strings.decode_uri( hlsvp )
                path = hlsvp
            end
        end

        if not path then
            vlc.msg.err( "Couldn't extract youtube video URL, please check for updates to this script" )
            return { }
        end

        local title = string.match( line, "%%22title%%22%%3A%%22(.-)%%22" )
        if title then
            title = string.gsub( title, "+", " " )
            title = vlc.strings.decode_uri( title )
            -- FIXME: do this properly
            title = string.gsub( title, "\\u0026", "&" )
        end
        -- FIXME: description gets truncated if it contains a double quote
        local description = string.match( line, "%%22shortDescription%%22%%3A%%22(.-)%%22" )
        if description then
            description = string.gsub( description, "+", " " )
            description = vlc.strings.decode_uri( description )
            -- FIXME: do this properly
            description = string.gsub( description, '\\(["\\/])', '%1' )
            description = string.gsub( description, '\\n', '\n' )
            description = string.gsub( description, "\\u0026", "&" )
        end
        local artist = string.match( line, "%%22author%%22%%3A%%22(.-)%%22" )
        if artist then
            artist = string.gsub( artist, "+", " " )
            artist = vlc.strings.decode_uri( artist )
            -- FIXME: do this properly
            artist = string.gsub( artist, "\\u0026", "&" )
        end
        local arturl = string.match( line, "%%22playerMicroformatRenderer%%22%%3A%%7B%%22thumbnail%%22%%3A%%7B%%22thumbnails%%22%%3A%%5B%%7B%%22url%%22%%3A%%22(.-)%%22" )
        if arturl then
            arturl = vlc.strings.decode_uri( arturl )
        end

        return { { path = path, title = title, description = description, artist = artist, arturl = arturl } }

    else -- Other supported URL formats
        local video_id = string.match( vlc.path, "/[^/]+/([^?]*)" )
        if not video_id then
            vlc.msg.err( "Couldn't extract youtube video URL" )
            return { }
        end
        return { { path = vlc.access.."://www.youtube.com/watch?v="..video_id..copy_url_param( vlc.path, "fmt" ) } }
    end
end
