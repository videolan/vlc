/*
 Copyright (c) 2011, Joachim Bengtsson
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 * Neither the name of the organization nor the names of its contributors may
   be used to endorse or promote products derived from this software without
   specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Cocoa/Cocoa.h>
#import <IOKit/hidsystem/ev_keymap.h>
#import <IOKit/hidsystem/IOLLEvent.h>

typedef NS_ENUM(uint8_t, SPKeyCode) {
    SPKeyCodePlay           = NX_KEYTYPE_PLAY,
    SPKeyCodeNext           = NX_KEYTYPE_NEXT,
    SPKeyCodePrevious       = NX_KEYTYPE_PREVIOUS,
    SPKeyCodeFastForward    = NX_KEYTYPE_FAST,
    SPKeyCodeRewind         = NX_KEYTYPE_REWIND
};

typedef NS_ENUM(uint8_t, SPKeyState) {
    SPKeyStateDown  = NX_KEYDOWN,
    SPKeyStateUp    = NX_KEYUP
};

@class SPMediaKeyTap;

@protocol SPMediaKeyTapDelegate <NSObject>
- (void)mediaKeyTap:(SPMediaKeyTap *)keyTap
   receivedMediaKey:(SPKeyCode)keyCode
              state:(SPKeyState)keyState
             repeat:(BOOL)isRepeat;
@end

@interface SPMediaKeyTap : NSObject
- (id)initWithDelegate:(id<SPMediaKeyTapDelegate>)delegate;

- (BOOL)startWatchingMediaKeys;
- (void)stopWatchingMediaKeys;
@end
