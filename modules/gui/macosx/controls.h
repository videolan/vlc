/*****************************************************************************
 * controls.h: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: controls.h,v 1.3 2003/05/06 20:12:28 hartman Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
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

/*****************************************************************************
 * VLCControls interface 
 *****************************************************************************/
@interface VLCControls : NSObject
{
    IBOutlet id o_open;
    IBOutlet id o_main;

    IBOutlet id o_volumeslider;
}

- (IBAction)play:(id)sender;
- (IBAction)stop:(id)sender;
- (IBAction)faster:(id)sender;
- (IBAction)slower:(id)sender;

- (IBAction)prev:(id)sender;
- (IBAction)next:(id)sender;
- (IBAction)loop:(id)sender;

- (IBAction)forward:(id)sender;
- (IBAction)backward:(id)sender;

- (IBAction)volumeUp:(id)sender;
- (IBAction)volumeDown:(id)sender;
- (IBAction)mute:(id)sender;
- (IBAction)volumeSliderUpdated:(id)sender;
- (void)updateVolumeSlider;

- (IBAction)windowAction:(id)sender;
- (IBAction)deinterlace:(id)sender;

- (IBAction)toggleProgram:(id)sender;
- (IBAction)toggleTitle:(id)sender;
- (IBAction)toggleChapter:(id)sender;
- (IBAction)toggleLanguage:(id)sender;
- (IBAction)toggleVar:(id)sender;

@end