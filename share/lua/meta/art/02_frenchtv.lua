--[[
 Gets an artwork from amazon

 $Id$
 Copyright © 2007 the VideoLAN team

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

-- Return the artwork
function fetch_art()
    local urlsForChannel = {
    -- on http://cyril.bourreau.free.fr/Vectoriel/
        ["TF1"] = "http://cyril.bourreau.free.fr/Vectoriel/TF1-2006.png",
        ["France 2"] = "http://cyril.bourreau.free.fr/Vectoriel/FR2-DSK-couleur.png",
        ["France 3"] = "http://cyril.bourreau.free.fr/Vectoriel/FR3-DSK-couleur.png",
        ["France 4"] = "http://cyril.bourreau.free.fr/Vectoriel/FR4-DSK-couleur.png",
        ["France 5"] = "http://cyril.bourreau.free.fr/Vectoriel/FR5-DSK-couleur.png",
        ["Direct 8"] = "http://cyril.bourreau.free.fr/Vectoriel/Direct8-2009.png",
        ["NRJ 12"] = "http://cyril.bourreau.free.fr/Vectoriel/NRJ12-2009.png",
        ["iTele"] = "http://cyril.bourreau.free.fr/Vectoriel/iTELE-2008.png",
        ["W9"] = "http://cyril.bourreau.free.fr/Vectoriel/W9.png",

        ["Arte"] = "http://www.artepro.com/fr_fichiers/upload/10594.jpg",
        ["TMC"] = "http://upload.wikimedia.org/wikipedia/fr/4/4b/Logo_de_TMC.gif",
        ["i> TELE"] = "http://upload.wikimedia.org/wikipedia/fr/5/56/Logo_I_tele.png",
        ["BFM TV"] = "http://upload.wikimedia.org/wikipedia/fr/3/30/Bfm_tv.jpg",
        ["Virgin 17"] = "http://upload.wikimedia.org/wikipedia/fr/3/39/Virgin17logo.png",
        ["La Chaîne Parlementaire"] = "http://upload.wikimedia.org/wikipedia/fr/9/98/Public-Senat-LCP-An_logo_2010.png"
    }
    local meta = vlc.item:metas();
    local channel
    if meta["title"] then
        channel = meta["title"]
    else
        channel = meta["filename"]
    end

    -- Replace "France 2 HD" by "France 2"
    channel = string.gsub(channel, "^(.-)%sHD%s*$", "%1")

    -- Replace "France 2 (bas débit)" by "France 2"
    channel = string.gsub(channel, "^(.-)%s%(bas débit%)%s*$", "%1")

    -- trim
    channel = string.gsub(channel, "^%s*(.-)%s*$", "%1")

    return urlsForChannel[channel]
end
