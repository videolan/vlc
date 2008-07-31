/*****************************************************************************
 * JVLC.java: Main Java Class, represents a libvlc_instance_t object
 *****************************************************************************
 *
 * Copyright (C) 1998-2006 the VideoLAN team
 * 
 * Author: Philippe Morin <phmorin@free.fr>
 *
 * Created on 18-jul-2006
 *
 * $Id $
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

import java.lang.Exception;


public class VLCException extends Exception
{

    /**
     * 
     */
    private static final long serialVersionUID = -3063632323017889L;

    public VLCException()
    {
        super();
    }

    public VLCException(String message)
    {
        super(message);
    }

    public VLCException(String message, Throwable cause)
    {
        super(message, cause);
    }

    public VLCException(Throwable cause)
    {
        super(cause);
    }
}
