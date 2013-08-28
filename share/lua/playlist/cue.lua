--[[
   Parse CUE files

 $Id$
 Copyright (C) 2009 Laurent Aimar

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

-- Probe function.
function probe()
	if( not string.match( string.upper( vlc.path ), ".CUE$" ) ) then
		return false
	end
	header = vlc.peek( 2048 )
	return string.match( header, "FILE.*WAVE%s*[\r\n]+" ) or
	       string.match( header, "FILE.*AIFF%s*[\r\n]+" ) or
	       string.match( header, "FILE.*MP3%s*[\r\n]+" )
end

-- Helpers
function is_utf8( src )
    return vlc.strings.from_charset( "UTF-8", src ) == src
end

function cue_string( src )
	if not is_utf8( src ) then
		-- Convert to UTF-8 since it's probably Latin1
		src = vlc.strings.from_charset( "ISO_8859-1", src )
	end
	local sub = string.match( src, "^\"(.*)\".*$" );
	if( sub ) then
		return sub
	end
	return string.match( src, "^(%S+).*$" )
end

function cue_path( src )
	if( string.match( src, "^/" ) or
		string.match( src, "^\\" ) or
		string.match( src, "^[%l%u]:\\" ) ) then
		return vlc.strings.make_uri(src)
	end

	local slash = string.find( string.reverse( vlc.path ), '/' )
        local prefix = vlc.access .. "://" .. string.sub( vlc.path, 1, -slash )
        -- FIXME: postfix may not be encoded correctly (esp. slashes)
        local postfix = vlc.strings.encode_uri_component(src)
	return prefix .. postfix
end

function cue_track( global, track )
	if( track.index01 == nil ) then
		return nil
	end

	t = {}
	t.path = cue_path( track.file or global.file )
	t.title = track.title
	t.album = global.title
	t.artist = track.performer or global.performer
	t.genre = track.genre or global.genre
	t.date = track.date or global.date
	t.description = global.comment
	t.tracknum = track.num
	t.options = { ":start-time=" .. math.floor(track.index01) }

	return t
end

function cue_append( tracks, global, track )
	local t = cue_track( global, track )
	if( t ~= nil ) then
		if( #tracks > 0 ) then
			local prev = tracks[#tracks]
			table.insert( prev.options, ":stop-time=" .. math.floor(track.index01) )
		end
		table.insert( tracks, t )
	end
end

-- Parse function.
function parse()
    p = {}
	global_data = nil
	data = {}
	file = nil

    while true
    do 
        line = vlc.readline()
        if not line then break end

		cmd, arg = string.match( line, "^%s*(%S+)%s*(.*)$" )

		if(  cmd == "REM" and arg ) then
			subcmd, value = string.match( arg, "^(%S+)%s*(.*)$" )
			if( subcmd == "GENRE" and value ) then
				data.genre = cue_string( value )
			elseif( subcmd == "DATE" and value ) then
				data.date = cue_string( value )
			elseif( subcmd == "COMMENT" and value ) then
				data.comment = cue_string( value )
			end
		elseif( cmd == "PERFORMER" and arg ) then
			data.performer = cue_string( arg )
		elseif( cmd == "TITLE" and arg ) then
			data.title = cue_string( arg )
		elseif( cmd == "FILE" ) then
			file = cue_string( arg )
		elseif( cmd == "TRACK" ) then
			if( not global_data ) then
				global_data = data
			else
				cue_append( p, global_data, data )
			end
			data =  { file = file, num = string.match( arg, "^(%d+)" ) }
		elseif( cmd == "INDEX" ) then
			local idx, m, s, f = string.match( arg, "(%d+)%s+(%d+):(%d+):(%d+)" )
			if( idx == "01" and m ~= nil and s ~= nil and f ~= nil ) then
				data.index01 = m * 60 + s + f / 75
			end
		end
    end

	cue_append( p, global_data, data )

    return p
end

