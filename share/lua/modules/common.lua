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
    local id = vlc.misc.action_id( arg )
    if id ~= nil then
        vlc.var.set( vlc.object.libvlc(), "key-action", id )
        return true
    else
        return false
    end
end

-- Take a video snapshot
function snapshot()
    local vout = vlc.object.vout()
    if not vout then return end
    vlc.var.set(vout,"video-snapshot",nil)
end

-- Naive (non recursive) table copy
function table_copy(t)
    c = {}
    for i,v in pairs(t) do c[i]=v end
    return c
end

-- tonumber() for decimals number, using a dot as decimal separator
-- regardless of the system locale 
function us_tonumber(str)
    local s, i, d = string.match(str, "^([+-]?)(%d*)%.?(%d*)$")
    if not s or not i or not d then
        return nil
    end

    if s == "-" then
        s = -1
    else
        s = 1
    end
    if i == "" then
        i = "0"
    end
    if d == nil or d == "" then
        d = "0"
    end
    return s * (tonumber(i) + tonumber(d)/(10^string.len(d)))
end

-- tostring() for decimals number, using a dot as decimal separator
-- regardless of the system locale 
function us_tostring(n)
    s = tostring(n):gsub(",", ".", 1)
    return s
end

-- strip leading and trailing spaces
function strip(str)
    return string.gsub(str, "^%s*(.-)%s*$", "%1")
end

-- print a table (recursively)
function table_print(t,prefix)
    local prefix = prefix or ""
    if not t then
        print(prefix.."/!\\ nil")
        return
    end
    for a,b in pairs_sorted(t) do
        print(prefix..tostring(a),b)
        if type(b)==type({}) then
            table_print(b,prefix.."\t")
        end
    end
end

-- print the list of callbacks registered in lua
-- useful for debug purposes
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

-- parse the time from a string and return the seconds
-- time format: [+ or -][<int><H or h>:][<int><M or m or '>:][<int><nothing or S or s or ">]
function parsetime(timestring)
    local seconds = 0
    local hourspattern = "(%d+)[hH]"
    local minutespattern = "(%d+)[mM']"
    local secondspattern = "(%d+)[sS\"]?$"

    local _, _, hoursmatch = string.find(timestring, hourspattern)
    if hoursmatch ~= nil then
        seconds = seconds + tonumber(hoursmatch) * 3600
    end
    local _, _, minutesmatch = string.find(timestring, minutespattern)
    if minutesmatch ~= nil then
        seconds = seconds + tonumber(minutesmatch) * 60
    end
    local _, _, secondsmatch = string.find(timestring, secondspattern)
    if secondsmatch ~= nil then
        seconds = seconds + tonumber(secondsmatch)
    end

    if string.sub(timestring,1,1) == "-" then
        seconds = seconds * -1
    end

    return seconds
end

-- seek
function seek(value)
    local input = vlc.object.input()
    if input ~= nil and value ~= nil then
        if string.sub(value,-1) == "%" then
            local number = us_tonumber(string.sub(value,1,-2))
            if number ~= nil then
                local posPercent = number/100
                if string.sub(value,1,1) == "+" or string.sub(value,1,1) == "-" then
                    vlc.var.set(input,"position",vlc.var.get(input,"position") + posPercent)
                else
                    vlc.var.set(input,"position",posPercent)
                end
            end
        else
            local posTime = parsetime(value)
            if string.sub(value,1,1) == "+" or string.sub(value,1,1) == "-" then
                vlc.var.set(input,"time",vlc.var.get(input,"time") + (posTime * 1000000))
            else
                vlc.var.set(input,"time",posTime * 1000000)
            end
        end
    end
end

function volume(value)
    if type(value)=="string" and string.sub(value,1,1) == "+" or string.sub(value,1,1) == "-" then
        vlc.volume.set(vlc.volume.get()+tonumber(value))
    else
        vlc.volume.set(tostring(value))
    end
end
