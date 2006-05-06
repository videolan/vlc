/*****************************************************************************
 * VLMIntf.java: VLM Interface
 *****************************************************************************
 * 
 * Copyright (C) 1998-2006 the VideoLAN team
 * 
 * Author: Filippo Carone <filippo@carone.org>
 * 
 * Created on 28-feb-2006
 *
 * $Id$
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 * 
 */

package org.videolan.jvlc;

public interface VLMIntf {
    void addBroadcast( String name, String input, String output, String[] options, boolean enabled, boolean loop );
    void deleteMedia( String name );
    void setEnabled( String name, boolean enabled );
    void setOutput( String name, String output );
    void setInput( String name, String input );
    void setLoop( String name, boolean loop );
    void changeMedia( String name, String input, String output, String[] options, boolean enabled, boolean loop );
    
}
