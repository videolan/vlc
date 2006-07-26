/*****************************************************************************
 * VlcClient.java: Sample Swing player
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 * 
 * Created on 28-feb-2006
 *
 * $Id: AudioIntf.java 8 2006-02-28 12:03:47Z little $
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

import java.awt.Frame;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;

import org.videolan.jvlc.JVLCPanel;
import org.videolan.jvlc.JVLC;
import org.videolan.jvlc.VLCException;

class VLCPlayerFrame  extends Frame {
    public VLCPlayerFrame() {
        initComponents();
    }
    
    private void initComponents() {

        java.awt.GridBagConstraints gridBagConstraints;	

        fullScreenButton    = new javax.swing.JButton();
        jTextField1         = new javax.swing.JTextField();
        setButton           = new javax.swing.JButton();
        pauseButton         = new javax.swing.JButton();
        stopButton          = new javax.swing.JButton();

        jvcc   = new JVLCPanel();
        jvlc = jvcc.getJVLCObject();

        setLayout(new java.awt.GridBagLayout());

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridwidth = java.awt.GridBagConstraints.CENTER;
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


        jTextField1.setText("file:///home/little/a.avi");
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
        try {
            //mci.stop(new Position(10000, PositionOrigin.ABSOLUTE, PositionKey.MEDIATIME));
	    jvlc.playlist.stop();
        } catch (Exception e) {
            e.printStackTrace();
	}
    }

    private void pauseButtonActionPerformed(java.awt.event.ActionEvent evt) {
        try {
            //System.out.println(mci.getMediaPosition(PositionOrigin.ABSOLUTE, PositionKey.BYTECOUNT));
	    jvlc.playlist.togglePause();
        } catch (Exception e) {
            e.printStackTrace();
	}
    }

    private void setButtonActionPerformed(java.awt.event.ActionEvent evt) {
        try {
			jvlc.playlist.add("file:///home/little/a.avi", "a.avi");
			jvlc.playlist.play(-1, null);
		} catch (VLCException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
    }

    private void fullScreenButtonActionPerformed(java.awt.event.ActionEvent evt) {
        //jvlc.fullScreen();
    }
    
    private javax.swing.JButton setButton;
    private javax.swing.JButton pauseButton;
    private javax.swing.JButton stopButton;
    private javax.swing.JButton fullScreenButton;
    private javax.swing.JTextField jTextField1;
    private JVLCPanel jvcc;
    public JVLC jvlc;
    //MediaControlInstance mci;
    
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
