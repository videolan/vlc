--[[
 VLSub Extension for VLC media player 1.1 and 2.0
 Copyright 2013 Guillaume Le Maout

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


						--[[ Global var ]]--

-- You can set here your default language by replacing nil with your language code (see below)
-- Example:
-- language = "fre",
-- language = "ger",
-- language = "eng",
-- ...

local options = {
	language = nil,
	downloadBehaviour = 'save',
	langExt = false,
	removeTag = false,
	showMediaInformation = true,
	progressBarSize = 80,
	intLang = 'eng',
	translations_avail = {
		eng = 'English',
		cze = 'Czech',
		dan = 'Danish',
		fre = 'Français',
		ell = 'Greek',
		baq = 'Basque',
		pob = 'Brazilian Portuguese',
		slo = 'Slovak',
		spa = 'Spanish',
		swe = 'Swedish',
		ukr = 'Ukrainian'
	},
	translation = {
		int_all = 'All',
		int_descr = 'Download subtitles from OpenSubtitles.org',
		int_research = 'Research',
		int_config = 'Config',
		int_configuration = 'Configuration',
		int_help = 'Help',
		int_search_hash = 'Search by hash',
		int_search_name = 'Search by name',
		int_title = 'Title',
		int_season = 'Season (series)',
		int_episode = 'Episode (series)',
		int_show_help = 'Show help',
		int_show_conf = 'Show config',
		int_dowload_sel = 'Download selection',
		int_close = 'Close',
		int_ok = 'Ok',
		int_save = 'Save',
		int_cancel = 'Cancel',
		int_bool_true = 'Yes',
		int_bool_false = 'No',
		int_search_transl = 'Search translations',
		int_searching_transl = 'Searching translations ...',
		int_int_lang = 'Interface language',
		int_default_lang = 'Subtitles language',
		int_dowload_behav = 'What to do with subtitles',
		int_dowload_save = 'Load and save',
		int_dowload_load = 'Load only',
		int_dowload_manual =  'Manual download',
		int_display_code = 'Display language code in file name',
		int_remove_tag = 'Remove tags',
		int_vlsub_work_dir = 'VLSub working directory',
		int_os_username = 'Username',
		int_os_password = 'Password',
		int_help_mess = " Download subtittles from <a href='http://www.opensubtitles.org/'>opensubtitles.org</a> and display them while watching a video.<br>"..
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
			" <b>/!\\ Beware :</b> Existing subtitles are overwrited without asking confirmation, so put them elsewhere if thet're important.<br>"..
			" <br>"..
			" Find more Vlc extensions at <a href='http://addons.videolan.org'>addons.videolan.org</a>.",

		action_login = 'Logging in',
		action_logout = 'Logging out',
		action_noop = 'Checking session',
		action_search = 'Searching subtitles',
		action_hash = 'Calculating movie hash',

		mess_success = 'Success',
		mess_error = 'Error',
		mess_no_response = 'Server not responding',
		mess_unauthorized = 'Request unauthorized',
		mess_expired = 'Session expired, retrying',
		mess_overloaded = 'Server overloaded, please retry later',
		mess_no_input = 'Please use this method during playing',
		mess_not_local = 'This method works with local file only (for now)',
		mess_not_found = 'File not found',
		mess_not_found2 = 'File not found (illegal character?)',
		mess_no_selection = 'No subtitles selected',
		mess_save_fail = 'Unable to save subtitles',
		mess_click_link = 'Click here to open the file',
		mess_complete = 'Research complete',
		mess_no_res = 'No result',
		mess_res = 'result(s)',
		mess_loaded = 'Subtitles loaded',
		mess_downloading = 'Downloading subtitle',
		mess_dowload_link = 'Download link',
		mess_err_conf_access ='Can\'t fount a suitable path to save config, please set it manually',
		mess_err_wrong_path ='the path contains illegal character, please correct it'
	}
}

local languages = {
	{'alb', 'Albanian'},
	{'ara', 'Arabic'},
	{'arm', 'Armenian'},
	{'baq', 'Basque'},
	{'ben', 'Bengali'},
	{'bos', 'Bosnian'},
	{'bre', 'Breton'},
	{'bul', 'Bulgarian'},
	{'bur', 'Burmese'},
	{'cat', 'Catalan'},
	{'chi', 'Chinese'},
	{'hrv', 'Croatian'},
	{'cze', 'Czech'},
	{'dan', 'Danish'},
	{'dut', 'Dutch'},
	{'eng', 'English'},
	{'epo', 'Esperanto'},
	{'est', 'Estonian'},
	{'fin', 'Finnish'},
	{'fre', 'French'},
	{'glg', 'Galician'},
	{'geo', 'Georgian'},
	{'ger', 'German'},
	{'ell', 'Greek'},
	{'heb', 'Hebrew'},
	{'hin', 'Hindi'},
	{'hun', 'Hungarian'},
	{'ice', 'Icelandic'},
	{'ind', 'Indonesian'},
	{'ita', 'Italian'},
	{'jpn', 'Japanese'},
	{'kaz', 'Kazakh'},
	{'khm', 'Khmer'},
	{'kor', 'Korean'},
	{'lav', 'Latvian'},
	{'lit', 'Lithuanian'},
	{'ltz', 'Luxembourgish'},
	{'mac', 'Macedonian'},
	{'may', 'Malay'},
	{'mal', 'Malayalam'},
	{'mon', 'Mongolian'},
	{'nor', 'Norwegian'},
	{'oci', 'Occitan'},
	{'per', 'Persian'},
	{'pol', 'Polish'},
	{'por', 'Portuguese'},
	{'pob', 'Brazilian Portuguese'},
	{'rum', 'Romanian'},
	{'rus', 'Russian'},
	{'scc', 'Serbian'},
	{'sin', 'Sinhalese'},
	{'slo', 'Slovak'},
	{'slv', 'Slovenian'},
	{'spa', 'Spanish'},
	{'swa', 'Swahili'},
	{'swe', 'Swedish'},
	{'syr', 'Syriac'},
	{'tgl', 'Tagalog'},
	{'tel', 'Telugu'},
	{'tha', 'Thai'},
	{'tur', 'Turkish'},
	{'ukr', 'Ukrainian'},
	{'urd', 'Urdu'},
	{'vie', 'Vietnamese'}
}

-- Languages code conversion table: iso-639-1 to iso-639-3
-- See https://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
local lang_os_to_iso = {
	sq = "alb",
	ar = "ara",
	hy = "arm",
	eu = "baq",
	bn = "ben",
	bs = "bos",
	br = "bre",
	bg = "bul",
	my = "bur",
	ca = "cat",
	zh = "chi",
	hr = "hrv",
	cs = "cze",
	da = "dan",
	nl = "dut",
	en = "eng",
	eo = "epo",
	et = "est",
	fi = "fin",
	fr = "fre",
	gl = "glg",
	ka = "geo",
	de = "ger",
	el = "ell",
	he = "heb",
	hi = "hin",
	hu = "hun",
	is = "ice",
	id = "ind",
	it = "ita",
	ja = "jpn",
	kk = "kaz",
	km = "khm",
	ko = "kor",
	lv = "lav",
	lt = "lit",
	lb = "ltz",
	mk = "mac",
	ms = "may",
	ml = "mal",
	mn = "mon",
	no = "nor",
	oc = "oci",
	fa = "per",
	pl = "pol",
	pt = "por",
	po = "pob",
	ro = "rum",
	ru = "rus",
	sr = "scc",
	si = "sin",
	sk = "slo",
	sl = "slv",
	es = "spa",
	sw = "swa",
	sv = "swe",
	tl = "tgl",
	te = "tel",
	th = "tha",
	tr = "tur",
	uk = "ukr",
	ur = "urd",
	vi = "vie"
}

local dlg = nil
local input_table = {} -- General widget id reference
local select_conf = {} -- Drop down widget / option table association

						--[[ Vlc extension stuff ]]--

function descriptor()
	return {
		title = "VLsub 0.9.10",
		version = "0.9.10",
		author = "exebetche",
		url = 'http://www.opensubtitles.org/',
		shortdesc = "VLsub";
		description = options.translation.int_descr,
		capabilities = {"menu", "input-listener" }
	}
end

function activate()
	vlc.msg.dbg("[VLsub] Welcome")

    check_config()

    if vlc.input.item() then
		openSub.getFileInfo()
		openSub.getMovieInfo()
	end

    show_main()
end

function close()
	deactivate()
end

function deactivate()
    vlc.msg.dbg("[VLsub] Bye bye!")
    if dlg then
		dlg:hide()
	end

	if openSub.session.token and openSub.session.token ~= "" then
		openSub.request("LogOut")
	end
   vlc.deactivate()
end

function menu()
	return {
		lang.int_research,
		lang.int_config,
		lang.int_help
	}
end

function meta_changed()
	return false
end

function input_changed()
	collectgarbage()
	set_interface_main()
	collectgarbage()
end

						--[[ Interface data ]]--

function interface_main()
	dlg:add_label(lang["int_default_lang"]..':', 1, 1, 1, 1)
	input_table['language'] =  dlg:add_dropdown(2, 1, 2, 1)
	dlg:add_button(lang["int_search_hash"], searchHash, 4, 1, 1, 1)

	dlg:add_label(lang["int_title"]..':', 1, 2, 1, 1)
	input_table['title'] = dlg:add_text_input(openSub.movie.title or "", 2, 2, 2, 1)
	dlg:add_button(lang["int_search_name"], searchIMBD, 4, 2, 1, 1)
	dlg:add_label(lang["int_season"]..':', 1, 3, 1, 1)
	input_table['seasonNumber'] = dlg:add_text_input(openSub.movie.seasonNumber or "", 2, 3, 2, 1)
	dlg:add_label(lang["int_episode"]..':', 1, 4, 1, 1)
	input_table['episodeNumber'] = dlg:add_text_input(openSub.movie.episodeNumber or "", 2, 4, 2, 1)
	input_table['mainlist'] = dlg:add_list(1, 5, 4, 1)
	input_table['message'] = nil
	input_table['message'] = dlg:add_label(' ', 1, 6, 4, 1)
	dlg:add_button(lang["int_show_help"], show_help, 1, 7, 1, 1)
	dlg:add_button('   '..lang["int_show_conf"]..'   ', show_conf, 2, 7, 1, 1)
	dlg:add_button(lang["int_dowload_sel"], download_subtitles, 3, 7, 1, 1)
	dlg:add_button(lang["int_close"], deactivate, 4, 7, 1, 1)

	assoc_select_conf('language', 'language', openSub.conf.languages, 2, lang["int_all"])
	display_subtitles()
end

function set_interface_main()
	-- Update movie title and co. if video input change
	if not type(input_table['title']) == 'userdata' then return false end

	openSub.getFileInfo()
	openSub.getMovieInfo()

	input_table['title']:set_text(openSub.movie.title or "")
	input_table['episodeNumber']:set_text(openSub.movie.episodeNumber or "")
	input_table['seasonNumber']:set_text(openSub.movie.seasonNumber or "")
end

function interface_config()
	input_table['intLangLab'] = dlg:add_label(lang["int_int_lang"]..':', 1, 1, 1, 1)
	input_table['intLangBut'] = dlg:add_button(lang["int_search_transl"], get_available_translations, 2, 1, 1, 1)
	input_table['intLang'] = dlg:add_dropdown(3, 1, 1, 1)
	dlg:add_label(lang["int_default_lang"]..':', 1, 2, 2, 1)
	input_table['default_language'] = dlg:add_dropdown(3, 2, 1, 1)
	dlg:add_label(lang["int_dowload_behav"]..':', 1, 3, 2, 1)
	input_table['downloadBehaviour'] = dlg:add_dropdown(3, 3, 1, 1)

	dlg:add_label(lang["int_display_code"]..':', 1, 4, 0, 1)
	input_table['langExt'] = dlg:add_dropdown(3, 4, 1, 1)
	dlg:add_label(lang["int_remove_tag"]..':', 1, 5, 0, 1)
	input_table['removeTag'] = dlg:add_dropdown(3, 5, 1, 1)

	if openSub.conf.dirPath then
		if openSub.conf.os == "win" then
			dlg	:add_label("<a href='file:///"..openSub.conf.dirPath.."'>"..lang["int_vlsub_work_dir"].."</a>", 1, 6, 2, 1)
		else
			dlg	:add_label("<a href='"..openSub.conf.dirPath.."'>"..lang["int_vlsub_work_dir"].."</a>", 1, 6, 2, 1)
		end
	else
		dlg	:add_label(lang["int_vlsub_work_dir"], 1, 6, 2, 1)
	end

	input_table['dir_path'] = dlg:add_text_input(openSub.conf.dirPath, 2, 6, 2, 1)

	dlg:add_label(lang["int_os_username"]..':', 1, 7, 0, 1)
	input_table['os_username'] = dlg:add_text_input(openSub.option.os_username or "", 2, 7, 2, 1)
	dlg:add_label(lang["int_os_password"]..':', 1, 8, 0, 1)
	input_table['os_password'] = dlg:add_text_input(openSub.option.os_password or "", 2, 8, 2, 1)

	input_table['message'] = nil
	input_table['message'] = dlg:add_label(' ', 1, 9, 3, 1)

	dlg:add_button(lang["int_cancel"], show_main, 2, 10, 1, 1)
	dlg:add_button(lang["int_save"], apply_config, 3, 10, 1, 1)

	input_table['langExt']:add_value(lang["int_bool_"..tostring(openSub.option.langExt)], 1)
	input_table['langExt']:add_value(lang["int_bool_"..tostring(not openSub.option.langExt)], 2)
	input_table['removeTag']:add_value(lang["int_bool_"..tostring(openSub.option.removeTag)], 1)
	input_table['removeTag']:add_value(lang["int_bool_"..tostring(not openSub.option.removeTag)], 2)

	assoc_select_conf('intLang', 'intLang', openSub.conf.translations_avail, 2)
	assoc_select_conf('default_language', 'language', openSub.conf.languages, 2, lang["int_all"])
	assoc_select_conf('downloadBehaviour', 'downloadBehaviour', openSub.conf.downloadBehaviours, 1)
end

function interface_help()
	local help_html = lang["int_help_mess"]

	input_table['help'] = dlg:add_html(help_html, 1, 1, 4, 1)
	dlg:add_label(string.rep ("&nbsp;", 100), 1, 2, 3, 1)
	dlg:add_button(lang["int_ok"], show_main, 4, 2, 1, 1)
end

function trigger_menu(dlg_id)
	if dlg_id == 1 then
		close_dlg()
		dlg = vlc.dialog(openSub.conf.useragent)
		interface_main()
	elseif dlg_id == 2 then
		close_dlg()
		dlg = vlc.dialog(openSub.conf.useragent..': '..lang["int_configuration"])
		interface_config()
	elseif dlg_id == 3 then
		close_dlg()
		dlg = vlc.dialog(openSub.conf.useragent..': '..lang["int_help"])
		interface_help()
	end
	collectgarbage() --~ !important
end

function show_main()
	trigger_menu(1)
end

function show_conf()
	trigger_menu(2)
end

function show_help()
	trigger_menu(3)
end

function close_dlg()
	vlc.msg.dbg("[VLSub] Closing dialog")

	if dlg ~= nil then
		--~ dlg:delete() -- Throw an error
		dlg:hide()
	end

	dlg = nil
	input_table = nil
	input_table = {}
	collectgarbage() --~ !important
end

						--[[ Drop down / config association]]--

function assoc_select_conf(select_id, option, conf, ind, default)
-- Helper for i/o interaction betwenn drop down and option list (lang...)
	select_conf[select_id] = {cf = conf, opt  = option, dflt = default, ind = ind}
	set_default_option(select_id)
	display_select(select_id)
end

function set_default_option(select_id)
-- Put the selected option of a list in first place of the associated table
	local opt = select_conf[select_id].opt
	local cfg = select_conf[select_id].cf
	local ind = select_conf[select_id].ind
	if openSub.option[opt] then
		table.sort(cfg, function(a, b)
			if a[1] == openSub.option[opt] then
				return true
			elseif b[1] == openSub.option[opt] then
				return false
			else
				return a[ind] < b[ind]
			end
		end)
	end
end

function display_select(select_id)
-- Display the drop down values with an optionnal default value at the top
	local conf = select_conf[select_id].cf
	local opt = select_conf[select_id].opt
	local option = openSub.option[opt]
	local default = select_conf[select_id].dflt
	local default_isset = false

	if not default then
		default_isset = true
	end

	for k, l in ipairs(conf) do
		if default_isset then
			input_table[select_id]:add_value(l[2], k)
		else
			if option then
				input_table[select_id]:add_value(l[2], k)
				input_table[select_id]:add_value(default, 0)
			else
				input_table[select_id]:add_value(default, 0)
				input_table[select_id]:add_value(l[2], k)
			end
			default_isset = true
		end
	end
end

						--[[ Config & interface localization]]--

function check_config()
	-- Make a copy of english translation to use it as default
	-- in case some element aren't translated in other translations
	eng_translation = {}
	for k, v in pairs(openSub.option.translation) do
		eng_translation[k] = v
	end

	-- Get available translation full name from code
	trsl_names = {}
	for i, lg in ipairs(languages) do
		trsl_names[lg[1]] = lg[2]
	end

	if is_window_path(vlc.config.datadir()) then
		openSub.conf.os = "win"
		slash = "\\"
	else
		openSub.conf.os = "lin"
		slash = "/"
	end

	local path_generic = {"lua", "extensions", "userdata", "vlsub"}
	local dirPath = slash..table.concat(path_generic, slash)
	local filePath	= slash.."vlsub_conf.xml"
	local config_saved = false
	sub_dir = slash.."vlsub_subtitles"

	-- Check if config file path is stored in vlc config
	local other_dirs = {}

	for path in vlc.config.get("sub-autodetect-path"):gmatch("[^,]+") do
		if path:match(".*"..sub_dir.."$") then
			openSub.conf.dirPath = path:gsub("%s*(.*)"..sub_dir.."%s*$", "%1")
			config_saved = true
		end
		table.insert(other_dirs, path)
	end

	-- if not stored in vlc config
	-- try to find a suitable config file path

    if openSub.conf.dirPath then
		if not is_dir(openSub.conf.dirPath) and
		(openSub.conf.os == "lin"  or
		is_win_safe(openSub.conf.dirPath)) then
			mkdir_p(openSub.conf.dirPath)
		end
    else
		local userdatadir = vlc.config.userdatadir()
		local datadir = vlc.config.datadir()

		-- check if the config already exist
		if file_exist(userdatadir..dirPath..filePath) then
			openSub.conf.dirPath = userdatadir..dirPath
			config_saved = true
		elseif file_exist(datadir..dirPath..filePath) then
			openSub.conf.dirPath = datadir..dirPath
			config_saved = true
		else
			local extension_path = slash..path_generic[1]
				..slash..path_generic[2]

			-- use the same folder as the extension if accessible
			if is_dir(userdatadir..extension_path)
			and file_touch(userdatadir..dirPath..filePath) then
					openSub.conf.dirPath = userdatadir..dirPath
			elseif file_touch(datadir..dirPath..filePath) then
				openSub.conf.dirPath = datadir..dirPath
			end

			-- try to create working dir in user folder
			if not openSub.conf.dirPath
			and is_dir(userdatadir) then
				if not is_dir(userdatadir..dirPath) then
					mkdir_p(userdatadir..dirPath)
				end
				if is_dir(userdatadir..dirPath) and
				file_touch(userdatadir..dirPath..filePath) then
					openSub.conf.dirPath = userdatadir..dirPath
				end
			end

			-- try to create working dir in vlc folder
			if not openSub.conf.dirPath and
			is_dir(datadir) then
				if not is_dir(datadir..dirPath) then
					mkdir_p(datadir..dirPath)
				end
				if file_touch(datadir..dirPath..filePath) then
					openSub.conf.dirPath = datadir..dirPath
				end
			end
		end
	end

	if openSub.conf.dirPath then
		vlc.msg.dbg("[VLSub] Working directory: " ..
			(openSub.conf.dirPath or "not found"))

		openSub.conf.filePath = openSub.conf.dirPath..filePath
		openSub.conf.localePath = openSub.conf.dirPath..slash.."locale"

		if config_saved
		and file_exist(openSub.conf.filePath) then
			vlc.msg.dbg("[VLSub] Loading config file: "..openSub.conf.filePath)
			load_config()
		else
			vlc.msg.dbg("[VLSub] No config file")
			getenv_lang()
			config_saved = save_config()
			if not config_saved then
				vlc.msg.dbg("[VLSub] Unable to save config")
			end
		end

		-- Check presence of a translation file in "%vlsub_directory%/locale"
		-- Add translation files to available translation list

		local file_list = list_dir(openSub.conf.localePath)
		local translations_avail = openSub.conf.translations_avail
		if file_list then
			for i, file_name in ipairs(file_list) do
				local lg =  string.gsub(file_name, "^(%w%w%w).xml$", "%1")
				if lg and not translations_avail[lg] then
					table.insert(translations_avail, {lg, trsl_names[lg]})
				end
			end
		end

		-- Load selected translation from file
		if openSub.option.intLang ~= "eng"
		and not openSub.conf.translated
		then
			local transl_file_path = openSub.conf.localePath..slash..openSub.option.intLang..".xml"
			if file_exist(transl_file_path) then
				vlc.msg.dbg("[VLSub] Loadin translation from file: " .. transl_file_path)
				load_transl(transl_file_path)
			end
		end
	else
		vlc.msg.dbg("[VLSub] Unable fount a suitable path to save config, please set it manually")
	end

	lang = nil
	lang = options.translation -- just a shortcut

	SetDownloadBehaviours()
	if not openSub.conf.dirPath then
		setError(lang["mess_err_conf_access"])
	end

	-- Set table list of available traduction from assoc. array
	-- so it is sortable

	for k, l in pairs(openSub.option.translations_avail) do
		if k == openSub.option.int_research then
			table.insert(openSub.conf.translations_avail, 1, {k, l})
		else
			table.insert(openSub.conf.translations_avail, {k, l})
		end
	end
	collectgarbage()
end

function load_config()
-- Overwrite default conf with loaded conf
	local tmpFile = io.open(openSub.conf.filePath, "rb")
	if not tmpFile then return false end
	local resp = tmpFile:read("*all")
	tmpFile:flush()
	tmpFile:close()
	local option = parse_xml(resp)

	for key, value in pairs(option) do
		if type(value) == "table" then
			if key == "translation" then
				openSub.conf.translated = true
				for k, v in pairs(value) do
					openSub.option.translation[k] = v
				end
			else
				openSub.option[key] = value
			end
		else
			if value == "true" then
				openSub.option[key] = true
			elseif value == "false" then
				openSub.option[key] = false
			else
				openSub.option[key] = value
			end
		end
	end
	collectgarbage()
end

function load_transl(path)
-- Overwrite default conf with loaded conf
	local tmpFile = assert(io.open(path, "rb"))
	local resp = tmpFile:read("*all")
	tmpFile:flush()
	tmpFile:close()
	openSub.option.translation = nil

	openSub.option.translation = parse_xml(resp)
	collectgarbage()
end

function apply_translation()
-- Overwrite default conf with loaded conf
	for k, v in pairs(eng_translation) do
		if not openSub.option.translation[k] then
			openSub.option.translation[k] = eng_translation[k]
		end
	end
end

function getenv_lang()
-- Retrieve the user OS language
	local os_lang = os.getenv("LANG")

	if os_lang then -- unix, mac
		os_lang = string.sub(os_lang, 0, 2)
		if type(lang_os_to_iso[os_lang]) then
			openSub.option.language = lang_os_to_iso[os_lang]
		end
	else -- Windows
		local lang_w = string.match(os.setlocale("", "collate"), "^[^_]+")
		for i, v in ipairs(openSub.conf.languages) do
		  if v[2] == lang_w then
			openSub.option.language = v[1]
		  end
		end
	end
end

function apply_config()
-- Apply user config selection to local config
	local lg_sel = input_table['intLang']:get_value()
	local sel_val
	local opt

	if lg_sel and lg_sel ~= 1
	and openSub.conf.translations_avail[lg_sel] then
		local lg = openSub.conf.translations_avail[lg_sel][1]
		set_translation(lg)
		SetDownloadBehaviours()
	end

	for select_id, v in pairs(select_conf) do
		if input_table[select_id] and select_conf[select_id] then
			sel_val = input_table[select_id]:get_value()
			opt = select_conf[select_id].opt

			if sel_val == 0 then
				openSub.option[opt] = nil
			else
				openSub.option[opt] = select_conf[select_id].cf[sel_val][1]
			end

			set_default_option(select_id)
		end
	end


	openSub.option.os_username = input_table['os_username']:get_text()
	openSub.option.os_password = input_table['os_password']:get_text()

	if input_table["langExt"]:get_value() == 2 then
		openSub.option.langExt = not openSub.option.langExt
	end

	if input_table["removeTag"]:get_value() == 2 then
		openSub.option.removeTag = not openSub.option.removeTag
	end

	-- Set a custom working directory
	local dir_path = input_table['dir_path']:get_text()
	local dir_path_err = false
	if trim(dir_path) == "" then dir_path = nil end

	if dir_path ~= openSub.conf.dirPath then
		if openSub.conf.os == "lin"
		or is_win_safe(dir_path)
		or not dir_path then
			local other_dirs = {}

			for path in vlc.config.get("sub-autodetect-path"):gmatch("[^,]+") do
				path = trim(path)
				if path ~= (openSub.conf.dirPath or "")..sub_dir then
					table.insert(other_dirs, path)
				end
			end
			openSub.conf.dirPath = dir_path
			if dir_path then
				table.insert(other_dirs,
				string.gsub(dir_path, "^(.-)[\\/]?$", "%1")..sub_dir)

				if not is_dir(dir_path) then
					mkdir_p(dir_path)
				end

				openSub.conf.filePath = openSub.conf.dirPath..slash.."vlsub_conf.xml"
				openSub.conf.localePath = openSub.conf.dirPath..slash.."locale"
			else
				openSub.conf.filePath = nil
				openSub.conf.localePath = nil
			end
			vlc.config.set("sub-autodetect-path", table.concat(other_dirs, ", "))
		else
			dir_path_err = true
			setError(lang["mess_err_wrong_path"].."<br><b>"..string.gsub(dir_path, "[^%:%w%p%s§¤]+", "<span style='color:#B23'>%1</span>").."</b>")
		end
	end

	if openSub.conf.dirPath and
	not dir_path_err then
		local config_saved = save_config()
		trigger_menu(1)
		if not config_saved then
			setError(lang["mess_err_conf_access"])
		end
	else
		setError(lang["mess_err_conf_access"])
	end
end

function save_config()
-- Dump local config into config file
	if openSub.conf.dirPath
	and openSub.conf.filePath then
		vlc.msg.dbg("[VLSub] Saving config file:  " .. openSub.conf.filePath)

		if file_touch(openSub.conf.filePath) then
			local tmpFile = assert(io.open(openSub.conf.filePath, "wb"))
			local resp = dump_xml(openSub.option)
			tmpFile:write(resp)
			tmpFile:flush()
			tmpFile:close()
			tmpFile = nil
		else
			return false
		end
		collectgarbage()
		return true
	else
		vlc.msg.dbg("[VLSub] Unable fount a suitable path to save config, please set it manually")
		setError(lang["mess_err_conf_access"])
		return false
	end
end

function SetDownloadBehaviours()
	openSub.conf.downloadBehaviours = nil
	openSub.conf.downloadBehaviours = {
		{'save', lang["int_dowload_save"]},
		{'load', lang["int_dowload_load"]},
		{'manual', lang["int_dowload_manual"]}
	}
end

function get_available_translations()
-- Get all available translation files from the internet
-- (drop previous direct download from github repo because of problem with github https CA certficate on OS X an XP)
-- https://github.com/exebetche/vlsub/tree/master/locale

	local translations_url = "http://addons.videolan.org/CONTENT/content-files/148752-vlsub_translations.xml"

	if input_table['intLangBut']:get_text() == lang["int_search_transl"] then
		openSub.actionLabel = lang["int_searching_transl"]

		local translations_content, lol = get(translations_url)

		all_trsl = parse_xml(translations_content)
		local lg, trsl

		for lg, trsl in pairs(all_trsl) do
			if lg ~= options.intLang[1] and not openSub.option.translations_avail[lg] then
				openSub.option.translations_avail[lg] = trsl_names[lg] or ""
				table.insert(openSub.conf.translations_avail, {lg, trsl_names[lg]})
				input_table['intLang']:add_value(trsl_names[lg], #openSub.conf.translations_avail)
			end
		end

		setMessage(success_tag(lang["mess_complete"]))
		collectgarbage()
	end
end

function set_translation(lg)
	openSub.option.translation = nil
	openSub.option.translation = {}

	if lg == 'eng' then
		for k, v in pairs(eng_translation) do
			openSub.option.translation[k] = v
		end
	else
		-- If translation file exists in /locale directory load it
		if openSub.conf.localePath
		and file_exist(openSub.conf.localePath..slash..lg..".xml") then
			local transl_file_path = openSub.conf.localePath..slash..lg..".xml"
			vlc.msg.dbg("[VLSub] Loading translation from file: " .. transl_file_path)
			load_transl(transl_file_path)
			apply_translation()
		else
		-- Load translation file from internet
			if not all_trsl then
				get_available_translations()
			end

			if not all_trsl or not all_trsl[lg] then
				vlc.msg.dbg("[VLSub] Error, translation not found")
				return false
			end
			openSub.option.translation = all_trsl[lg]
			apply_translation()
			all_trsl = nil
		end
	end

	lang = nil
	lang = openSub.option.translation
	collectgarbage()
end

						--[[ Core ]]--

openSub = {
	itemStore = nil,
	actionLabel = "",
	conf = {
		url = "http://api.opensubtitles.org/xml-rpc",
		path = nil,
		userAgentHTTP = "VLSub",
		useragent = "VLSub 0.9",
		translations_avail = {},
		downloadBehaviours = nil,
		languages = languages
	},
	option = options,
	session = {
		loginTime = 0,
		token = ""
	},
	file = {
		hasInput = false,
		uri = nil,
		ext = nil,
		name = nil,
		path = nil,
		protocol = nil,
		cleanName = nil,
		dir = nil,
		hash = nil,
		bytesize = nil,
		fps = nil,
		timems = nil,
		frames = nil
	},
	movie = {
		title = "",
		seasonNumber = "",
		episodeNumber = "",
		sublanguageid = ""
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
			if response then
				if response.status == "200 OK" then
					return openSub.methods[methodName].callback(response)
				elseif response.status == "406 No session" then
					openSub.request("LogIn")
				elseif response then
					setError("code '"..response.status.."' ("..status..")")
					return false
				end
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
				openSub.actionLabel = lang["action_login"]
				return {
					{ value={ string=openSub.option.os_username } },
					{ value={ string=openSub.option.os_password } },
					{ value={ string=openSub.movie.sublanguageid } },
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
				openSub.actionLabel = lang["action_logout"]
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
				openSub.actionLabel = lang["action_noop"]
				return {
					{ value={ string=openSub.session.token } }
				}
			end,
			callback = function(resp)
				return true
			end
		},
		SearchSubtitlesByHash = {
			methodName = "SearchSubtitles",
			params = function()
				openSub.actionLabel = lang["action_search"]
				setMessage(openSub.actionLabel..": "..progressBarContent(0))

				return {
					{ value={ string=openSub.session.token } },
					{ value={
						array={
						  data={
							value={
							  struct={
								member={
								  { name="sublanguageid", value={
									string=openSub.movie.sublanguageid } },
								  { name="moviehash", value={
									string=openSub.file.hash } },
								  { name="moviebytesize", value={
									double=openSub.file.bytesize } } }}}}}}}
				}
			end,
			callback = function(resp)
				openSub.itemStore = resp.data
			end
		},
		SearchSubtitles = {
			methodName = "SearchSubtitles",
			params = function()
				openSub.actionLabel = lang["action_search"]
				setMessage(openSub.actionLabel..": "..progressBarContent(0))

				local member = {
						  { name="sublanguageid", value={
							string=openSub.movie.sublanguageid } },
						  { name="query", value={
							string=openSub.movie.title } } }


				if openSub.movie.seasonNumber ~= nil then
					table.insert(member, { name="season", value={
						string=openSub.movie.seasonNumber } })
				end

				if openSub.movie.episodeNumber ~= nil then
					table.insert(member, { name="episode", value={
						string=openSub.movie.episodeNumber } })
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
			end
		}
	},
	getInputItem = function()
		return vlc.item or vlc.input.item()
	end,
	getFileInfo = function()
	-- Get video file path, name, extension from input uri
		local item = openSub.getInputItem()
		local file = openSub.file
		if not item then
			file.hasInput = false;
			file.cleanName = nil;
			file.protocol = nil;
			file.path = nil;
			file.ext = nil;
			file.uri = nil;
		else
			vlc.msg.dbg("[VLSub] Video URI: "..item:uri())
			local parsed_uri = vlc.net.url_parse(item:uri())
			file.uri = item:uri()
			file.protocol = parsed_uri["protocol"]
			file.path = parsed_uri["path"]

		-- Corrections

			-- For windows
			file.path = string.match(file.path, "^/(%a:/.+)$") or file.path

			-- For file in archive
			local archive_path, name_in_archive = string.match(file.path, '^([^!]+)!/([^!/]*)$')
			if archive_path and archive_path ~= "" then
				file.path = string.gsub(archive_path, '\063', '%%')
				file.path = vlc.strings.decode_uri(file.path)
				file.completeName = string.gsub(name_in_archive, '\063', '%%')
				file.completeName = vlc.strings.decode_uri(file.completeName)
				file.is_archive = true
			else -- "classic" input
				file.path = vlc.strings.decode_uri(file.path)
				file.dir, file.completeName = string.match(file.path, '^(.+/)([^/]*)$')

				local file_stat = vlc.net.stat(file.path)
				if file_stat
				then
					file.stat = file_stat
				end

				file.is_archive = false
			end

			file.name, file.ext = string.match(file.completeName, '^([^/]-)%.?([^%.]*)$')

			if file.ext == "part" then
				file.name, file.ext = string.match(file.name, '^([^/]+)%.([^%.]+)$')
			end

			file.hasInput = true;
			file.cleanName = string.gsub(file.name, "[%._]", " ")
			vlc.msg.dbg("[VLSub] file info "..(dump_xml(file)))
		end
		collectgarbage()
	end,
	getMovieInfo = function()
	-- Clean video file name and check for season/episode pattern in title
		if not openSub.file.name then
			openSub.movie.title = ""
			openSub.movie.seasonNumber = ""
			openSub.movie.episodeNumber = ""
			return false
		end

		local showName, seasonNumber, episodeNumber = string.match(openSub.file.cleanName, "(.+)[sS](%d%d)[eE](%d%d).*")

		if not showName then
		   showName, seasonNumber, episodeNumber = string.match(openSub.file.cleanName, "(.+)(%d)[xX](%d%d).*")
		end

		if showName then
			openSub.movie.title = showName
			openSub.movie.seasonNumber = seasonNumber
			openSub.movie.episodeNumber = episodeNumber
		else
			openSub.movie.title = openSub.file.cleanName
			openSub.movie.seasonNumber = ""
			openSub.movie.episodeNumber = ""
		end
		collectgarbage()
	end,
	getMovieHash = function()
	-- Calculate movie hash
		openSub.actionLabel = lang["action_hash"]
		setMessage(openSub.actionLabel..": "..progressBarContent(0))

		local item = openSub.getInputItem()

		if not item then
			setError(lang["mess_no_input"])
			return false
		end

		openSub.getFileInfo()

		if not openSub.file.path then
			setError(lang["mess_not_found"])
			return false
		end

		local data_start = ""
		local data_end = ""
        local size
        local chunk_size = 65536

		-- Get data for hash calculation
		if openSub.file.is_archive then
			vlc.msg.dbg("[VLSub] Read hash data from stream")

			local file = vlc.stream(openSub.file.uri)
			local dataTmp1 = ""
			local dataTmp2 = ""
			size = chunk_size

			data_start = file:read(chunk_size)

			while data_end do
				size = size + string.len(data_end)
				dataTmp1 = dataTmp2
				dataTmp2 = data_end
				data_end = file:read(chunk_size)
				collectgarbage()
			end
			data_end = string.sub((dataTmp1..dataTmp2), -chunk_size)
		elseif not file_exist(openSub.file.path)
		and openSub.file.stat then
			vlc.msg.dbg("[VLSub] Read hash data from stream")

			local file = vlc.stream(openSub.file.uri)

			if not file then
				vlc.msg.dbg("[VLSub] No stream")
				return false
			end

			size = openSub.file.stat.size
			local decal = size%chunk_size

			data_start = file:read(chunk_size)

			-- "Seek" to the end
			file:read(decal)

			for i = 1, math.floor(((size-decal)/chunk_size))-2 do
				file:read(chunk_size)
			end

			data_end = file:read(chunk_size)

			file = nil
		else
			vlc.msg.dbg("[VLSub] Read hash data from file")
			local file = io.open( openSub.file.path, "rb")
			if not file then
				vlc.msg.dbg("[VLSub] No stream")
				return false
			end

			data_start = file:read(chunk_size)
			size = file:seek("end", -chunk_size) + chunk_size
			data_end = file:read(chunk_size)
			file = nil
		end

	-- Hash calculation
        local lo = size
        local hi = 0
        local o,a,b,c,d,e,f,g,h
        local hash_data = data_start..data_end
        local max_size = 4294967296
        local overflow

		for i = 1,  #hash_data, 8 do
			a,b,c,d,e,f,g,h = hash_data:byte(i,i+7)
			lo = lo + a + b*256 + c*65536 + d*16777216
			hi = hi + e + f*256 + g*65536 + h*16777216

			if lo > max_size then
				overflow = math.floor(lo/max_size)
				lo = lo-(overflow*max_size)
				hi = hi+overflow
			end

			if hi > max_size then
				overflow = math.floor(hi/max_size)
				hi = hi-(overflow*max_size)
			end
        end

		openSub.file.bytesize = size
		openSub.file.hash = string.format("%08x%08x", hi,lo)
		vlc.msg.dbg("[VLSub] Video hash: "..openSub.file.hash)
		vlc.msg.dbg("[VLSub] Video bytesize: "..size)
		collectgarbage()
		return true
	end,
	checkSession = function()

		if openSub.session.token == "" then
			openSub.request("LogIn")
		else
			openSub.request("NoOperation")
		end
	end
}

function searchHash()
	local sel = input_table["language"]:get_value()
	if sel == 0 then
		openSub.movie.sublanguageid = 'all'
	else
		openSub.movie.sublanguageid = openSub.conf.languages[sel][1]
	end

	openSub.getMovieHash()

	if openSub.file.hash then
		openSub.checkSession()
		openSub.request("SearchSubtitlesByHash")
		display_subtitles()
	end
end

function searchIMBD()
	openSub.movie.title = trim(input_table["title"]:get_text())
	openSub.movie.seasonNumber = tonumber(input_table["seasonNumber"]:get_text())
	openSub.movie.episodeNumber = tonumber(input_table["episodeNumber"]:get_text())

	local sel = input_table["language"]:get_value()
	if sel == 0 then
		openSub.movie.sublanguageid = 'all'
	else
		openSub.movie.sublanguageid = openSub.conf.languages[sel][1]
	end

	if openSub.movie.title ~= "" then
		openSub.checkSession()
		openSub.request("SearchSubtitles")
		display_subtitles()
	end
end

function display_subtitles()
	local mainlist = input_table["mainlist"]
	mainlist:clear()

	if openSub.itemStore == "0" then
		mainlist:add_value(lang["mess_no_res"], 1)
		setMessage("<b>"..lang["mess_complete"]..":</b> "..lang["mess_no_res"])
	elseif openSub.itemStore then
		for i, item in ipairs(openSub.itemStore) do
			mainlist:add_value(
			item.SubFileName..
			" ["..item.SubLanguageID.."]"..
			" ("..item.SubSumCD.." CD)", i)
		end
		setMessage("<b>"..lang["mess_complete"]..":</b> "..#(openSub.itemStore).."  "..lang["mess_res"])
	end
end

function get_first_sel(list)
	local selection = list:get_selection()
	for index, name in pairs(selection) do
		return index
	end
	return 0
end

function download_subtitles()
	local index = get_first_sel(input_table["mainlist"])

	if index == 0 then
		setMessage(lang["mess_no_selection"])
		return false
	end

	openSub.actionLabel = lang["mess_downloading"]

	display_subtitles() -- reset selection

	local item = openSub.itemStore[index]

	if openSub.option.downloadBehaviour == 'manual' then

		local link = "<span style='color:#181'>"
		link = link.."<b>"..lang["mess_dowload_link"]..":</b>"
		link = link.."</span> &nbsp;"
		link = link.."</span> &nbsp;<a href='"..item.ZipDownloadLink.."'>"
		link = link..item.MovieReleaseName.."</a>"

		setMessage(link)
		return false
	elseif openSub.option.downloadBehaviour == 'load' then
		if add_sub("zip://"..item.ZipDownloadLink.."!/"..item.SubFileName) then
			setMessage(success_tag(lang["mess_loaded"]))
		end
		return false
	end

	local message = ""
	local subfileName = openSub.file.name

	if openSub.option.langExt then
		subfileName = subfileName.."."..item.SubLanguageID
	end

	subfileName = subfileName.."."..item.SubFormat
	local tmp_dir
	local file_target_access = true

	if is_dir(openSub.file.dir) then
		tmp_dir = openSub.file.dir
	elseif openSub.conf.dirPath then
		tmp_dir = openSub.conf.dirPath

		message = "<br> "..error_tag(lang["mess_save_fail"].." &nbsp;"..
		"<a href='"..vlc.strings.make_uri(openSub.conf.dirPath).."'>"..
		lang["mess_click_link"].."</a>")
	else
		setError(lang["mess_save_fail"].." &nbsp;"..
		"<a href='"..item.ZipDownloadLink.."'>"..
		lang["mess_click_link"].."</a>")
		return false
	end

	local tmpFileURI, tmpFileName = dump_zip(
		item.ZipDownloadLink,
		tmp_dir,
		item.SubFileName)

	vlc.msg.dbg("[VLsub] tmpFileName: "..tmpFileName)

	-- Determine if the path to the video file is accessible for writing

	local target = openSub.file.dir..subfileName

	if not file_touch(target) then
		if openSub.conf.dirPath then
			target =  openSub.conf.dirPath..slash..subfileName
			message = "<br> "..error_tag(lang["mess_save_fail"].." &nbsp;"..
			"<a href='"..vlc.strings.make_uri(openSub.conf.dirPath).."'>"..
			lang["mess_click_link"].."</a>")
		else
			setError(lang["mess_save_fail"].." &nbsp;"..
			"<a href='"..item.ZipDownloadLink.."'>"..
			lang["mess_click_link"].."</a>")
			return false
		end
	end

	vlc.msg.dbg("[VLsub] Subtitles files: "..target)

	-- Unzipped data into file target

	local stream = vlc.stream(tmpFileURI)
	local data = ""
	local subfile = io.open(target, "wb")

	while data do
		subfile:write(data)
		data = stream:read(65536)
	end

	subfile:flush()
	subfile:close()

	stream = nil
	collectgarbage()

	if not os.remove(tmpFileName) then
		vlc.msg.err("[VLsub] Unable to remove temp: "..tmpFileName)
	end

	subfileURI = vlc.strings.make_uri(target)

	if not subfileURI then
		subfileURI = make_uri(target, true)
	end

	-- load subtitles
	if add_sub(subfileURI) then
		message = success_tag(lang["mess_loaded"]) .. message
	end


	setMessage(message)
end

function dump_zip(url, dir, subfileName)
	-- Dump zipped data in a temporary file
	setMessage(openSub.actionLabel..": "..progressBarContent(0))
	local resp = get(url)

	if not resp then
		setError(lang["mess_no_response"])
		return false
	end

	local tmpFileName = dir..slash..subfileName..".gz"
	if not file_touch(tmpFileName) then
		return false
	end
	local tmpFile = assert(io.open(tmpFileName, "wb"))

	tmpFile:write(resp)
	tmpFile:flush()
	tmpFile:close()
	tmpFile = nil
	collectgarbage()
	return "zip://"..make_uri(tmpFileName, true).."!/"..subfileName, tmpFileName
end

function add_sub(subfileURI)
	if vlc.item or vlc.input.item() then
		vlc.msg.dbg("[VLsub] Adding subtitle :" .. subfileURI)
		return vlc.input.add_subtitle(subfileURI)
	end
	return false
end

						--[[ Interface helpers]]--

function progressBarContent(pct)
	local accomplished = math.ceil(openSub.option.progressBarSize*pct/100)
	local left = openSub.option.progressBarSize - accomplished
	local content = "<span style='background-color:#181;color:#181;'>"..
		string.rep ("-", accomplished).."</span>"..
		"<span style='background-color:#fff;color:#fff;'>"..
		string.rep ("-", left)..
		"</span>"
	return content
end

function setMessage(str)
	if input_table["message"] then
		input_table["message"]:set_text(str)
		dlg:update()
	end
end

function setError(mess)
	setMessage(error_tag(mess))
end

function success_tag(str)
	return "<span style='color:#181'><b>"..
	lang["mess_success"]..":</b></span> "..str..""
end

function error_tag(str)
	return "<span style='color:#B23'><b>"..
	lang["mess_error"]..":</b></span> "..str..""
end

						--[[ Network utils]]--

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
		return false, status, response
	end
end

function http_req(host, port, request)
	local fd = vlc.net.connect_tcp(host, port)
	if not fd then return false end
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

	--~ if status ~= 200 then return status end

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

						--[[ XML utils]]--

function parse_xml(data)
	local tree = {}
	local stack = {}
	local tmp = {}
	local level = 0
	local op, tag, p, empty, val
	table.insert(stack, tree)

	for op, tag, p, empty, val in string.gmatch(
		data,
		"[%s\r\n\t]*<(%/?)([%w:_]+)(.-)(%/?)>[%s\r\n\t]*([^<]*)[%s\r\n\t]*"
	) do
		if op=="/" then
			if level>0 then
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

	collectgarbage()
	return tree
end

function parse_xmlrpc(data)
	local tree = {}
	local stack = {}
	local tmp = {}
	local tmpTag = ""
	local level = 0
	local op, tag, p, empty, val
	table.insert(stack, tree)

	for op, tag, p, empty, val in string.gmatch(
		data,
		"<(%/?)([%w:]+)(.-)(%/?)>[%s\r\n\t]*([^<]*)"
	) do
		if op=="/" then
			if tag == "member" or tag == "array" then
				if level>0  then
					level = level - 1
					table.remove(stack)
				end
			end
		elseif tag == "name" then
			level = level + 1
			if val~= "" then tmpTag  = vlc.strings.resolve_xml_special_chars(val) end

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
	collectgarbage()
	return tree
end

function dump_xml(data)
	local level = 0
	local stack = {}
	local dump = ""

	local function parse(data, stack)
		local data_index = {}
		local k
		local v
		local i
		local tb

		for k,v in pairs(data) do
			table.insert(data_index, {k, v})
			table.sort(data_index, function(a, b)
				return a[1] < b[1]
			end)
		end

		for i,tb in pairs(data_index) do
			k = tb[1]
			v = tb[2]
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
				dump = dump..(vlc.strings.convert_xml_special_chars(v) or v)
			elseif type(v)=="number" then
				dump = dump..v
			else
				dump = dump..tostring(v)
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
	collectgarbage()
	return dump
end

						--[[ Misc utils]]--

function make_uri(str, encode)
    local windowdrive = string.match(str, "^(%a:%).+$")
	if encode then
		local encodedPath = ""
		for w in string.gmatch(str, "/([^/]+)") do
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

function file_touch(name) -- test writetability
	if not name or trim(name) == ""
	then return false end

	local f=io.open(name ,"w")
	if f~=nil then
		io.close(f)
		return true
	else
		return false
	end
end

function file_exist(name) -- test readability
	if not name or trim(name) == ""
	then return false end
	local f=io.open(name ,"r")
	if f~=nil then
		io.close(f)
		return true
	else
		return false
	end
end

function is_dir(path)
	if not path or trim(path) == ""
	then return false end
	-- Remove slash at the or it won't work on Windows
	path = string.gsub(path, "^(.-)[\\/]?$", "%1")
	local f, _, code = io.open(path, "rb")

	if f then
		_, _, code = f:read("*a")
		f:close()
		if code == 21 then
			return true
		end
	elseif code == 13 then
		return true
	end

	return false
end

function list_dir(path)
	if not path or trim(path) == ""
	then return false end
	local dir_list_cmd
	local list = {}
	if not is_dir(path) then return false end

	if openSub.conf.os == "win" then
		dir_list_cmd = io.popen('dir /b "'..path..'"')
	elseif openSub.conf.os == "lin" then
		dir_list_cmd = io.popen('ls -1 "'..path..'"')
	end

	if dir_list_cmd then
		for filename in dir_list_cmd:lines() do
			if string.match(filename, "^[^%s]+.+$") then
				table.insert(list, filename)
			end
		end
		return list
	else
		return false
	end
end

function mkdir_p(path)
	if not path or trim(path) == ""
	then return false end
	if openSub.conf.os == "win" then
		os.execute('mkdir "' .. path..'"')
	elseif openSub.conf.os == "lin" then
		os.execute("mkdir -p '" .. path.."'")
	end
end

function is_window_path(path)
	return string.match(path, "^(%a:%).+$")
end

function is_win_safe(path)
	if not path or trim(path) == ""
	or not is_window_path(path)
	then return false end
	return string.match(path, "^%a?%:?[\\%w%p%s§¤]+$")
end

function trim(str)
    if not str then return "" end
    return string.gsub(str, "^[\r\n%s]*(.-)[\r\n%s]*$", "%1")
end

function remove_tag(str)
	return string.gsub(str, "{[^}]+}", "")
end

function sleep(sec)
   local t = vlc.misc.mdate()
   vlc.misc.mwait(t + sec*1000*1000)
end

