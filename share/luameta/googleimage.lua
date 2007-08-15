-- Get's an artwork from images.google.com
-- $Id$

-- Replace non alphanumeric char by +
function get_query( title )
    return title:gsub( "[^(a-z|A-Z|0-9)]", "+" )
end

-- Return the artwork
function fetch_art()
    if vlc.title then
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
    return arturl
end
