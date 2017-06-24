//
//  VLCHUDScroller.h
//  HUDScroller
//
//  Created by BinaryGod on 5/22/08.
//
//  Copyright (c) 2008, Tim Davis (BinaryMethod.com, binary.god@gmail.com)
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification,
//  are permitted provided that the following conditions are met:
//
//        Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
//        Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation and/or
//    other materials provided with the distribution.
//
//        Neither the name of the BinaryMethod.com nor the names of its contributors
//    may be used to endorse or promote products derived from this software without
//    specific prior written permission.
//
//    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS AND
//    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
//    IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
//    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
//    OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
//    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//    POSSIBILITY OF SUCH DAMAGE.

// Special thanks to Matt Gemmell (http://mattgemmell.com/) for helping me solve the
// transparent drawing issues.  Your awesome man!!!

#import <Cocoa/Cocoa.h>

@interface VLCHUDScroller : NSScroller {
    NSString *arrowPosition;
}

@property (strong) NSColor *scrollerStroke;
@property (strong) NSGradient *scrollerKnobGradient;
@property (strong) NSGradient *scrollerTrackGradient;
@property (strong) NSGradient *scrollerArrowNormalGradient;
@property (strong) NSGradient *scrollerArrowPushedGradient;

- (void)drawDecrementArrow:(bool)highlighted;
- (void)drawIncrementArrow:(bool)highlighted;

- (bool)isHoriz;

@end
