--[[
    Translate Daily Motion video webpages URLs to the corresponding
    FLV URL.

 $Id$

 Copyright © 2007-2011 the VideoLAN team

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

function get_prefres()
    local prefres = -1
    if vlc.var and vlc.var.inherit then
        prefres = vlc.var.inherit(nil, "preferred-resolution")
        if prefres == nil then
            prefres = -1
        end
    end

    return prefres
end

-- Probe function.
function probe()
	if vlc.access ~= "http" and vlc.access ~= "https" then
        return false
    end

    return ( string.match( vlc.path, "www.dailymotion.com/video" ) )
end

-- Parse function.
function parse()
	prefres = get_prefres()


	while true
    do
        line = vlc.readline()
        if not line then break end
		if string.match( line, "<meta property=\"og:title\"" ) then
			_,_,name = string.find( line, "content=\"(.-)\"" )
			name = vlc.strings.resolve_xml_special_chars( name )
		end
		if string.match( line, "<meta name=\"description\"" ) then
			_,_,description = string.find( line, "content=\"(.-)\"" )
            if (description ~= nil) then
                description = vlc.strings.resolve_xml_special_chars( description )
            end
		end
		if string.match( line, "<meta name=\"author\"" ) then
			_,_,artist = string.find( line, "content=\"(.-)\"" )
			artist = vlc.strings.resolve_xml_special_chars( artist )
		end
		if string.match( line, "<link rel=\"thumbnail\" type=\"image/jpeg\"" ) then
			_,_,arturl = string.find( line, "href=\"(.-)\"" )
		end
    end

	page_embed = string.gsub(vlc.path, "dailymotion.com/video/", "dailymotion.com/embed/video/")
	page_url = vlc.stream(vlc.access .. "://" .. page_embed )
	if not page_url then return nil end
    page = page_url:read( 65653 )


	hd1080url = string.match( page, "\"stream_h264_hd1080_url\"%s*:%s*\"([^\"]*)\"")
	hdurl = string.match( page, "\"stream_h264_hd_url\"%s*:%s*\"([^\"]*)\"")
	hqurl = string.match( page, "\"stream_h264_hq_url\"%s*:%s*\"([^\"]*)\"")
	baseurl = string.match( page, "\"stream_h264_url\"%s*:%s*\"([^\"]*)\"")
	ldurl = string.match( page, "\"stream_h264_ld_url\"%s*:%s*\"([^\"]*)\"")
	livehlsurl = string.match( page, "\"stream_live_hls_url\"%s*:%s*\"([^\"]*)\"")


	arr_videos_urls = {}
	if hd1080url then	table.insert(arr_videos_urls,hd1080url)	end
	if hdurl then table.insert(arr_videos_urls,hdurl) end
	if hqurl then	table.insert(arr_videos_urls,hqurl)	end
	if baseurl then table.insert(arr_videos_urls,baseurl) end
	if ldurl then table.insert(arr_videos_urls,baseurl) end


	if livehlsurl then
		return { { path = livehlsurl:gsub("\\/", "/"); name = name; description = description; url = vlc.path; arturl = arturl ; artist = artist} }
	else
		if table.getn(arr_videos_urls) > 0 then
			for i=1 , table.getn(arr_videos_urls)  do
				video_url_out = arr_videos_urls[i]:gsub("\\/", "/")

				if prefres < 0 then
					break
				end
				height = string.match( video_url_out, "/cdn/%w+%-%d+x(%d+)/video/" )
				if not height or tonumber(height) <= prefres then
					break
				end
			end
			return { { path = video_url_out; name = name; description = description; url = vlc.path; arturl = arturl; artist = artist} }
		else
			vlc.msg.err("Couldn't extract the video URL from dailymotion")
			return { }
		end
	end
end
