/*****************************************************************************
 * JVLC.java: global class for vlc Java Bindings
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 *
 * $Id$
 *
 * Authors: Filippo Carone <filippo@carone.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

import org.videolan.jvlc.*;

import java.awt.*;
import java.awt.event.*;

class VLCPlayerFrame  extends Frame {
    public VLCPlayerFrame() {
        initComponents();
    }
    
    private void initComponents() {
	java.awt.GridBagConstraints gridBagConstraints;	

        fullScreenButton = new javax.swing.JButton();
        jTextField1 = new javax.swing.JTextField();
        setButton = new javax.swing.JButton();
        pauseButton = new javax.swing.JButton();
        stopButton = new javax.swing.JButton();

        jvcc  = new JVLCCanvas();
	jvcc.setSize( 100, 100 );
 	jvlc = jvcc.getJVLCObject();

	// FIXME: Does not work with GridBagLayout
	setLayout(new java.awt.GridBagLayout());

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridwidth = java.awt.GridBagConstraints.REMAINDER;
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 0;
        add( jvcc , gridBagConstraints);


        fullScreenButton.setText("FullScreen");
        fullScreenButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                fullScreenButtonActionPerformed(evt);
            }
        });

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 2;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        add( fullScreenButton, gridBagConstraints);


        jTextField1.setText("URL");
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 1;
        gridBagConstraints.gridwidth = 2;
        gridBagConstraints.fill = java.awt.GridBagConstraints.BOTH;
        add(jTextField1, gridBagConstraints);


        setButton.setText("Set item");
        setButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                setButtonActionPerformed(evt);
            }
        });
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 2;
        gridBagConstraints.gridy = 1;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        add(setButton, gridBagConstraints);


        pauseButton.setText("Play/Pause");
        pauseButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                pauseButtonActionPerformed(evt);
            }
        });
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 1;
        gridBagConstraints.gridy = 2;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        add(pauseButton, gridBagConstraints);

        stopButton.setText("Stop");
        stopButton.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
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

    private void stopButtonActionPerformed(java.awt.event.ActionEvent evt) {
	jvlc.stop();
    }

    private void pauseButtonActionPerformed(java.awt.event.ActionEvent evt) {
	jvlc.pause();
    }

    private void setButtonActionPerformed(java.awt.event.ActionEvent evt) {
	jvlc.stop();
	jvlc.playlistClear();
	jvlc.addTarget( jTextField1.getText(), null, 1, -666 );
	jvlc.play();
    }

    private void fullScreenButtonActionPerformed(java.awt.event.ActionEvent evt) {
	jvlc.fullScreen();
    }
    
    private javax.swing.JButton setButton;
    private javax.swing.JButton pauseButton;
    private javax.swing.JButton stopButton;
    private javax.swing.JButton fullScreenButton;
    private javax.swing.JTextField jTextField1;
    private javax.swing.JPanel jPanel;
    private JVLCCanvas jvcc;
    public JVLC jvlc;
}


public class VlcClient {

    public static void main(String[] args) {
    Frame f = new  VLCPlayerFrame();
    f.setBounds(0, 0, 500, 500);
    f.addWindowListener( new WindowAdapter() {
        public void windowClosing(WindowEvent ev) {
            System.exit(0);
        }   
    } );    
    f.setVisible(true);
   }
}
