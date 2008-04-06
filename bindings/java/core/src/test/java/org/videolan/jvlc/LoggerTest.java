/*****************************************************************************
 * LoggerTest.java: VLC Java Bindings
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *
 *
 * $Id $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

package org.videolan.jvlc;

import java.util.Iterator;

import junit.framework.Assert;

import org.junit.Before;
import org.junit.Test;


public class LoggerTest
{

    private JVLC jvlc;
    
    private String mrl = getClass().getResource("/raffa_voice.ogg").getFile();
    
    @Before
    public void setup()
    {
        jvlc = new JVLC("-I dummy --aout=dummy --vout=dummy");
    }
    
    @Test
    public void testLogDebug()
    {
        jvlc.setLogVerbosity(LoggerVerbosityLevel.DEBUG);
        Logger logger = jvlc.getLogger();
        jvlc.play(mrl);
        Assert.assertTrue(logger.count() > 0);
        logger.close();
    }
    
    /**
     * 
     */
    @Test
    public void testLogError()
    {
        jvlc.setLogVerbosity(LoggerVerbosityLevel.DEBUG);
        Logger logger = jvlc.getLogger();
        logger.clear();
        Assert.assertEquals(0, logger.count());
        
        jvlc.play(mrl);
        
        Iterator<LoggerMessage> loggerIterator = logger.iterator();
        while (loggerIterator.hasNext())
        {
            LoggerMessage message = loggerIterator.next();
            Assert.assertNotNull(message.getMessage());
        }
        logger.close();
    }
    
}
