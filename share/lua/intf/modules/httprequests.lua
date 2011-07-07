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


--input utilities

local function stripslashes(s)
  return string.gsub(s,"\\(.)","%1")
end

--main function to process commands sent with the request

processcommands = function ()
	
	local input = _GET['input']
	local command = _GET['command']
	local id = tonumber(_GET['id'] or -1)
	local val = _GET['val']
	local options = _GET['option']
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
	  vlc.playlist.add({{path=stripslashes(input),options=options}})
	elseif command == "in_enqueue" then
	  vlc.playlist.enqueue({{path=stripslashes(input),options=options}})
	elseif command == "pl_play" then
	  if id == -1 then
		vlc.playlist.play()
	  else
		vlc.playlist.goto(id)
	  end
	elseif command == "pl_pause" then
	  if vlc.playlist.status() == "stopped" then
		if id == -1 then
		  vlc.playlist.play()
		else
		  vlc.playlist.goto(id)
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
	  vlc.playlist.loop()
	elseif command == "pl_repeat" then
	  vlc.playlist.repeat_()
	elseif command == "pl_sd" then
	  if vlc.sd.is_loaded(val) then
		vlc.sd.remove(val)
	  else
		vlc.sd.add(val)
	  end
	elseif command == "fullscreen" then
	  vlc.video.fullscreen()
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
	   vlc.var.set(vlc.object.input(),"audio-delay",val)
	  end
	elseif command == "rate" then
	  if vlc.object.input() and tonumber(val) >= 0 then
	   vlc.var.set(vlc.object.input(),"rate",val)
	  end
	elseif command == "subdelay" then
	  if vlc.object.input() then
	   vlc.var.set(vlc.object.input(),"spu-delay",val)
	  end
	end
	
	local input = nil
	local command = nil
	local id = nil
	local val = nil

end

--utilities for formatting output

local function xmlString(s)
  if (type(s)=="string") then
  	return vlc.strings.convert_xml_special_chars(s)
  else
  	return tostring(s)
  end
end

local printJsonKeyValue = function (k,v,indent)
	print("\n")
	for i=1,indent do print(" ") end
	if (k) then
		print("\""..k.."\":")
	end
	
	if (type(v)=="number") then
		print(xmlString(v))
	elseif (type(v)=="table") then 
		 if (v._array==NULL) then         		
          	print("{\n")
    		printTableAsJson(v,indent+2)
    		print("\n}")  
          else 
          	print("[")
          	printArrayAsJson(v._array,indent+2)
          	print("\n]") 
          end
	else
    	print("\""..xmlString(v).."\"")
    end
end


printArrayAsJson = function(array,indent)
	first=true
	for i,v in ipairs(array) do
		if not first then print(",") end
		printJsonKeyValue(NULL,v,indent)	
		first=false
	end
end

printTableAsJson = function (dict,indent)
	first=true
	for k,v in pairs(dict) do
		if not first then print(",") end
		printJsonKeyValue(k,v,indent)
		first=false
    end
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
		print("</"..k..">")
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
		local name = vlc.strings.convert_xml_special_chars(item.name or "")
		
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
		local name, path = vlc.strings.convert_xml_special_chars(item.name or "", item.path or "")
		local current_item = vlc.input.item()
		
		-- Is the item the one currently played
		if(current_item ~= nil) then
            if(vlc.input.item().uri(current_item) == path) then
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


getstatus = function (includecategories)


local input = vlc.object.input()
local item = vlc.input.item()
local playlist = vlc.object.playlist()
local vout = input and vlc.object.find(input,'vout','child')

	local s ={}
	
	--update api version when new data/commands added
	s.apiversion=1
	s.version=vlc.misc.version()
	s.volume=vlc.volume.get()
	
	if input then 
		s.length=math.floor(vlc.var.get(input,"length"))
		s.time=math.floor(vlc.var.get(input,"time"))
		s.position=vlc.var.get(input,"position")
		s.audiodelay=vlc.var.get(input,"audio-delay")
		s.rate=vlc.var.get(input,"rate")
		s.subtitledelay=vlc.var.get(input,"spu-delay")
	else 
		s.length=0
		s.time=0
		s.position=0
		s.audiodelay=0
		s.rate=1
		s.subtitledelay=0
	end
	
	if vout then
		s.fullscreen=vlc.var.get(vout,"fullscreen")
	else
		s.fullscreen=0
	end
	
	s.state=vlc.playlist.status()
	s.random=vlc.var.get(playlist,"random")
	s.loop=vlc.var.get(playlist,"loop")
	s["repeat"]=vlc.var.get(playlist,"repeat")
	
	if (includecategories and item) then
		s.information={}
		s.information.category={}
		s.information.category.meta=item:metas()
		
		local info = item:info()
		for k, v in pairs(info) do
			local streamTable={}
			for k2, v2 in pairs(v) do
				local tag = string.gsub(k2," ","_")
				streamTable[xmlString(tag)]=xmlString(v2)
			end
			
			s.information.category[xmlString(k)]=streamTable
		end
		
		s.stats={}
		
		local statsdata = item:stats()
      	for k,v in pairs(statsdata) do
        	local tag = string.gsub(k,"_","")
        s.stats[tag]=xmlString(v)
      end
		
		
	end

	return s
end
