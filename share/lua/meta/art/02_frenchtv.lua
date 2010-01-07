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
        ["TF1"] = "TF1-2006.png",
        ["France 2"] = "FR2-DSK-couleur.png",
        ["France 3"] = "FR3-DSK-couleur.png",
        ["France 4"] = "FR4-DSK-couleur.png",
        ["France 5"] = "FR5-DSK-couleur.png",
        ["Direct 8"] = "Direct8-2009.png",
        ["NRJ 12"] = "NRJ12-2009.png",
        ["iTele"] = "iTELE-2008.png",
        ["W9"] = "W9.png"
    }
    local meta = vlc.item.metas(vlc.item);
    local channel
    if meta["title"] then
        channel = meta["title"]
    else
        channel = meta["filename"]
    end

    -- trim
    channel = string.gsub(channel, "^%s*(.-)%s*$", "%1")

    local url = urlsForChannel[channel];

    if url then
        return "http://cyril.bourreau.free.fr/Vectoriel/"..url
    else
        return nil
    end
end
