--[[
 VLSub Extension for VLC media player 1.1 and 2.0
 Copyright 2010 - 2013 Guillaume Le Maout

 Authors:  Guillaume Le Maout
 Contact: http://addons.videolan.org/messages/?action=newmessage&username=exebetche
 Bug report: http://addons.videolan.org/content/show.php/?content=148752

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

function descriptor()
	return { title = "VLsub 0.9" ;
		version = "0.9" ;
		author = "exebetche" ;
		url = 'http://www.opensubtitles.org/';
		shortdesc = "VLsub";
		description = "<center><b>VLsub</b></center>"
				.. "Dowload subtitles from OpenSubtitles.org" ;
		capabilities = { "input-listener", "meta-listener" }
	}
end

require "os"

-- Global variables
dlg = nil     -- Dialog
--~ conflocation = 'subdownloader.conf'
url = "http://api.opensubtitles.org/xml-rpc"
progressBarSize = 70

--~ default_language = "fre"
default_language = nil
refresh_toggle = false

function set_default_language()
	if default_language then
		for k,v in ipairs(languages) do
			if v[2] == default_language then
				table.insert(languages, 1, v)
				return true
			end
		end
	end
end

function activate()
    vlc.msg.dbg("[VLsub] Welcome")
    set_default_language()

    create_dialog()
	openSub.request("LogIn")
	update_fields()
end

function deactivate()
	if openSub.token then
		openSub.LogOut()
	end
    vlc.msg.dbg("[VLsub] Bye bye!")
end

function close()
    vlc.deactivate()
end

function meta_changed()
	update_fields()
end

function input_changed()
	--~ Crash !?
	--~ wait(3)
	--~ update_fields()
end

function update_fields()
	openSub.getFileInfo()
	openSub.getMovieInfo()

	if openSub.movie.name ~= nil then
		widget.get("title").input:set_text(openSub.movie.name)
	end

	if openSub.movie.seasonNumber ~= nil then
		widget.get("season").input:set_text(openSub.movie.seasonNumber)
	end

	if openSub.movie.episodeNumber ~= nil then
		widget.get("episode").input:set_text(openSub.movie.episodeNumber)
	end
end

openSub = {
	itemStore = nil,
	actionLabel = "",
	conf = {
		url = "http://api.opensubtitles.org/xml-rpc",
		userAgentHTTP = "VLSub",
		useragent = "VLSub 0.9",
		username = "",
		password = "",
		language = "",
		downloadSub = true,
		removeTag = false,
		justgetlink = false
	},
	session = {
		loginTime = 0,
		token = ""
	},
	file = {
		uri = nil,
		ext = nil,
		name = nil,
		path = nil,
		dir = nil,
		hash = nil,
		bytesize = nil,
		fps = nil,
		timems = nil,
		frames = nil
	},
	movie = {
		name = "",
		season = "",
		episode = ""
	},
	sub = {
		id = nil,
		authorcomment = nil,
		hash = nil,
		idfile = nil,
		filename = nil,
		content = nil,
		IDSubMovieFile = nil,
		score = nil,
		comment = nil,
		bad = nil,
		languageid = nil
	},
	request = function(methodName)
		local params = openSub.methods[methodName].params()
		local reqTable = openSub.getMethodBase(methodName, params)
		local request = "<?xml version='1.0'?>"..dump_xml(reqTable)
		local host, path = parse_url(openSub.conf.url)
		local header = {
			"POST "..path.." HTTP/1.1",
			"Host: "..host,
			"User-Agent: "..openSub.conf.userAgentHTTP,
			"Content-Type: text/xml",
			"Content-Length: "..string.len(request),
			"",
			""
		}
		request = table.concat(header, "\r\n")..request

		local response
		local status, responseStr = http_req(host, 80, request)

		if status == 200 then
			response = parse_xmlrpc(responseStr)
			--~ vlc.msg.dbg(responseStr)
			if (response and response.status == "200 OK") then
				return openSub.methods[methodName].callback(response)
			elseif response then
				setError("code "..response.status.."("..status..")")
				return false
			else
				setError("Server not responding")
				return false
			end
		elseif status == 401 then
			setError("Request unauthorized")

			response = parse_xmlrpc(responseStr)
			if openSub.session.token ~= response.token then
				setMessage("Session expired, retrying")
				openSub.session.token = response.token
				openSub.request(methodName)
			end
			return false
		elseif status == 503 then
			setError("Server overloaded, please retry later")
			return false
		end

	end,
	getMethodBase = function(methodName, param)
		if openSub.methods[methodName].methodName then
			methodName = openSub.methods[methodName].methodName
		end

		local request = {
		  methodCall={
			methodName=methodName,
			params={ param=param }}}

		return request
	end,
	methods = {
		LogIn = {
			params = function()
				openSub.actionLabel = "Logging in"
				return {
					{ value={ string=openSub.conf.username } },
					{ value={ string=openSub.conf.password } },
					{ value={ string=openSub.conf.language } },
					{ value={ string=openSub.conf.useragent } }
				}
			end,
			callback = function(resp)
				openSub.session.token = resp.token
				openSub.session.loginTime = os.time()
				return true
			end
		},
		LogOut = {
			params = function()
				openSub.actionLabel = "Logging out"
				return {
					{ value={ string=openSub.session.token } }
				}
			end,
			callback = function()
				return true
			end
		},
		NoOperation = {
			params = function()
				return {
					{ value={ string=openSub.session.token } }
				}
			end,
			callback = function()
				return true
			end
		},
		SearchSubtitlesByHash = {
			methodName = "SearchSubtitles",
			params = function()
				openSub.actionLabel = "Searching subtitles"
				setMessage(openSub.actionLabel..": "..progressBarContent(0))

				return {
					{ value={ string=openSub.session.token } },
					{ value={
						array={
						  data={
							value={
							  struct={
								member={
								  { name="sublanguageid", value={ string=openSub.sub.languageid } },
								  { name="moviehash", value={ string=openSub.file.hash } },
								  { name="moviebytesize", value={ double=openSub.file.bytesize } } }}}}}}}
				}
			end,
			callback = function(resp)
				openSub.itemStore = resp.data

				if openSub.itemStore ~= "0" then
					return true
				else
					openSub.itemStore = nil
					return false
				end
			end
		},
		SearchSubtitles = {
			methodName = "SearchSubtitles",
			params = function()
				openSub.actionLabel = "Searching subtitles"
				setMessage(openSub.actionLabel..": "..progressBarContent(0))

				local member = {
						  { name="sublanguageid", value={ string=openSub.sub.languageid } },
						  { name="query", value={ string=openSub.movie.name } } }


				if openSub.movie.season ~= nil then
					table.insert(member, { name="season", value={ string=openSub.movie.season } })
				end

				if openSub.movie.episode ~= nil then
					table.insert(member, { name="episode", value={ string=openSub.movie.episode } })
				end

				return {
					{ value={ string=openSub.session.token } },
					{ value={
						array={
						  data={
							value={
							  struct={
								member=member
								   }}}}}}
				}
			end,
			callback = function(resp)
				openSub.itemStore = resp.data

				if openSub.itemStore ~= "0" then
					return true
				else
					openSub.itemStore = nil
					return false
				end
			end
		}
	},
	getInputItem = function()
		return vlc.item or vlc.input.item()
	end,
	getFileInfo = function()
		local item = openSub.getInputItem()
		local file = openSub.file
		if not item then
			file.hasInput = false;
			file.cleanName = "";
			return false
		else
			local parsed_uri = vlc.net.url_parse(item:uri())
			file.uri = item:uri()
			file.protocol = parsed_uri["protocol"]
			file.path = vlc.strings.decode_uri(parsed_uri["path"])
			--correction needed for windows
			local windowPath = string.match(file.path, "^/(%a:/.+)$")
			if windowPath then
				file.path = windowPath
			end
			file.dir, file.completeName = string.match(file.path, "^([^\n]-/?)([^/]+)$")
			file.name, file.ext = string.match(file.path, "([^/]-)%.?([^%.]*)$")

			if file.ext == "part" then
				file.name, file.ext = string.match(file.name, "^([^/]+)%.([^%.]+)$")
			end
			file.hasInput = true;
			file.cleanName = string.gsub(file.name, "[%._]", " ")
			vlc.msg.dbg(file.cleanName)
		end
	end,
	getMovieInfo = function()
		if not openSub.file.name then
			openSub.movie.name = ""
			openSub.movie.seasonNumber = ""
			openSub.movie.episodeNumber = ""
			return false
		end

		local showName, seasonNumber, episodeNumber = string.match(openSub.file.cleanName, "(.+)[sS](%d%d)[eE](%d%d).*")

		if not showName then
		   showName, seasonNumber, episodeNumber = string.match(openSub.file.cleanName, "(.+)(%d)[xX](%d%d).*")
		end

		if showName then
			openSub.movie.name = showName
			openSub.movie.seasonNumber = seasonNumber
			openSub.movie.episodeNumber = episodeNumber
		else
			openSub.movie.name = openSub.file.cleanName
			openSub.movie.seasonNumber = ""
			openSub.movie.episodeNumber = ""

			vlc.msg.dbg(openSub.movie.name)
		end
	end,
	getMovieHash = function()
		openSub.actionLabel = "Calculating movie hash"
		setMessage(openSub.actionLabel..": "..progressBarContent(0))

		local item = openSub.getInputItem()

		if not item then
			setError("Please use this method during playing")
			return false
		end

		openSub.getFileInfo()
		if openSub.file.protocol ~= "file" then
			setError("This method works with local file only (for now)")
			return false
		end

		local path = openSub.file.path
		if not path then
			setError("File not found")
			return false
		end

		local file = assert(io.open(path, "rb"))
		if not file then
			setError("File not found")
			return false
		end

        local lo,hi=0,0
        for i=1,8192 do
                local a,b,c,d = file:read(4):byte(1,4)
                lo = lo + a + b*256 + c*65536 + d*16777216
                a,b,c,d = file:read(4):byte(1,4)
                hi = hi + a + b*256 + c*65536 + d*16777216
                while lo>=4294967296 do
                        lo = lo-4294967296
                        hi = hi+1
                end
                while hi>=4294967296 do
                        hi = hi-4294967296
                end
        end
        local size = file:seek("end", -65536) + 65536
        for i=1,8192 do
                local a,b,c,d = file:read(4):byte(1,4)
                lo = lo + a + b*256 + c*65536 + d*16777216
                a,b,c,d = file:read(4):byte(1,4)
                hi = hi + a + b*256 + c*65536 + d*16777216
                while lo>=4294967296 do
                        lo = lo-4294967296
                        hi = hi+1
                end
                while hi>=4294967296 do
                        hi = hi-4294967296
                end
        end
        lo = lo + size
                while lo>=4294967296 do
                        lo = lo-4294967296
                        hi = hi+1
                end
                while hi>=4294967296 do
                        hi = hi-4294967296
                end

		openSub.file.bytesize = size
		openSub.file.hash = string.format("%08x%08x", hi,lo)

		return true
	end,
    loadSubtitles = function(url, SubFileName, target)
        openSub.actionLabel = "Downloading subtitle"
        setMessage(openSub.actionLabel..": "..progressBarContent(0))
        local subfileURI = nil
        local resp = get(url)
        if resp then
            local tmpFileName = openSub.file.dir..SubFileName..".zip"
            subfileURI = "zip://"..make_uri(tmpFileName, true).."!/"..SubFileName
            local tmpFile = assert(io.open(tmpFileName, "wb"))
            tmpFile:write(resp)
            tmpFile:flush()
            tmpFile:close()

            if target then
                local stream = vlc.stream(subfileURI)
                local data = ""
                local subfile = assert(io.open(target, "w")) -- FIXME: check for file presence before overwrite (maybe ask what to do)

                while data do
                    if openSub.conf.removeTag then
                        subfile:write(remove_tag(data).."\n")
                    else
                        subfile:write(data.."\n")
                    end
                    data = stream:readline()
                end

                subfile:flush()
                subfile:close()
                stream = nil
                collectgarbage()
				subfileURI = make_uri(target, true)
            end
			os.remove(tmpFileName)
        end

        if vlc.item or vlc.input.item() then
			vlc.msg.dbg("Adding subtitle :" .. subfileURI)
            vlc.input.add_subtitle(subfileURI)
            setMessage("Success: Subtitles loaded.")
        else
            setError("No current input, unable to add subtitles "..target)
        end
    end
}

function make_uri(str, encode)
    local windowdrive = string.match(str, "^(%a:/).+$")
	if encode then
		local encodedPath = ""
		for w in string.gmatch(str, "/([^/]+)") do
			vlc.msg.dbg(w)
			encodedPath = encodedPath.."/"..vlc.strings.encode_uri_component(w)
		end
		str = encodedPath
	end
    if windowdrive then
        return "file:///"..windowdrive..str
    else
        return "file://"..str
    end
end

function download_selection()
	local selection = widget.getVal("mainlist")
	if #selection > 0 and openSub.itemStore then
		download_subtitles(selection)
	end
end

function searchHash()
	openSub.sub.languageid = languages[widget.getVal("language")][2]

	message = widget.get("message")
	if message.display == "none" then
		message.display = "block"
		widget.set_interface(interface)
	end

	openSub.getMovieHash()

	if openSub.file.hash then
		openSub.request("SearchSubtitlesByHash")
		display_subtitles()
	end
end

function searchIMBD()
	openSub.movie.name = trim(widget.getVal("title"))
	openSub.movie.season = tonumber(widget.getVal("season"))
	openSub.movie.episode = tonumber(widget.getVal("episode"))
	openSub.sub.languageid  = languages[widget.getVal("language")][2]

	message = widget.get("message")
	if message.display == "none" then
		message.display = "block"
		widget.set_interface(interface)
	end

	if openSub.file.name ~= "" then
		openSub.request("SearchSubtitles")
		display_subtitles()
	end
end

function display_subtitles()
	local list = "mainlist"
	widget.setVal(list)	--~ Reset list
	if openSub.itemStore then
		for i, item in ipairs(openSub.itemStore) do
			widget.setVal(list, item.SubFileName.." ["..item.SubLanguageID.."] ("..item.SubSumCD.." CD)")
		end
	else
		widget.setVal(list, "No result")
	end
end

function download_subtitles(selection)
	local list = "mainlist"
	widget.resetSel(list) -- reset selection
	local index = selection[1][1]
	local item = openSub.itemStore[index]
	local subfileTarget = ""

	if openSub.conf.justgetlink
	or not (vlc.item or vlc.input.item())
	or not openSub.file.dir
	or not openSub.file.name then
		setMessage("Link : <a href='"..item.ZipDownloadLink.."'>"..item.ZipDownloadLink.."</a>")
	else
		subfileTarget = openSub.file.dir..openSub.file.name.."."..item.SubLanguageID.."."..item.SubFormat
		vlc.msg.dbg("subfileTarget: "..subfileTarget)
		openSub.loadSubtitles(item.ZipDownloadLink, item.SubFileName, subfileTarget)
	end
end

widget = {
	stack = {},
    meta = {},
	registered_table = {},
	main_table = {},
	set_node = function(node, parent)
		local left = parent.left
		for k, l in pairs(node) do --parse items
			local tmpTop = parent.height
			local tmpLeft = left
			local ltmpLeft = l.left
			local ltmpTop = l.top
			local tmphidden = l.hidden

			l.top = parent.height + parent.top
			l.left = left
			l.parent = parent

			if l.display == "none" or parent.hidden then
				l.hidden = true
			else
				l.hidden = false
			end

			if l.type == "div" then --that's a container
				l.display = (l.display or "block")
				l.height = 1
				for _, newNode in ipairs(l.content) do --parse lines
					widget.set_node(newNode, l)
					l.height = l.height+1
				end
				l.height = l.height - 1
				left = left - 1
			else --that's an item
				l.display = (l.display or "inline")

				if not l.input then
					tmphidden = true
				end

				if tmphidden and not l.hidden then --~ create
					widget.create(l)
				elseif not tmphidden and l.hidden then --~ destroy
					widget.destroy(l)
				end

				if not l.hidden and (ltmpTop ~= l.top or ltmpLeft ~= l.left) then
					if l.input then --~ destroy
						widget.destroy(l)
					end
					--~ recreate
					widget.create(l)
				end
			end

			--~  Store reference ID
			if l.id and not widget.registered_table[l.id] then
				widget.registered_table[l.id] = l
			end

			if l.display == "block" then
				parent.height = parent.height + (l.height or 1)
				left = parent.left
			elseif l.display == "none" then
				parent.height = (tmpTop or parent.height)
				left = (tmpLeft or left)
			elseif l.display == "inline" then
				left = left + (l.width or 1)
			end
		end
	end,
	set_interface = function(intf_map)
		local root = {left = 1, top = 0, height = 0, hidden = false}
		widget.set_node(intf_map, root)
		widget.force_refresh()
	end,
	force_refresh = function() --~ Hacky
		if refresh_toggle then
			refresh_toggle = false
			dlg:set_title(openSub.conf.useragent)
		else
			refresh_toggle = true
			dlg:set_title(openSub.conf.useragent.." ")
		end
	end,
	destroy = function(w)
		dlg:del_widget(w.input)
		if widget.registered_table[w.id] then
			widget.registered_table[w.id] = nil
		end
	end,
	create = function(w)
		local cur_widget
		if w.type == "button" then
			cur_widget = dlg:add_button(w.value or "", w.callback, w.left, w.top, w.width or 1, w.height or 1)
		elseif w.type == "label" then
			cur_widget = dlg:add_label(w.value or "", w.left, w.top, w.width or 1, w.height or 1)
		elseif w.type == "html" then
			cur_widget = dlg:add_html(w.value or "", w.left, w.top, w.width or 1, w.height or 1)
		elseif w.type == "text_input" then
			cur_widget = dlg:add_text_input(w.value or "", w.left, w.top, w.width or 1, w.height or 1)
		elseif w.type == "password" then
			cur_widget = dlg:add_password(w.value or "", w.left, w.top, w.width or 1, w.height or 1)
		elseif w.type == "check_box" then
			cur_widget = dlg:add_check_box(w.value or "", w.left, w.top, w.width or 1, w.height or 1)
		elseif w.type == "dropdown" then
			cur_widget = dlg:add_dropdown(w.left, w.top, w.width or 1, w.height or 1)
		elseif w.type == "list" then
			cur_widget = dlg:add_list(w.left, w.top, w.width or 1, w.height or 1)
		elseif w.type == "image" then

		end

		if w.type == "dropdown" or w.type == "list" then
			if type(w.value) == "table" then
				for k, l in ipairs(w.value) do
					if type(l) == "table" then
						cur_widget:add_value(l[1], k)
					else
						cur_widget:add_value(l, k)
					end
				end
			end
		end

		if w.type and w.type ~= "div" then
			w.input = cur_widget
		end
	end,
	get = function(h)
		 return widget.registered_table[h]
	end,
	setVal = function(h, val, index)
		widget.set_val(widget.registered_table[h], val, index)
	end,
	set_val = function(w, val, index)
		local input = w.input
		local t = w.type
		if t == "button" or
		t == "label" or
		t == "html" or
		t == "text_input" or
		t == "password" then
			if type(val) == "string" then
				input:set_text(val)
				w.value = val
			end
		elseif t == "check_box" then
			if type(val) == "bool" then
				input:set_checked(val)
			else
				input:set_text(val)
			end
		elseif t == "dropdown" or t == "list" then
			if val and index then
				input:add_value(val, index)
				w.value[index] = val
			elseif val and not index then
				if type(val) == "table" then
					for k, l in ipairs(val) do
						input:add_value(l, k)
						table.insert(w.value, l)
					end
				else
					input:add_value(val, #w.value+1)
					table.insert(w.value, val)
				end
			elseif not val and not index then
				input:clear()
				w.value = nil
				w.value = {}
			end
		end
	end,
	getVal = function(h, typeval)
		if not widget.registered_table[h] then print(h) return false end
		return widget.get_val(widget.registered_table[h], typeval)
	end,
	get_val = function(w, typeval)
		local input = w.input
		local t = w.type

		if t == "button" or
		   t == "label" or
		   t == "html" or
		   t == "text_input" or
		   t == "password" then
			return input:get_text()
		elseif t == "check_box" then
			if typeval == "checked" then
				return input:get_checked()
			else
				return input:get_text()
			end
		elseif t == "dropdown" then
			return input:get_value()
		elseif t == "list" then
			local selection = input:get_selection()
			local output = {}

			for index, name in  pairs(selection)do
				table.insert(output, {index, name})
			end
			return output
		end
	end,
	resetSel = function(h, typeval)
		local w = widget.registered_table[h]
		local val = w.value
		widget.set_val(w)
		widget.set_val(w, val)
	end
}

function create_dialog()
	dlg = vlc.dialog(openSub.conf.useragent)
	widget.set_interface(interface)
end


function toggle_help()
	helpMessage = widget.get("helpMessage")
	if helpMessage.display == "block" then
		helpMessage.display = "none"
	elseif helpMessage.display == "none" then
		helpMessage.display = "block"
	end

	widget.set_interface(interface)
end


function set_interface()  --~ old
	local method_index = widget.getVal("method")
	local method_id = methods[method_index][2]
	if tmp_method_id then
		if tmp_method_id == method_id then
			return false
		end
		widget.get(tmp_method_id).display = "none"
	else
		openSub.request("LogIn")
	end
	tmp_method_id = method_id
	widget.get(method_id).display = "block"
	widget.set_interface(interface)
	setMessage("")

	if method_id == "hash" then
		searchHash()
	elseif method_id == "imdb" then
		if openSub.file.name and not hasAssociatedResult() then
			associatedResult()
			widget.get("title").input:set_text(openSub.movie.name)
			widget.get("season").input:set_text(openSub.movie.seasonNumber)
			widget.get("episode").input:set_text(openSub.movie.episodeNumber)
		end
	end
end

function progressBarContent(pct)
	local content = "<span style='background-color:#181;color:#181;'>"
	local accomplished = math.ceil(progressBarSize*pct/100)

	local left = progressBarSize - accomplished
	content = content .. string.rep ("-", accomplished)
	content = content .. "</span><span style='background-color:#fff;color:#fff;'>"
	content = content .. string.rep ("-", left)
	content = content .. "</span>"
	return content
end

function setError(str)
	setMessage("<span style='color:#B23'>Error: "..str.."</span>")
end

function setMessage(str)
	if widget.get("message") then
		widget.setVal("message", str)
		dlg:update()
	end
end

--~ Misc utils

function file_exists(name)
	local f=io.open(name ,"r")
	if f~=nil then
		io.close(f)
		vlc.msg.dbg("File found!" .. name)
		return true
	else
		vlc.msg.dbg("File not found. "..name)
		return false
	end
end

function wait(seconds)
	local _start = os.time()
	local _end = _start+seconds
	while (_end ~= os.time()) do
	end
end

function trim(str)
    if not str then return "" end
    return string.gsub(str, "^%s*(.-)%s*$", "%1")
end

function remove_tag(str)
	return string.gsub(str, "{[^}]+}", "")
end

--~ Network utils

function get(url)
	local host, path = parse_url(url)
	local header = {
		"GET "..path.." HTTP/1.1",
		"Host: "..host,
		"User-Agent: "..openSub.conf.userAgentHTTP,
		"",
		""
	}
	local request = table.concat(header, "\r\n")

	local response
	local status, response = http_req(host, 80, request)

	if status == 200 then
		return response
	else
		return false
	end
end

function http_req(host, port, request)
	local fd = vlc.net.connect_tcp(host, port)
	if fd >= 0 then
		local pollfds = {}

		pollfds[fd] = vlc.net.POLLIN
		vlc.net.send(fd, request)
		vlc.net.poll(pollfds)

		local response = vlc.net.recv(fd, 1024)
		local headerStr, body = string.match(response, "(.-\r?\n)\r?\n(.*)")
		local header = parse_header(headerStr)
		local contentLength = tonumber(header["Content-Length"])
		local TransferEncoding = header["Transfer-Encoding"]
		local status = tonumber(header["statuscode"])
		local bodyLenght = string.len(body)
		local pct = 0

		if status ~= 200 then return status end

		while contentLength and bodyLenght < contentLength do
			vlc.net.poll(pollfds)
			response = vlc.net.recv(fd, 1024)

			if response then
				body = body..response
			else
				vlc.net.close(fd)
				return false
			end
			bodyLenght = string.len(body)
			pct = bodyLenght / contentLength * 100
			setMessage(openSub.actionLabel..": "..progressBarContent(pct))
		end
		vlc.net.close(fd)

		return status, body
	end
	return ""
end

function parse_header(data)
	local header = {}

	for name, s, val in string.gfind(data, "([^%s:]+)(:?)%s([^\n]+)\r?\n") do
		if s == "" then header['statuscode'] =  tonumber(string.sub (val, 1 , 3))
		else header[name] = val end
	end
	return header
end

function parse_url(url)
	local url_parsed = vlc.net.url_parse(url)
	return  url_parsed["host"], url_parsed["path"], url_parsed["option"]
end

--~ XML utils

function parse_xml(data)
	local tree = {}
	local stack = {}
	local tmp = {}
	local level = 0

	table.insert(stack, tree)

	for op, tag, p, empty, val in string.gmatch(data, "<(%/?)([%w:]+)(.-)(%/?)>[%s\r\n\t]*([^<]*)") do
		if op=="/" then
			if level>1 then
				level = level - 1
				table.remove(stack)
			end
		else
			level = level + 1
			if val == "" then
				if type(stack[level][tag]) == "nil" then
					stack[level][tag] = {}
					table.insert(stack, stack[level][tag])
				else
					if type(stack[level][tag][1]) == "nil" then
						tmp = nil
						tmp = stack[level][tag]
						stack[level][tag] = nil
						stack[level][tag] = {}
						table.insert(stack[level][tag], tmp)
					end
					tmp = nil
					tmp = {}
					table.insert(stack[level][tag], tmp)
					table.insert(stack, tmp)
				end
			else
				if type(stack[level][tag]) == "nil" then
					stack[level][tag] = {}
				end
				stack[level][tag] = vlc.strings.resolve_xml_special_chars(val)
				table.insert(stack,  {})
			end
			if empty ~= "" then
				stack[level][tag] = ""
				level = level - 1
				table.remove(stack)
			end
		end
	end
	return tree
end

function parse_xmlrpc(data)
	local tree = {}
	local stack = {}
	local tmp = {}
	local tmpTag = ""
	local level = 0
	table.insert(stack, tree)

	for op, tag, p, empty, val in string.gmatch(data, "<(%/?)([%w:]+)(.-)(%/?)>[%s\r\n\t]*([^<]*)") do
		if op=="/" then
			if tag == "member" or tag == "array" then
				if level>0  then
					level = level - 1
					table.remove(stack)
				end
			end
		elseif tag == "name" then
			level = level + 1
			if val~=""then tmpTag  = vlc.strings.resolve_xml_special_chars(val) end

			if type(stack[level][tmpTag]) == "nil" then
				stack[level][tmpTag] = {}
				table.insert(stack, stack[level][tmpTag])
			else
				tmp = nil
				tmp = {}
				table.insert(stack[level-1], tmp)

				stack[level] = nil
				stack[level] = tmp
				table.insert(stack, tmp)
			end
			if empty ~= "" then
				level = level - 1
				stack[level][tmpTag] = ""
				table.remove(stack)
			end
		elseif tag == "array" then
			level = level + 1
			tmp = nil
			tmp = {}
			table.insert(stack[level], tmp)
			table.insert(stack, tmp)
		elseif val ~= "" then
			stack[level][tmpTag] = vlc.strings.resolve_xml_special_chars(val)
		end
	end
	return tree
end

function dump_xml(data)
	local level = 0
	local stack = {}
	local dump = ""

	local function parse(data, stack)
		for k,v in pairs(data) do
			if type(k)=="string" then
				dump = dump.."\r\n"..string.rep (" ", level).."<"..k..">"
				table.insert(stack, k)
				level = level + 1
			elseif type(k)=="number" and k ~= 1 then
				dump = dump.."\r\n"..string.rep (" ", level-1).."<"..stack[level]..">"
			end

			if type(v)=="table" then
				parse(v, stack)
			elseif type(v)=="string" then
				dump = dump..vlc.strings.convert_xml_special_chars(v)
			elseif type(v)=="number" then
				dump = dump..v
			end

			if type(k)=="string" then
				if type(v)=="table" then
					dump = dump.."\r\n"..string.rep (" ", level-1).."</"..k..">"
				else
					dump = dump.."</"..k..">"
				end
				table.remove(stack)
				level = level - 1

			elseif type(k)=="number" and k ~= #data then
				if type(v)=="table" then
					dump = dump.."\r\n"..string.rep (" ", level-1).."</"..stack[level]..">"
				else
					dump = dump.."</"..stack[level]..">"
				end
			end
		end
	end
	parse(data, stack)
	return dump
end

--~ Interface data

languages = {
	{'All', 'all'},
	{'Albanian', 'alb'},
	{'Arabic', 'ara'},
	{'Armenian', 'arm'},
	{'Malay', 'may'},
	{'Bosnian', 'bos'},
	{'Bulgarian', 'bul'},
	{'Catalan', 'cat'},
	{'Basque', 'eus'},
	{'Chinese (China)', 'chi'},
	{'Croatian', 'hrv'},
	{'Czech', 'cze'},
	{'Danish', 'dan'},
	{'Dutch', 'dut'},
	{'English (US)', 'eng'},
	{'English (UK)', 'bre'},
	{'Esperanto', 'epo'},
	{'Estonian', 'est'},
	{'Finnish', 'fin'},
	{'French', 'fre'},
	{'Galician', 'glg'},
	{'Georgian', 'geo'},
	{'German', 'ger'},
	{'Greek', 'ell'},
	{'Hebrew', 'heb'},
	{'Hungarian', 'hun'},
	{'Indonesian', 'ind'},
	{'Italian', 'ita'},
	{'Japanese', 'jpn'},
	{'Kazakh', 'kaz'},
	{'Korean', 'kor'},
	{'Latvian', 'lav'},
	{'Lithuanian', 'lit'},
	{'Luxembourgish', 'ltz'},
	{'Macedonian', 'mac'},
	{'Norwegian', 'nor'},
	{'Persian', 'per'},
	{'Polish', 'pol'},
	{'Portuguese (Portugal)', 'por'},
	{'Portuguese (Brazil)', 'pob'},
	{'Romanian', 'rum'},
	{'Russian', 'rus'},
	{'Serbian', 'scc'},
	{'Slovak', 'slo'},
	{'Slovenian', 'slv'},
	{'Spanish (Spain)', 'spa'},
	{'Swedish', 'swe'},
	{'Thai', 'tha'},
	{'Turkish', 'tur'},
	{'Ukrainian', 'ukr'},
	{'Vietnamese', 'vie'}
}

methods = {
	{"Video hash", "hash"},
	{"IMDB ID", "imdb"}
}

interface = {
	{
		id = "header",
		type = "div",
		content = {
			{
				{ type = "label", value = "Language:" },
				{ type = "dropdown", value = languages, id = "language" , width = 2 },
				{ type = "button", value = "Search by hash", callback = searchHash },
			}
		}
	},
	{
		id = "imdb",
		type = "div",
		content = {
			{
				{ type = "label", value = "Title:"},
				{ type = "text_input", value = openSub.movie.name or "", id = "title", width = 2 },
				{ type = "button", value = "Search by name", callback = searchIMBD }
			},{
				{ type = "label", value = "Season (series):"},
				{ type = "text_input", value = openSub.movie.seasonNumber or "", id = "season", width = 2 }
			},{
				{ type = "label", value = "Episode (series):"},
				{ type = "text_input", value = openSub.movie.episodeNumber or "", id = "episode", width = 2 }
			},{
				{ type = "list", width = 4, id = "mainlist" }
			}
		}
	},
	{
		type = "div",
		content = {
			{
				{ type = "button", value = "Help", callback = toggle_help },
				{ type = "span", width = 1},
				{ type = "button", value = "Download", callback = download_selection },
				{ type = "button", value = "Close", callback = close }
			}
		}
	},
	{
		id = "progressBar",
		type = "div",
		content = {
			{
				{ type = "html", width = 4,
					display = "none",
					value = ""..
					" Download subtittles from <a href='http://www.opensubtitles.org/'>opensubtitles.org</a> and display them while watching a video.<br>"..
					" <br>"..
					" <b><u>Usage:</u></b><br>"..
					" <br>"..
					" VLSub is meant to be used while your watching the video, so start it first (if nothing is playing you will get a link to download the subtitles in your browser).<br>"..
					" <br>"..
					" Choose the language for your subtitles and click on the button corresponding to one of the two research method provided by VLSub:<br>"..
					" <br>"..
					" <b>Method 1: Search by hash</b><br>"..
					" It is recommended to try this method first, because it performs a research based on the video file print, so you can find subtitles synchronized with your video.<br>"..
					" <br>"..
					" <b>Method 2: Search by name</b><br>"..
					" If you have no luck with the first method, just check the title is correct before clicking. If you search subtitles for a serie, you can also provide a season and episode number.<br>"..
					" <br>"..
					" <b>Downloading Subtitles</b><br>"..
					" Select one subtitle in the list and click on 'Download'.<br>"..
					" It will be put in the same directory that your video, with the same name (different extension)"..
					" so Vlc will load them automatically the next time you'll start the video.<br>"..
					" <br>"..
					" <b>/!\\ Beware :</b> Existing subtitles are overwrited without asking confirmation, so put them elsewhere if thet're important."
					, id = "helpMessage"
				}
			},{
				{
					type = "label",
					width = 4,
					display = "none",
					value = "  ",
					id = "message"
				}
			}
		}
	}
}
