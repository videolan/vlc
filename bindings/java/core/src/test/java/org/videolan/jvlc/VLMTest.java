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

import org.junit.Test;


public class VLMTest extends AbstractJVLCTest
{
    private String mediaName = "test";
    
    @Test
    public void testVLMInit()
    {
        VLM vlm = jvlc.getVLM();
        Assert.assertNotNull(vlm);
    }
    
    //@Test
    public void testAddBroadcast()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
    }

    //@Test(timeout = 2000L)
    public void testAddVod()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addVod(mediaName, "file://" + mrl, null, true, null);
    }
    
    //@Test
    public void testShowBroadcastMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.showMedia(mediaName);
    }

    //@Test
    public void testShowVodMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addVod(mediaName, "file://" + mrl, null, true, null);
        vlm.showMedia(mediaName);
    }
    
    //@Test
    public void testDisableBroadcastMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.disableMedia(mediaName);
    }

    //@Test
    public void testDisableVodMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addVod(mediaName, "file://" + mrl, null, true, null);
        vlm.disableMedia(mediaName);
    }
    
    //@Test
    public void testPauseBroadcastMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.playMedia(mediaName);
        vlm.pauseMedia(mediaName);
        vlm.stopMedia(mediaName);
    }

    //@Test
    public void testPauseVodMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addVod(mediaName, "file://" + mrl, null, true, null);
        vlm.playMedia(mediaName);
        vlm.pauseMedia(mediaName);
        vlm.stopMedia(mediaName);
    }


    //@Test
    public void testStopBroadcastMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.playMedia(mediaName);
        vlm.stopMedia(mediaName);
    }

    //@Test
    public void testStopVodMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addVod(mediaName, "file://" + mrl, null, true, null);
        vlm.playMedia(mediaName);
        vlm.stopMedia(mediaName);
    }

    //@Test
    public void testSeekBroadcastMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.playMedia(mediaName);
        vlm.seekMedia(mediaName, 0.3f);
        vlm.stopMedia(mediaName);
    }

    //@Test
    public void testSeekVodMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addVod(mediaName, "file://" + mrl, null, true, null);
        vlm.playMedia(mediaName);
        vlm.seekMedia(mediaName, 0.3f);
        vlm.stopMedia(mediaName);
    }

    //@Test
    public void testAddMediaInputToBroadcast()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, true, false);
        vlm.addMediaInput(mediaName, "file://" + mrl);
    }

    //@Test
    public void testAddMediaInputToVod()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addVod(mediaName, "file://" + mrl, null, true, null);
        vlm.addMediaInput(mediaName, "file://" + mrl);
    }

    //@Test
    public void testEnableBroadcastMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, false, false);
        vlm.enableMedia(mediaName);
    }

    //@Test
    public void testEnableVodMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addVod(mediaName, "file://" + mrl, null, true, null);
        vlm.enableMedia(mediaName);
    }
    
    //@Test
    public void testDeleteBroadcastMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, false, false);
        vlm.deleteMedia(mediaName);
    }

    //@Test
    public void testDeleteVodMedia()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addVod(mediaName, "file://" + mrl, null, true, null);
        vlm.deleteMedia(mediaName);
    }

    //@Test
    public void testMediaLoop()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addBroadcast(mediaName, "file://" + mrl, "", null, false, false);
        vlm.setMediaLoop(mediaName, true);
    }

    //@Test
    public void testSetMux()
    {
        VLM vlm = jvlc.getVLM();
        vlm.addVod(mediaName, "file://" + mrl, null, true, null);
        vlm.setMux(mediaName, "ts");
    }

}
