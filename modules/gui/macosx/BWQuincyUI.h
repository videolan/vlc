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

#import <Cocoa/Cocoa.h>

@class BWQuincyManager;

@interface BWQuincyUI : NSWindowController {
  IBOutlet NSTextField  *descriptionTextField;
  IBOutlet NSTextView   *crashLogTextView;

    IBOutlet NSTextField  *noteText;
    IBOutlet NSTextField  *titleText;
    IBOutlet NSTextField  *commentsText;
    IBOutlet NSTextField  *detailsText;

    IBOutlet NSButton   *showButton;
    IBOutlet NSButton   *hideButton;
    IBOutlet NSButton   *cancelButton;
    IBOutlet NSButton   *submitButton;

  BWQuincyManager   *_quincyManager;

  NSString      *_xml;

  NSString      *_crashFile;
  NSString      *_companyName;
  NSString      *_applicationName;

  NSMutableString   *_consoleContent;
  NSString      *_crashLogContent;

  BOOL showComments;
  BOOL showDetails;
}

- (id)initWithManager:(BWQuincyManager *)quincyManager crashFile:(NSString *)crashFile companyName:(NSString *)companyName applicationName:(NSString *)applicationName;

- (void) askCrashReportDetails;

- (IBAction) cancelReport:(id)sender;
- (IBAction) submitReport:(id)sender;
- (IBAction) showDetails:(id)sender;
- (IBAction) hideDetails:(id)sender;
- (IBAction) showComments:(id)sender;

- (BOOL)showComments;
- (void)setShowComments:(BOOL)value;

- (BOOL)showDetails;
- (void)setShowDetails:(BOOL)value;

@end