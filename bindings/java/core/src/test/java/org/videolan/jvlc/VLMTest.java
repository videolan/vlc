/*****************************************************************************
 * VLMTest.java: VLC Java Bindings
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

import junit.framework.Assert;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;


public class VLMTest
{
    private JVLC jvlc;
    
    private String mrl = getClass().getResource("/raffa_voice.ogg").getFile();
    
    private String mediaName = "test";
    
    @Before
    public void setup()
    {
        jvlc = new JVLC("-I dummy --aout=dummy --vout=dummy");
        jvlc.setLogVerbosity(LoggerVerbosityLevel.INFO);
    }

    @After
    public void tearDown()
    {
        jvlc.release();
    }
    
    @Test
    public void testVLMInit()
    {
        VLM vlm = jvlc.getVLM();
        Assert.assertNotNull(vlm);
    }
    
    @Test
    public void testAddBroadcast()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
    }
    
    @Test
    public void testShowMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.showMedia(mediaName);
    }
    
    @Test
    public void testDisableMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.disableMedia(mediaName);
    }
    
    @Test
    public void testPlayMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.playMedia(mediaName);
    }
    
    @Test
    public void testPauseMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.playMedia(mediaName);
        vlm.pauseMedia(mediaName);
    }

    @Test
    public void testStopMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.playMedia(mediaName);
        vlm.stopMedia(mediaName);
    }

    @Test
    public void testSeekMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.playMedia(mediaName);
        vlm.seekMedia(mediaName, 0.3f);
    }
    
    @Test
    public void testAddMediaInput()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.addMediaInput(mediaName, "file://" + mrl);
    }
    
    @Test
    public void testEnableMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, false, false);
        vlm.enableMedia(mediaName);
    }
    
    @Test
    public void testDeleteMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, false, false);
        vlm.deleteMedia(mediaName);
    }
    
    @Test
    public void testMediaLoop()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, false, false);
        vlm.setMediaLoop(mediaName, true);
    }
}
