/*****************************************************************************
 * VideoEffects.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011 Felix Paul Kühne
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import "intf.h"
#import <vlc_common.h>
#import "VideoEffects.h"

@implementation VLCVideoEffects
static VLCVideoEffects *_o_sharedInstance = nil;

+ (VLCVideoEffects *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
    } else {
        p_intf = VLCIntf;
        _o_sharedInstance = [super init];
    }
    
    return _o_sharedInstance;
}

#pragma mark -
#pragma mark basic
- (IBAction)enableAdjust:(id)sender
{
}

- (IBAction)adjustSliderChanged:(id)sender
{
}

- (IBAction)enableAdjustBrightnessThreshold:(id)sender
{
}

- (IBAction)enableSharpen:(id)sender
{
}

- (IBAction)sharpenSliderChanged:(id)sender
{
}

- (IBAction)enableBanding:(id)sender
{
}

- (IBAction)bandingSliderChanged:(id)sender
{
}

- (IBAction)enableGrain:(id)sender
{
}

- (IBAction)grainSliderChanged:(id)sender
{
}


#pragma mark -
#pragma mark crop
- (IBAction)cropFieldChanged:(id)sender
{
}

- (IBAction)enableCropModifier:(id)sender
{
}


#pragma mark -
#pragma mark geometry
- (IBAction)enableTransform:(id)sender
{
}

- (IBAction)transformModifierChanged:(id)sender
{
}

- (IBAction)enableZoom:(id)sender
{
}

- (IBAction)enablePuzzle:(id)sender
{
}

- (IBAction)puzzleModifierChanged:(id)sender
{
}


#pragma mark -
#pragma mark color
- (IBAction)enableThreshold:(id)sender
{
}

- (IBAction)thresholdModifierChanged:(id)sender
{
}

- (IBAction)enableSepia:(id)sender
{
}

- (IBAction)sepiaModifierChanged:(id)sender
{
}

- (IBAction)enableNoise:(id)sender
{
}

- (IBAction)enableGradient:(id)sender
{
}

- (IBAction)gradientModifierChanged:(id)sender
{
}

- (IBAction)enableExtract:(id)sender
{
}

- (IBAction)extractModifierChanged:(id)sender
{
}

- (IBAction)enableInvert:(id)sender
{
}

- (IBAction)enablePosterize:(id)sender
{
}

- (IBAction)posterizeModifierChanged:(id)sender
{
}

- (IBAction)enableBlur:(id)sender
{
}

- (IBAction)blurModifierChanged:(id)sender
{
}

- (IBAction)enableMotionDetect:(id)sender
{
}

- (IBAction)enableWaterEffect:(id)sender
{
}

- (IBAction)enableWaves:(id)sender
{
}

- (IBAction)enablePsychedelic:(id)sender
{
}


#pragma mark -
#pragma mark video output & overlay
- (IBAction)enableClone:(id)sender
{
}

- (IBAction)cloneModifierChanged:(id)sender
{
}

- (IBAction)enableAddText:(id)sender
{
}

- (IBAction)addTextModifierChanged:(id)sender
{
}


#pragma mark -
#pragma mark logo
- (IBAction)enableAddLogo:(id)sender
{
}

- (IBAction)addLogoModifierChanged:(id)sender
{
}

- (IBAction)enableEraseLogo:(id)sender
{
}

- (IBAction)eraseLogoModifierChanged:(id)sender
{
}


@end