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
import java.util.LinkedHashSet;
import java.util.List;

import org.videolan.jvlc.internal.LibVlc.LibVlcEventManager;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaDescriptor;
import org.videolan.jvlc.internal.LibVlc.LibVlcMediaList;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public class MediaList
{

    private final JVLC jvlc;

    private final LibVlcMediaList instance;

    private final LibVlcEventManager eventManager;

    private List<String> items = new ArrayList<String>();

    public MediaList(JVLC jvlc)
    {
        this.jvlc = jvlc;
        libvlc_exception_t exception = new libvlc_exception_t();
        instance = jvlc.getLibvlc().libvlc_media_list_new(jvlc.getInstance(), exception);
        eventManager = jvlc.getLibvlc().libvlc_media_list_event_manager(instance, exception);
    }

    public void addMedia(String media)
    {
        MediaDescriptor descriptor = new MediaDescriptor(jvlc, media);
        addMediaDescriptor(descriptor);
    }

    public void addMediaDescriptor(MediaDescriptor descriptor)
    {
        if (items.contains(descriptor.getMrl()))
        {
            return;
        }
        items.add(descriptor.getMrl());
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc.getLibvlc().libvlc_media_list_add_media_descriptor(instance, descriptor.getInstance(), exception);
    }

    public int itemsCount()
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return jvlc.getLibvlc().libvlc_media_list_count(instance, exception);
    }

    public int indexOf(MediaDescriptor descriptor)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        return jvlc.getLibvlc().libvlc_media_list_index_of_item(instance, descriptor.getInstance(), exception);
    }

    public MediaDescriptor getMediaDescriptorAtIndex(int index)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        LibVlcMediaDescriptor descriptor = jvlc.getLibvlc().libvlc_media_list_item_at_index(instance, index, exception);
        return new MediaDescriptor(jvlc, descriptor);
    }

    /**
     * @param index The index of the media to remove
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
     * @param media The media descriptor mrl
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

    public void insertMediaDescriptor(MediaDescriptor descriptor, int index)
    {
        libvlc_exception_t exception = new libvlc_exception_t();
        jvlc
            .getLibvlc()
            .libvlc_media_list_insert_media_descriptor(instance, descriptor.getInstance(), index, exception);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    protected void finalize() throws Throwable
    {
        jvlc.getLibvlc().libvlc_media_list_release(instance);
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
     * @param mediaDescriptor
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

}
