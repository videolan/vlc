--[[
	vrt.lua
	version 1.0

	Open VRT videos in VLC.

	Authors: Gautier <midas002 at gmail dot com>

	Currently enabled for: 
		deredactie.be	X
		vrtnu.be		-
		een.be		-
		canvas.be		-
		sporza.be		X
--]]

-- Probe function.
function probe()
    return ( vlc.access == "http" or vlc.access == "https" )
        and string.match( vlc.path, "deredactie%.be/.+" )
        or string.match( vlc.path, "sporza%.be/.+")
end

-- Parse function.
function parse()
	while true
    do
        line = vlc.readline()

        if not line then break end

	-- Find video URL "data-video-src"
		if string.match( line, "data%-video%-src") then
			path  = string.match( line, "data%-video%-src=\"(.-)\"" )
 	  		vlc.msg.info("Video URL found:", path)
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

    if not path then
        vlc.msg.err("Couldn't extract the video URL")
        return { }
    end

    return { { path = path; name = name; description = description; arturl = arturl; } }
end
