/*****************************************************************************
 * dialogProvider.m: Minimal Dialog Provider for Mac OS X
 *****************************************************************************
 * Copyright (C) 2009-2010 the VideoLAN team
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#import <stdlib.h>                                      /* malloc(), free() */
#import <string.h>

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_dialog.h>
#import <vlc_interface.h>
#import <vlc_extensions.h>

#import <Cocoa/Cocoa.h>
#import "VLCLoginPanel.h"
#import "VLCProgressPanel.h"

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
static int  OpenIntf(vlc_object_t *);
static void CloseIntf(vlc_object_t *);
static void Run(intf_thread_t * );

static int DisplayError(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayCritical(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayQuestion(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayLogin(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayProgressPanelAction(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );
static int DisplayExtension(vlc_object_t *,const char *,vlc_value_t,vlc_value_t,void * );

static void updateProgressPanel (void *, const char *, float);
static bool checkProgressPanel (void *);
static void destroyProgressPanel (void *);

@interface VLCDialogDisplayer : NSObject
{
    VLCProgressPanel *_currentProgressBarPanel;
}

+ (NSDictionary *)dictionaryForDialog:(const char *)title :(const char *)message :(const char *)yes :(const char *)no :(const char *)cancel;

- (void)displayError:(NSDictionary *)dialog;
- (void)displayCritical:(NSDictionary *)dialog;
- (NSNumber *)displayQuestion:(NSDictionary *)dialog;
- (NSDictionary *)displayLogin:(NSDictionary *)dialog;

- (void)displayProgressBar:(NSDictionary *)dict;
- (void)updateProgressPanel:(NSDictionary *)dict;
- (void)destroyProgressPanel;
- (NSNumber *)checkProgressPanel;

- (void)updateExtensionDialog:(NSValue *)extensionDialog;

- (id)resultFromSelectorOnMainThread:(SEL)sel withObject:(id)object;
@end

@interface VLCDialogButton : NSButton
{
    extension_widget_t *widget;
}
@property (readwrite) extension_widget_t *widget;
@end

@implementation VLCDialogButton
@synthesize widget;
@end

@interface VLCDialogPopUpButton : NSPopUpButton
{
    extension_widget_t *widget;
}
@property (readwrite) extension_widget_t *widget;
@end

@implementation VLCDialogPopUpButton
@synthesize widget;
@end


@interface VLCDialogTextField : NSTextField
{
    extension_widget_t *widget;
}
@property (readwrite) extension_widget_t *widget;
@end

@implementation VLCDialogTextField
@synthesize widget;
@end

@interface VLCDialogWindow : NSWindow
{
    extension_dialog_t *dialog;
}
@property (readwrite) extension_dialog_t *dialog;
@end

@implementation VLCDialogWindow
@synthesize dialog;
@end


@interface VLCDialogList : NSTableView
{
    extension_widget_t *widget;
    NSMutableArray *contentArray;
}
@property (readwrite) extension_widget_t *widget;
@property (readwrite, retain) NSMutableArray *contentArray;
@end

@implementation VLCDialogList
@synthesize widget;
@synthesize contentArray;

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [contentArray count];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    return [[contentArray objectAtIndex:rowIndex] objectForKey:@"text"];
}
@end

@interface VLCDialogGridView : NSView {
    NSUInteger _rowCount, _colCount;
    NSMutableArray *_gridedViews;
}

- (NSSize)flexSize:(NSSize)size;
- (void)removeSubview:(NSView *)view;
@end


// Move this to separate file
@implementation VLCDialogGridView

- (void)dealloc
{
    [_gridedViews release];
    [super dealloc];
}

- (void)recomputeCount
{
    _colCount = 0;
    _rowCount = 0;
    for (NSDictionary *obj in _gridedViews)
    {
        NSUInteger row = [[obj objectForKey:@"row"] intValue];
        NSUInteger col = [[obj objectForKey:@"col"] intValue];
        if (col + 1 > _colCount)
            _colCount = col + 1;
        if (row + 1 > _rowCount)
            _rowCount = row + 1;
    }
}

- (void)recomputeWindowSize
{
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(recomputeWindowSize) object:nil];

    NSWindow *window = [self window];
    NSRect frame = [window frame];
    NSRect contentRect = [window contentRectForFrameRect:frame];
    contentRect.size = [self flexSize:frame.size];
    NSRect newFrame = [window frameRectForContentRect:contentRect];
    newFrame.origin.y -= newFrame.size.height - frame.size.height;
    newFrame.origin.x -= (newFrame.size.width - frame.size.width) / 2;
    [window setFrame:newFrame display:YES animate:YES];
}

- (NSSize)objectSizeToFit:(NSView *)view
{
    if ([view isKindOfClass:[NSControl class]]) {
        NSControl *control = (NSControl *)view;
        return [[control cell] cellSize];
    }
    return [view frame].size;
}

- (CGFloat)marginX
{
    return 16;
}
- (CGFloat)marginY
{
    return 8;
}

- (CGFloat)constrainedHeightOfRow:(NSUInteger)targetRow
{
    CGFloat height = 0;
    for(NSDictionary *obj in _gridedViews) {
        NSUInteger row = [[obj objectForKey:@"row"] intValue];
        if (row != targetRow)
            continue;
        NSUInteger rowSpan = [[obj objectForKey:@"rowSpan"] intValue];
        if (rowSpan != 1)
            continue;
        NSView *view = [obj objectForKey:@"view"];
        if ([view autoresizingMask] & NSViewHeightSizable)
            continue;
        NSSize sizeToFit = [self objectSizeToFit:view];
        if (height < sizeToFit.height)
            height = sizeToFit.height;
    }
    return height;
}

- (CGFloat)remainingRowsHeight
{
    NSUInteger height = [self marginY];
    if (!_rowCount)
        return 0;
    NSUInteger autosizedRows = 0;
    for (NSUInteger i = 0; i < _rowCount; i++) {
        CGFloat constrainedHeight = [self constrainedHeightOfRow:i];
        if (!constrainedHeight)
            autosizedRows++;
        height += constrainedHeight + [self marginY];
    }
    CGFloat remaining = 0;
    if (height < self.bounds.size.height && autosizedRows)
        remaining = (self.bounds.size.height - height) / autosizedRows;
    if (remaining < 0)
        remaining = 0;

    return remaining;
}

- (CGFloat)heightOfRow:(NSUInteger)targetRow
{
    NSAssert(targetRow < _rowCount, @"accessing a non existing row");
    CGFloat height = [self constrainedHeightOfRow:targetRow];
    if (!height)
        height = [self remainingRowsHeight];
    return height;
}


- (CGFloat)topOfRow:(NSUInteger)targetRow
{
    CGFloat top = [self marginY];
    for (NSUInteger i = 1; i < _rowCount - targetRow; i++)
    {
        top += [self heightOfRow:_rowCount - i] + [self marginY];
    }
    return top;
}

- (CGFloat)constrainedWidthOfColumn:(NSUInteger)targetColumn
{
    CGFloat width = 0;
    for(NSDictionary *obj in _gridedViews) {
        NSUInteger col = [[obj objectForKey:@"col"] intValue];
        if (col != targetColumn)
            continue;
        NSUInteger colSpan = [[obj objectForKey:@"colSpan"] intValue];
        if (colSpan != 1)
            continue;
        NSView *view = [obj objectForKey:@"view"];
        if ([view autoresizingMask] & NSViewWidthSizable)
            return 0;
        NSSize sizeToFit = [self objectSizeToFit:view];
        if (width < sizeToFit.width)
            width = sizeToFit.width;
    }
    return width;
}

- (CGFloat)remainingColumnWidth
{
    NSUInteger width = [self marginX];
    if (!_colCount)
        return 0;
    NSUInteger autosizedCol = 0;
    for (NSUInteger i = 0; i < _colCount; i++) {
        CGFloat constrainedWidth = [self constrainedWidthOfColumn:i];
        if (!constrainedWidth)
            autosizedCol++;
        width += constrainedWidth + [self marginX];

    }
    CGFloat remaining = 0;
    if (width < self.bounds.size.width && autosizedCol)
        remaining = (self.bounds.size.width - width) / autosizedCol;
    if (remaining < 0)
        remaining = 0;
    return remaining;
}

- (CGFloat)widthOfColumn:(NSUInteger)targetColumn
{
    CGFloat width = [self constrainedWidthOfColumn:targetColumn];
    if (!width)
        width = [self remainingColumnWidth];
    return width;
}


- (CGFloat)leftOfColumn:(NSUInteger)targetColumn
{
    CGFloat left = [self marginX];
    for (NSUInteger i = 0; i < targetColumn; i++)
    {
        left += [self widthOfColumn:i] + [self marginX];

    }
    return left;
}

- (void)relayout
{
    for(NSDictionary *obj in _gridedViews) {
        NSUInteger row = [[obj objectForKey:@"row"] intValue];
        NSUInteger col = [[obj objectForKey:@"col"] intValue];
        NSUInteger rowSpan = [[obj objectForKey:@"rowSpan"] intValue];
        NSUInteger colSpan = [[obj objectForKey:@"colSpan"] intValue];
        NSView *view = [obj objectForKey:@"view"];
        NSRect rect;

        // Get the height
        if ([view autoresizingMask] & NSViewHeightSizable || rowSpan > 1) {
            CGFloat height = 0;
            for (NSUInteger r = 0; r < rowSpan; r++) {
                if (row + r >= _rowCount)
                    break;
                height += [self heightOfRow:row + r] + [self marginY];
            }
            rect.size.height = height - [self marginY];
        }
        else
            rect.size.height = [self objectSizeToFit:view].height;

        // Get the width
        if ([view autoresizingMask] & NSViewWidthSizable) {
            CGFloat width = 0;
            for (NSUInteger c = 0; c < colSpan; c++)
                width += [self widthOfColumn:col + c] + [self marginX];
            rect.size.width = width - [self marginX];
        }
        else
            rect.size.width = [self objectSizeToFit:view].width;

        // Top corner
        rect.origin.y = [self topOfRow:row] + ([self heightOfRow:row] - rect.size.height) / 2;
        rect.origin.x = [self leftOfColumn:col];

        [view setFrame:rect];
        [view setNeedsDisplay:YES];
    }
}

- (NSMutableDictionary *)objectForView:(NSView *)view
{
    for (NSMutableDictionary *dict in _gridedViews)
    {
        if ([dict objectForKey:@"view"] == view)
            return dict;
    }
    return nil;
}

- (void)addSubview:(NSView *)view atRow:(NSUInteger)row column:(NSUInteger)column rowSpan:(NSUInteger)rowSpan colSpan:(NSUInteger)colSpan
{
    if (row + 1 > _rowCount)
        _rowCount = row + 1;
    if (column + 1 > _colCount)
        _colCount = column + 1;

    if (!_gridedViews)
        _gridedViews = [[NSMutableArray alloc] init];

    NSMutableDictionary *dict = [self objectForView:view];
    if (!dict) {
        dict = [NSMutableDictionary dictionary];
        [dict setObject:view forKey:@"view"];
        [_gridedViews addObject:dict];
    }
    [dict setObject:[NSNumber numberWithInt:rowSpan] forKey:@"rowSpan"];
    [dict setObject:[NSNumber numberWithInt:colSpan] forKey:@"colSpan"];
    [dict setObject:[NSNumber numberWithInt:row] forKey:@"row"];
    [dict setObject:[NSNumber numberWithInt:column] forKey:@"col"];


    [self addSubview:view];
    [self relayout];

    // Recompute the size of the window after making sure we won't see anymore update
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(recomputeWindowSize) object:nil];
    [self performSelector:@selector(recomputeWindowSize) withObject:nil afterDelay:0.1];
}

- (void)removeSubview:(NSView *)view
{
    NSDictionary *dict = [self objectForView:view];
    if (dict)
        [_gridedViews removeObject:dict];
    [view removeFromSuperview];

    [self recomputeCount];
    [self recomputeWindowSize];

    [self relayout];
    [self setNeedsDisplay:YES];
}

- (void)setFrame:(NSRect)frameRect
{
    [super setFrame:frameRect];
    [self relayout];
}

- (NSSize)flexSize:(NSSize)size
{
    if (!_rowCount || !_colCount)
        return size;

    CGFloat minHeight = [self marginY];
    BOOL canFlexHeight = NO;
    for (NSUInteger i = 0; i < _rowCount; i++) {
        CGFloat constrained = [self constrainedHeightOfRow:i];
        if (!constrained) {
            canFlexHeight = YES;
            constrained = 128;
        }
        minHeight += constrained + [self marginY];
    }

    CGFloat minWidth = [self marginX];
    BOOL canFlexWidth = NO;
    for (NSUInteger i = 0; i < _colCount; i++) {
        CGFloat constrained = [self constrainedWidthOfColumn:i];
        if (!constrained) {
            canFlexWidth = YES;
            constrained = 128;
        }
        minWidth += constrained + [self marginX];
    }
    if (size.width < minWidth)
        size.width = minWidth;
    if (size.height < minHeight)
        size.height = minHeight;
    if (!canFlexHeight)
        size.height = minHeight;
    if (!canFlexWidth)
        size.width = minWidth;
    return size;
}

@end


static inline NSDictionary *DictFromDialogFatal(dialog_fatal_t *dialog) {
    return [VLCDialogDisplayer dictionaryForDialog:dialog->title :dialog->message :NULL :NULL :NULL];
}
static inline NSDictionary *DictFromDialogLogin(dialog_login_t *dialog) {
    return [VLCDialogDisplayer dictionaryForDialog:dialog->title :dialog->message :NULL :NULL :NULL];
}
static inline NSDictionary *DictFromDialogQuestion(dialog_question_t *dialog) {
    return [VLCDialogDisplayer dictionaryForDialog:dialog->title :dialog->message :dialog->yes :dialog->no :dialog->cancel];
}
static inline NSDictionary *DictFromDialogProgressBar(dialog_progress_bar_t *dialog) {
    return [VLCDialogDisplayer dictionaryForDialog:dialog->title :dialog->message :NULL :NULL :dialog->cancel];
}

struct intf_sys_t
{
    VLCDialogDisplayer *displayer;

    vlc_mutex_t lock;
    vlc_cond_t wait;
    bool is_hidding_noaction_dialogs;
};


#define T_HIDE_NOACTION N_("Hide no user action dialogs")
#define LT_HIDE_NOACTION N_("Don't display dialogs that don't require user action (Critical and error panel).")

#define prefix "macosx-dialog-provider"
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

vlc_module_begin()
    /* Minimal interface. see intf.m */
    set_shortname("Mac OS X Dialogs")
    add_shortcut("macosx_dialog_provider")
    add_shortcut("miosx")
    set_description("Minimal Mac OS X Dialog Provider")
    set_capability("interface", 0)

    /* This setting is interesting, because when used with a libvlc app
     * it's almost certain that the client program will display error by
     * itself. Moreover certain action might end up in an error, but
     * the client wants to ignored them completely. */
    add_bool(prefix "hide-no-user-action-dialogs", true, NULL, T_HIDE_NOACTION, LT_HIDE_NOACTION, false)

    set_callbacks(OpenIntf, CloseIntf)
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_MAIN)
vlc_module_end()

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int OpenIntf(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    p_intf->p_sys = malloc(sizeof(intf_sys_t));
    if(!p_intf->p_sys)
        return VLC_ENOMEM;

    memset(p_intf->p_sys,0,sizeof(*p_intf->p_sys));

    p_intf->p_sys->displayer = [[VLCDialogDisplayer alloc] init];

    bool hide = var_CreateGetBool(p_intf, prefix "hide-no-user-action-dialogs");
    p_intf->p_sys->is_hidding_noaction_dialogs = hide;

    /* subscribe to various interactive dialogues */

    if (!hide)
    {
        var_Create(p_intf,"dialog-error",VLC_VAR_ADDRESS);
        var_AddCallback(p_intf,"dialog-error",DisplayError,p_intf);
        var_Create(p_intf,"dialog-critical",VLC_VAR_ADDRESS);
        var_AddCallback(p_intf,"dialog-critical",DisplayCritical,p_intf);
    }
    var_Create(p_intf,"dialog-login",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-login",DisplayLogin,p_intf);
    var_Create(p_intf,"dialog-question",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-question",DisplayQuestion,p_intf);
    var_Create(p_intf,"dialog-progress-bar",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-progress-bar",DisplayProgressPanelAction,p_intf);
    var_Create(p_intf,"dialog-extension",VLC_VAR_ADDRESS);
    var_AddCallback(p_intf,"dialog-extension",DisplayExtension,p_intf);
    dialog_Register(p_intf);

    msg_Dbg(p_intf,"Mac OS X dialog provider initialised");

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void CloseIntf(vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    /* unsubscribe from the interactive dialogues */
    dialog_Unregister(p_intf);

    if (!p_intf->p_sys->is_hidding_noaction_dialogs)
    {
        var_DelCallback(p_intf,"dialog-error",DisplayError,p_intf);
        var_DelCallback(p_intf,"dialog-critical",DisplayCritical,p_intf);
    }
    var_DelCallback(p_intf,"dialog-login",DisplayLogin,p_intf);
    var_DelCallback(p_intf,"dialog-question",DisplayQuestion,p_intf);
    var_DelCallback(p_intf,"dialog-progress-bar",DisplayProgressPanelAction,p_intf);
    var_DelCallback(p_intf,"dialog-extension",DisplayExtension,p_intf);

    [p_intf->p_sys->displayer release];

    msg_Dbg(p_intf,"Mac OS X dialog provider closed");
    free(p_intf->p_sys);
}


/*****************************************************************************
 * Callbacks triggered by the "dialog-*" variables
 *****************************************************************************/
static int DisplayError(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    dialog_fatal_t *dialog = value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;
    [sys->displayer performSelectorOnMainThread:@selector(displayError:) withObject:DictFromDialogFatal(dialog) waitUntilDone:NO];
    [pool release];
    return VLC_SUCCESS;
}

static int DisplayCritical(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    dialog_fatal_t *dialog = value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;
    [sys->displayer performSelectorOnMainThread:@selector(displayCritical:) withObject:DictFromDialogFatal(dialog) waitUntilDone:NO];
    [pool release];
    return VLC_SUCCESS;
}

static int DisplayQuestion(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    dialog_question_t *dialog = value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;
    dialog->answer = [[sys->displayer resultFromSelectorOnMainThread:@selector(displayQuestion:) withObject:DictFromDialogQuestion(dialog)] intValue];
    [pool release];
    return VLC_SUCCESS;
}

static int DisplayLogin(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    dialog_login_t *dialog = value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;
    NSDictionary *dict = [sys->displayer resultFromSelectorOnMainThread:@selector(displayLogin:) withObject:DictFromDialogLogin(dialog)];
    if (dict) {
        *dialog->username = strdup([[dict objectForKey:@"username"] UTF8String]);
        *dialog->password = strdup([[dict objectForKey:@"password"] UTF8String]);
    }
    [pool release];
    return VLC_SUCCESS;
}

static int DisplayProgressPanelAction(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    dialog_progress_bar_t *dialog = value.p_address;
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;

    [sys->displayer performSelectorOnMainThread:@selector(displayProgressBar:) withObject:DictFromDialogProgressBar(dialog) waitUntilDone:YES];

    dialog->pf_update = updateProgressPanel;
    dialog->pf_check = checkProgressPanel;
    dialog->pf_destroy = destroyProgressPanel;
    dialog->p_sys = p_intf->p_sys;

    [pool release];
    return VLC_SUCCESS;
}

static int DisplayExtension(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    intf_thread_t *p_intf = (intf_thread_t*) p_this;
    intf_sys_t *sys = p_intf->p_sys;
    extension_dialog_t *dialog = value.p_address;

    // -updateExtensionDialog: Open its own runloop, so be sure to run on DefaultRunLoop.
    [sys->displayer performSelectorOnMainThread:@selector(updateExtensionDialog:) withObject:[NSValue valueWithPointer:dialog] waitUntilDone:YES];
    [pool release];
    return VLC_SUCCESS;
}


void updateProgressPanel (void *priv, const char *text, float value)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    intf_sys_t *sys = (intf_sys_t *)priv;

    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
                          [NSNumber numberWithFloat:value], @"value",
                          text ? [NSString stringWithUTF8String:text] : nil, @"text",
                          nil];

    [sys->displayer performSelectorOnMainThread:@selector(updateProgressPanel:) withObject:dict waitUntilDone:YES];

    [pool release];
}

void destroyProgressPanel (void *priv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    intf_sys_t *sys = (intf_sys_t *)priv;

    [sys->displayer performSelectorOnMainThread:@selector(destroyProgressPanel) withObject:nil waitUntilDone:YES];

    [pool release];
}

bool checkProgressPanel (void *priv)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    intf_sys_t *sys = (intf_sys_t *)priv;
    BOOL ret;

    ret = [[sys->displayer resultFromSelectorOnMainThread:@selector(checkProgressPanel) withObject:nil] boolValue];

    [pool release];
    return ret;
}


@implementation VLCDialogDisplayer
- (void)dealloc
{
    assert(!_currentProgressBarPanel); // This has to be closed on main thread.
    [super dealloc];
}

+ (NSDictionary *)dictionaryForDialog:(const char *)title :(const char *)message :(const char *)yes :(const char *)no :(const char *)cancel
{
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    if (title)
        [dict setObject:[NSString stringWithUTF8String:title] forKey:@"title"];
    if (message)
        [dict setObject:[NSString stringWithUTF8String:message] forKey:@"message"];
    if (yes)
        [dict setObject:[NSString stringWithUTF8String:yes] forKey:@"yes"];
    if (no)
        [dict setObject:[NSString stringWithUTF8String:no] forKey:@"no"];
    if (cancel)
        [dict setObject:[NSString stringWithUTF8String:cancel] forKey:@"cancel"];

    return dict;
}
#define VLCAssertIsMainThread() assert([NSThread isMainThread])

- (void)displayError:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();

    NSRunInformationalAlertPanel([dialog objectForKey:@"title"],
                                 [dialog objectForKey:@"message"],
                                 @"OK", nil, nil);
}

- (void)displayCritical:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();

    NSRunCriticalAlertPanel([dialog objectForKey:@"title"],
                                 [dialog objectForKey:@"message"],
                                 @"OK", nil, nil);
}

- (NSNumber *)displayQuestion:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();

    NSInteger alertRet = 0;

    NSAlert *alert = [NSAlert alertWithMessageText:[dialog objectForKey:@"title"]
                              defaultButton:[dialog objectForKey:@"yes"]
                            alternateButton:[dialog objectForKey:@"no"]
                                otherButton:[dialog objectForKey:@"cancel"]
                  informativeTextWithFormat:[dialog objectForKey:@"message"]];
    [alert setAlertStyle:NSInformationalAlertStyle];
    alertRet = [alert runModal];

    int ret;
    switch (alertRet) {
        case NSAlertDefaultReturn:
            ret = 1;
            break;
        case NSAlertAlternateReturn:
            ret = 2;
            break;
        case NSAlertOtherReturn:
            ret = 3;
            break;
        default:
            assert(0);
            ret = 0;
            break;
    }

    return [NSNumber numberWithInt:ret];
}

- (NSDictionary *)displayLogin:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();

    VLCLoginPanel *panel = [[VLCLoginPanel alloc] init];
    [panel createContentView];
    [panel setDialogTitle:[dialog objectForKey:@"title"]];
    [panel setDialogMessage:[dialog objectForKey:@"message"]];
    [panel center];
    NSInteger ret = [NSApp runModalForWindow:panel];
    [panel close];

    if (!ret)
        return nil;

    return [NSDictionary dictionaryWithObjectsAndKeys:
            [panel userName], @"username",
            [panel password], @"password",
            nil];
}

- (void)displayProgressBar:(NSDictionary *)dialog
{
    VLCAssertIsMainThread();

    if(_currentProgressBarPanel)
        [self destroyProgressPanel];

    assert(!_currentProgressBarPanel);
    _currentProgressBarPanel = [[VLCProgressPanel alloc] init];
    [_currentProgressBarPanel createContentView];
    [_currentProgressBarPanel setDialogTitle:[dialog objectForKey:@"title"]];
    [_currentProgressBarPanel setDialogMessage:[dialog objectForKey:@"message"] ?: @""];
    [_currentProgressBarPanel setCancelButtonLabel:[dialog objectForKey:@"cancel"]];

    [_currentProgressBarPanel center];
    [_currentProgressBarPanel makeKeyAndOrderFront:nil];
}

- (void)updateProgressPanel:(NSDictionary *)dict
{
    VLCAssertIsMainThread();

    assert(_currentProgressBarPanel);
    [_currentProgressBarPanel setDialogMessage:[dict objectForKey:@"text"] ?: @""];
    [_currentProgressBarPanel setProgressAsDouble:[[dict objectForKey:@"value"] doubleValue] * 1000.];
}

- (void)destroyProgressPanel
{
    VLCAssertIsMainThread();

    [_currentProgressBarPanel close];
    [_currentProgressBarPanel release];
    _currentProgressBarPanel = nil;
}

- (NSNumber *)checkProgressPanel
{
    VLCAssertIsMainThread();

    return [NSNumber numberWithBool:[_currentProgressBarPanel isCancelled]];
}

#pragma mark -
#pragma mark Extensions Dialog

- (void)triggerClick:(id)sender
{
    assert([sender isKindOfClass:[VLCDialogButton class]]);
    VLCDialogButton *button = sender;
    extension_widget_t *widget = [button widget];

    NSLog(@"(triggerClick)");
    vlc_mutex_lock(&widget->p_dialog->lock);
    extension_WidgetClicked(widget->p_dialog, widget);
    vlc_mutex_unlock(&widget->p_dialog->lock);
}

- (void)syncTextField:(NSNotification *)notifcation
{
    id sender = [notifcation object];
    assert([sender isKindOfClass:[VLCDialogTextField class]]);
    VLCDialogTextField *field = sender;
    extension_widget_t *widget = [field widget];

    vlc_mutex_lock(&widget->p_dialog->lock);
    free(widget->psz_text);
    widget->psz_text = strdup([[field stringValue] UTF8String]);
    vlc_mutex_unlock(&widget->p_dialog->lock);
}

- (void)tableViewSelectionDidChange:(NSNotification *)notifcation
{
    id sender = [notifcation object];
    assert(sender && [sender isKindOfClass:[VLCDialogList class]]);
    VLCDialogList *list = sender;

    struct extension_widget_value_t *value;
    unsigned i = 0;
    for(value = [list widget]->p_values; value != NULL; value = value->p_next, i++)
        value->b_selected = (i == [list selectedRow]);
}

- (void)popUpSelectionChanged:(id)sender
{
    assert([sender isKindOfClass:[VLCDialogPopUpButton class]]);
    VLCDialogPopUpButton *popup = sender;
    struct extension_widget_value_t *value;
    unsigned i = 0;
    for(value = [popup widget]->p_values; value != NULL; value = value->p_next, i++)
        value->b_selected = (i == [popup indexOfSelectedItem]);

}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)frameSize
{
    NSView *contentView = [sender contentView];
    assert([contentView isKindOfClass:[VLCDialogGridView class]]);
    VLCDialogGridView *gridView = (VLCDialogGridView *)contentView;

    NSRect rect = NSMakeRect(0, 0, 0, 0);
    rect.size = frameSize;
    rect = [sender contentRectForFrameRect:rect];
    rect.size = [gridView flexSize:rect.size];
    rect = [sender frameRectForContentRect:rect];
    return rect.size;
}

- (BOOL)windowShouldClose:(id)sender
{
    assert([sender isKindOfClass:[VLCDialogWindow class]]);
    VLCDialogWindow *window = sender;
    extension_dialog_t *dialog = [window dialog];
    extension_DialogClosed(dialog);
    dialog->p_sys_intf = NULL;
    return YES;
}

static NSView *createControlFromWidget(extension_widget_t *widget, id self)
{
    assert(!widget->p_sys_intf);

    switch (widget->type)
    {
        case EXTENSION_WIDGET_HTML:
        {
//            NSScrollView *scrollView = [[NSScrollView alloc] init];
//            [scrollView setHasVerticalScroller:YES];
//            NSTextView *field = [[NSTextView alloc] init];
//            [scrollView setDocumentView:field];
//            [scrollView setAutoresizesSubviews:YES];
//            [scrollView setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
//            [field release];
//            return scrollView;
            NSTextView *field = [[NSTextView alloc] init];
            [field setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
            [field setDrawsBackground:NO];
            return field;
        }
        case EXTENSION_WIDGET_LABEL:
        {
            NSTextField *field = [[NSTextField alloc] init];
            [field setEditable:NO];
            [field setBordered:NO];
            [field setDrawsBackground:NO];
            [field setFont:[NSFont systemFontOfSize:0]];
            [[field cell] setControlSize:NSRegularControlSize];
            [field setAutoresizingMask:NSViewNotSizable];
            return field;
        }
        case EXTENSION_WIDGET_TEXT_FIELD:
        {
            VLCDialogTextField *field = [[VLCDialogTextField alloc] init];
            [field setWidget:widget];
            [field setAutoresizingMask:NSViewWidthSizable];
            [field setFont:[NSFont systemFontOfSize:0]];
            [[field cell] setControlSize:NSRegularControlSize];
            [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(syncTextField:)  name:NSControlTextDidChangeNotification object:field];
            return field;
        }
        case EXTENSION_WIDGET_BUTTON:
        {
            VLCDialogButton *button = [[VLCDialogButton alloc] init];
            [button setBezelStyle:NSRoundedBezelStyle];
            [button setWidget:widget];
            [button setAction:@selector(triggerClick:)];
            [button setTarget:self];
            [[button cell] setControlSize:NSRegularControlSize];
            [button setAutoresizingMask:NSViewNotSizable];
            return button;
        }
        case EXTENSION_WIDGET_DROPDOWN:
        {
            VLCDialogPopUpButton *popup = [[VLCDialogPopUpButton alloc] init];
            [popup setAction:@selector(popUpSelectionChanged:)];
            [popup setTarget:self];
            [popup setWidget:widget];
            return popup;
        }
        case EXTENSION_WIDGET_LIST:
        {
            NSScrollView *scrollView = [[NSScrollView alloc] init];
            [scrollView setHasVerticalScroller:YES];
            VLCDialogList *list = [[VLCDialogList alloc] init];
            [list setUsesAlternatingRowBackgroundColors:YES];
            [list setHeaderView:nil];
            [scrollView setDocumentView:list];
            [scrollView setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];

            NSTableColumn *column = [[NSTableColumn alloc] init];
            [list addTableColumn:column];
            [column release];
            [list setDataSource:list];
            [list setDelegate:self];
            [list setWidget:widget];
            [list release];
            return scrollView;
        }
        case EXTENSION_WIDGET_IMAGE:
        {
            NSImageView *imageView = [[NSImageView alloc] init];
            [imageView setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
            [imageView setImageFrameStyle:NSImageFramePhoto];
            [imageView setImageScaling:NSImageScaleProportionallyUpOrDown];
            return imageView;
        }
        default:
            assert(0);
            return nil;
    }

}

static void updateControlFromWidget(NSView *control, extension_widget_t *widget, id self)
{
    switch (widget->type)
    {
        case EXTENSION_WIDGET_HTML:
//        {
//            // Get the scroll view
//            assert([control isKindOfClass:[NSScrollView class]]);
//            NSScrollView *scrollView = (NSScrollView *)control;
//            control = [scrollView documentView];
//
//            assert([control isKindOfClass:[NSTextView class]]);
//            NSTextView *textView = (NSTextView *)control;
//            NSString *string = [NSString stringWithUTF8String:widget->psz_text];
//            NSAttributedString *attrString = [[NSAttributedString alloc] initWithHTML:[string dataUsingEncoding:NSUTF8StringEncoding] documentAttributes:NULL];
//            [[textView textStorage] setAttributedString:[[NSAttributedString alloc] initWithString:@"Hello"]];
//            NSLog(@"%@", string);
//            [textView setNeedsDisplay:YES];
//            [textView scrollRangeToVisible:NSMakeRange(0, 0)];
//            [attrString release];
//            break;
//
//        }
        {
            assert([control isKindOfClass:[NSTextView class]]);
            NSTextView *textView = (NSTextView *)control;
            NSString *string = [NSString stringWithUTF8String:widget->psz_text];
            NSAttributedString *attrString = [[NSAttributedString alloc] initWithHTML:[string dataUsingEncoding:NSUTF8StringEncoding] documentAttributes:NULL];
            [[textView textStorage] setAttributedString:attrString];
            [textView setNeedsDisplay:YES];
            [textView scrollRangeToVisible:NSMakeRange(0, 0)];
            [attrString release];
            break;

        }
        case EXTENSION_WIDGET_LABEL:
        case EXTENSION_WIDGET_PASSWORD:
        case EXTENSION_WIDGET_TEXT_FIELD:
        {
            if (!widget->psz_text)
                break;
            assert([control isKindOfClass:[NSControl class]]);
            NSControl *field = (NSControl *)control;
            NSString *string = [NSString stringWithUTF8String:widget->psz_text];
            NSAttributedString *attrString = [[NSAttributedString alloc] initWithHTML:[string dataUsingEncoding:NSUTF8StringEncoding] documentAttributes:NULL];
            [field setAttributedStringValue:attrString];
            [attrString release];
            break;
        }
        case EXTENSION_WIDGET_BUTTON:
        {
            assert([control isKindOfClass:[NSButton class]]);
            NSButton *button = (NSButton *)control;
            if (!widget->psz_text)
                break;
            [button setTitle:[NSString stringWithUTF8String:widget->psz_text]];
            break;
        }
        case EXTENSION_WIDGET_DROPDOWN:
        {
            assert([control isKindOfClass:[NSPopUpButton class]]);
            NSPopUpButton *popup = (NSPopUpButton *)control;
            [popup removeAllItems];
            struct extension_widget_value_t *value;
            for(value = widget->p_values; value != NULL; value = value->p_next)
            {
                [popup addItemWithTitle:[NSString stringWithUTF8String:value->psz_text]];
            }
            [popup synchronizeTitleAndSelectedItem];
            [self popUpSelectionChanged:popup];
            break;
        }

        case EXTENSION_WIDGET_LIST:
        {
            assert([control isKindOfClass:[NSScrollView class]]);
            NSScrollView *scrollView = (NSScrollView *)control;
            assert([[scrollView documentView] isKindOfClass:[VLCDialogList class]]);
            VLCDialogList *list = (VLCDialogList *)[scrollView documentView];

            NSMutableArray *contentArray = [NSMutableArray array];
            struct extension_widget_value_t *value;
            for(value = widget->p_values; value != NULL; value = value->p_next)
            {
                NSDictionary *entry = [NSDictionary dictionaryWithObjectsAndKeys:
                                       [NSNumber numberWithInt:value->i_id], @"id",
                                       [NSString stringWithUTF8String:value->psz_text], @"text",
                                       nil];
                [contentArray addObject:entry];
            }
            list.contentArray = contentArray;
            [list reloadData];
            break;
        }
        case EXTENSION_WIDGET_IMAGE:
        {
            assert([control isKindOfClass:[NSImageView class]]);
            NSImageView *imageView = (NSImageView *)control;
            NSString *string = widget->psz_text ? [NSString stringWithUTF8String:widget->psz_text] : nil;
            NSImage *image = nil;
            NSLog(@"Setting image to %@", string);
            if (string)
                image = [[NSImage alloc] initWithContentsOfURL:[NSURL fileURLWithPath:string]];
            [imageView setImage:image];
            [image release];
            break;
        }
    }

}

- (void)updateWidgets:(extension_dialog_t *)dialog
{
    extension_widget_t *widget;
    NSWindow *window = dialog->p_sys_intf;
    FOREACH_ARRAY(widget, dialog->widgets)
    {
        if (!widget)
            continue; /* Some widgets may be NULL at this point */

        BOOL shouldDestroy = widget->b_kill;
        NSView *control = widget->p_sys_intf;
        BOOL update = widget->b_update;


        if (!control && !shouldDestroy)
        {
            control = createControlFromWidget(widget, self);
            updateControlFromWidget(control, widget, self);
            widget->p_sys_intf = control;
            update = YES; // Force update and repositionning
            [control setHidden:widget->b_hide];
        }

        if (update && !shouldDestroy)
        {
            updateControlFromWidget(control, widget, self);
            [control setHidden:widget->b_hide];

            int row = widget->i_row - 1;
            int col = widget->i_column - 1;
            int hsp = __MAX( 1, widget->i_horiz_span );
            int vsp = __MAX( 1, widget->i_vert_span );
            if( row < 0 )
            {
                row = 4;
                col = 0;
            }

            VLCDialogGridView *gridView = (VLCDialogGridView *)[window contentView];
            [gridView addSubview:control atRow:row column:col rowSpan:vsp colSpan:hsp];

            //this->resize( sizeHint() );
            widget->b_update = false;
        }

        if (shouldDestroy)
        {
            VLCDialogGridView *gridView = (VLCDialogGridView *)[window contentView];
            [gridView removeSubview:control];
            [control release];
            widget->p_sys_intf = NULL;
        }
    }
    FOREACH_END()
}

- (void)updateExtensionDialog:(NSValue *)extensionDialog
{
    extension_dialog_t *dialog = [extensionDialog pointerValue];

    vlc_mutex_lock(&dialog->lock);

    NSSize size = NSMakeSize(dialog->i_width, dialog->i_height);

    BOOL shouldDestroy = dialog->b_kill;

    if (!dialog->i_width || !dialog->i_height)
        size = NSMakeSize(640, 480);

    VLCDialogWindow *window = dialog->p_sys_intf;
    if (!window && !shouldDestroy)
    {
        NSRect content = NSMakeRect(0, 0, 1, 1);
        window = [[VLCDialogWindow alloc] initWithContentRect:content styleMask:NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask backing:NSBackingStoreBuffered defer:NO];
        [window setDelegate:self];
        [window setDialog:dialog];
        [window setTitle:[NSString stringWithUTF8String:dialog->psz_title]];
        VLCDialogGridView *gridView = [[VLCDialogGridView alloc] init];
        [gridView setAutoresizingMask:NSViewHeightSizable | NSViewWidthSizable];
        [window setContentView:gridView];
        [gridView release];
        dialog->p_sys_intf = window;
    }

    [self updateWidgets:dialog];

    if (shouldDestroy)
    {
        [window setDelegate:nil];
        [window close];
        dialog->p_sys_intf = NULL;
        window = nil;
    }

    if (![window isVisible]) {
        [window center];
        [window makeKeyAndOrderFront:self];
    }

    vlc_cond_signal(&dialog->cond);
    vlc_mutex_unlock(&dialog->lock);
}


/**
 * Helper to execute a function on main thread and get its return value.
 */
- (void)execute:(NSDictionary *)dict
{
    SEL sel = [[dict objectForKey:@"sel"] pointerValue];
    id *result = [[dict objectForKey:@"result"] pointerValue];
    id object = [dict objectForKey:@"object"];

    NSAssert(sel, @"Try to execute a NULL selector");

    *result = [self performSelector:sel withObject:object];
    [*result retain]; // Balanced in -resultFromSelectorOnMainThread
}

- (id)resultFromSelectorOnMainThread:(SEL)sel withObject:(id)object
{
    id result = nil;
    NSAssert(sel, @"Try to execute a NULL selector");
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
     [NSValue valueWithPointer:sel], @"sel",
     [NSValue valueWithPointer:&result], @"result",
     object, @"object", nil];
    [self performSelectorOnMainThread:@selector(execute:) withObject:dict waitUntilDone:YES];
    return [result autorelease];
}
@end



