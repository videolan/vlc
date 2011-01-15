--[[
 Allocine Extension for VLC media player 1.1
 French website only

 Copyright © 2010 VideoLAN and AUTHORS

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

-- Lua modules
require "simplexml"

-- Global variables
dlg = nil     -- Dialog
title = nil   -- Text input widget
message = nil -- Label
list = nil    -- List widget
okay = nil    -- Okay button
html = nil    -- HTML box
films = {}

-- Extension description
function descriptor()
    return { title = "Allociné (France)" ;
             version = "1.0" ;
             author = "VideoLAN" ;
             url = 'http://www.allocine.fr/';
             shortdesc = "Allocine.com";
             description = "<center><b>ALLOCINE.COM</b></center>"
                        .. "Récupère des informations sur le média en cours "
                        .. "de lecture depuis Allocine.fr, telles que :<br />"
                        .. "- Casting,<br />- Résumé,<br />- Note des utilisateurs,"
                        .. "<br />- Lien direct vers la fiche du film sur "
                        .. "<a href=\"http://www.allocine.fr\">allocine.fr</a>." ;
             capabilities = { "input-listener", "meta-listener" } }
end

-- Activation hook
function activate()
    vlc.msg.dbg("[ALLOCINE.COM] Welcome on Allocine.fr")
    create_dialog()
end

-- Deactivation hook
function deactivate()
    vlc.msg.dbg("[ALLOCINE.COM] Bye bye!")
end

-- Dialog close hook
function close()
    -- Deactivate this extension
    vlc.deactivate()
end

-- Input change hook
function input_changed()
    title:set_text(get_title())
end

-- Meta change hook
function meta_changed()
    title:set_text(get_title())
end

-- Create the dialog
function create_dialog()
    dlg = vlc.dialog("ALLOCINE.COM")
    dlg:add_label("<b>Titre du film:</b>", 1, 1, 1, 1)
    title = dlg:add_text_input(get_title(), 2, 1, 1, 1)
    dlg:add_button("Rechercher", click_chercher, 3, 1, 1, 1)
end

-- Get clean title from filename
function get_title(str)
    local item = vlc.item or vlc.input.item()
    if not item then
        return ""
    end
    local metas = item:metas()
    if metas["title"] then
        return metas["title"]
    else
        local filename = string.gsub(item:name(), "^(.+)%.%w+$", "%1")
        return trim(filename or item:name())
    end
end

-- Remove leading and trailing spaces
function trim(str)
    if not str then return "" end
    return string.gsub(str, "^%s*(.-)%s*$", "%1")
end

-- Lookup for this movie title
function click_chercher()
    -- Get title
    local name = title:get_text()
    if not name or name == "" then
        vlc.msg.dbg("[ALLOCINE.COM] No title")
        return
    end

    -- Update dialog
    if list then
        dlg:del_widget(list)
        list = nil
    end

    -- Transform spaces and dots into +
    name = string.gsub(string.gsub(name, "[%p%s%c]", "+"), "%++", "+")

    -- Build URL
    local url = "http://www.allocine.fr/recherche/?q=" .. name

    -- Please wait...
    local message_text = "Recherche <a href=\"" .. url .. "\">" .. string.gsub(name, "%+", " ") .. "</a> sur Allociné..."
    if not message then
        message = dlg:add_label(message_text, 1, 2, 3, 1)
    else
        message:set_text(message_text)
    end
    if list then dlg:del_widget(list) end
    if okay then dlg:del_widget(okay) end
    if html then dlg:del_widget(html) end
    list = nil
    okay = nil
    html = nil
    dlg:update()

    -- Open URL
    local s, msg = vlc.stream(url)
    if not s then
        vlc.msg.warn("[ALLOCINE.COM] " .. msg)
    end

    -- Fetch HTML data (max 65 kb)
    local data = s:read(65535)

    -- Clean data
    data = string.gsub(data, "<b>", "")
    data = string.gsub(data, "</b>", "")
    data = string.gsub(data, "%s+", " ")

    -- Data storage
    films = {}

    -- Find categories
    for category in string.gmatch(data, "<[hH]2>%s*([^<]+)%s*</[hH]2>") do
        local category = trim(category)

        -- Split substring corresponding to this table
        local _, first = string.find(data, "<[hH]2>%s*" .. category .. "%s*</[hH]2>")
        first, _ = string.find(data, "<table", first)
        local _, last = string.find(data, "</table>", first)

        -- Find movies and TV shows
        if category == "Films" or category == "Séries TV" then
            -- Read <table> tag as xml
            local substring = string.sub(data, first, last or -1)

            -- Fix Allocine's broken XML (!!!)
            substring = string.gsub(substring, "<div class=\"spacer vmargin10\">", "")

            local xml = simplexml.parse_string(substring)
            for _, tr in ipairs(xml.children) do
                -- Get film title & year
                local film_title = nil
                local film_year = nil
                local node = tr.children[2] -- td
                if node then node = node.children[1] end -- div (1)
                if node then node = node.children[1] end -- div (2)
                local subnode = nil
                if node then
                    for _, subnode in ipairs(node.children) do
                        if subnode.name == "a" and type(subnode.children[1]) == "string" then
                            film_title = trim(subnode.children[1]) -- content of a tag
                        else if subnode.name == "span" and type(subnode.children[1]) == "string" then
                            film_year = trim(subnode.children[1])
                        end end
                    end
                end

                -- Get film cover & URL
                local film_image = nil
                local film_url = nil
                local node = tr.children[1] -- td
                if node then node = node.children[1] end -- a
                if node and node.name == "a" then
                    film_url = node.attributes["href"]
                    node = node.children[1]
                    if node and node.name == "img" then
                        film_image = node.attributes["src"]
                    end
                end

                -- Append fetched information
                if film_title then
                    if string.sub(film_url, 1, 4) ~= "http" then
                        film_url = "http://www.allocine.fr" .. film_url
                    end
                    films[#films+1] = { url = film_url ; image = film_image ; year = film_year ; title = film_title }
                end
            end
        end
    end

    -- Print information
    -- No results found
    if #films == 0 then
        message_text = "<center>Aucun résultat trouvé pour <b>" .. string.gsub(name, "%+", " ") .. "</b>.</center>"
                    .. "Vous pouvez aussi chercher directement sur <a href=\"" .. url .. "\">Allociné</a>."
        message:set_text(message_text)
    end

    -- Only one movie or TV show matches, let's open its page directly
    if #films == 1 then
        message_text = "<center><a href=\"" .. films[1].url .. "\">" .. films[1].title .. "</a></center>"
        message:set_text(message_text)
        dlg:update()
        open_fiche(films[1].url)
    end

    -- More than 1 match, display a list
    if #films > 1 then
        message_text = tostring(#films) .. " films ou séries TV trouvés sur Allociné :"
        message:set_text(message_text)
        list = dlg:add_list(1, 3, 3, 1)
        for idx, film in ipairs(films) do
            local txt = film.title
            if film.year then txt = txt .. " (" .. film.year .. ")" end
            list:add_value(txt, idx)
        end
        okay = dlg:add_button("Voir la fiche", click_okay, 3, 4, 1, 1)
    end
end

-- Click after selection
function click_okay()
    if not films or not #films then return end
    local selection = list:get_selection()
    if not selection then return end

    local sel, _ = next(selection, nil)
    if not sel then return end

    message_text = "<center><a href=\"" .. films[sel].url .. "\">" .. films[sel].title .. "</a></center>"
    message:set_text(message_text)
    dlg:update()
    open_fiche(films[sel].url)
end

-- Open a movie's information page
function open_fiche(url)
    if okay then
        dlg:del_widget(okay)
        okay = nil
    end
    if list then
        dlg:del_widget(list)
        list = nil
    end

    if not html then
        html = dlg:add_html("<center><i>Chargement en cours...</i></center>", 1, 3, 3, 1)
    end
    dlg:update()

    -- Open stream
    local s = vlc.stream(url)
    -- Read max 500k (Note: 65k is not enough for the average note)
    local data = s:read(500000)

    -- Buffer & temp variables
    local first = nil
    local last = nil
    local page = nil
    local sub = nil
    local name = nil

    first, _ = string.find(data, '<div class="rubric">')

    if not first then
        message:set_text("<h2>Erreur !</h2>Désolé, une erreur est survenue pendant le chargement de la fiche.<br />"
                      .. "<a href=\"" .. url .. "\">Cliquez ici pour consulter la page sur Allociné.fr</a>.")
        dlg:del_widget(html)
        return
    end

    -- Extract information
    local last, _ = string.find(data, '<ul id="link_open"')
    if not last then
        last, _ = string.find(data, 'notationbar')
    end
    sub = string.sub(data, first, last-1)

    -- Clean data
    sub = string.gsub(sub, "%s+", " ")
    sub = string.gsub(sub, "</?p>", "<br/>")
    sub = string.gsub(sub, "</?div[^>]*>", "")
    sub = string.gsub(sub, "</?span[^>]*>", "")
    sub = string.gsub(sub, "<%!%-%-[^%-]+%-%->", "")
    sub = string.gsub(sub, "<br%s*/>%s*<br%s*/>", "<br/>")
    page = string.gsub(sub, "Synopsis :.*$", "")

    -- Style
    local synopsis = string.gsub(sub, ".*Synopsis :(.*)", "<h2>Synposis</h2>%1")

    -- Note
    for w in string.gmatch(data, "property=\"v:average\"[^>]*>([^<]+)</span>") do
        local note = trim(w)
        page = page .. "Note moyenne: <b>" .. note .. " / 4</b>"
        for y in string.gmatch(data, "property=\"v:count\"[^>]*>([^<]+)</span>") do
           local nbpeople = trim(y)
           page = page .. " (" .. nbpeople .. " votes)"
           break
        end
        break
    end

    -- Synopsis
    page = page .. synopsis

    -- Movie title
    if string.find(data, '<h1>.*</h1>') then
        name = string.gsub(data, '^.*<h1>%s*(.*)%s*</h1>.*$', '%1')
        name = trim(name)
    end

    page = page .. "<h2>Source</h2>"
    if name then
        page = page .. name .. " sur <a href='" .. url .. "'>Allociné</a>"
    else
        page = page .. "<a href='" .. url .. "'>Allociné</a>"
    end

    page = string.gsub(page, "href=([\"'])/", "href=%1http://www.allocine.fr/")
    html:set_text(page)
end
