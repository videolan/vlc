--[[ This code is public domain (since it really isn't very interesting) ]]--

module("common",package.seeall)

-- Iterate over a table in the keys' alphabetical order
function pairs_sorted(t)
    local s = {}
    for k,_ in pairs(t) do table.insert(s,k) end
    table.sort(s)
    local i = 0
    return function () i = i + 1; return s[i], t[s[i]] end
end

-- Return a function such as skip(foo)(a,b,c) = foo(b,c)
function skip(foo)
    return function(discard,...) return foo(...) end
end

-- Return a function such as setarg(foo,a)(b,c) = foo(a,b,c)
function setarg(foo,a)
    return function(...) return foo(a,...) end
end

-- Trigger a hotkey
function hotkey(arg)
    vlc.var.set( vlc.object.libvlc(), "key-pressed", vlc.config.get( arg ) )
end

-- Take a video snapshot
function snapshot()
    local vout = vlc.object.find(nil,"vout","anywhere")
    if not vout then return end
    vlc.var.set(vout,"video-snapshot",nil)
end

-- Naive (non recursive) table copy
function table_copy(t)
    c = {}
    for i,v in pairs(t) do c[i]=v end
    return c
end

-- strip leading and trailing spaces
function strip(str)
    return string.gsub(str, "^%s*(.-)%s*$", "%1")
end

-- print a table (recursively)
function table_print(t,prefix)
    local prefix = prefix or ""
    for a,b in pairs_sorted(t) do
        print(prefix..tostring(a),b)
        if type(b)==type({}) then
            table_print(b,prefix.."\t")
        end
    end
end

-- print the list of callbacks registered in lua
-- usefull for debug purposes
function print_callbacks()
    print "callbacks:"
    table_print(vlc.callbacks)
end 

-- convert a duration (in seconds) to a string
function durationtostring(duration)
    return string.format("%02d:%02d:%02d",
                         math.floor(duration/3600),
                         math.floor(duration/60)%60,
                         math.floor(duration%60))
end

-- realpath
function realpath(path)
    return string.gsub(string.gsub(string.gsub(string.gsub(path,"/%.%./[^/]+","/"),"/[^/]+/%.%./","/"),"/%./","/"),"//","/")
end

-- seek
function seek(value)
    local input = vlc.object.input()
    if string.sub(value,#value)=="%" then
        vlc.var.set(input,"position",tonumber(string.sub(value,1,#value-1))/100.)
    else
        vlc.var.set(input,"time",tonumber(value))
    end
end
