--[[
 Get information about a movie from IMDb

 Copyright © 2009-2010 VideoLAN and AUTHORS

 Authors:  Jean-Philippe André (jpeg@videolan.org)

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

dlg = nil
txt = nil
function descriptor()
    return { title = "IMDb - The Internet Movie Database" ;
             version = "0.1" ;
             author = "Jean-Philippe André" ;
             url = 'http://www.imdb.org/';
             description = "<center><b>The Internet Movie Database</b></center>\n"
                        .. "Get information about movies from the Internet "
                        .. "Movie Database (IMDb).\nThis Extension will show "
                        .. "you the cast, a short plot summary and a link to "
                        .. "the web page on imdb.org." ;
             capabilities = {} }
end

-- Update title text field. Removes file extensions.
function update_title()
    local title = vlc.input.get_title()
    if title ~= nil then
        title = string.gsub(title, "(.*)(%.%w+)$", "%1")
    end
    if title ~= nil then
        txt:set_text(title)
    end
end

function create_dialog()
    dlg = vlc.dialog("IMDb Search")
    dlg:add_label("The Internet Movie Database", 1, 1, 4, 1)
    dlg:add_label("<b>Movie Title</b>", 1, 2, 1, 1)
    txt = dlg:add_text_input(vlc.input.get_title(), 2, 2, 1, 1)
    dlg:add_button("Okay", "click_okay", 3, 2, 1, 1)
    dlg:add_button("*", "update_title", 4, 2, 1, 1)
    dlg:show() -- Show, if not already visible
end

function activate()
    create_dialog()
end

function deactivate()
end

-- Dialog closed
function close()
    -- Deactivate this extension
    vlc.deactivate()
end

-- Some global variables: widgets
list = nil
button_open = nil
titles = nil
html = nil

function click_okay()
    vlc.msg.dbg("Searching for " .. txt:get_text() .. " on IMDb")

    if html then
        dlg:del_widget(html)
        html = nil
    end

    if not list then
        list = dlg:add_list(1, 3, 4, 1)
        button_open = dlg:add_button("Open", "click_open", 1, 4, 4, 1)
    end

    -- Clear previous results
    list:clear()

    -- Search IMDb
    local url = "http://www.imdb.com/find?s=all&q="
    local title = string.gsub(txt:get_text(), " ", "+")
    local s = vlc.stream(url .. title)

    -- Fetch HTML data
    local data = s:read(65000)

    -- Find titles
    titles = {}
    local count = 0

    idxEnd = 1
    while idxEnd ~= nil do
        -- Find title types
        _, idxEnd, titleType = string.find(data, "<b>([^<]*Titles[^<]*)</b>", idxEnd)
        _, _, nextTitle = string.find(data, "<b>([^<]*Titles[^<]*)</b>", idxEnd)
        if not titleType then
            break
        else
            -- Find current scope
            if not nextTitle then
                _, _, table = string.find(data, "<table>(.*)</table>", idxEnd)
            else
                nextTitle = string.gsub(nextTitle, "%(", "%%(")
                nextTitle = string.gsub(nextTitle, "%)", "%%)")
                _, _, table = string.find(data, "<table>(.*)</table>.*"..nextTitle, idxEnd)
            end
            -- Find all titles in this scope
            if not table then break end
            pos = 0
            while pos ~= nil do
                _, _, link = string.find(table, "<a href=\"([^\"]+title[^\"]+)\"", pos)
                if not link then break end -- this would not be normal behavior...
                _, pos, title = string.find(table, "<a href=\"" .. link .. "\"[^>]*>([^<]+)</a>", pos)
                if not title then break end -- this would not be normal behavior...
                _, _, year = string.find(table, "\((%d+)\)", pos)
                -- Add this title to the list
                count = count + 1
                _, _, imdbID = string.find(link, "/([^/]+)/$")
                title = replace_html_chars(title)
                titles[count] = { id = imdbID ; title = title ; year = year ; link = link }
            end
        end
    end

    for idx, title in ipairs(titles) do
        list:add_value("[" .. title.id .. "] " .. title.title .. " (" .. title.year .. ")", idx)
    end
end

function click_open()
    selection = list:get_selection()
    if not selection then return 1 end
    if not html then
        html = dlg:add_html("Loading IMDb page...", 1, 3, 4, 1)
        -- userLink = dlg:add_label("", 1, 4, 5, 1)
    end

    dlg:del_widget(list)
    dlg:del_widget(button_open)
    list = nil
    button_open = nil

    local sel = nil
    for idx, selectedItem in pairs(selection) do
        sel = idx
        break
    end
    imdbID = titles[sel].id
    url = "http://www.imdb.org/title/" .. imdbID .. "/"

    -- userLink:set_text("<a href=\"url\">" .. url .. "</a>")

    local s = vlc.stream(url)
    data = s:read(65000)

    text = "<h1>" .. titles[sel].title .. " (" .. titles[sel].year .. ")</h1>"
    text = text .. "<h2>Overview</h2><table>"

    -- Director
    local director = nil
    _, nextIdx, _ = string.find(data, "<div id=\"director-info\"", 1, true)
    if nextIdx then
        _, _, director = string.find(data, "<a href[^>]+>([%w%s]+)</a>", nextIdx)
    end
    if not director then
        director = "(Unknown)"
    end
    text = text .. "<tr><td><b>Director</b></td><td>" .. director .. "</td></tr>"

    -- Main genres
    local genres = "<tr><td><b>Genres</b></td>"
    local first = true
    for genre, _ in string.gmatch(data, "/Sections/Genres/(%w+)/\">") do
        if first then
            genres = genres .. "<td>" .. genre .. "</td></tr>"
        else
            genres = genres .. "<tr><td /><td>" .. genre .. "</td></tr>"
        end
        first = false
    end
    text = text .. genres

    -- List main actors
    local actors = "<tr><td><b>Cast</b></td>"
    first = true
    for nm, char in string.gmatch(data, "<td class=\"nm\"><a[^>]+>([%w%s]+)</a></td><td class=\"ddd\"> ... </td><td class=\"char\"><a[^>]+>([%w%s]+)</a>") do
        if not first then
            actors = actors .. "<tr><td />"
        end
        actors = actors .. "<td>" .. nm .. "</td><td><i>" .. char .. "</i></td></tr>"
        first = false
    end
    text = text .. actors .. "</table>"

    text = text .. "<h2>Plot Summary</h2>"
    s = vlc.stream(url .. "plotsummary")
    data = s:read(65000)

    -- We read only the first summary
    _, _, summary = string.find(data, "<p class=\"plotpar\">([^<]+)")
    if not summary then
        summary = "(Unknown)"
    end
    text = text .. "<p>" .. summary .. "</p>"
    text = text .. "<p><h2>Source IMDb</h2><a href=\"" .. url .. "\">" .. url .. "</a></p>"

    html:set_text(text)
end

-- Convert some HTML characters into UTF8
function replace_html_chars(txt)
    if not txt then return nil end
    -- return vlc.strings.resolve_xml_special_chars(txt)
    for num in string.gmatch(txt, "&#x(%x+);") do
        -- Convert to decimal (any better way?)
        dec = 0
        for c in string.gmatch(num, "%x") do
            cc = string.byte(c) - string.byte("0")
            if (cc >= 10 or cc < 0) then
                cc = string.byte(string.lower(c)) - string.byte("a") + 10
            end
            dec = dec * 16 + cc
        end
        txt = string.gsub(txt, "&#x" .. num .. ";", string.char(dec))
    end
    return txt
end

