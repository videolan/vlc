/*****************************************************************************
 * MediaListTest.java: VLC Java Bindings
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

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;


public class MediaListTest
{

    private JVLC jvlc;
    
    private String mrl = getClass().getResource("/raffa_voice.ogg").getFile();
    
    @Before
    public void setup()
    {
        jvlc = new JVLC("-vvv -I dummy --aout=dummy --vout=dummy");
    }
    
    @Test
    public void mediaListAddMedia()
    {
        MediaList mlist = new MediaList(jvlc);
        mlist.addMedia(mrl);
        Assert.assertEquals(1, mlist.size());
    }
    
    @Test
    public void mediaListAddMedia2()
    {
        MediaList mlist = new MediaList(jvlc);
        mlist.addMedia(mrl);
        Assert.assertEquals(1, mlist.size());
        mlist.addMedia(mrl);
        Assert.assertEquals(1, mlist.size());
        mlist.addMedia(new MediaDescriptor(jvlc, mrl));
        Assert.assertEquals(1, mlist.size());
        mlist.addMedia("non-existing");
        Assert.assertEquals(2, mlist.size());
    }
    
    @Test
    public void mediaListRemoveMedia()
    {
        MediaList mlist = new MediaList(jvlc);
        mlist.addMedia(mrl);
        Assert.assertEquals(1, mlist.size());
        mlist.removeMedia(0);
        Assert.assertEquals(0, mlist.size());
    }

    @Test
    public void mediaListRemoveMedia2()
    {
        MediaList mlist = new MediaList(jvlc);
        mlist.addMedia(mrl);
        Assert.assertEquals(1, mlist.size());
        mlist.removeMedia(0);
        Assert.assertEquals(0, mlist.size());
        
        mlist.addMedia(mrl);
        mlist.removeMedia(0);
        Assert.assertEquals(0, mlist.size());
        
        mlist.addMedia(new MediaDescriptor(jvlc, mrl));
        mlist.removeMedia(0);
        Assert.assertEquals(0, mlist.size());
        
        mlist.addMedia(new MediaDescriptor(jvlc, mrl));
        mlist.removeMedia(mrl);
        Assert.assertEquals(0, mlist.size());
        
        mlist.addMedia(new MediaDescriptor(jvlc, mrl));
        mlist.removeMedia(new MediaDescriptor(jvlc, mrl));
        Assert.assertEquals(0, mlist.size());
    }
    
    @Test
    public void mediaListRemoveNonExistingMedia()
    {
        MediaList mlist = new MediaList(jvlc);
        boolean result = mlist.removeMedia(3);
        Assert.assertFalse(result);
    }
    
    @Test
    public void mediaListIndexOfNonExistingMediaDescriptor()
    {
        MediaList mlist = new MediaList(jvlc);
        MediaDescriptor md = new MediaDescriptor(jvlc, "dummy");
        int result = mlist.indexOf(md);
        Assert.assertEquals(-1, result);
    }
    
    @Test(expected = IndexOutOfBoundsException.class)
    public void mediaListGetMediaDesciptorAtInvalidIndex()
    {
        MediaList mlist = new MediaList(jvlc);
        mlist.getMediaDescriptorAtIndex(5);
    }
    
    @Test(expected = IndexOutOfBoundsException.class)
    public void mediaListGetMediaDesciptorAtInvalidIndex2()
    {
        MediaList mlist = new MediaList(jvlc);
        mlist.getMediaDescriptorAtIndex(-5);
    }
    
    @Test(expected = IndexOutOfBoundsException.class)
    public void mediaListGetMediaDesciptorAtInvalidIndex3()
    {
        MediaList mlist = new MediaList(jvlc);
        mlist.getMediaDescriptorAtIndex(0);
    }
}
