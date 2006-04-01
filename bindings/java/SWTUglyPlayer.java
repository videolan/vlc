/*****************************************************************************
 * JVLC.java: global class for vlc Java Bindings
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
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

import java.awt.Dialog;

import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Canvas;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Text;

import org.videolan.jvlc.*;


public class SWTUglyPlayer {

	private Shell sShell = null;  //  @jve:decl-index=0:visual-constraint="230,12"
	private Canvas canvas = null;
	private SWTVideoWidget vlc = null;
	private Button button = null;
	private Text text = null;
	private Button button1 = null;
	private Button button2 = null;
	private Display display = new Display(); 
	/**
	 * This method initializes sShell
	 */
	private void createSShell() {
		sShell = new Shell(display);
		sShell.setText("Shell");
		createCanvas();
		sShell.setSize(new org.eclipse.swt.graphics.Point(339,303));
		button = new Button(sShell, SWT.NONE);
		button.setText("Play");
		button.setSize(new org.eclipse.swt.graphics.Point(70,30));
		button.setLocation(new org.eclipse.swt.graphics.Point(26,185));
		button.addMouseListener(new org.eclipse.swt.events.MouseAdapter() {
			public void mouseDown(org.eclipse.swt.events.MouseEvent e) {
				vlc.getJVLC().playlist.add(text.getText(), text.getText());
				vlc.getJVLC().playlist.play(-1, null);
			}
		});
		text = new Text(sShell, SWT.BORDER);
		text.setBounds(new org.eclipse.swt.graphics.Rectangle(26,222,200,25));
		text.setText("~/a.avi");
		button1 = new Button(sShell, SWT.NONE);
		button1.setLocation(new org.eclipse.swt.graphics.Point(120,186));
		button1.setText("Pause");
		button1.setSize(new org.eclipse.swt.graphics.Point(70,30));
		button1.addMouseListener(new org.eclipse.swt.events.MouseAdapter() {
			public void mouseDown(org.eclipse.swt.events.MouseEvent e) {
				vlc.getJVLC().playlist.pause();
			}
		});
		button2 = new Button(sShell, SWT.NONE);
		button2.setText("Stop");
		button2.setSize(new org.eclipse.swt.graphics.Point(70,30));
		button2.setLocation(new org.eclipse.swt.graphics.Point(221,188));
		button2.addSelectionListener(new org.eclipse.swt.events.SelectionAdapter() {
			public void widgetSelected(org.eclipse.swt.events.SelectionEvent e) {
				vlc.getJVLC().playlist.stop();
			}
		});
	}

	/**
	 * This method initializes canvas	
	 *
	 */
	private void createCanvas() {
		canvas = new Canvas(sShell, SWT.EMBEDDED);
		canvas.setBounds(new org.eclipse.swt.graphics.Rectangle(22,15,248,145));
		vlc = new SWTVideoWidget( canvas );
	}

	public Canvas getCanvas() {
		return canvas;
	}
	
	public SWTUglyPlayer( ) {
		createSShell();
		sShell.open();
		while( !sShell.isDisposed())
	    {
	      if(!display.readAndDispatch()) 
	      display.sleep();
	    }
		display.dispose();
	}
	
	static public void main( String[] args ) {
		SWTUglyPlayer swt = new SWTUglyPlayer();
	}

}
