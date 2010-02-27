--[[
 $Id$

 Copyright © 2010 VideoLAN and AUTHORS

 Authors: Fabio Ritrovato <sephiroth87 at videolan dot org>

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
    return { title="French TV" }
end

function main()
    node = vlc.sd.add_node( {title="Canal +"} )
    node:add_subitem( {title="Le SAV des émissions ",path="http://www.canalplus.fr/index.php?pid=1782",options={"http-forward-cookies"}} )
    node:add_subitem( {title="Les Guignols",path="http://www.canalplus.fr/index.php?pid=1784",options={"http-forward-cookies"}} )
    node:add_subitem( {title="La Météo de Pauline Lefevre",path="http://www.canalplus.fr/index.php?pid=2834",options={"http-forward-cookies"}} )
end
