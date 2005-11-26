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

//	jPanel = new javax.swing.JPanel();
        jvcc  = new JVLCCanvas();
 	jvlc = jvcc.getJVLCObject();
//	jPanel.add( jvcc );
	add( jvcc );

	// FIXME: Does not work with GridBagLayout
	setLayout(new java.awt.GridLayout(3,2));

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridwidth = java.awt.GridBagConstraints.REMAINDER;
//        add( jPanel , gridBagConstraints);


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
