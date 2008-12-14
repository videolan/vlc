/*****************************************************************************
 * AbstractJVLCTest.java: VLC Java Bindings
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

import java.io.BufferedOutputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import java.net.URLConnection;

import org.apache.commons.io.IOUtils;
import org.junit.After;
import org.junit.Before;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.videolan.jvlc.internal.AbstractVLCInternalTest;


public abstract class AbstractJVLCTest
{
    
    protected JVLC jvlc;

    protected String mrl;

    private String address = "http://streams.videolan.org/streams-videolan/avi/Hero-Div3.avi";

    /**
     * Logger.
     */
    private Logger log = LoggerFactory.getLogger(AbstractVLCInternalTest.class);

    @Before
    public void testSetup()
    {
        jvlc = new JVLC("-vvv --ignore-config --no-media-library -I dummy -A dummy -V dummy --rtsp-host 127.0.0.1:5554");
        jvlc.setLogVerbosity(LoggerVerbosityLevel.DEBUG);
        downloadSample();
    }

    @After
    public void tearDown()
    {
        jvlc.release();
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
