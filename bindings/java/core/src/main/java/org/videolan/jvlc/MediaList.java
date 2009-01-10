/*****************************************************************************
 * MediaList.java: VLC Java Bindings, MediaList
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

import java.util.ArrayList;
import java.util.List;

import org.videolan.jvlc.internal.LibVlc.LibVlcEventManager;
import org.videolan.jvlc.internal.LibVlc.LibVlcMedia;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaList;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class MediaList
{

    private final JVLC jvlc;

    private final LibVlcMediaList instance;

    private List<String> items = new ArrayList<String>();

    private LibVlcEventManager eventManager;

    private volatile boolean released;
    
    public MediaList(JVLC jvlc)
    {
        this.jvlc = jvlc;
        libvlc_exception_t exception = new libvlc_exception_t();
        instance = jvlc.getLibvlc().libvlc_media_list_new(jvlc.getInstance(), exception);
        eventManager = jvlc.getLibvlc().libvlc_media_list_event_manager(instance, exception);
    }

    /**
     * @param mrl The media resource locator to add to the media list.
     */
    public void addMedia(String mrl)
    {
        MediaDescriptor descriptor = new MediaDescriptor(jvlc, mrl);
        addMedia(descriptor);
    }

    /**
     * @param descriptor The media descriptor to add to the media list.
     */
    public void addMedia(MediaDescriptor descriptor)
    {
        if (items.contains(descriptor.getMrl()))
        {
            return;
        }
        items.add(descriptor.getMrl());
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_add_media(instance, descriptor.getInstance(), exception);
    }

    /**
     * @return The current number of items in the media list.
     */
    public int size()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return jvlc.getLibvlc().libvlc_media_list_count(instance, exception);
    }

    /**
     * @param descriptor The media descriptor to get the index of.
     * @return The index of the media descriptor, or -1 if not found.
     */
    public int indexOf(MediaDescriptor descriptor)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return jvlc.getLibvlc().libvlc_media_list_index_of_item(instance, descriptor.getInstance(), exception);
    }

    /**
     * @param index The index of the media descriptor to get.
     * @return The media descriptor at the given index.
     * @throws IndexOutOfBoundsException if index is bigger than size() or < 0, or there are no items in the media_list.
     */
    public MediaDescriptor getMediaDescriptorAtIndex(int index)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        if (size() == 0)
        {
            throw new IndexOutOfBoundsException();
        }
        if (index < 0 || index > size())
        {
            throw new IndexOutOfBoundsException();
        }
        LibVlcMedia descriptor = jvlc.getLibvlc().libvlc_media_list_item_at_index(instance, index, exception);
        return new MediaDescriptor(jvlc, descriptor);
    }

    /**
     * @param index The index of the media to remove.
     * @return True if the media was successfully removed, false otherwise.
     */
    public boolean removeMedia(int index)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_remove_index(instance, index, exception);
        if (exception.raised == 0)
        {
            items.remove(index);
            return true;
        }
        return false;
    }

    /**
     * @param mrl The media descriptor mrl.
     */
    public boolean removeMedia(String mrl)
    {
        int index = items.indexOf(mrl);
        if (index == -1)
        {
            return false;
        }
        return removeMedia(index);
    }
    
    /**
     * @param mediaDescriptor The media descriptor to remove.
     */
    public boolean removeMedia(MediaDescriptor mediaDescriptor)
    {
        String mrl = mediaDescriptor.getMrl();
        int index = items.indexOf(mrl);
        if (index == -1)
        {
            return false;
        }
        return removeMedia(index);
    }
    
    /**
     * Removes all items from the media list.
     */
    public void clear()
    {
        for (int i = 0; i < size(); i++)
        {
            removeMedia(i);
        }
    }

    /**
     * @param descriptor The media descriptor to insert.
     * @param index The index of the inserted media descriptor.
     */
    public void insertMediaDescriptor(MediaDescriptor descriptor, int index)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc
            .getLibvlc()
            .libvlc_media_list_insert_media(instance, descriptor.getInstance(), index, exception);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected void finalize() throws Throwable
    {
        release();
        super.finalize();
    }

    /**
     * Returns the instance.
     * @return the instance
     */
    LibVlcMediaList getInstance()
    {
        return instance;
    }

    /**
     * 
     */
    public void release()
    {
        if (released)
        {
            return;
        }
        released = true;
        
        jvlc.getLibvlc().libvlc_media_list_release(instance);
    }



}
