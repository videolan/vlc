--[[
	vrt.lua
	version 1.1

	Changelog:
	v1.0: first version supporting deredactie.be and sporza.be
	v1.1: added canvas.be and een.be

	Play VRT videos in VLC

	Authors: Gautier <midas002 at gmail dot com>

	Currently enabled for:
		deredactie.be		X
		vrtnu.be		-
		een.be			X
		canvas.be		X
		sporza.be		X
--]]

-- Probe function.
function probe()
    -- return ( vlc.access == "http" or vlc.access == "https" ) -- Is this check required?
	return string.match( vlc.path, "deredactie%.be/.+" )
        or string.match( vlc.path, "sporza%.be/.+" )
		or string.match( vlc.path, "een%.be/.+" )
		or string.match( vlc.path, "canvas%.be/.+" )
		or string.match( vlc.path, "mediazone%.vrt%.be/api/.+" )
end

-- Parse function.
function parse()
	if string.match ( vlc.path, "deredactie%.be/.+" ) or string.match( vlc.path, "sporza%.be/.+" ) then
		while true do
			line = vlc.readline()
			if not line then break end

			-- Find video URL "data-video-src"; only grab the first video URL if there is more than one
				if not path then
					if string.match( line, "data%-video%-src") then
						path  = string.match( line, "data%-video%-src=\"(.-)\"" )
						vlc.msg.info("Video URL found:", path)
					end
				end

			-- Find title
				if string.match( line, "<meta property=\"og:title\"" ) then
					_,_,name = string.find( line, "content=\"(.-)\"" )
					name = vlc.strings.resolve_xml_special_chars( name )
					vlc.msg.info("Title found:", name)
				end

			-- Find description
				if string.match( line, "<meta property=\"og:description\"" ) then
					_,_,description = string.find( line, "content=\"(.-)\"" )
					if (description ~= nil) then
						description = vlc.strings.resolve_xml_special_chars( description )
						vlc.msg.info("Description found:", description)
					end
				end

			-- Find image/art
				if string.match( line, "<meta property=\"og:image\"" ) then
					_,_,arturl = string.find( line, "content=\"(.-)\"" )
				end
		end
	end

	if string.match ( vlc.path, "een%.be/.+" ) or string.match ( vlc.path, "canvas%.be/.+" ) then
		while true do
			line = vlc.readline()
			if not line then break end

			-- Data is provided in the form of a JSON file, need to find the JSON id ("data-video") and then return it
				if not path then
					if string.match( line, "data%-video=") then
						video_id = string.match( line, "data%-video=\"(.-)\"" )
						vlc.msg.info("Video ID found:", video_id)
						if string.match ( vlc.path, "een%.be/.+" ) then
							path = ("https://mediazone.vrt.be/api/v1/een/assets/"..video_id)
						end
						if string.match ( vlc.path, "canvas%.be/.+" ) then
							path = ("https://mediazone.vrt.be/api/v1/canvas/assets/"..video_id)
						end
					end
				end

			-- Find title
				if string.match( line, "<meta property=\"og:title\"" ) then
					_,_,name = string.find( line, "content=\"(.-)\"" )
					name = vlc.strings.resolve_xml_special_chars( name )
					vlc.msg.info("Title found:", name)
				end

			-- Find description
				if string.match( line, "<meta property=\"og:description\"" ) then
					_,_,description = string.find( line, "content=\"(.-)\"" )
					if (description ~= nil) then
						description = vlc.strings.resolve_xml_special_chars( description )
						vlc.msg.info("Description found:", description)
					end
				end

			-- Find image/art
				if string.match( line, "<meta property=\"og:image\"" ) then
					_,_,arturl = string.find( line, "content=\"(.-)\"" )
				end
		end
	end

	if string.match ( vlc.path, "mediazone%.vrt%.be/api/.+" ) then
			-- Expected data is a JSON file, so can't read it with "line = vlc.readline()"
			-- Using vlc.stream to grab a remote json file and place it in a string
			-- Only one iteration is needed, because everything will be loaded in the same string
			local stream = vlc.stream(vlc.access.."://"..vlc.path)
			line = stream:read(65000) -- Reading 65000 characters should be more than sufficient
			-- line = stream:readline() -- Alternative to read(65000) I guess

			-- Multiple video formats are being offered, order of preference chosen here: PROGRESSIVE_DOWNLOAD, HLS, RTMP, RTSP
			-- First try to find a "PROGRESSIVE_DOWNLOAD" video URL
				if string.match( line, "PROGRESSIVE_DOWNLOAD") then
					path  = string.match( line, "PROGRESSIVE_DOWNLOAD\",\"url\":\"(.-)\"" )
					vlc.msg.info("Progressive video URL found:", path)
				end

				if not path then
					if string.match( line, "\"HLS\",") then
					path  = string.match( line, "HLS\",\"url\":\"(.-)\"" )
					vlc.msg.info("HLS video URL found:", path)
					end
				end

				if not path then
					if string.match( line, "\"RTMP\",") then
					path  = string.match( line, "RTMP\",\"url\":\"(.-)\"" )
					vlc.msg.info("RTMP video URL found:", path)
					end
				end

				if not path then
					if string.match( line, "\"RTSP\",") then
					path  = string.match( line, "RTSP\",\"url\":\"(.-)\"" )
					vlc.msg.info("RTSP video URL found:", path)
					end
				end

			-- Extracting title
				if string.match( line, "\"title\":") then
					name = string.match( line, "title\":\"(.-)\"" )
					name = vlc.strings.resolve_xml_special_chars( name )
					vlc.msg.info("Title found:", name)
				end

			-- Extracting description
				if string.match( line, "\"description\":") then
					description = string.match( line, "description\":\"(.-)\"" )
					description = vlc.strings.resolve_xml_special_chars( description )
					vlc.msg.info("Description found:", description)
				end

			-- Extracting artwork
				if string.match( line, "\"posterImageUrl\":") then
					arturl = string.match( line, "posterImageUrl\":\"(.-)\"" )
				end

			-- Extracting duration
				if string.match( line, "\"duration\":") then
					duration = string.match( line, "duration\":(.-)\," )/1000
					vlc.msg.info("Duration found:", duration)
				end

			-- Extracting subtitles ... disabling, because I don't believe subtitles can be returned ???
			--	if string.match( line, "\"CLOSED\",") then
			--		subtitles = string.match( line, "CLOSED\",\"url\":\"(.-)\"" )
			--		vlc.msg.info("Subtitles found:", subtitles)
			--	end

	end

    if not path then
        vlc.msg.err("Couldn't extract the video URL")
        return { }
    end

    return { { path = path; name = name; description = description; arturl = arturl; artist = "VRT"; duration = duration;} }
end
