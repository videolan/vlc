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

function descriptor()
    return { scope="network" }
end

-- Return the artwork
function fetch_art()
    local urlsForChannel = {
        ["TF1"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/7/77/TF1_(2013).svg/610px-TF1_(2013).svg.png",
        ["France 2"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/e/e8/France_2_logo_antenne_(2008).png/270px-France_2_logo_antenne_(2008).png",
        ["France 3"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/3/33/France_3_logo_2016.svg/275px-France_3_logo_2016.svg.png",
        ["Canal+"] = "https://upload.wikimedia.org/wikipedia/commons/thumb/1/1a/Canal%2B.svg/langfr-685px-Canal%2B.svg.png",
        ["France 5"] = "https://upload.wikimedia.org/wikipedia/commons/thumb/4/41/France_5_logo_antenne_(2008).png/270px-France_5_logo_antenne_(2008).png",
        ["M6"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/2/22/M6_2009.svg/450px-M6_2009.svg.png",
        ["Arte"] = "https://upload.wikimedia.org/wikipedia/commons/thumb/0/0e/Arte_Logo_2011.svg/720px-Arte_Logo_2011.svg.png",
        ["C8"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/7/7a/C8_vector.svg/490px-C8_vector.svg.png",
        ["W9"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/2/2b/W9_2012.png/560px-W9_2012.png",
        ["TMC"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/a/a8/TMC_logo_2016.svg/610px-TMC_logo_2016.svg.png",
        ["NT1"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/8/83/NT1_2012.svg/430px-NT1_2012.svg.png",
        ["NRJ 12"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/9/93/NRJ_12_logo_2015.svg/440px-NRJ_12_logo_2015.svg.png",
        ["LCP"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/1/1f/La_cha%C3%AEne_parlementaire.svg/460px-La_cha%C3%AEne_parlementaire.svg.png",
        ["France 4"] = "https://upload.wikimedia.org/wikipedia/commons/thumb/e/ea/France_4_2014.png/270px-France_4_2014.png",
        ["BFM TV"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/c/c3/BFMTV2016.svg/430px-BFMTV2016.svg.png",
        ["CNews"] = "https://upload.wikimedia.org/wikipedia/commons/thumb/e/e3/Canal_News_logo.svg/685px-Canal_News_logo.svg.png",
        ["CStar"] = "https://upload.wikimedia.org/wikipedia/commons/thumb/b/b7/CStar_vector.svg/675px-CStar_vector.svg.png",
        ["Gulli"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/c/c6/Gulli_2010_avril_logo.svg/440px-Gulli_2010_avril_logo.svg.png",
        ["France Ô"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/b/b0/France_%C3%94_logo_antenne_(2008).png/270px-France_%C3%94_logo_antenne_(2008).png",
        ["HD1"] = "https://upload.wikimedia.org/wikipedia/commons/thumb/6/6a/Logo_HD1_2012.svg/530px-Logo_HD1_2012.svg.png",
        ["L'Equipe"] = "https://upload.wikimedia.org/wikipedia/commons/thumb/3/32/L'%C3%89quipe_wordmark.svg/800px-L'%C3%89quipe_wordmark.svg.png",
        ["6ter"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/d/dd/Logo_6ter.png/650px-Logo_6ter.png",
        ["Numéro 23"] = "https://upload.wikimedia.org/wikipedia/commons/thumb/f/fe/Num%C3%A9ro_23_logo.png/530px-Num%C3%A9ro_23_logo.png",
        ["RMC Découverte"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/7/73/Logo_RMC_D%C3%A9couverte_2012.png/630px-Logo_RMC_D%C3%A9couverte_2012.png",
        ["Chérie 25"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/f/f0/Ch%C3%A9rie_25_logo_2015.svg/430px-Ch%C3%A9rie_25_logo_2015.svg.png",
        ["LCI"] = "https://upload.wikimedia.org/wikipedia/fr/thumb/b/b4/LCI_logo_(2016).png/610px-LCI_logo_(2016).png",
        ["France Info"] = "https://upload.wikimedia.org/wikipedia/commons/thumb/0/03/Franceinfo.svg/800px-Franceinfo.svg.png"
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
