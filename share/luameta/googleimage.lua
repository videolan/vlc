-- Get's an artwork from images.google.com
-- $Id$

-- Replace non alphanumeric char by +
function get_query( title )
    -- If we have a .EXT remove the extension.
    str = string.gsub( title, "(.*)%....$", "%1" )
    return string.gsub( str, "([^%w ])",
         function (c) return string.format ("%%%02X", string.byte(c)) end)
end

-- Return the artwork
function fetch_art()
    if vlc.title and vlc.artist then
        title = vlc.artist.." "..vlc.title
    elseif vlc.title then
        title = vlc.title
    elseif vlc.name then
        title = vlc.name
    else
        return nil
    end
    fd = vlc.stream_new( "http://images.google.com/images?q=" .. get_query( title ) )
    page = vlc.stream_read( fd, 65653 )
    vlc.stream_delete( fd )
    _, _, arturl = string.find( page, "imgurl=([^&]+)" )
    return vlc.decode_uri(arturl)
end
