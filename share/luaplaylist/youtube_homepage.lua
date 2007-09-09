--[[
    Parse YouTube homepage and browse pages. Next step is to recode firefox
    in VLC ... using Lua of course ;)
--]]

function probe()
    return vlc.access == "http" and ( string.match( vlc.path, "youtube.com/?$" ) or string.match( vlc.path, "youtube.com/browse" ) )
end

function parse()
    p = {}
    while true
    do
        line = vlc.readline()
        if not line then break end
        for _path, _artist, _name in string.gmatch( line, "href=\"(/watch%?v=[^\"]*)\" onclick=\"_hbLink%('([^']*)','Vid[^\']*'%);\">([^<]*)</a><br/>" )
        do
            path = "http://www.youtube.com" .. _path
            name = vlc.resolve_xml_special_chars( _name )
            artist = _artist
        end
        for _min, _sec in string.gmatch( line, "<span class=\"runtime\">(%d*):(%d*)</span>" )
        do
            duration = 60 * _min + _sec
        end
        if path and name and artist and duration then
            table.insert( p, { path = path; name = name; artist = artist; duration = duration } )
            path = nil
            name = nil
            artist = nil
            duration = nil
        end
    end
    return p
end
