-- Global variables
dlg = nil
dialog_is_opened = false
dialog_is_hidden = false
update_title_needed = false
website = nil
language = nil
main_text_input = nil
search_button = nil
load_button = nil
subtitles_list = nil
subtitles_result = nil
type_text_input = nil

-- Extension description
function descriptor()
  return {
		title = "AutoSubtitles";
		version = "1";
		author = "jean caffou";
		url = 'http://www.kafol.net';
		description = "";
		shortdesc = "";
		capabilities = { "input-listener" ; "meta-listener" }
	}
end

-- Get clean title from filename
function get_title(str)
    local item = vlc.item or vlc.input.item()
    if not item then
        return ""
    end
    local metas = item:metas()
    if metas["title"] then
        return metas["title"]
    else
        local filename = string.gsub(item:name(), "^(.+)%.%w+$", "%1")
        return trim(filename or item:name())
    end
end

-- Remove leading and trailing spaces
function trim(str)
    if not str then return "" end
    return string.gsub(str, "^%s*(.-)%s*$", "%1")
end

-- Function triggered when the extension is activated
function activate()
	new_dialog("Download subtitles")
	return show_dialog_download()
end

-- Function triggered when the extension is deactivated
function deactivate()
	if dialog_is_opened then
		close()
	else
		reset_variables()
		dlg = nil
	end
	return true
end

-- self explanatory
function reset_variables()
	update_title_needed = false
	website = nil
	language = nil
	main_text_input = nil
	search_button = nil
	load_button = nil
	subtitles_list = nil
	subtitles_result = nil
	type_text_input = nil
end

-- Function triggered when the dialog is closed
function close()
	return true
end

-- Current input changed
function input_changed()
	vlc.msg.dbg("Input is changed")
	update_title()
	click_search()
end

-- Update title in search dialog
function update_title()
	if dialog_is_hidden or not update_title_needed then return true end
	main_text_input:set_text(get_title())
	dlg:update()
	return false
end

function show_dialog_download()
	-- column, row, colspan, rowspan
	dlg:add_label("<right><b>Database: </b></right>", 1, 1, 1, 1)
	website = dlg:add_dropdown(2, 1, 3, 1)

	dlg:add_label("<right><b>Language: </b></right>", 1, 2, 1, 1)
	language = dlg:add_dropdown(2, 2, 3, 1)

	dlg:add_label("<right><b>Search: </b></right>", 1, 3, 1, 1)
	main_text_input = dlg:add_text_input("", 2, 3, 1, 1)
	search_button = dlg:add_button("Search", click_search, 3, 3, 1, 1)
	--dlg:add_button("Hide", hide_dialog, 4, 3, 1, 1)

	for idx, ws in ipairs(websites) do
		website:add_value(ws.title, idx)
	end
	for idx, ws in ipairs(languages) do
		language:add_value(ws.title, idx)
	end

	update_title_needed = true
	update_title()
	click_search()
	dlg:update()
	return true
end

function new_dialog(title)
	if(dlg == nil) then
		dlg = vlc.dialog(title)
	end
end

function hide_dialog()
	dialog_is_hidden = true
	dlg:hide()
end

function click_search()
	local search_term = main_text_input:get_text()
	if(search_term == "") then return false end

	local old_button_name = search_button:get_text()
	search_button:set_text("Wait...")
	if subtitles_list ~= nil then subtitles_list:clear() end
	dlg:update()

	subtitles_result = nil

	local idx = website:get_value()
	local idx2 = language:get_value()
	if idx < 1 or idx2 < 1 then vlc.msg.err("Invalid index in dropdown") search_button:set_text(old_button_name) return false end

	local ws = websites[idx]
	local lang = languages[idx2]
	local url = ws.urlfunc(search_term,lang.tag)

	-- vlc.msg.info("Url: '" .. url .. "'")
	local stream = vlc.stream(url)
	if stream == nil then vlc.msg.err("The site of subtitles isn't reachable") search_button:set_text(old_button_name) return false end

	local reading = "blah"
	local xmlpage = ""
	while(reading ~= nil and reading ~= "") do
		reading = stream:read(65653)
		if(reading) then
			xmlpage = xmlpage .. reading
		end
	end
	if xmlpage == "" then search_button:set_text(old_button_name) return false end

	subtitles_result = ws.parsefunc(xmlpage)

	if subtitles_list == nil then
		subtitles_list = dlg:add_list(1, 4, 4, 1)
		load_button = dlg:add_button("Load selected subtitles", click_load_from_search_button, 1, 5, 4, 1)
	end

	if not subtitles_result then
		subtitles_result = {}
		subtitles_result[1]= { url = "-1" }
		subtitles_list:add_value("Nothing found", 1)
		search_button:set_text(old_button_name)
		dlg:update()
		return false
	end

	for idx, res in ipairs(subtitles_result) do
		if(not res.language or lang.tag == "all" or lang.tag == res.language) then
			subtitles_list:add_value("["..res.language.."] "..res.name, idx)
		end
	end

	search_button:set_text(old_button_name)
	dlg:update()

	load_first_result()

	return true
end

function load_unknown_subtitles(url, language)
	vlc.msg.dbg("Loading "..language.." subtitle: "..url)
	vlc.input.add_subtitle(url)
end

function load_subtitles_in_the_archive(dataBuffer, language)
	local buffer_length = dataBuffer:len()
	local files_found_in_the_compressed_file = 0
	local subtitles_found_in_the_compressed_file = 0
	local endIdx = 1
	local srturl, extension

	-- Find subtitles
	while(endIdx < buffer_length) do
		_, endIdx, srturl, extension = dataBuffer:find("<location>([^<]+)%.(%a%a%a?)</location>", endIdx)
		if(srturl == nil ) then break end

		--vlc.msg.dbg("File found in the archive: " .. srturl .. extension)
		files_found_in_the_compressed_file = files_found_in_the_compressed_file + 1
		srturl = string.gsub(srturl, "^(%a%a%a)://", "%1://http://")

		if(extension == "ass" or extension == "ssa" or extension == "srt" or extension == "smi" or extension == "sub" or extension == "rt" or extension == "txt" or extension == "mpl") then
			subtitles_found_in_the_compressed_file = subtitles_found_in_the_compressed_file + 1
			vlc.msg.dbg("Loading "..language.." subtitle: "..srturl)
			vlc.input.add_subtitle(srturl.."."..extension)
		end
	end
	vlc.msg.info("Files found in the compressed file: "..files_found_in_the_compressed_file)
	vlc.msg.info("Subtitles found in the compressed file: "..subtitles_found_in_the_compressed_file)

	if(subtitles_found_in_the_compressed_file > 0) then return true end

	vlc.msg.warn("No subtitles found in the compressed file")
	return false
end

function parse_archive(url, language)
	if url == "-1" then vlc.msg.dbg("Dummy result") return true end

	local stream = vlc.stream(url)
	if stream == nil then vlc.msg.err("The site of subtitles isn't reachable") return false end
	stream:addfilter("zip,stream_filter_rar")

	local data = stream:read(2048)
	if(data == nil or data:find("<?xml version", 1, true) ~= 1) then
		vlc.msg.info("Type: RAR or unknown file")
		load_unknown_subtitles(url, language)
	else
		vlc.msg.info("Type: ZIP file")
		local dataBuffer = ""
		while(data ~= nil and data ~= "") do
			vlc.msg.dbg("Buffering...")
			dataBuffer = dataBuffer..data
			data = stream:read(8192)
		end
		load_subtitles_in_the_archive(dataBuffer, language)
	end
	--vlc.msg.dbg("Subtitle data: "..dataBuffer)

	return true
end

function click_load_from_search_button()
	vlc.msg.dbg("Clicked load button from \"Download subtitles\" dialog")
	if(not vlc.input.is_playing()) then
		vlc.msg.warn("You cannot load subtitles if you aren't playing any file")
		return true
	end
	local old_button_name = load_button:get_text()
	load_button:set_text("Wait...")
	dlg:update()

	local selection = subtitles_list:get_selection()
	local index, name

	for index, name in pairs(selection) do
		vlc.msg.dbg("Selected the item "..index.." with the name: "..name)
		vlc.msg.dbg("URL: "..subtitles_result[index].url)

		parse_archive(subtitles_result[index].url, subtitles_result[index].language) -- ZIP, RAR or unknown file
	end

	load_button:set_text(old_button_name)
	dlg:update()
	return true
end

function load_first_result()
	vlc.msg.dbg("Loading first result")
	if(not vlc.input.is_playing()) then
		vlc.msg.warn("You cannot load subtitles if you aren't playing any file")
		return true
	end

	local old_button_name = load_button:get_text()
	load_button:set_text("Wait...")
	dlg:update()

	parse_archive(subtitles_result[1].url, subtitles_result[1].language)

	load_button:set_text(old_button_name)
	dlg:update()
	return true
end

function click_load_from_url_button()
	vlc.msg.dbg("Clicked load button in \"Load subtitles from url...\" dialog")
	if(not vlc.input.is_playing()) then
		vlc.msg.warn("You cannot load subtitles if you aren't playing any file")
		return true
	end
	local old_button_name = load_button:get_text()
	load_button:set_text("Wait...")
	type_text_input:set_text("")
	dlg:update()

	local url_to_load = main_text_input:get_text()
	if(url_to_load == "") then return false end
	vlc.msg.dbg("URL: "..url_to_load)

	local _, ext_pos, extension = url_to_load:find("%.(%a%a%a?)", -4)

	if(ext_pos == url_to_load:len()) then
		type_text_input:set_text(extension)
		if(extension == "ass" or extension == "ssa" or extension == "srt" or extension == "smi" or extension == "sub" or extension == "rt" or extension == "txt" or extension == "mpl") then
			load_button:set_text(old_button_name)
			dlg:update()
			return vlc.input.add_subtitle(url_to_load)
		end
	end

	local result = parse_archive(url_to_load, "")
	if not result then
		vlc.msg.info("Waiting 5 seconds before retry...")
		result = parse_archive(url_to_load, "")
	end

	load_button:set_text(old_button_name)
	dlg:update()
	return result
end



-- XML Parsing
function parseargs(s)
	local arg = {}
	string.gsub(s, "(%w+)=([\"'])(.-)%2", function (w, _, a)
		arg[w] = a
	end)
	return arg
end

function collect(s)
	local stack = {}
	local top = {}
	table.insert(stack, top)
	local ni,c,label,xarg, empty
	local i, j = 1, 1
	while true do
		ni,j,c,label,xarg, empty = string.find(s, "<(%/?)([%w:]+)(.-)(%/?)>", i)
		if not ni then break end
		local text = string.sub(s, i, ni-1)
		if not string.find(text, "^%s*$") then
			table.insert(top, text)
		end
		if empty == "/" then -- empty element tag
			table.insert(top, {label=label, xarg=parseargs(xarg), empty=1})
		elseif c == "" then -- start tag
			top = {label=label, xarg=parseargs(xarg)}
			table.insert(stack, top) -- new level
		else -- end tag
			local toclose = table.remove(stack) -- remove top
			top = stack[#stack]
			if #stack < 1 then
				error("nothing to close with "..label)
			end
			if toclose.label ~= label then
				error("trying to close "..toclose.label.." with "..label)
			end
			table.insert(top, toclose)
		end
		i = j+1
	end
	local text = string.sub(s, i)
	if not string.find(text, "^%s*$") then
		table.insert(stack[#stack], text)
	end
	if #stack > 1 then
		error("unclosed "..stack[stack.n].label)
	end
	return stack[1]
end

function urlOpenSub(search_term,lang)
	-- base = "http://api.opensubtitles.org/en/search/"
	search_term = string.gsub(search_term, "%%", "%%37")
	search_term = string.gsub(search_term, " ", "%%20")
	return "http://kafol.net/code/subtitles/search.php?s=" .. search_term .. "&l=" .. lang
	-- return base .. "moviename-" .. search_term .. "/simplexml"
	-- http://api.opensubtitles.org/en/search/moviename- .. search_term .. /simplexml
end

function parseOpenSub(xmltext)
	vlc.msg.dbg("Parsing XML data...")
	local xmltext = string.gsub(xmltext, "<%?xml version=\"1%.0\" encoding=\"utf-8\"%?>", "")
	local xmldata = collect(xmltext)
	for a,b in pairs(xmldata) do
		if type(b) == "table" then
			if b.label == "search" then
				xmldata = b
				break
			end
		end
	end


	if xmldata == nil then return nil end

	-- Subtitles information data
	local subname = {}
	local sub_movie = {}
	local suburl = {}
	local sublang = {}
	local sub_language = {}
	local subformat = {}
	local subfilenum = {}
	local subnum = 1
	local baseurl = ""

	-- Let's browse iteratively the 'xmldata' tree
	-- OK, the variables' names aren't explicit enough, but just remember a couple
	-- a,b contains the index (a) and the data (b) of the table, which might also be a table
	for a,b in pairs(xmldata) do
		if type(b) == "table" then
			if b.label == "results" then
				for c,d in pairs(b) do
					if type(d) == "table" then
						if d.label == "subtitle" then
							for e,f in pairs(d) do
								if type(f) == "table" then
									if f.label == "releasename" then
										if f[1] ~= nil then subname[subnum] = f[1]
										else subname[subnum] = "" end
									elseif f.label == "movie" then
										if f[1] ~= nil then sub_movie[subnum] = f[1]
										else sub_movie[subnum] = "" end
									elseif f.label == "download" then
										if f[1] ~= nil then suburl[subnum] = f[1]
										else suburl[subnum] = "" end
									elseif f.label == "iso639" then	-- two letter language code
										if f[1] ~= nil then sublang[subnum] = f[1]
										else sublang[subnum] = "" end
									elseif f.label == "language" then
										if f[1] ~= nil then sub_language[subnum] = f[1]
										else sub_language[subnum] = "" end
									elseif f.label == "format" then
										if f[1] ~= nil then subformat[subnum] = f[1]
										else subformat[subnum] = "" end
									end
								end
							end
							subnum = subnum + 1
						end
					end
				end
			elseif b.label == "base" then
				baseurl = b[1]
			end
		end
	end

	if subnum <= 1 then
		return nil
	end

	ret = {}

	for i = 1,(subnum - 1) do
		fullURL = suburl[i] -- baseurl .. "/" .. suburl[i]
		realName = string.gsub( subname[i], "<..CDATA.", "" )
		realName = string.gsub( realName, "..>", "" )
		if realName == "" then
			realName = string.gsub( sub_movie[i], "<..CDATA.", "" )
			realName = string.gsub( realName, "..>", "" )
		end

		ret[i] = { name = realName,
			   url = fullURL,
			   language = sublang[i],
			   extension = ".zip" 
		}

		vlc.msg.dbg("Found subtitle " .. i .. ": ")
		vlc.msg.dbg(realName)
		vlc.msg.dbg(fullURL)
	end

	return ret
end

-- These tables must be after all function definitions
websites = {
	{ title = "Kafol.net",
	  urlfunc = urlOpenSub,
	  parsefunc = parseOpenSub } --[[;
	{ title = "Fake (OS)",
	  urlfunc = url2,
	  parsefunc = parse2 }]]
}

languages = {
	{ title = "English", tag = "en" },
	-- { title = "All", tag = "all" },
	{ title = "Slovenian", tag = "sl" },
	{ title = "French", tag = "fr" }
