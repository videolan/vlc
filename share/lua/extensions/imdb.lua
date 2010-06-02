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

-- TODO: Use simplexml module to simplify parsing

-- Global variables
url = nil          -- string
title = nil        -- string
titles = {}        -- table, see code below

-- Some global variables: widgets
dlg = nil          -- dialog
txt = nil          -- text field
list = nil         -- list widget
button_open = nil  -- button widget
html = nil         -- rich text (HTML) widget
waitlbl = nil      -- text label widget

-- Script descriptor, called when the extensions are scanned
function descriptor()
    return { title = "IMDb - The Internet Movie Database" ;
             version = "1.0" ;
             author = "Jean-Philippe André" ;
             url = 'http://www.imdb.org/';
             shortdesc = "The Internet Movie Database";
             description = "<center><b>The Internet Movie Database</b></center><br />"
                        .. "Get information about movies from the Internet "
                        .. "Movie Database (IMDb).<br />This Extension will show "
                        .. "you the cast, a short plot summary and a link to "
                        .. "the web page on imdb.org." ;
             capabilities = { "input-listener" } }
end

-- Remove trailing & leading spaces
function trim(str)
    if not str then return "" end
    return string.gsub(str, "^%s*(.*)+%s$", "%1")
end

-- Update title text field. Removes file extensions.
function update_title()
    local item = vlc.input.item()
    local name = item and item:name()
    if name ~= nil then
        name = string.gsub(name, "(.*)(%.%w+)$", "%1")
    end
    if name ~= nil then
        txt:set_text(trim(name))
    end
end

-- Function called when the input (media being read) changes
function input_changed()
    update_title()
end

-- First function to be called when the extension is activated
function activate()
    create_dialog()
end

-- This function is called when the extension is disabled
function deactivate()
end

-- Create the main dialog with a simple search bar
function create_dialog()
    dlg = vlc.dialog("IMDb")
    dlg:add_label("<b>Movie Title:</b>", 1, 1, 1, 1)
    local item = vlc.input.item()
    txt = dlg:add_text_input(item and item:name() or "", 2, 1, 1, 1)
    dlg:add_button("Search", click_okay, 3, 1, 1, 1)
    -- Show, if not already visible
    dlg:show()
end

-- Dialog closed
function close()
    -- Deactivate this extension
    vlc.deactivate()
end

-- Called when the user presses the "Search" button
function click_okay()
    vlc.msg.dbg("[IMDb] Searching for " .. txt:get_text())

    -- Search IMDb: build URL
    title = string.gsub(string.gsub(txt:get_text(), "[%p%s%c]", "+"), "%++", " ")
    url = "http://www.imdb.com/find?s=all&q=" .. string.gsub(title, " ", "+")

    -- Recreate dialog structure: delete useless widgets
    if html then
        dlg:del_widget(html)
        html = nil
    end

    if list then
        dlg:del_widget(list)
        dlg:del_widget(button_open)
        list = nil
        button_open = nil
    end

    -- Ask the user to wait some time...
    local waitmsg = 'Searching for <a href="' .. url .. '">' .. title .. "</a> on IMDb..."
    if not waitlbl then
        waitlbl = dlg:add_label(waitmsg, 1, 2, 3, 1)
    else
        waitlbl:set_text(waitmsg)
    end
    dlg:update()

    -- Download the data
    local s, msg = vlc.stream(url)
    if not s then
        vlc.msg.warn("[IMDb] " .. msg)
        waitlbl:set_text('Sorry, an error occured while searching for <a href="'
                         .. url .. '">' .. title .. "</a>.<br />Please try again later.")
        return
    end

    -- Fetch HTML data
    local data = s:read(65000)
    if not data then
        vlc.msg.warn("[IMDb] Not data received!")
        waitlbl:set_text('Sorry, an error occured while searching for <a href="'
                         .. url .. '">' .. title .. "</a>.<br />Please try again later.")
        return
    end

    -- Probe result & parse it
    if string.find(data, "<h6>Overview</h6>") then
        -- We found a direct match
        parse_moviepage(data)
    else
        -- We have a list of results to parse
        parse_resultspage(data)
    end
end

-- Called when clicked on the "Open" button
function click_open()
    -- Get user selection
    selection = list:get_selection()
    if not selection then return end

    local sel = nil
    for idx, selectedItem in pairs(selection) do
        sel = idx
        break
    end
    if not sel then return end
    local imdbID = titles[sel].id

    -- Update information message
    url = "http://www.imdb.org/title/" .. imdbID .. "/"
    title = titles[sel].title

    dlg:del_widget(list)
    dlg:del_widget(button_open)
    list = nil
    button_open = nil
    waitlbl:set_text("Loading IMDb page for <a href=\"" .. url .. "\">" .. title .. "</a>.")
    dlg:update()

    local s, msg = vlc.stream(url)
    if not s then
        waitlbl:set_text('Sorry, an error occured while looking for <a href="'
                         .. url .. '">' .. title .. "</a>.")
        vlc.msg.warn("[IMDb] " .. msg)
        return
    end

    data = s:read(65000)
    if data and string.find(data, "<h6>Overview</h6>") then
        parse_moviepage(data)
    else
        waitlbl:set_text('Sorry, no results found for <a href="'
                         .. url .. '">' .. title .. "</a>.")
    end
end

-- Parse the results page and find titles, years & URL's
function parse_resultspage(data)
    vlc.msg.dbg("[IMDb] Analysing results page")

    -- Find titles
    titles = {}
    local count = 0

    local idxEnd = 1
    while idxEnd ~= nil do
        -- Find title types
        local titleType = nil
        _, idxEnd, titleType = string.find(data, "<b>([^<]*Titles[^<]*)</b>", idxEnd)
        local _, _, nextTitle = string.find(data, "<b>([^<]*Titles[^<]*)</b>", idxEnd)
        if not titleType then
            break
        else
            -- Find current scope
            local table = nil
            if not nextTitle then
                _, _, table = string.find(data, "<table>(.*)</table>", idxEnd)
            else
                nextTitle = string.gsub(nextTitle, "%(", "%%(")
                nextTitle = string.gsub(nextTitle, "%)", "%%)")
                _, _, table = string.find(data, "<table>(.*)</table>.*"..nextTitle, idxEnd)
            end

            if not table then break end
            local pos = 0
            local thistitle = nil

            -- Find all titles in this scope
            while pos ~= nil do
                local _, _, link = string.find(table, "<a href=\"([^\"]+title[^\"]+)\"", pos)
                if not link then break end -- this would not be normal behavior...
                _, pos, thistitle = string.find(table, "<a href=\"" .. link .. "\"[^>]*>([^<]+)</a>", pos)
                if not thistitle then break end -- this would not be normal behavior...
                local _, _, year = string.find(table, "\((%d+)\)", pos)
                -- Add this title to the list
                count = count + 1
                local _, _, imdbID = string.find(link, "/([^/]+)/$")
                thistitle = replace_html_chars(thistitle)
                titles[count] = { id = imdbID ; title = thistitle ; year = year ; link = link }
            end
        end
    end

    -- Did we find anything at all?
    if not count or count == 0 then
        waitlbl:set_text('Sorry, no results found for <a href="'
                         .. url .. '">' .. title .. "</a>.")
        return
    end

    -- Sounds good, we found some results, let's display them
    waitlbl:set_text(count .. " results found for <a href=\"" .. url .. "\">" .. title .. "</a>.")
    list = dlg:add_list(1, 3, 3, 1)
    button_open = dlg:add_button("Open", click_open, 3, 4, 1, 1)

    for idx, title in ipairs(titles) do
        --list:add_value("[" .. title.id .. "] " .. title.title .. " (" .. title.year .. ")", idx)
        list:add_value(title.title .. " (" .. title.year .. ")", idx)
    end
end

-- Parse a movie description page
function parse_moviepage(data)
    -- Title & year
    title = string.gsub(data, "^.*<title>(.*)</title>.*$", "%1")
    local text = "<h1>" .. title .. "</h1>"
    text = text .. "<h2>Overview</h2><table>"

    -- Real URL
    url = string.gsub(data, "^.*<link rel=\"canonical\" href=\"([^\"]+)\".*$", "%1")
    local imdbID = string.gsub(url, "^.*/title/([^/]+)/.*$", "%1")
    if imdbID then
        url = "http://www.imdb.org/title/" .. imdbID .. "/"
    end

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
    local first = true
    for nm, char in string.gmatch(data, "<td class=\"nm\"><a[^>]+>([%w%s]+)</a></td><td class=\"ddd\"> ... </td><td class=\"char\"><a[^>]+>([%w%s]+)</a>") do
        if not first then
            actors = actors .. "<tr><td />"
        end
        actors = actors .. "<td>" .. nm .. "</td><td><i>" .. char .. "</i></td></tr>"
        first = false
    end
    text = text .. actors .. "</table>"

    waitlbl:set_text("<center><a href=\"" .. url .. "\">" .. title .. "</a></center>")
    if list then
        dlg:del_widget(list)
        dlg:del_widget(button_open)
    end
    html = dlg:add_html(text .. "<br />Loading summary...", 1, 3, 3, 1)
    dlg:update()

    text = text .. "<h2>Plot Summary</h2>"
    local s, msg = vlc.stream(url .. "plotsummary")
    if not s then
        vlc.msg.warn("[IMDb] " .. msg)
        return
    end
    local data = s:read(65000)

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

