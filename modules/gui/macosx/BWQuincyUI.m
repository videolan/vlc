/*
 * Author: Andreas Linde <mail@andreaslinde.de>
 *         Kent Sutherland
 *
 * Copyright (c) 2011 Andreas Linde & Kent Sutherland.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#import "intf.h"
#import "BWQuincyUI.h"
#import "BWQuincyManager.h"
#import <sys/sysctl.h>

#define CRASHREPORTSENDER_MAX_CONSOLE_SIZE 50000

@interface BWQuincyUI(private)
- (void) askCrashReportDetails;
- (void) endCrashReporter;
@end

const CGFloat kCommentsHeight = 105;
const CGFloat kDetailsHeight = 285;

@implementation BWQuincyUI

- (id)initWithManager:(BWQuincyManager *)quincyManager crashFile:(NSString *)crashFile companyName:(NSString *)companyName applicationName:(NSString *)applicationName {

  self = [super initWithWindowNibName: @"BWQuincyMain"];

  if ( self != nil) {
    _xml = nil;
    _quincyManager = quincyManager;
    _crashFile = crashFile;
    _companyName = companyName;
    _applicationName = applicationName;
    [self setShowComments: YES];
    [self setShowDetails: NO];

    NSRect windowFrame = [[self window] frame];
    windowFrame.size = NSMakeSize(windowFrame.size.width, windowFrame.size.height - kDetailsHeight);
    windowFrame.origin.y -= kDetailsHeight;
    [[self window] setFrame: windowFrame
                    display: YES
                    animate: NO];

  }
  return self;
}


- (void)awakeFromNib
{
	crashLogTextView.editable = NO;
	crashLogTextView.selectable = NO;
	crashLogTextView.automaticSpellingCorrectionEnabled = NO;

    [showButton setTitle:_NS("Show Details")];
    [hideButton setTitle:_NS("Hide Details")];
    [cancelButton setTitle:_NS("Cancel")];
    [submitButton setTitle:_NS("Send")];
    [titleText setStringValue:[NSString stringWithFormat:_NS("%@ unexpectedly quit the last time it was run. Would you like to send a crash report to %@?"), _applicationName, _companyName]];
    [commentsText setStringValue:_NS("Comments")];
    [detailsText setStringValue:_NS("Problem details and system configuration")];
}


- (void) endCrashReporter {
  [self close];
}


- (IBAction) showComments: (id) sender {
  NSRect windowFrame = [[self window] frame];

  if ([sender intValue]) {
    [self setShowComments: NO];

    windowFrame.size = NSMakeSize(windowFrame.size.width, windowFrame.size.height + kCommentsHeight);
    windowFrame.origin.y -= kCommentsHeight;
    [[self window] setFrame: windowFrame
                    display: YES
                    animate: YES];

    [self setShowComments: YES];
  } else {
    [self setShowComments: NO];

    windowFrame.size = NSMakeSize(windowFrame.size.width, windowFrame.size.height - kCommentsHeight);
    windowFrame.origin.y += kCommentsHeight;
    [[self window] setFrame: windowFrame
                    display: YES
                    animate: YES];
  }
}


- (IBAction) showDetails:(id)sender {
  NSRect windowFrame = [[self window] frame];

  windowFrame.size = NSMakeSize(windowFrame.size.width, windowFrame.size.height + kDetailsHeight);
  windowFrame.origin.y -= kDetailsHeight;
  [[self window] setFrame: windowFrame
                  display: YES
                  animate: YES];

  [self setShowDetails:YES];

}


- (IBAction) hideDetails:(id)sender {
  NSRect windowFrame = [[self window] frame];

  [self setShowDetails:NO];

  windowFrame.size = NSMakeSize(windowFrame.size.width, windowFrame.size.height - kDetailsHeight);
  windowFrame.origin.y += kDetailsHeight;
  [[self window] setFrame: windowFrame
                  display: YES
                  animate: YES];
}


- (IBAction) cancelReport:(id)sender {
  [self endCrashReporter];
  [NSApp stopModal];

  [_quincyManager cancelReport];
}

- (void) _sendReportAfterDelay {
  NSString *notes = [NSString stringWithFormat:@"Comments:\n%@\n\nConsole:\n%@", [descriptionTextField stringValue], _consoleContent];

  [_quincyManager sendReportCrash:_crashLogContent description:notes];
  [_crashLogContent release];
  _crashLogContent = nil;
}

- (IBAction) submitReport:(id)sender {
  [submitButton setEnabled:NO];

  [[self window] makeFirstResponder: nil];

  [self performSelector:@selector(_sendReportAfterDelay) withObject:nil afterDelay:0.01];

  [self endCrashReporter];
  [NSApp stopModal];
}


- (void) askCrashReportDetails {
  NSError *error;

   [[self window] setTitle:[NSString stringWithFormat:_NS("Problem Report for %@"), _applicationName]];

   [[descriptionTextField cell] setPlaceholderString:_NS("Please describe any steps needed to trigger the problem")];
   [noteText setStringValue:_NS("No personal information will be sent with this report.")];

  // get the crash log
  NSString *crashLogs = [NSString stringWithContentsOfFile:_crashFile encoding:NSUTF8StringEncoding error:&error];
  NSString *lastCrash = [[crashLogs componentsSeparatedByString: @"**********\n\n"] lastObject];

  _crashLogContent = [lastCrash retain];

  // get the console log
  NSEnumerator *theEnum = [[[NSString stringWithContentsOfFile:@"/private/var/log/system.log" encoding:NSUTF8StringEncoding error:&error] componentsSeparatedByString: @"\n"] objectEnumerator];
  NSString* currentObject;
  NSMutableArray* applicationStrings = [NSMutableArray array];

  NSString* searchString = [_applicationName stringByAppendingString:@"["];
  while ( (currentObject = [theEnum nextObject]) ) {
    if ([currentObject rangeOfString:searchString].location != NSNotFound)
      [applicationStrings addObject: currentObject];
  }

  _consoleContent = [[NSMutableString alloc] initWithString:@""];

  NSInteger i;
  for(i = ((NSInteger)[applicationStrings count])-1; (i>=0 && i>((NSInteger)[applicationStrings count])-100); i--) {
    [_consoleContent appendString:[applicationStrings objectAtIndex:i]];
    [_consoleContent appendString:@"\n"];
  }

  // Now limit the content to CRASHREPORTSENDER_MAX_CONSOLE_SIZE (default: 50kByte)
  if ([_consoleContent length] > CRASHREPORTSENDER_MAX_CONSOLE_SIZE) {
    _consoleContent = (NSMutableString *)[_consoleContent substringWithRange:NSMakeRange([_consoleContent length]-CRASHREPORTSENDER_MAX_CONSOLE_SIZE-1, CRASHREPORTSENDER_MAX_CONSOLE_SIZE)]; 
  }

  [crashLogTextView setString:[NSString stringWithFormat:@"%@\n\n%@", _crashLogContent, _consoleContent]];

  [NSApp runModalForWindow:[self window]];
}


- (void)dealloc {
  [_consoleContent release]; _consoleContent = nil;
  _companyName = nil;
  _quincyManager = nil;

  [super dealloc];
}


- (BOOL)showComments {
  return showComments;
}


- (void)setShowComments:(BOOL)value {
  showComments = value;
}


- (BOOL)showDetails {
  return showDetails;
}


- (void)setShowDetails:(BOOL)value {
  showDetails = value;
}

#pragma mark NSTextField Delegate

- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)commandSelector {
  BOOL commandHandled = NO;

  if (commandSelector == @selector(insertNewline:)) {
    [textView insertNewlineIgnoringFieldEditor:self];
    commandHandled = YES;
  }

  return commandHandled;
}

@end

