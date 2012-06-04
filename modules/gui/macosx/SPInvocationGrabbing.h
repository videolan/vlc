/*
 Copyright (c) 2011, Joachim Bengtsson
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 * Neither the name of the organization nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>

@interface SPInvocationGrabber : NSObject {
    id _object;
    NSInvocation *_invocation;
    int frameCount;
    char **frameStrings;
    BOOL backgroundAfterForward;
    BOOL onMainAfterForward;
    BOOL waitUntilDone;
}
-(id)initWithObject:(id)obj;
-(id)initWithObject:(id)obj stacktraceSaving:(BOOL)saveStack;
@property (readonly, retain, nonatomic) id object;
@property (readonly, retain, nonatomic) NSInvocation *invocation;
@property BOOL backgroundAfterForward;
@property BOOL onMainAfterForward;
@property BOOL waitUntilDone;
-(void)invoke; // will release object and invocation
-(void)printBacktrace;
-(void)saveBacktrace;
@end

@interface NSObject (SPInvocationGrabbing)
-(id)grab;
-(id)invokeAfter:(NSTimeInterval)delta;
-(id)nextRunloop;
-(id)inBackground;
-(id)onMainAsync:(BOOL)async;
@end
