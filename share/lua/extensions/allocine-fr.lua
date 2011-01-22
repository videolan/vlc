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
spin = nil    -- spinning icon
films = {}

-- Extension description
function descriptor()
    return { title = "Allociné (France)" ;
             version = "1.0" ;
             author = "VideoLAN" ;
             url = 'http://www.allocine.fr/' ;
             shortdesc = "Allocine.com" ;
             description = "<center><b>ALLOCINE.COM</b></center>"
                        .. "Récupère des informations sur le média en cours "
                        .. "de lecture depuis Allocine.fr, telles que :<br />"
                        .. "- Casting,<br />- Résumé,<br />- Note des utilisateurs,"
                        .. "<br />- Lien direct vers la fiche du film sur "
                        .. "<a href=\"http://www.allocine.fr\">allocine.fr</a>." ;
             icon = icon_allocine ;
             capabilities = { "input-listener", "meta-listener" }}
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
    spin = dlg:add_spin_icon(4, 1, 1, 1)
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
        message = dlg:add_label(message_text, 1, 2, 4, 1)
    else
        message:set_text(message_text)
    end
    if list then dlg:del_widget(list) end
    if okay then dlg:del_widget(okay) end
    if html then dlg:del_widget(html) end
    list = nil
    okay = nil
    html = nil

    -- Show progress
    spin:animate()
    dlg:update()

    -- Open URL
    local s, msg = vlc.stream(url)
    if not s then
        vlc.msg.warn("[ALLOCINE.COM] " .. msg)
        spin:stop()
        return
    end
    vlc.keep_alive()

    -- Fetch HTML data (max 65 kb)
    local data = "", tmpdata
    repeat
       tmpdata = s:read(65535)
       vlc.keep_alive()
       if not tmpdata then break end
       data = data .. tmpdata
    until tmpdata == ""

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
        list = dlg:add_list(1, 3, 4, 1)
        for idx, film in ipairs(films) do
            local txt = film.title
            if film.year then txt = txt .. " (" .. film.year .. ")" end
            list:add_value(txt, idx)
        end
        okay = dlg:add_button("Voir la fiche", click_okay, 3, 4, 2, 1)
    end

    -- We're done now
    spin:stop()
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

    -- Show progress
    spin:animate()
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
        html = dlg:add_html("<center><i>Chargement en cours...</i></center>", 1, 3, 4, 1)
    end
    dlg:update()

    -- Open stream
    local s = vlc.stream(url)
    vlc.keep_alive()

    -- Read max 500k (Note: 65k is not enough for the average note)
    local data = s:read(500000)
    vlc.keep_alive()

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
        spin:stop()
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

    spin:stop()
end

-- Icone Allocine
icon_allocine = "\137\80\78\71\13\10\26\10\0\0\0\13\73\72\68\82\0\0\0\32\0\0\0\32\8\3\0\0\0\68\164\138\198\0\0\2\199\80\76\84\69\255\0\255\227\238\241\219\231\234\213\229\233\202\218\222\197\216\222\206\221\225\197\220\225\212\225\228\206\224\228\189\217\223\180\204\211\166\194\203\163\190\200\153\182\193\141\173\185\149\179\189\178\198\203\236\244\246\183\211\219\172\201\211\156\186\197\135\170\182\112\150\172\99\139\163\110\147\165\145\174\188\245\250\251\213\232\238\131\153\157\92\107\110\62\75\78\58\69\70\65\83\89\87\110\122\120\149\164\116\155\178\76\118\145\66\108\137\96\133\155\164\188\196\91\111\114\19\27\28\1\1\1\34\28\1\59\50\5\34\46\48\95\120\128\121\158\176\90\130\155\138\169\183\252\254\254\242\247\249\216\229\231\148\170\170\53\76\77\5\16\20\45\38\4\154\121\5\198\152\3\216\163\3\184\129\0\22\6\0\37\49\51\115\146\159\104\143\165\242\246\244\154\180\190\44\73\84\33\42\41\10\5\1\135\112\10\255\212\13\234\178\4\218\170\1\209\152\0\68\39\1\10\18\19\113\138\145\142\177\193\154\171\175\158\175\155\145\146\74\182\158\25\222\186\18\56\41\4\15\14\1\205\166\11\254\204\13\250\197\11\249\187\2\125\86\0\5\10\11\124\148\153\139\157\160\199\198\129\242\198\8\255\211\18\219\179\14\109\91\7\100\79\4\193\159\12\255\218\18\182\147\11\83\67\4\49\35\1\168\119\0\16\21\22\148\173\181\133\166\179\172\195\203\187\202\202\195\203\161\244\200\16\253\203\19\250\199\17\255\218\15\115\89\1\47\31\0\180\207\216\151\181\192\226\234\236\154\175\178\144\168\164\224\192\24\170\130\2\91\66\1\187\205\210\133\160\162\178\164\43\28\20\1\242\190\11\146\102\1\62\86\90\37\66\83\109\100\27\119\98\9\130\107\8\187\138\3\175\195\199\208\219\223\219\233\237\21\33\34\18\34\42\43\49\26\227\188\18\21\17\2\123\102\11\53\45\3\90\78\7\13\25\27\201\168\14\237\186\5\240\184\3\84\60\0\114\126\129\216\236\241\175\200\206\196\214\219\179\200\205\147\124\11\98\84\10\233\187\13\138\87\0\228\241\244\185\209\215\194\209\213\12\15\16\184\154\15\221\178\5\183\134\0\97\110\112\229\246\250\186\210\217\30\27\3\243\195\16\123\99\5\226\182\10\215\171\2\204\158\1\42\20\0\121\134\138\238\251\254\40\51\52\67\56\4\137\109\8\204\162\1\152\165\167\191\217\224\73\90\92\140\112\4\141\100\0\230\179\5\17\12\1\211\165\1\174\127\0\68\78\80\167\196\208\110\140\151\78\59\1\138\110\6\197\149\1\195\140\0\93\61\3\52\60\61\101\82\4\202\148\0\113\80\1\194\135\0\205\143\2\192\201\202\209\223\225\84\49\1\156\110\1\163\107\1\124\78\3\28\13\1\144\156\156\116\152\169\130\147\155\84\126\151\125\161\178\202\134\2\180\121\3\173\185\186\63\106\132\47\63\72\54\41\11\68\108\132\52\95\123\132\161\173\52\64\66\103\117\120\164\182\190\85\119\139\205\227\233\140\166\177\8\106\210\201\0\0\0\1\116\82\78\83\0\64\230\216\102\0\0\0\1\98\75\71\68\43\36\185\228\8\0\0\0\9\112\72\89\115\0\0\0\72\0\0\0\72\0\70\201\107\62\0\0\3\15\73\68\65\84\56\203\99\96\64\2\140\76\204\44\172\108\236\28\12\216\1\35\39\23\55\15\47\31\63\191\0\159\32\22\105\33\102\118\86\86\46\97\17\17\30\81\49\113\9\73\41\52\105\105\70\102\25\25\110\89\57\121\5\5\69\37\101\126\21\85\53\117\13\20\121\33\70\70\65\77\45\109\48\208\209\213\211\55\48\84\51\52\66\200\27\11\153\152\154\153\91\128\100\45\173\172\109\108\237\180\237\29\84\12\29\249\225\242\78\198\28\206\142\46\174\22\218\110\218\238\30\158\94\222\62\218\218\190\126\254\226\42\220\48\11\140\153\2\2\131\130\67\66\181\181\195\194\35\34\61\163\162\129\70\197\196\138\198\65\45\49\54\118\226\141\79\136\72\76\76\74\78\73\77\75\207\200\76\201\114\3\170\200\206\17\205\205\131\26\144\207\92\80\88\84\84\236\145\86\146\102\5\114\72\105\25\136\84\40\175\16\0\203\75\87\86\113\87\215\20\1\65\100\98\58\216\31\181\117\96\42\174\92\160\30\20\66\210\249\177\13\14\141\105\32\21\53\58\218\64\135\54\53\183\128\21\200\179\243\242\128\130\64\40\174\181\181\173\189\4\40\159\214\161\29\22\166\173\221\233\209\5\86\224\219\45\204\211\195\32\196\216\171\104\209\215\63\1\232\136\180\137\147\180\39\135\79\153\90\24\105\211\4\177\131\51\175\155\129\177\151\213\53\102\154\111\204\244\196\226\52\119\237\73\33\105\137\137\197\51\102\206\2\43\152\61\103\238\60\134\94\153\249\190\32\206\130\180\226\162\41\218\11\129\100\81\241\162\25\139\193\10\228\150\44\93\198\192\204\156\183\28\196\89\152\24\49\93\91\123\5\200\173\197\139\86\174\2\133\132\246\234\53\107\129\10\88\231\130\35\105\221\250\146\13\218\150\105\32\5\235\55\110\218\188\5\36\182\117\27\235\60\6\38\97\225\237\96\243\86\36\238\208\182\220\25\14\148\95\180\114\215\38\176\35\118\75\179\212\51\48\113\239\217\171\13\242\253\190\8\144\211\39\37\69\46\90\185\105\243\166\253\160\232\88\38\205\2\244\102\125\121\44\216\132\140\3\7\65\10\195\155\189\14\237\218\117\232\48\144\125\68\90\136\5\24\146\107\121\121\92\193\233\100\243\36\160\57\59\14\0\165\55\111\222\220\5\116\196\110\227\74\144\2\54\209\163\199\32\49\112\28\72\156\240\58\228\181\217\250\100\215\169\211\218\103\164\141\33\233\59\207\136\71\30\164\224\172\117\166\182\238\161\115\45\231\15\159\235\186\112\113\113\204\37\99\198\203\96\5\172\252\98\82\246\64\5\186\155\79\89\117\157\187\162\173\125\240\234\181\235\87\110\220\52\150\238\129\166\168\185\183\114\115\129\94\213\233\178\61\117\238\26\200\172\48\96\48\221\54\150\230\96\129\37\74\141\59\226\119\91\99\180\247\223\187\127\237\138\118\12\72\137\235\3\99\161\203\203\16\201\218\232\161\170\202\177\71\143\125\110\64\18\190\235\86\230\109\189\172\245\200\25\67\249\201\83\9\255\103\250\138\207\207\156\57\242\162\138\115\13\80\122\30\106\214\122\249\234\201\157\187\162\34\229\220\92\236\175\217\230\113\119\207\199\146\59\149\213\29\111\61\227\23\112\22\213\208\192\38\13\2\57\111\114\158\229\104\228\160\88\14\0\116\98\15\30\53\254\119\27\0\0\0\37\116\69\88\116\100\97\116\101\58\99\114\101\97\116\101\0\50\48\49\49\45\48\49\45\49\56\84\50\50\58\53\53\58\50\51\43\48\49\58\48\48\147\59\131\62\0\0\0\37\116\69\88\116\100\97\116\101\58\109\111\100\105\102\121\0\50\48\49\49\45\48\49\45\49\56\84\50\50\58\53\53\58\50\51\43\48\49\58\48\48\226\102\59\130\0\0\0\0\73\69\78\68\174\66\96\130"
