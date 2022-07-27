--[[

 Copyright © 2007-2022 the VideoLAN team

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

-- Helper emulating vlc.readline() to work around its failure on
-- very long lines (see #24957)
function read_long_line()
    local eol
    local pos = 0
    local len = 32768
    repeat
        len = len * 2
        local line = vlc.peek( len )
        if not line then return nil end
        eol = string.find( line, "\n", pos + 1 )
        pos = len
    until eol or len >= 1024 * 1024 -- No EOF detection, loop until limit
    return vlc.read( eol or len )
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
    return nil
end

-- Descramble the "n" parameter using the javascript code that does that
-- in the web page
function n_descramble( nparam, js )
    if not js then
        return nil
    end

    -- Look for the descrambler function's name
    -- a.C&&(b=a.get("n"))&&(b=Bpa[0](b),a.set("n",b),Bpa.length||iha(""))}};
    -- var Bpa=[iha];
    local callsite = js_extract( js, '[^;]*%.set%("n",[^};]*' )
    if not callsite then
        vlc.msg.dbg( "Couldn't extract YouTube video throttling parameter descrambling function name" )
        return nil
    end

    -- Try direct function name from following clause
    local descrambler = string.match( callsite, '%.set%("n",.%),...?%.length||(...?)%(' )
    local itm = nil
    if not descrambler then
        -- Try from main call site
        descrambler = string.match( callsite, '[=%(,&|]([a-zA-Z0-9_$%[%]]+)%(.%),.%.set%("n",' )
        if descrambler then
            -- Check if this is only an intermediate variable
            itm = string.match( descrambler, '^([^%[%]]+)%[' )
            if itm then
                descrambler = nil
            end
        else
            -- Last chance: intermediate variable in following clause
            itm = string.match( callsite, '%.set%("n",.%),(...?)%.length' )
        end
    end

    if not descrambler and itm then
        -- Resolve intermediate variable
        descrambler = js_extract( js, 'var '..itm..'=%[(...?)[%],]' )
    end

    if not descrambler then
        vlc.msg.dbg( "Couldn't extract YouTube video throttling parameter descrambling function name" )
        return nil
    end

    -- Fetch the code of the descrambler function
    -- lha=function(a){var b=a.split(""),c=[310282131,"KLf3",b,null,function(d,e){d.push(e)},-45817231, [data and transformations...] ,1248130556];c[3]=c;c[15]=c;c[18]=c;try{c[40](c[14],c[2]),c[25](c[48]),c[21](c[32],c[23]), [scripted calls...] ,c[25](c[33],c[3])}catch(d){return"enhanced_except_4ZMBnuz-_w8_"+a}return b.join("")};
    local code = js_extract( js, "^"..descrambler.."=function%([^)]*%){(.-)};" )
    if not code then
        vlc.msg.dbg( "Couldn't extract YouTube video throttling parameter descrambling code" )
        return nil
    end

    -- Split code into two main sections: 1/ data and transformations,
    -- and 2/ a script of calls
    local datac, script = string.match( code, "c=%[(.*)%];.-;try{(.*)}catch%(" )
    if ( not datac ) or ( not script ) then
        vlc.msg.dbg( "Couldn't extract YouTube video throttling parameter descrambling rules" )
        return nil
    end

    -- Split "n" parameter into a table as descrambling operates on it
    -- as one of several arrays
    local n = {}
    for c in string.gmatch( nparam, "." ) do
        table.insert( n, c )
    end

    -- Helper
    local table_len = function( tab )
        local len = 0
        for i, val in ipairs( tab ) do
            len = len + 1
        end
        return len
    end

    -- Shared core section of compound transformations: it compounds
    -- the "n" parameter with an input string, character by character,
    -- using a Base64 alphabet as algebraic modulo group.
    -- var h=f.length;d.forEach(function(l,m,n){this.push(n[m]=f[(f.indexOf(l)-f.indexOf(this[m])+m+h--)%f.length])},e.split(""))
    local compound = function( ntab, str, alphabet )
        if ntab ~= n or
           type( str ) ~= "string" or
           type( alphabet ) ~= "string" then
            return true
        end
        local input = {}
        for c in string.gmatch( str, "." ) do
            table.insert( input, c )
        end

        local len = string.len( alphabet )
        for i, c in ipairs( ntab ) do
            if type( c ) ~= "string" then
                return true
            end
            local pos1 = string.find( alphabet, c, 1, true )
            local pos2 = string.find( alphabet, input[i], 1, true )
            if ( not pos1 ) or ( not pos2 ) then
                return true
            end
            local pos = ( pos1 - pos2 ) % len
            local newc = string.sub( alphabet, pos + 1, pos + 1 )
            ntab[i] = newc
            table.insert( input, newc )
        end
    end

    -- The data section contains among others function code for a number
    -- of transformations, most of which are basic array operations.
    -- We can match these functions' code to identify them, and emulate
    -- the corresponding transformations.
    local trans = {
        reverse = {
            func = function( tab )
                local len = table_len( tab )
                local tmp = {}
                for i, val in ipairs( tab ) do
                    tmp[len - i + 1] = val
                end
                for i, val in ipairs( tmp ) do
                    tab[i] = val
                end
            end,
            match = {
                -- function(d){d.reverse()}
                -- function(d){for(var e=d.length;e;)d.push(d.splice(--e,1)[0])}
                "^function%(d%)",
            }
        },
        append = {
            func = function( tab, val )
                table.insert( tab, val )
            end,
            match = {
                -- function(d,e){d.push(e)}
                "^function%(d,e%){d%.push%(e%)},",
            }
        },
        remove = {
            func = function( tab, i )
                if type( i ) ~= "number" then
                    return true
                end
                i = i % table_len( tab )
                table.remove( tab, i + 1 )
            end,
            match = {
                -- function(d,e){e=(e%d.length+d.length)%d.length;d.splice(e,1)}
                "^[^}]-;d%.splice%(e,1%)},",
            }
        },
        swap = {
            func = function( tab, i )
                if type( i ) ~= "number" then
                    return true
                end
                i = i % table_len( tab )
                local tmp = tab[1]
                tab[1] = tab[i + 1]
                tab[i + 1] = tmp
            end,
            match = {
                -- function(d,e){e=(e%d.length+d.length)%d.length;var f=d[0];d[0]=d[e];d[e]=f}
                -- function(d,e){e=(e%d.length+d.length)%d.length;d.splice(0,1,d.splice(e,1,d[0])[0])}
                "^[^}]-;var f=d%[0%];d%[0%]=d%[e%];d%[e%]=f},",
                "^[^}]-;d%.splice%(0,1,d%.splice%(e,1,d%[0%]%)%[0%]%)},",
            }
        },
        rotate = {
            func = function( tab, shift )
                if type( shift ) ~= "number" then
                    return true
                end
                local len = table_len( tab )
                shift = shift % len
                local tmp = {}
                for i, val in ipairs( tab ) do
                    tmp[( i - 1 + shift ) % len + 1] = val
                end
                for i, val in ipairs( tmp ) do
                    tab[i] = val
                end
            end,
            match = {
                -- function(d,e){for(e=(e%d.length+d.length)%d.length;e--;)d.unshift(d.pop())}
                -- function(d,e){e=(e%d.length+d.length)%d.length;d.splice(-e).reverse().forEach(function(f){d.unshift(f)})}
                "^[^}]-d%.unshift%(d.pop%(%)%)},",
                "^[^}]-d%.unshift%(f%)}%)},",
            }
        },
        -- Here functions with no arguments are not really functions,
        -- they're constants: treat them as such. These alphabets are
        -- passed to and used by the compound transformations.
        alphabet1 = {
            func = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_",
            match = {
                -- function(){for(var d=64,e=[];++d-e.length-32;){switch(d){case 91:d=44;continue;case 123:d=65;break;case 65:d-=18;continue;case 58:d=96;continue;case 46:d=95}e.push(String.fromCharCode(d))}return e}
                "^function%(%){[^}]-case 58:d=96;",
            }
        },
        alphabet2 = {
            func = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_",
            match = {
                -- function(){for(var d=64,e=[];++d-e.length-32;){switch(d){case 58:d-=14;case 91:case 92:case 93:continue;case 123:d=47;case 94:case 95:case 96:continue;case 46:d=95}e.push(String.fromCharCode(d))}return e}
                -- function(){for(var d=64,e=[];++d-e.length-32;)switch(d){case 46:d=95;default:e.push(String.fromCharCode(d));case 94:case 95:case 96:break;case 123:d-=76;case 92:case 93:continue;case 58:d=44;case 91:}return e}
                "^function%(%){[^}]-case 58:d%-=14;",
                "^function%(%){[^}]-case 58:d=44;",
            }
        },
        -- Compound transformations are based on a shared core section
        -- that compounds the "n" parameter with an input string,
        -- character by character, using a variation of a Base64
        -- alphabet as algebraic modulo group.
        compound = {
            func = compound,
            match = {
                -- function(d,e,f){var h=f.length;d.forEach(function(l,m,n){this.push(n[m]=f[(f.indexOf(l)-f.indexOf(this[m])+m+h--)%f.length])},e.split(""))}
                "^function%(d,e,f%)",
            }
        },
        -- These compound transformation variants first build their
        -- Base64 alphabet themselves, before using it.
        compound1 = {
            func = function( ntab, str )
                return compound( ntab, str, "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_" )
            end,
            match = {
                -- function(d,e){for(var f=64,h=[];++f-h.length-32;)switch(f){case 58:f=96;continue;case 91:f=44;break;case 65:f=47;continue;case 46:f=153;case 123:f-=58;default:h.push(String.fromCharCode(f))} [ compound... ] }
                "^function%(d,e%){[^}]-case 58:f=96;",
            }
        },
        compound2 = {
            func = function( ntab, str )
                return compound( ntab, str,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_" )
            end,
            match = {
                -- function(d,e){for(var f=64,h=[];++f-h.length-32;){switch(f){case 58:f-=14;case 91:case 92:case 93:continue;case 123:f=47;case 94:case 95:case 96:continue;case 46:f=95}h.push(String.fromCharCode(f))} [ compound... ] }
                -- function(d,e){for(var f=64,h=[];++f-h.length-32;)switch(f){case 46:f=95;default:h.push(String.fromCharCode(f));case 94:case 95:case 96:break;case 123:f-=76;case 92:case 93:continue;case 58:f=44;case 91:} [ compound... ] }
                "^function%(d,e%){[^}]-case 58:f%-=14;",
                "^function%(d,e%){[^}]-case 58:f=44;",
            }
        },
        -- Fallback
        unid = {
            func = function( )
                vlc.msg.dbg( "Couldn't apply unidentified YouTube video throttling parameter transformation, aborting descrambling" )
                return true
            end,
            match = {
            }
        },
    }

    -- The data section actually mixes input data, reference to the
    -- "n" parameter array, and self-reference to its own array, with
    -- transformation functions used to modify itself. We parse it
    -- as such into a table.
    local data = {}
    datac = datac..","
    while datac and datac ~= "" do
        local el = nil
        -- Transformation functions
        if string.match( datac, "^function%(" ) then
            for name, tr in pairs( trans ) do
                for i, match in ipairs( tr.match ) do
                    if string.match( datac, match ) then
                        el = tr.func
                        break
                    end
                end
                if el then
                    break
                end
            end
            if not el then
                el = trans.unid.func
                vlc.msg.warn( "Couldn't parse unidentified YouTube video throttling parameter transformation" )
            end

            -- Compounding functions use a subfunction, so we need to be
            -- more specific in how much parsed data we consume.
            if el == trans.compound.func or
               el == trans.compound1.func or
               el == trans.compound2.func then
                datac = string.match( datac, '^.-},e%.split%(""%)%)},(.*)$' )
                        or string.match( datac, "^.-},(.*)$" )
            else
                datac = string.match( datac, "^.-},(.*)$" )
            end

        -- String input data
        elseif string.match( datac, '^"[^"]*",' ) then
            el, datac = string.match( datac, '^"([^"]*)",(.*)$' )
        -- Integer input data
        -- 1818016376,-648890305,-1200559E3, ...
        elseif string.match( datac, '^%-?%d+,' ) or
               string.match( datac, '^%-?%d+[eE]%-?%d+,' ) then
            el, datac = string.match( datac, "^(.-),(.*)$" )
            el = tonumber( el )
        -- Reference to "n" parameter array
        elseif string.match( datac, '^b,' ) then
            el = n
            datac = string.match( datac, "^b,(.*)$" )
        -- Replaced by self-reference to data array after its declaration
        elseif string.match( datac, '^null,' ) then
            el = data
            datac = string.match( datac, "^null,(.*)$" )
        else
            vlc.msg.warn( "Couldn't parse unidentified YouTube video throttling parameter descrambling data" )
            el = false -- Lua tables can't contain nil values
            datac = string.match( datac, "^[^,]-,(.*)$" )
        end

        table.insert( data, el )
    end

    -- Debugging helper to print data array elements
    local prd = function( el, tab )
        if not el then
            return "???"
        elseif el == n then
            return "n"
        elseif el == data then
            return "data"
        elseif type( el ) == "string" then
            return '"'..el..'"'
        elseif type( el ) == "number" then
            el = tostring( el )
            if type( tab ) == "table" then
                el = el.." -> "..( el % table_len( tab ) )
            end
            return el
        else
            for name, tr in pairs( trans ) do
                if el == tr.func then
                    return name
                end
            end
            return tostring( el )
        end
    end

    -- The script section contains a series of calls to elements of
    -- the data section array onto other elements of it: calls to
    -- transformations, with a reference to the data array itself or
    -- the "n" parameter array as first argument, and often input data
    -- as a second argument. We parse and emulate those calls to follow
    -- the descrambling script.
    -- c[40](c[14],c[2]),c[25](c[48]),c[14](c[1],c[24],c[42]()), [...]
    if not string.match( script, "c%[(%d+)%]%(c%[(%d+)%]([^)]-)%)" ) then
        vlc.msg.dbg( "Couldn't parse and execute YouTube video throttling parameter descrambling rules" )
        return nil
    end
    for ifunc, itab, args in string.gmatch( script, "c%[(%d+)%]%(c%[(%d+)%]([^)]-)%)" ) do
        local iarg1 = string.match( args, "^,c%[(%d+)%]" )
        local iarg2 = string.match( args, "^,[^,]-,c%[(%d+)%]" )

        local func = data[tonumber( ifunc ) + 1]
        local tab = data[tonumber( itab ) + 1]
        local arg1 = iarg1 and data[tonumber( iarg1 ) + 1]
        local arg2 = iarg2 and data[tonumber( iarg2 ) + 1]

        -- Uncomment to debug transformation chain
        --vlc.msg.err( '"n" parameter transformation: '..prd( func ).."("..prd( tab )..( arg1 ~= nil and ( ", "..prd( arg1, tab ) ) or "" )..( arg2 ~= nil and ( ", "..prd( arg2, tab ) ) or "" )..") "..ifunc.."("..itab..( iarg1 and ( ", "..iarg1 ) or "" )..( iarg2 and ( ", "..iarg2 ) or "" )..")" )
        --local nprev = table.concat( n )

        if type( func ) ~= "function" or type( tab ) ~= "table"
            or func( tab, arg1, arg2 ) then
            vlc.msg.dbg( "Invalid data type encountered during YouTube video throttling parameter descrambling transformation chain, aborting" )
            vlc.msg.dbg( "Couldn't descramble YouTube throttling URL parameter: data transfer will get throttled" )
            vlc.msg.err( "Couldn't process youtube video URL, please check for updates to this script" )
            break
        end

        -- Uncomment to debug transformation chain
        --local nnew = table.concat( n )
        --if nprev ~= nnew then
        --    vlc.msg.dbg( '"n" parameter transformation: '..nprev.." -> "..nnew )
        --end
    end

    return table.concat( n )
end

-- Descramble the URL signature using the javascript code that does that
-- in the web page
function sig_descramble( sig, js )
    if not js then
        return nil
    end

    -- Look for the descrambler function's name
    -- if(h.s){var l=h.sp,m=wja(decodeURIComponent(h.s));f.set(l,encodeURIComponent(m))}
    -- k.s (from stream map field "s") holds the input scrambled signature
    -- k.sp (from stream map field "sp") holds a parameter name (normally
    -- "signature" or "sig") to set with the output, descrambled signature
    local descrambler = js_extract( js, "[=%(,&|](...?)%(decodeURIComponent%(.%.s%)%)" )
    if not descrambler then
        vlc.msg.dbg( "Couldn't extract youtube video URL signature descrambling function name" )
        return nil
    end

    -- Fetch the code of the descrambler function
    -- Go=function(a){a=a.split("");Fo.sH(a,2);Fo.TU(a,28);Fo.TU(a,44);Fo.TU(a,26);Fo.TU(a,40);Fo.TU(a,64);Fo.TR(a,26);Fo.sH(a,1);return a.join("")};
    local rules = js_extract( js, "^"..descrambler.."=function%([^)]*%){(.-)};" )
    if not rules then
        vlc.msg.dbg( "Couldn't extract youtube video URL signature descrambling rules" )
        return nil
    end

    -- Get the name of the helper object providing transformation definitions
    local helper = string.match( rules, ";(..)%...%(" )
    if not helper then
        vlc.msg.dbg( "Couldn't extract youtube video URL signature transformation helper name" )
        return nil
    end

    -- Fetch the helper object code
    -- var Fo={TR:function(a){a.reverse()},TU:function(a,b){var c=a[0];a[0]=a[b%a.length];a[b]=c},sH:function(a,b){a.splice(0,b)}};
    local transformations = js_extract( js, "[ ,]"..helper.."={(.-)};" )
    if not transformations then
        vlc.msg.dbg( "Couldn't extract youtube video URL signature transformation code" )
        return nil
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
function stream_url( params, js )
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
        local ds = sig_descramble( s, js )
        if not ds then
            vlc.msg.dbg( "Couldn't descramble YouTube video URL signature" )
            vlc.msg.err( "Couldn't process youtube video URL, please check for updates to this script" )
            ds = s
        end

        local sp = string.match( params, "sp=([^&]+)" )
        if not sp then
            vlc.msg.warn( "Couldn't extract signature parameters for youtube video URL, guessing" )
            sp = "signature"
        end
        url = url.."&"..sp.."="..vlc.strings.encode_uri_component( ds )
    end

    return url
end

-- Parse and pick our video stream URL (classic parameters, out of use)
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

    -- Fetch javascript code: we'll need this to descramble maybe the
    -- URL signature, and normally always the "n" throttling parameter.
    local js = nil
    if js_url then
        js = { stream = vlc.stream( js_url ), lines = {}, i = 0 }
        if not js.stream then
            -- Retry once for transient errors
            js.stream = vlc.stream( js_url )
            if not js.stream then
                js = nil
            end
        end
    end

    -- Either the "url" or the "signatureCipher" parameter is present,
    -- depending on whether the URL signature is scrambled.
    local url
    local cipher = string.match( pick, '"signatureCipher":"(.-)"' )
        or string.match( pick, '"[a-zA-Z]*[Cc]ipher":"(.-)"' )
    if cipher then
        -- Scrambled signature: some assembly required
        url = stream_url( cipher, js )
    end
    if not url then
        -- Unscrambled signature, already included in ready-to-use URL
        url = string.match( pick, '"url":"(.-)"' )
    end

    if not url then
        return nil
    end

    -- The "n" parameter is scrambled too, and needs to be descrambled
    -- and replaced in place, otherwise the data transfer gets throttled
    -- down to between 40 and 80 kB/s, below real-time playability level.
    local n = string.match( url, "[?&]n=([^&]+)" )
    if n then
        n = vlc.strings.decode_uri( n )
        local dn = n_descramble( n, js )
        if dn then
            url = string.gsub( url, "([?&])n=[^&]+", "%1n="..vlc.strings.encode_uri_component( dn ), 1 )
        else
            vlc.msg.err( "Couldn't descramble YouTube throttling URL parameter: data transfer will get throttled" )
            --vlc.msg.err( "Couldn't process youtube video URL, please check for updates to this script" )
        end
    end

    return url
end

-- Probe function.
function probe()
    return ( ( vlc.access == "http" or vlc.access == "https" ) and (
            ((
               string.match( vlc.path, "^www%.youtube%.com/" )
            or string.match( vlc.path, "^music%.youtube%.com/" )
            or string.match( vlc.path, "^gaming%.youtube%.com/" ) -- out of use
             ) and (
               string.match( vlc.path, "/watch%?" ) -- the html page
            or string.match( vlc.path, "/live$" ) -- user live stream html page
            or string.match( vlc.path, "/live%?" ) -- user live stream html page
            or string.match( vlc.path, "/get_video_info%?" ) -- info API
            or string.match( vlc.path, "/v/" ) -- video in swf player
            or string.match( vlc.path, "/embed/" ) -- embedded player iframe
             )) or
               string.match( vlc.path, "^consent%.youtube%.com/" )
         ) )
end

-- Parse function.
function parse()
    if string.match( vlc.path, "^consent%.youtube%.com/" ) then
        -- Cookie consent redirection
        -- Location: https://consent.youtube.com/m?continue=https%3A%2F%2Fwww.youtube.com%2Fwatch%3Fv%3DXXXXXXXXXXX&gl=FR&m=0&pc=yt&uxe=23983172&hl=fr&src=1
        -- Set-Cookie: CONSENT=PENDING+355; expires=Fri, 01-Jan-2038 00:00:00 GMT; path=/; domain=.youtube.com
        local url = get_url_param( vlc.path, "continue" )
        if not url then
            vlc.msg.err( "Couldn't handle YouTube cookie consent redirection, please check for updates to this script or try disabling HTTP cookie forwarding" )
            return { }
        end
        return { { path = vlc.strings.decode_uri( url ), options = { ":no-http-forward-cookies" } } }
    elseif not string.match( vlc.path, "^www%.youtube%.com/" ) then
        -- Skin subdomain
        return { { path = vlc.access.."://"..string.gsub( vlc.path, "^([^/]*)/", "www.youtube.com/" ) } }

    elseif string.match( vlc.path, "/watch%?" )
        or string.match( vlc.path, "/live$" )
        or string.match( vlc.path, "/live%?" )
    then -- This is the HTML page's URL
        local js_url
        -- fmt is the format of the video
        -- (cf. http://en.wikipedia.org/wiki/YouTube#Quality_and_formats)
        fmt = get_url_param( vlc.path, "fmt" )
        while true do
            -- The new HTML code layout has fewer and longer lines; always
            -- use the long line workaround until we get more visibility.
            local line = new_layout and read_long_line() or vlc.readline()
            if not line then break end

            -- The next line is the major configuration line that we need.
            -- It is very long so we need this workaround (see #24957).
            if string.match( line, '^ *<div id="player%-api">' ) then
                line = read_long_line()
                if not line then break end
            end

            if not title then
                local meta = string.match( line, '<meta property="og:title"( .-)>' )
                if meta then
                    title = string.match( meta, ' content="(.-)"' )
                    if title then
                        title = vlc.strings.resolve_xml_special_chars( title )
                    end
                end
            end

            if not description then
                -- FIXME: there is another version of this available,
                -- without the double JSON string encoding, but we're
                -- unlikely to access it due to #24957
                description = string.match( line, '\\"shortDescription\\":\\"(.-[^\\])\\"')
                if description then
                    -- FIXME: do this properly (see #24958)
                    description = string.gsub( description, '\\(["\\/])', '%1' )
                else
                    description = string.match( line, '"shortDescription":"(.-[^\\])"')
                end
                if description then
                    if string.match( description, '^"' ) then
                        description = ""
                    end
                    -- FIXME: do this properly (see #24958)
                    -- This way of unescaping is technically wrong
                    -- so as little as possible of it should be done
                    description = string.gsub( description, '\\(["\\/])', '%1' )
                    description = string.gsub( description, '\\n', '\n' )
                    description = string.gsub( description, '\\r', '\r' )
                    description = string.gsub( description, "\\u0026", "&" )
                end
            end

            if not arturl then
                local meta = string.match( line, '<meta property="og:image"( .-)>' )
                if meta then
                    arturl = string.match( meta, ' content="(.-)"' )
                    if arturl then
                        arturl = vlc.strings.resolve_xml_special_chars( arturl )
                    end
                end
            end

            if not artist then
                artist = string.match(line, '\\"author\\":\\"(.-)\\"')
                if artist then
                    -- FIXME: do this properly (see #24958)
                    artist = string.gsub( artist, '\\(["\\/])', '%1' )
                else
                    artist = string.match( line, '"author":"(.-)"' )
                end
                if artist then
                    -- FIXME: do this properly (see #24958)
                    artist = string.gsub( artist, "\\u0026", "&" )
                end
            end

            if not new_layout then
                if string.match( line, '<script nonce="' ) then
                    vlc.msg.dbg( "Detected new YouTube HTML code layout" )
                    new_layout = true
                end
            end

            -- We need this when parsing the main stream configuration;
            -- it can indeed be found on that same line (among others).
            if not js_url then
                js_url = string.match( line, '"jsUrl":"(.-)"' )
                    or string.match( line, "\"js\": *\"(.-)\"" )
                if js_url then
                    js_url = string.gsub( js_url, "\\/", "/" )
                    -- Resolve URL
                    if string.match( js_url, "^/[^/]" ) then
                        local authority = string.match( vlc.path, "^([^/]*)/" )
                        js_url = "//"..authority..js_url
                    end
                    js_url = string.gsub( js_url, "^//", vlc.access.."://" )
                end
            end

            -- JSON parameters, also formerly known as "swfConfig",
            -- "SWF_ARGS", "swfArgs", "PLAYER_CONFIG", "playerConfig" ...
            if string.match( line, "ytplayer%.config" ) then

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
                    -- FIXME: do this properly (see #24958)
                    url_map = string.gsub( url_map, "\\u0026", "&" )
                    path = pick_url( url_map, fmt, js_url )
                end

                -- New-style parameters
                if not path then
                    local stream_map = string.match( line, '\\"formats\\":%[(.-)%]' )
                    if stream_map then
                        -- FIXME: do this properly (see #24958)
                        stream_map = string.gsub( stream_map, '\\(["\\/])', '%1' )
                    else
                        stream_map = string.match( line, '"formats":%[(.-)%]' )
                    end
                    if stream_map then
                        vlc.msg.dbg( "Found new-style parameters for youtube video stream, parsing..." )
                        -- FIXME: do this properly (see #24958)
                        stream_map = string.gsub( stream_map, "\\u0026", "&" )
                        path = pick_stream( stream_map, js_url )
                    end
                end

                if not path then
                    -- If this is a live stream, the URL map will be empty
                    -- and we get the URL from this field instead
                    local hlsvp = string.match( line, '\\"hlsManifestUrl\\": *\\"(.-)\\"' )
                        or string.match( line, '"hlsManifestUrl":"(.-)"' )
                    if hlsvp then
                        hlsvp = string.gsub( hlsvp, "\\/", "/" )
                        path = hlsvp
                    end
                end
            end
        end

        if not path then
            vlc.msg.err( "Couldn't extract youtube video URL, please check for updates to this script" )
            return { }
        end

        if not arturl then
            arturl = get_arturl()
        end

        return { { path = path; name = title; description = description; artist = artist; arturl = arturl } }

    elseif string.match( vlc.path, "/get_video_info%?" ) then
        -- video info API, retired since summer 2021
        -- Replacement Innertube API requires HTTP POST requests
        -- and so remains for now unworkable from lua parser scripts
        -- (see #26185)

        local line = vlc.read( 1024*1024 ) -- data is on one line only
        if not line then
            vlc.msg.err( "YouTube API output missing" )
            return { }
        end

        local js_url = get_url_param( vlc.path, "jsurl" )
        if js_url then
            js_url= vlc.strings.decode_uri( js_url )
        end

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
            path = pick_url( url_map, fmt, js_url )
        end

        -- New-style parameters
        if not path then
            local stream_map = string.match( line, '%%22formats%%22%%3A%%5B(.-)%%5D' )
            if stream_map then
                vlc.msg.dbg( "Found new-style parameters for youtube video stream, parsing..." )
                stream_map = vlc.strings.decode_uri( stream_map )
                -- FIXME: do this properly (see #24958)
                stream_map = string.gsub( stream_map, "\\u0026", "&" )
                path = pick_stream( stream_map, js_url )
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

        if not path and get_url_param( vlc.path, "el" ) ~= "detailpage" then
            -- Retry with the other known value for the "el" parameter;
            -- either value has historically been wrong and failed for
            -- some videos but not others.
            local video_id = get_url_param( vlc.path, "video_id" )
            if video_id then
                path = vlc.access.."://www.youtube.com/get_video_info?video_id="..video_id.."&el=detailpage"..copy_url_param( vlc.path, "fmt" )..copy_url_param( vlc.path, "jsurl" )
                vlc.msg.warn( "Couldn't extract video URL, retrying with alternate YouTube API parameters" )
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
            -- FIXME: do this properly (see #24958)
            title = string.gsub( title, "\\u0026", "&" )
        end
        -- FIXME: description gets truncated if it contains a double quote
        local description = string.match( line, "%%22shortDescription%%22%%3A%%22(.-)%%22" )
        if description then
            description = string.gsub( description, "+", " " )
            description = vlc.strings.decode_uri( description )
            -- FIXME: do this properly (see #24958)
            description = string.gsub( description, '\\(["\\/])', '%1' )
            description = string.gsub( description, '\\n', '\n' )
            description = string.gsub( description, '\\r', '\r' )
            description = string.gsub( description, "\\u0026", "&" )
        end
        local artist = string.match( line, "%%22author%%22%%3A%%22(.-)%%22" )
        if artist then
            artist = string.gsub( artist, "+", " " )
            artist = vlc.strings.decode_uri( artist )
            -- FIXME: do this properly (see #24958)
            artist = string.gsub( artist, "\\u0026", "&" )
        end
        local arturl = string.match( line, "%%22playerMicroformatRenderer%%22%%3A%%7B%%22thumbnail%%22%%3A%%7B%%22thumbnails%%22%%3A%%5B%%7B%%22url%%22%%3A%%22(.-)%%22" )
        if arturl then
            arturl = vlc.strings.decode_uri( arturl )
        end

        return { { path = path, name = title, description = description, artist = artist, arturl = arturl } }

    else -- Other supported URL formats
        local video_id = string.match( vlc.path, "/[^/]+/([^?]*)" )
        if not video_id then
            vlc.msg.err( "Couldn't extract youtube video URL" )
            return { }
        end
        return { { path = vlc.access.."://www.youtube.com/watch?v="..video_id..copy_url_param( vlc.path, "fmt" ) } }
    end
end
