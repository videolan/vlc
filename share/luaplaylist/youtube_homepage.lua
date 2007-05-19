function probe()
    return vlc.access == "http" and ( string.match( vlc.path, "youtube.com/$" ) or string.match( vlc.path, "youtube.com/browse" ) )
end

function parse()
    p = {}
    while true
    do
        line = vlc.readline()
        if not line then break end
        for path, artist, name in string.gmatch( line, "href=\"(/watch%?v=[^\"]*)\" onclick=\"_hbLink%('([^']*)','Vid[^\']*'%);\">([^<]*)</a><br/>" )
        do
            path = "http://www.youtube.com" .. path
            name = vlc.resolve_xml_special_chars( name )
            table.insert( p, { path = path; name = name; artist = artist } )
        end
    end
    return p
end
