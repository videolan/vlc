/*****************************************************************************
 * VLMInternalTest.java: VLC Java Bindings
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

package org.videolan.jvlc.internal;

import java.io.BufferedOutputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.net.URLConnection;

import junit.framework.Assert;

import org.apache.commons.io.IOUtils;
import org.junit.After;
import org.junit.Before;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.videolan.jvlc.internal.LibVlc.LibVlcInstance;
import org.videolan.jvlc.internal.LibVlc.libvlc_exception_t;


public abstract class AbstractVLCInternalTest
{

    protected LibVlc libvlc = LibVlc.SYNC_INSTANCE;

    protected LibVlcInstance libvlcInstance;

    protected String mrl;

    protected libvlc_exception_t exception;

    private String address = "http://streams.videolan.org/streams-videolan/avi/Hero-Div3.avi";

    /**
     * Logger.
     */
    private Logger log = LoggerFactory.getLogger(AbstractVLCInternalTest.class);

    @Before
    public void testSetup()
    {
        exception = new libvlc_exception_t();
        String[] args = new String[]{
            "-vvv",
            "--ignore-config",
            "--reset-plugins-cache",
            "--no-media-library",
            "-I",
            "dummy",
            "-A",
            "dummy",
            "-V",
            "dummy" };
        libvlcInstance = libvlc.libvlc_new(args.length, args, exception);
        libvlc.libvlc_exception_clear(exception);

        downloadSample();
    }

    @After
    public void tearDown()
    {
        libvlc.libvlc_release(libvlcInstance);
        libvlc.libvlc_exception_clear(exception);
    }

    protected void catchException(libvlc_exception_t exception)
    {
        Assert.assertEquals(libvlc.libvlc_errmsg(), 0, libvlc.libvlc_exception_raised(exception));
    }

    private void downloadSample()
    {
        OutputStream out = null;
        URLConnection conn = null;
        InputStream in = null;
        URL sampleResource = this.getClass().getResource("/sample.avi");
        if (sampleResource != null)
        {
            log.debug("Sample file already downloaded");
            mrl = sampleResource.getPath();
            return;
        }
        try
        {
            log.info("Downloading sample: {}", address);
            String testResoucesPath = this.getClass().getResource("/sample").getPath();
            URL url = new URL(address);
            out = new BufferedOutputStream(new FileOutputStream(testResoucesPath + ".avi"));
            conn = url.openConnection();
            in = conn.getInputStream();
            byte[] buffer = new byte[1024];
            int numRead;
            long numWritten = 0;
            while ((numRead = in.read(buffer)) != -1)
            {
                out.write(buffer, 0, numRead);
                numWritten += numRead;
            }
            if (numWritten == 0)
            {
                throw new RuntimeException("Cannot download sample, please check the url or your internet connection.");
            }
            log.info("Sample downloaded.");
            mrl = testResoucesPath + ".avi";
        }
        catch (Exception e)
        {
            log.error("{}", e);
        }
        finally
        {
            IOUtils.closeQuietly(in);
            IOUtils.closeQuietly(out);
        }
    }

}
