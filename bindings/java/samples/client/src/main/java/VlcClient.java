/*****************************************************************************
 * VlcClient.java: Sample Swing player
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 * 
 * Created on 28-feb-2006
 *
 * $Id: $
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 * 
 */

import java.awt.Canvas;
import java.awt.Frame;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;

import javax.swing.JPanel;

import org.videolan.jvlc.JVLC;
import org.videolan.jvlc.MediaPlayer;


class VLCPlayerFrame extends Frame
{

    /**
     * 
     */
    private static final long serialVersionUID = -7471950211795850421L;

    public Canvas jvcanvas;

    private MediaPlayer mediaPlayer;

    public VLCPlayerFrame(String[] args)
    {
        initComponents(args);
    }

    private void initComponents(String[] args)
    {

        java.awt.GridBagConstraints gridBagConstraints;

        fullScreenButton = new javax.swing.JButton();
        jTextField1 = new javax.swing.JTextField();
        setButton = new javax.swing.JButton();
        pauseButton = new javax.swing.JButton();
        stopButton = new javax.swing.JButton();

        jvcc = new JPanel();
        jvcanvas = new java.awt.Canvas();
        jvcanvas.setSize(200, 200);
        jvcc.add(jvcanvas);

        jvlc = new JVLC(args);
        jvlc.setVideoOutput(jvcanvas);
        
        setLayout(new java.awt.GridBagLayout());

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridwidth = java.awt.GridBagConstraints.CENTER;
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 0;
        add(jvcc, gridBagConstraints);

        fullScreenButton.setText("FullScreen");
        fullScreenButton.addActionListener(new java.awt.event.ActionListener()
        {

            public void actionPerformed(java.awt.event.ActionEvent evt)
            {
                fullScreenButtonActionPerformed(evt);
            }
        });

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 2;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        add(fullScreenButton, gridBagConstraints);

        jTextField1.setText("file://a.avi");
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 1;
        gridBagConstraints.gridwidth = 2;
        gridBagConstraints.fill = java.awt.GridBagConstraints.BOTH;
        add(jTextField1, gridBagConstraints);

        setButton.setText("Set item");
        setButton.addActionListener(new java.awt.event.ActionListener()
        {

            public void actionPerformed(java.awt.event.ActionEvent evt)
            {
                setButtonActionPerformed(evt);
            }
        });
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 2;
        gridBagConstraints.gridy = 1;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        add(setButton, gridBagConstraints);

        pauseButton.setText("Play/Pause");
        pauseButton.addActionListener(new java.awt.event.ActionListener()
        {

            public void actionPerformed(java.awt.event.ActionEvent evt)
            {
                pauseButtonActionPerformed(evt);
            }
        });
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 1;
        gridBagConstraints.gridy = 2;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        add(pauseButton, gridBagConstraints);

        stopButton.setText("Stop");
        stopButton.addActionListener(new java.awt.event.ActionListener()
        {

            public void actionPerformed(java.awt.event.ActionEvent evt)
            {
                stopButtonActionPerformed(evt);
            }
        });
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 2;
        gridBagConstraints.gridy = 2;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        add(stopButton, gridBagConstraints);

        pack();

    }

    private void stopButtonActionPerformed(java.awt.event.ActionEvent evt)
    {
        if (mediaPlayer == null)
        {
            return;
        }
        mediaPlayer.stop();
    }

    private void pauseButtonActionPerformed(java.awt.event.ActionEvent evt)
    {
        if (mediaPlayer == null)
        {
            return;
        }
        mediaPlayer.pause();
    }

    private void setButtonActionPerformed(java.awt.event.ActionEvent evt)
    {
        if (mediaPlayer != null)
        {
            mediaPlayer.stop();
            mediaPlayer.release();
            jvcanvas = new java.awt.Canvas();
        }
        mediaPlayer = jvlc.play(jTextField1.getText());
    }

    private void fullScreenButtonActionPerformed(java.awt.event.ActionEvent evt)
    {
        // jvlc.fullScreen();
    }

    private javax.swing.JButton setButton;

    private javax.swing.JButton pauseButton;

    private javax.swing.JButton stopButton;

    private javax.swing.JButton fullScreenButton;

    private javax.swing.JTextField jTextField1;

    private JPanel jvcc;

    public JVLC jvlc;
    // MediaControlInstance mci;

}


public class VlcClient
{

    public static void main(String[] args)
    {
        Frame f = new VLCPlayerFrame(args);
        f.setBounds(0, 0, 500, 500);
        f.addWindowListener(new WindowAdapter()
        {

            @Override
            public void windowClosing(WindowEvent ev)
            {
                System.exit(0);
            }
        });
        f.setVisible(true);
    }
}
