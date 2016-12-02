--[==========================================================================[
 httprequests.lua: code for processing httprequests commands and output
--[==========================================================================[
 Copyright (C) 2007 the VideoLAN team
 $Id$

 Authors: Antoine Cellerier <dionoea at videolan dot org>
 Rob Jonson <rob at hobbyistsoftware.com>

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
--]==========================================================================]

module("httprequests",package.seeall)

local common = require ("common")
local dkjson = require ("dkjson")



--Round the number to the specified precision
function round(what, precision)
    if type(what) == "string" then
        what = common.us_tonumber(what)
    end
    if type(what) == "number" then
        return math.floor(what*math.pow(10,precision)+0.5) / math.pow(10,precision)
    end
    return nil
end

--split text where it matches the delimiter
function strsplit(text, delimiter)
    local strfind = string.find
    local strsub = string.sub
    local tinsert = table.insert
    local list = {}
    local pos = 1
    if strfind("", delimiter, 1) then -- this would result in endless loops
        error("delimiter matches empty string!")
    end
    local i=1
    while 1 do
        local first, last = strfind(text, delimiter, pos)
        if first then -- found?
            tinsert(list,i, strsub(text, pos, first-1))
            pos = last+1
        else
            tinsert(list,i, strsub(text, pos))
            break
        end
        i = i+1
    end
    return list
end

--main function to process commands sent with the request

processcommands = function ()

    local input = _GET['input']
    local command = _GET['command']
    local id = tonumber(_GET['id'] or -1)
    local val = _GET['val']
    local options = _GET['option']
    local band = tonumber(_GET['band'])
    local name = _GET['name']
    local duration = tonumber(_GET['duration'])
    if type(options) ~= "table" then -- Deal with the 0 or 1 option case
        options = { options }
    end

    if command == "in_play" then
        --[[
        vlc.msg.err( "<options>" )
        for a,b in ipairs(options) do
        vlc.msg.err(b)
        end
        vlc.msg.err( "</options>" )
        --]]
        vlc.playlist.add({{path=vlc.strings.make_uri(input),options=options,name=name,duration=duration}})
    elseif command == "addsubtitle" then
        vlc.input.add_subtitle (val)
    elseif command == "in_enqueue" then
        vlc.playlist.enqueue({{path=vlc.strings.make_uri(input),options=options,name=name,duration=duration}})
    elseif command == "pl_play" then
        if id == -1 then
            vlc.playlist.play()
        else
            vlc.playlist.gotoitem(id)
        end
    elseif command == "pl_pause" then
        if vlc.playlist.status() == "stopped" then
            if id == -1 then
                vlc.playlist.play()
            else
                vlc.playlist.gotoitem(id)
            end
        else
            vlc.playlist.pause()
        end
    elseif command == "pl_forcepause" then
        if vlc.playlist.status() == "playing" then
            vlc.playlist.pause()
        end
    elseif command == "pl_forceresume" then
        if vlc.playlist.status() == "paused" then
            vlc.playlist.pause()
        end
    elseif command == "pl_stop" then
        vlc.playlist.stop()
    elseif command == "pl_next" then
        vlc.playlist.next()
    elseif command == "pl_previous" then
        vlc.playlist.prev()
    elseif command == "pl_delete" then
        vlc.playlist.delete(id)
    elseif command == "pl_empty" then
        vlc.playlist.clear()
    elseif command == "pl_sort" then
        vlc.playlist.sort( val, id > 0 )
    elseif command == "pl_random" then
        vlc.playlist.random()
    elseif command == "pl_loop" then
        --if loop is set true, then repeat needs to be set false
        if vlc.playlist.loop() then
            vlc.playlist.repeat_("off")
        end
    elseif command == "pl_repeat" then
        --if repeat is set true, then loop needs to be set false
        if vlc.playlist.repeat_() then
            vlc.playlist.loop("off")
        end
    elseif command == "pl_sd" then
        if vlc.sd.is_loaded(val) then
            vlc.sd.remove(val)
        else
            vlc.sd.add(val)
        end
    elseif command == "fullscreen" then
        if vlc.object.vout() then
            vlc.video.fullscreen()
        end
    elseif command == "snapshot" then
        common.snapshot()
    elseif command == "volume" then
        common.volume(val)
    elseif command == "seek" then
        common.seek(val)
    elseif command == "key" then
        common.hotkey("key-"..val)
    elseif command == "audiodelay" then
        if vlc.object.input() and val then
            val = common.us_tonumber(val)
            vlc.var.set(vlc.object.input(),"audio-delay",val * 1000000)
        end
    elseif command == "rate" then
        val = common.us_tonumber(val)
        if vlc.object.input() and val >= 0 then
            vlc.var.set(vlc.object.input(),"rate",val)
        end
    elseif command == "subdelay" then
        if vlc.object.input() then
            val = common.us_tonumber(val)
            vlc.var.set(vlc.object.input(),"spu-delay",val * 1000000)
        end
    elseif command == "aspectratio" then
        if vlc.object.vout() then
            vlc.var.set(vlc.object.vout(),"aspect-ratio",val)
        end
    elseif command == "preamp" then
        val = common.us_tonumber(val)
        vlc.equalizer.preampset(val)
    elseif command == "equalizer" then
        val = common.us_tonumber(val)
        vlc.equalizer.equalizerset(band,val)
    elseif command == "enableeq" then
        if val == '0' then vlc.equalizer.enable(false) else vlc.equalizer.enable(true) end
    elseif command == "setpreset" then
        vlc.equalizer.setpreset(val)
    elseif command == "title" then
        vlc.var.set(vlc.object.input(), "title", val)
    elseif command == "chapter" then
        vlc.var.set(vlc.object.input(), "chapter", val)
    elseif command == "audio_track" then
        vlc.var.set(vlc.object.input(), "audio-es", val)
    elseif command == "video_track" then
        vlc.var.set(vlc.object.input(), "video-es", val)
    elseif command == "subtitle_track" then
        vlc.var.set(vlc.object.input(), "spu-es", val)
    end

    local input = nil
    local command = nil
    local id = nil
    local val = nil

end

--utilities for formatting output

function xmlString(s)
    if (type(s)=="string") then
        return vlc.strings.convert_xml_special_chars(s)
    elseif (type(s)=="number") then
        return common.us_tostring(s)
    else
        return tostring(s)
    end
end

--dkjson outputs numbered tables as arrays
--so we don't need the array indicators
function removeArrayIndicators(dict)
    local newDict=dict

    for k,v in pairs(dict) do
        if (type(v)=="table") then
            local arrayEntry=v._array
            if arrayEntry then
                v=arrayEntry
            end

            dict[k]=removeArrayIndicators(v)
        end
    end

    return newDict
end

printTableAsJson = function (dict)
    dict=removeArrayIndicators(dict)

    local output=dkjson.encode (dict, { indent = true })
    print(output)
end

local printXmlKeyValue = function (k,v,indent)
    print("\n")
    for i=1,indent do print(" ") end
    if (k) then
        print("<"..k..">")
    end

    if (type(v)=="table") then
        printTableAsXml(v,indent+2)
    else
        print(xmlString(v))
    end

    if (k) then
        xs=xmlString(k)
        space_loc=string.find(xs," ")
        if space_loc == nil then
            print("</"..xs..">")
        else
            xs=string.sub(xs,1,space_loc)
            print("</"..xs..">")
        end
    end
end

printTableAsXml = function (dict,indent)
    for k,v in pairs(dict) do
        printXmlKeyValue(k,v,indent)
    end
end

--[[
function logTable(t,pre)
local pre = pre or ""
for k,v in pairs(t) do
vlc.msg.err(pre..tostring(k).." : "..tostring(v))
if type(v) == "table" then
a(v,pre.."  ")
end
end
end
--]]

--main accessors

getplaylist = function ()
    local p

    if _GET["search"] then
        if _GET["search"] ~= "" then
            _G.search_key = _GET["search"]
        else
            _G.search_key = nil
        end
        local key = vlc.strings.decode_uri(_GET["search"])
        p = vlc.playlist.search(key)
    else
        p = vlc.playlist.get()
    end

    --logTable(p) --Uncomment to debug

    return p
end

parseplaylist = function (item)
    if item.flags.disabled then return end

    if (item.children) then
        local result={}
        local name = (item.name or "")

        result["type"]="node"
        result.id=tostring(item.id)
        result.name=tostring(name)
        result.ro=item.flags.ro and "ro" or "rw"

        --store children in an array
        --we use _array as a proxy for arrays
        result.children={}
        result.children._array={}

        for _, child in ipairs(item.children) do
            local nextChild=parseplaylist(child)
            table.insert(result.children._array,nextChild)
        end

        return result
    else
        local result={}
        local name, path = item.name or ""
        local path = item.path or ""
        local current_item_id = vlc.playlist.current()

        -- Is the item the one currently played
        if(current_item_id ~= nil) then
            if(current_item_id == item.id) then
                result.current = "current"
            end
        end

        result["type"]="leaf"
        result.id=tostring(item.id)
        result.uri=tostring(path)
        result.name=name
        result.ro=item.flags.ro and "ro" or "rw"
        result.duration=math.floor(item.duration)

        return result
    end

end

playlisttable = function ()

    local basePlaylist=getplaylist()

    return parseplaylist(basePlaylist)
end

getbrowsetable = function ()

    local dir = nil
    local uri = _GET["uri"]
    --uri takes precedence, but fall back to dir
    if uri then
        if uri == "file://~" then
            dir = uri
        else
            dir = vlc.strings.make_path(uri)
        end
    else
        dir = _GET["dir"]
    end

    --backwards compatibility with old format driveLetter:\\..
    --this is forgiving with the slash type and number
    if dir then
        local position=string.find(dir, '%a:[\\/]*%.%.',0)
        if position==1 then dir="" end
    end

    local result={}
    --paths are returned as an array of elements
    result.element={}
    result.element._array={}

    if dir then
        if dir == "~" or dir == "file://~" then dir = vlc.config.homedir() end
        -- FIXME: hack for Win32 drive list
        if dir~="" then
            dir = common.realpath(dir.."/")
        end

        local d = vlc.net.opendir(dir)
        table.sort(d)

        for _,f in pairs(d) do
            if f == ".." or not string.match(f,"^%.") then
                local df = common.realpath(dir..f)
                local s = vlc.net.stat(df)
                local path, name =  df, f
                local element={}

                if (s) then
                    for k,v in pairs(s) do
                        element[k]=v
                    end
                else
                    element["type"]="unknown"
                end
                element["path"]=path
                element["name"]=name

                local uri=vlc.strings.make_uri(df)
                --windows paths are returned with / separators, but make_uri expects \ for windows and returns nil
                if not uri then
                    --convert failed path to windows format and try again
                    path=string.gsub(path,"/","\\")
                    uri=vlc.strings.make_uri(df)
                end
                element["uri"]=uri

                table.insert(result.element._array,element)
            end

        end
    end

    return result;
end


getstatus = function (includecategories)


    local input = vlc.object.input()
    local item = vlc.input.item()
    local playlist = vlc.object.playlist()
    local vout = vlc.object.vout()
    local aout = vlc.object.aout()

    local s ={}

    --update api version when new data/commands added
    s.apiversion=3
    s.version=vlc.misc.version()
    s.volume=vlc.volume.get()

    if input then
        s.time=math.floor(vlc.var.get(input,"time") / 1000000)
        s.position=vlc.var.get(input,"position")
        s.currentplid=vlc.playlist.current()
        s.audiodelay=vlc.var.get(input,"audio-delay") / 1000000
        s.rate=vlc.var.get(input,"rate")
        s.subtitledelay=vlc.var.get(input,"spu-delay") / 1000000
    else
        s.time=0
        s.position=0
        s.currentplid=-1
        s.audiodelay=0
        s.rate=1
        s.subtitledelay=0
    end

    if item then
        s.length=math.floor(item:duration())
    else
        s.length=0
    end

    if vout then
        s.fullscreen=vlc.var.get(vout,"fullscreen")
        s.aspectratio=vlc.var.get(vout,"aspect-ratio");
        if s.aspectratio=="" then s.aspectratio = "default" end
    else
        s.fullscreen=0
    end

    if aout then
        local filters=vlc.var.get(aout,"audio-filter")
        local temp=strsplit(filters,":")
        s.audiofilters={}
        local id=0
        for i,j in pairs(temp) do
            s.audiofilters['filter_'..id]=j
            id=id+1
        end
    end

    s.videoeffects={}
    s.videoeffects.hue=round(vlc.config.get("hue"),2)
    s.videoeffects.brightness=round(vlc.config.get("brightness"),2)
    s.videoeffects.contrast=round(vlc.config.get("contrast"),2)
    s.videoeffects.saturation=round(vlc.config.get("saturation"),2)
    s.videoeffects.gamma=round(vlc.config.get("gamma"),2)

    s.state=vlc.playlist.status()
    s.random=vlc.var.get(playlist,"random")
    s.loop=vlc.var.get(playlist,"loop")
    s["repeat"]=vlc.var.get(playlist,"repeat")

    s.equalizer={}
    s.equalizer.preamp=round(vlc.equalizer.preampget(),2)
    s.equalizer.bands=vlc.equalizer.equalizerget()
    if s.equalizer.bands ~= null then
        for k,i in pairs(s.equalizer.bands) do s.equalizer.bands[k]=round(i,2) end
        s.equalizer.presets=vlc.equalizer.presets()
    end

    if (includecategories and item) then
        s.information={}
        s.information.category={}
        s.information.category.meta=item:metas()

        local info = item:info()
        for k, v in pairs(info) do
            local streamTable={}
            for k2, v2 in pairs(v) do
                local tag = string.gsub(k2," ","_")
                streamTable[tag]=v2
            end

            s.information.category[k]=streamTable
        end

        s.stats={}

        local statsdata = item:stats()
        for k,v in pairs(statsdata) do
            local tag = string.gsub(k,"_","")
            s.stats[tag]=v
        end

        s.information.chapter=vlc.var.get(input, "chapter")
        s.information.title=vlc.var.get(input, "title")

        s.information.chapters=vlc.var.get_list(input, "chapter")
        s.information.titles=vlc.var.get_list(input, "title")

    end
    return s
end

